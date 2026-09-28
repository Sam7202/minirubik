/* ida.c - target-side solver. Everything in this file is what would later
 * run on RV32I.
 *
 * The search never builds a cube: a node is a pair of coordinates, a move
 * is one table lookup per coordinate, and the heuristic is one lookup per
 * PDB followed by a max. Nothing here divides or takes a modulo, and every
 * multiply is by a constant: array row offsets such as perm_q[face][...]
 * (face * 10080 bytes; the RV32I translation can keep one row pointer per
 * face instead) and, with CORNER_PDB, cp * 81 (three shifts, two adds).
 * Ranking uses additions of precomputed weights instead of multiplies.
 */
#include <string.h>

#include "ida.h"

/* ---- Data: this is the whole table footprint on the target ---- */

uint16_t perm_q[3][PERMS]; /* 30,240 B: permutation rank after a quarter turn */
uint16_t ori_q[3][ORIS];   /*  4,374 B: orientation rank after a quarter turn */
uint8_t perm_h[PERMS];     /*  5,040 B: distance in the permutation-only puzzle */
uint8_t ori_h[ORIS];       /*    729 B: distance in the orientation-only puzzle */

#if CORNER_PDB
uint16_t c4pos_q[3][C4POS];  /*  5,040 B: tracked corners' positions after a turn */
uint8_t c4tw_q[3][C4POS];    /*  2,520 B: twist that turn adds to them, base 3 */
uint8_t add81[C4ORI][C4ORI]; /*  6,561 B: digit-wise base-3 sum of two twist vectors */
uint8_t c4_h[C4SIZE];        /* 68,040 B: distance in the 4-corner puzzle */

static const uint8_t c4_cubies[4] = {0, 1, 2, 3}; /* the corners it tracks */
static uint16_t c4_goal; /* their arrangement index in the solved cube */
#endif

uint32_t ida_expanded, ida_generated;

/* ---- Ranking without multiply or divide ----
 * Lehmer rank = sum over i of c_i * (6 - i)!, where c_i counts later
 * entries smaller than p[i]. Adding the weight once per such entry gives
 * the same sum with additions only. Identical result to solver.c. */

static const uint16_t fact_w[CUBIES] = {720, 120, 24, 6, 2, 1, 0};
static const uint16_t pow3_w[6] = {243, 81, 27, 9, 3, 1};

static uint16_t perm_rank(const uint8_t *p)
{
    uint16_t r = 0;
    for (int i = 0; i < CUBIES - 1; i++)
        for (int j = i + 1; j < CUBIES; j++)
            if (p[j] < p[i])
                r = (uint16_t) (r + fact_w[i]);
    return r;
}

/* Base-3 rank of o[0..5], most significant first, as solver.c. */
static uint16_t ori_rank(const uint8_t *o)
{
    uint16_t r = 0;
    for (int i = 0; i < 6; i++)
        for (uint8_t k = 0; k < o[i]; k++)
            r = (uint16_t) (r + pow3_w[i]);
    return r;
}

static uint8_t mod3(uint8_t v) /* v is at most 12 wherever this is called */
{
    while (v >= 3)
        v = (uint8_t) (v - 3);
    return v;
}

/* Step a to the next permutation in lexicographic order. Lexicographic
 * order is Lehmer-rank order, so stepping replaces unranking. */
static void next_perm(uint8_t *a)
{
    int i = CUBIES - 2;
    while (i >= 0 && a[i] > a[i + 1])
        i--;
    if (i < 0)
        return;
    int j = CUBIES - 1;
    while (a[j] < a[i])
        j--;
    uint8_t t = a[i];
    a[i] = a[j];
    a[j] = t;
    for (int l = i + 1, r = CUBIES - 1; l < r; l++, r--) {
        t = a[l];
        a[l] = a[r];
        a[r] = t;
    }
}

/* ---- Coordinate move tables ---- */

static void build_move_tables(void)
{
    uint8_t p[CUBIES] = {0, 1, 2, 3, 4, 5, 6}, q[CUBIES];

    /* p runs through the permutations in rank order 0, 1, 2, ... */
    for (uint16_t r = 0; r < PERMS; r++) {
        for (int f = 0; f < 3; f++) {
            for (int i = 0; i < CUBIES; i++)
                q[i] = p[source[f][i]];
            perm_q[f][r] = perm_rank(q);
        }
        next_perm(p);
    }

    /* o[0..5] counts in base 3 like an odometer, so it also visits the
     * orientation ranks in order; o[6] makes the twist sum a multiple of 3. */
    uint8_t o[CUBIES] = {0};
    for (uint16_t r = 0; r < ORIS; r++) {
        uint8_t sum = 0;
        for (int i = 0; i < 6; i++)
            sum = (uint8_t) (sum + o[i]);
        sum = mod3(sum);
        o[6] = sum ? (uint8_t) (3 - sum) : 0;
        for (int f = 0; f < 3; f++) {
            for (int i = 0; i < CUBIES; i++)
                q[i] = mod3((uint8_t) (o[source[f][i]] + twist[f][i]));
            ori_q[f][r] = ori_rank(q);
        }
        for (int i = 5; i >= 0; i--) {
            if (++o[i] < 3)
                break;
            o[i] = 0;
        }
    }
}

