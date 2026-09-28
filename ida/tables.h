/* tables.h - the data contract between the host table generator, the C
 * search and the RV32I assembly search.
 *
 * gen_tables (host) fills these tables and writes them to tables.c and
 * tables.s; the search on the target only reads them. Every convention the
 * assembly must reproduce is written down here. If the assembly indexes a
 * table differently it silently reads another entry, h may overestimate,
 * and the search can return a solution that works but is not the shortest.
 *
 * State: p[i] = cubie in position i, o[i] = its twist (0..2); positions and
 * cubies are numbered 0..6, which is input label 1..7 minus one.
 *
 *   p   Lehmer rank of p[0..6] = sum of c_i * (6-i)!, where c_i counts the
 *       j > i with p[j] < p[i].                     solved 0, range 0..5039
 *   o   base 3 of o[0..5], o[0] most significant.   solved 0, range 0..728
 *   cp  where the tracked cubies 0,1,2,3 (labels 1-4) are: pos[k] is the
 *       position of cubie k, digit r_k = pos[k] minus the number of j < k
 *       with pos[j] < pos[k], cp = r0*120 + r1*20 + r2*4 + r3.
 *                                                   solved 0, range 0..839
 *   co  their twists t_k in cubie order, t0 most significant:
 *       co = t0*27 + t1*9 + t2*3 + t3.              solved 0, range 0..80
 *
 * One quarter turn (90 degrees clockwise) of face f, 0 = R, 1 = B, 2 = D:
 *   p'  = perm_q[f][p]                o' = ori_q[f][o]
 *   co' = add81[co][c4tw_q[f][cp]]    cp' = c4pos_q[f][cp]   (both use old cp)
 * Applying it once, twice, three times gives move f*3+0, f*3+1, f*3+2,
 * named X, X2, X'. Faces are tried in the order R, B, D and each face in
 * the order 90, 180, 270 degrees; node counts depend on this order.
 *
 * Heuristic: h = max(perm_h[p], ori_h[o], c4_row[co][cp]); solved when
 * p == 0 and o == 0.
 */
#ifndef TABLES_H
#define TABLES_H

#include <stdint.h>

#include "cube.h"

/* 1: h also uses the 4-corner PDB (three PDBs in all). 0: two PDBs only. */
#ifndef CORNER_PDB
#define CORNER_PDB 1
#endif

enum { C4POS = 840, C4ORI = 81, ADD81_COLS = 128 };

extern const uint16_t perm_q[3][PERMS]; /* 30,240 B */
extern const uint16_t ori_q[3][ORIS];   /*  4,374 B */
extern const uint8_t perm_h[PERMS];     /*  5,040 B */
extern const uint8_t ori_h[ORIS];       /*    729 B */
#if CORNER_PDB
extern const uint16_t c4pos_q[3][C4POS];          /*  5,040 B */
extern const uint8_t c4tw_q[3][C4POS];            /*  2,520 B */
extern const uint8_t add81[C4ORI][ADD81_COLS];    /* 10,368 B, columns 81..127 unused */
extern const uint8_t c4_h[C4ORI][C4POS];          /* 68,040 B, laid out [co][cp] */
extern const uint8_t *const c4_row[C4ORI];        /*    324 B on RV32: c4_h[co] */
#endif
extern const uint32_t tables_bytes; /* total of the above on RV32 */

/* ---- Ranking ----
 * The generator uses these to fill the tables; the search uses them once
 * per query to turn the input state into its start node. They add
 * precomputed weights instead of multiplying, as the assembly would. */

static const uint16_t fact_w[CUBIES] = {720, 120, 24, 6, 2, 1, 0};
static const uint16_t pow3_w[6] = {243, 81, 27, 9, 3, 1};

static inline uint16_t perm_rank(const uint8_t *p)
{
    uint16_t r = 0;
    for (int i = 0; i < CUBIES - 1; i++)
        for (int j = i + 1; j < CUBIES; j++)
            if (p[j] < p[i])
                r = (uint16_t) (r + fact_w[i]);
    return r;
}

static inline uint16_t ori_rank(const uint8_t *o)
{
    uint16_t r = 0;
    for (int i = 0; i < 6; i++)
        for (uint8_t k = 0; k < o[i]; k++)
            r = (uint16_t) (r + pow3_w[i]);
    return r;
}

#if CORNER_PDB
static const uint8_t c4_cubies[4] = {0, 1, 2, 3}; /* the tracked corners */
static const uint8_t c4_w[4] = {120, 20, 4, 1};   /* mixed radix 7, 6, 5, 4 */
static const uint8_t pow3_4[4] = {27, 9, 3, 1};

static inline uint16_t c4_rank(const uint8_t *pos)
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

static inline uint8_t tw_rank(const uint8_t *t)
{
    uint8_t v = 0;
    for (int k = 0; k < 4; k++)
        for (uint8_t i = 0; i < t[k]; i++)
            v = (uint8_t) (v + pow3_4[k]);
    return v;
}
#endif

#endif
