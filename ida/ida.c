/* ida.c - target-side solver. Everything in this file is what would later
 * run on RV32I.
 *
 * The search never builds a cube: a node is a pair of coordinates, a move
 * is one table lookup per coordinate, and the heuristic is one lookup per
 * PDB followed by a max. Nothing here divides or takes a modulo, and every
 * multiply is by a constant: array row offsets such as perm_q[face][...]
 * (face * 10080 bytes; the RV32I translation can keep one row pointer per
 * face instead).
 * Ranking uses additions of precomputed weights instead of multiplies.
 */
#include <string.h>

#include "ida.h"

/* ---- Data: this is the whole table footprint on the target ---- */

uint16_t perm_q[3][PERMS]; /* 30,240 B: permutation rank after a quarter turn */
uint16_t ori_q[3][ORIS];   /*  4,374 B: orientation rank after a quarter turn */
uint8_t perm_h[PERMS];     /*  5,040 B: distance in the permutation-only puzzle */
uint8_t ori_h[ORIS];       /*    729 B: distance in the orientation-only puzzle */


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


void ida_init(void)
{
    build_move_tables();
    build_pdb(perm_h, PERMS, &perm_q[0][0]);
    build_pdb(ori_h, ORIS, &ori_q[0][0]);
}

size_t ida_table_bytes(void)
{
    size_t n = sizeof perm_q + sizeof ori_q + sizeof perm_h + sizeof ori_h;
    return n;
}

/* ---- Nodes ---- */

node_t node_from_state(const state_t *s) /* once per query, not in the search */
{
    node_t n;
    n.p = perm_rank(s->p);
    n.o = ori_rank(s->o);
    return n;
}

node_t node_quarter(node_t n, int face)
{
    node_t r;
    r.p = perm_q[face][n.p];
    r.o = ori_q[face][n.o];
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