/* ---- Pattern databases ----
 * Breadth-first search from the solved abstract state (index 0) by level
 * sweep: no queue, just one pass over the table per depth, 0xFF meaning
 * unvisited. q holds three rows of n entries, one row per face; applying a
 * row once, twice and three times gives the 90, 180 and 270 degree turns. */
static void build_pdb(uint8_t *h, uint16_t n, const uint16_t *q)
{
    memset(h, 0xFF, n);
    h[0] = 0;
    for (uint8_t d = 0;; d++) {
        uint16_t found = 0;
        for (uint16_t x = 0; x < n; x++) {
            if (h[x] != d)
                continue;
            const uint16_t *row = q;
            for (int f = 0; f < 3; f++, row += n) {
                uint16_t y = x;
                for (int t = 0; t < 3; t++) {
                    y = row[y];
                    if (h[y] == 0xFF) {
                        h[y] = (uint8_t) (d + 1);
                        found++;
                    }
                }
            }
        }
        if (!found)
            break;
    }
}

#if CORNER_PDB
/* ---- 4-corner PDB (fallback) ----
 * Abstract state: where the 4 tracked corners are (an ordered choice of 4
 * of the 7 positions, 7*6*5*4 = 840) and their twists (3^4 = 81). A quarter
 * turn moves each tracked corner to dst[face][pos] and adds the twist of its
 * new position, so the new twists are the old ones plus a vector that
 * depends only on the old positions: c4tw_q holds that vector, add81 adds. */

static const uint8_t c4_w[4] = {120, 20, 4, 1}; /* mixed radix 7, 6, 5, 4 */
static const uint8_t pow3_4[4] = {27, 9, 3, 1};

/* Digit k is pos[k]'s rank among positions not used by pos[0..k-1]. */
static uint16_t c4_rank(const uint8_t *pos)
{
    uint16_t a = 0;
    for (int k = 0; k < 4; k++) {
        uint8_t r = pos[k];
        for (int j = 0; j < k; j++)
            if (pos[j] < pos[k])
                r--;
        for (; r; r--)
            a = (uint16_t) (a + c4_w[k]);
    }
    return a;
}

static uint8_t tw_rank(const uint8_t *t) /* base 3, t[0] most significant */
{
    uint8_t v = 0;
    for (int k = 0; k < 4; k++)
        for (uint8_t i = 0; i < t[k]; i++)
            v = (uint8_t) (v + pow3_4[k]);
    return v;
}

static void tw_digits(uint8_t v, uint8_t *t)
{
    for (int k = 0; k < 4; k++)
        for (t[k] = 0; v >= pow3_4[k]; t[k]++)
            v = (uint8_t) (v - pow3_4[k]);
}

static void build_c4_tables(void)
{
    uint8_t dst[3][CUBIES]; /* dst[f][j]: where a quarter turn of f sends j */
    for (int f = 0; f < 3; f++)
        for (int i = 0; i < CUBIES; i++)
            dst[f][source[f][i]] = (uint8_t) i;

    /* Enumerate arrangements in rank order: digit r[k] picks the r[k]-th
     * smallest position not yet taken. */
    uint16_t a = 0;
    uint8_t r[4];
    for (r[0] = 0; r[0] < 7; r[0]++)
        for (r[1] = 0; r[1] < 6; r[1]++)
            for (r[2] = 0; r[2] < 5; r[2]++)
                for (r[3] = 0; r[3] < 4; r[3]++, a++) {
                    uint8_t pos[4], used = 0;
                    for (int k = 0; k < 4; k++) {
                        uint8_t x = 0, skip = r[k];
                        for (;; x++) {
                            if ((used >> x) & 1)
                                continue;
                            if (!skip)
                                break;
                            skip--;
                        }
                        pos[k] = x;
                        used = (uint8_t) (used | (1u << x));
                    }
                    for (int f = 0; f < 3; f++) {
                        uint8_t np[4], tw[4];
                        for (int k = 0; k < 4; k++) {
                            np[k] = dst[f][pos[k]];
                            tw[k] = twist[f][np[k]];
                        }
                        c4pos_q[f][a] = c4_rank(np);
                        c4tw_q[f][a] = tw_rank(tw);
                    }
                }

    for (uint8_t x = 0; x < C4ORI; x++)
        for (uint8_t y = 0; y < C4ORI; y++) {
            uint8_t dx[4], dy[4], s[4];
            tw_digits(x, dx);
            tw_digits(y, dy);
            for (int k = 0; k < 4; k++)
                s[k] = mod3((uint8_t) (dx[k] + dy[k]));
            add81[x][y] = tw_rank(s);
        }

    c4_goal = c4_rank(c4_cubies); /* solved: corner c sits in position c */
}

