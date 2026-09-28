/* build.c - host-side construction of the transition tables and PDBs.
 * Runs on the host only (inside gen_tables and verify); the target never
 * executes any of it, it links the result as read-only data.
 */
#include <string.h>

#include "build.h"

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

/* ---- Transition tables for p and o ---- */

static void build_move_tables(tables_t *t)
{
    uint8_t p[CUBIES] = {0, 1, 2, 3, 4, 5, 6}, q[CUBIES];

    /* p runs through the permutations in rank order 0, 1, 2, ... */
    for (uint16_t r = 0; r < PERMS; r++) {
        for (int f = 0; f < 3; f++) {
            for (int i = 0; i < CUBIES; i++)
                q[i] = p[source[f][i]];
            t->perm_q[f][r] = perm_rank(q);
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
            t->ori_q[f][r] = ori_rank(q);
        }
        for (int i = 5; i >= 0; i--) {
            if (++o[i] < 3)
                break;
            o[i] = 0;
        }
    }
}

/* ---- PDBs for p and o ----
 * Breadth-first search from the solved abstract state (index 0) by level
 * sweep: one pass over the table per depth, 0xFF meaning unvisited. q holds
 * three rows of n entries, one per face; applying a row once, twice and
 * three times gives the 90, 180 and 270 degree turns. */
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
                for (int k = 0; k < 3; k++) {
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
/* ---- 4-corner tables ----
 * Abstract state: where the 4 tracked corners are (an ordered choice of 4
 * of the 7 positions, 7*6*5*4 = 840) and their twists (3^4 = 81). A quarter
 * turn moves each tracked corner to dst[face][pos] and adds the twist of its
 * new position, so the new twists are the old ones plus a vector that
 * depends only on the old positions: c4tw_q holds that vector, add81 adds. */

static void tw_digits(uint8_t v, uint8_t *d)
{
    for (int k = 0; k < 4; k++)
        for (d[k] = 0; v >= pow3_4[k]; d[k]++)
            v = (uint8_t) (v - pow3_4[k]);
}

static uint16_t c4_goal(void) /* solved: corner c sits in position c */
{
    return c4_rank(c4_cubies);
}

static void build_c4_tables(tables_t *t)
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
                        t->c4pos_q[f][a] = c4_rank(np);
                        t->c4tw_q[f][a] = tw_rank(tw);
                    }
                }

    memset(t->add81, 0, sizeof t->add81); /* padding columns stay 0 */
    for (uint8_t x = 0; x < C4ORI; x++)
        for (uint8_t y = 0; y < C4ORI; y++) {
            uint8_t dx[4], dy[4], s[4];
            tw_digits(x, dx);
            tw_digits(y, dy);
            for (int k = 0; k < 4; k++)
                s[k] = mod3((uint8_t) (dx[k] + dy[k]));
            t->add81[x][y] = tw_rank(s);
        }
}

static void build_c4_pdb(tables_t *t) /* same level sweep, over (co, cp) */
{
    memset(t->c4_h, 0xFF, sizeof t->c4_h);
    t->c4_h[0][c4_goal()] = 0; /* twists all zero, corners home */
    for (uint8_t d = 0;; d++) {
        uint32_t found = 0;
        for (uint8_t co = 0; co < C4ORI; co++)
            for (uint16_t cp = 0; cp < C4POS; cp++) {
                if (t->c4_h[co][cp] != d)
                    continue;
                for (int f = 0; f < 3; f++) {
                    uint16_t p = cp;
                    uint8_t o = co;
                    for (int k = 0; k < 3; k++) {
                        o = t->add81[o][t->c4tw_q[f][p]]; /* uses the old p */
                        p = t->c4pos_q[f][p];
                        if (t->c4_h[o][p] == 0xFF) {
                            t->c4_h[o][p] = (uint8_t) (d + 1);
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

void build_tables(tables_t *t)
{
    build_move_tables(t);
    build_pdb(t->perm_h, PERMS, &t->perm_q[0][0]);
    build_pdb(t->ori_h, ORIS, &t->ori_q[0][0]);
#if CORNER_PDB
    build_c4_tables(t);
    build_c4_pdb(t);
#endif
}