static void build_c4_pdb(void) /* same level sweep, over (cp, co) pairs */
{
    memset(c4_h, 0xFF, sizeof c4_h);
    c4_h[(uint32_t) c4_goal * C4ORI] = 0; /* twists all zero */
    for (uint8_t d = 0;; d++) {
        uint32_t found = 0, x = 0;
        for (uint16_t cp = 0; cp < C4POS; cp++)
            for (uint8_t co = 0; co < C4ORI; co++, x++) {
                if (c4_h[x] != d)
                    continue;
                for (int f = 0; f < 3; f++) {
                    uint16_t p = cp;
                    uint8_t o = co;
                    for (int t = 0; t < 3; t++) {
                        o = add81[o][c4tw_q[f][p]]; /* uses the old p */
                        p = c4pos_q[f][p];
                        uint32_t y = (uint32_t) p * C4ORI + o;
                        if (c4_h[y] == 0xFF) {
                            c4_h[y] = (uint8_t) (d + 1);
                            found++;
                        }
                    }
                }
            }
        if (!found)
            break;
    }
}
#endif

void ida_init(void)
{
    build_move_tables();
    build_pdb(perm_h, PERMS, &perm_q[0][0]);
    build_pdb(ori_h, ORIS, &ori_q[0][0]);
#if CORNER_PDB
    build_c4_tables();
    build_c4_pdb();
#endif
}

size_t ida_table_bytes(void)
{
    size_t n = sizeof perm_q + sizeof ori_q + sizeof perm_h + sizeof ori_h;
#if CORNER_PDB
    n += sizeof c4pos_q + sizeof c4tw_q + sizeof add81 + sizeof c4_h;
#endif
    return n;
}

/* ---- Nodes ---- */

node_t node_from_state(const state_t *s) /* once per query, not in the search */
{
    node_t n;
    n.p = perm_rank(s->p);
    n.o = ori_rank(s->o);
#if CORNER_PDB
    uint8_t pos[4] = {0}, tw[4] = {0};
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < CUBIES; i++)
            if (s->p[i] == c4_cubies[k]) {
                pos[k] = (uint8_t) i;
                tw[k] = s->o[i];
            }
    n.cp = c4_rank(pos);
    n.co = tw_rank(tw);
#endif
    return n;
}

node_t node_quarter(node_t n, int face)
{
    node_t r;
    r.p = perm_q[face][n.p];
    r.o = ori_q[face][n.o];
#if CORNER_PDB
    r.co = add81[n.co][c4tw_q[face][n.cp]]; /* twist depends on the old cp */
    r.cp = c4pos_q[face][n.cp];
#endif
    return r;
}

/* Each PDB is an exact distance in a smaller puzzle that every real move
 * sequence also solves, so each is a lower bound, and so is their max.
 * Their sum is not: every R or B turn changes both coordinates at once. */
uint8_t node_h(node_t n)
{
    uint8_t h = perm_h[n.p], x = ori_h[n.o];
    if (x > h)
        h = x;
#if CORNER_PDB
    x = c4_h[(uint32_t) n.cp * C4ORI + n.co]; /* *81 = (cp<<6)+(cp<<4)+cp */
    if (x > h)
        h = x;
#endif
    return h;
}

/* ---- IDA* ---- */

static uint8_t *path, bound, found_len;

/* Expand n, which sits at depth g, has g + h(n) <= bound and is not solved.
 * Each child is tested here, before any call: most children fail the
 * bound test in the last iteration, and rejecting them in the parent saves
 * a call, a stack frame and a return per rejected child.
 *
 * last is the face turned to reach n. Turning it again is never optimal
 * (two turns of one face merge into one move or none), which cuts the
 * branching factor from 9 to 6. R, B and D share corners pairwise, so no
 * two of them commute and nothing more can be pruned that way.
 * Since g + h(n) <= bound, h(n) >= 1 and bound <= MAX_DEPTH, g stays below
 * MAX_DEPTH and path[g] is in range. */
static int dfs(node_t n, uint8_t g, int last)
{
    ida_expanded++;
    uint8_t m = 0; /* move index face * 3, kept by addition */
    for (int f = 0; f < 3; f++, m = (uint8_t) (m + 3)) {
        if (f == last)
            continue;
        node_t c = n;
        for (uint8_t t = 0; t < 3; t++) {
            c = node_quarter(c, f); /* 90, then 180, then 270 degrees */
            ida_generated++;
            if (g + 1 + node_h(c) > bound)
                continue;
            path[g] = (uint8_t) (m + t);
            if (c.p == 0 && c.o == 0) { /* both coordinates solved */
                found_len = (uint8_t) (g + 1);
                return 1;
            }
            if (dfs(c, (uint8_t) (g + 1), f))
                return 1;
        }
    }
    return 0;
}

int ida_solve(node_t start, uint8_t moves[MAX_DEPTH])
{
    ida_expanded = ida_generated = 0;
    path = moves;
    if (start.p == 0 && start.o == 0)
        return 0;
    for (bound = node_h(start); bound <= MAX_DEPTH; bound++)
        if (dfs(start, 0, -1))
            return found_len;
    return -1;
}
