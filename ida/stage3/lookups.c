/* lookups.c - search cost of the C search on all 2,644 distance-11 states,
 * the raw data for note section 4: expanded nodes, generated nodes and
 * table lookups, mean and maximum. Host only; it links the C search and
 * the generated tables of either build.
 *
 * Every generated child costs the same table reads, one per coordinate
 * update and one per PDB (search.c step() and node_h()):
 *   PDB=3: perm_q, ori_q, c4tw_q, add81, c4pos_q, perm_h, ori_h, c4_row,
 *          c4_h                                              9 reads
 *   PDB=2: perm_q, ori_q, perm_h, ori_h                      4 reads
 * so lookups = generated x reads per child. node_from_state reads a few
 * more once per query; they are left out.
 *
 * True distances come from a breadth-first search over (p, o) with perm_q
 * and ori_q, as in stage2/pdb_merge.c.
 *
 * usage: ./lookups    (PDB=3 build)   ./lookups2    (PDB=2 build)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"

#if CORNER_PDB
enum { READS_PER_CHILD = 9 };
#else
enum { READS_PER_CHILD = 4 };
#endif

static uint8_t *dist; /* true distance of every rank p * ORIS + o */

/* Level sweep from the solved state, 0xFF meaning unvisited. */
static void build_dist(void)
{
    dist = malloc(STATES);
    if (!dist) {
        puts("out of memory");
        exit(1);
    }
    memset(dist, 0xFF, STATES);
    dist[0] = 0;
    for (uint8_t d = 0;; d++) {
        uint32_t found = 0;
        for (uint32_t r = 0; r < STATES; r++) {
            if (dist[r] != d)
                continue;
            for (int f = 0; f < 3; f++) {
                uint16_t p = (uint16_t) (r / ORIS), o = (uint16_t) (r % ORIS);
                for (int t = 0; t < 3; t++) {
                    p = perm_q[f][p];
                    o = ori_q[f][o];
                    uint32_t r2 = (uint32_t) p * ORIS + o;
                    if (dist[r2] == 0xFF) {
                        dist[r2] = (uint8_t) (d + 1);
                        found++;
                    }
                }
            }
        }
        if (!found)
            break;
    }
}

/* Copied from verify.c (reference model, as in solver.c). */
static void ref_unrank(uint32_t rank, state_t *s)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIS, o = rank % ORIS, f = 720;
    unsigned sum = 0;
    for (int i = 0; i < CUBIES; i++) {
        uint32_t q = p / f;
        p %= f;
        s->p[i] = available[q];
        for (uint32_t j = q; j + 1 < (uint32_t) (CUBIES - i); j++)
            available[j] = available[j + 1];
        if (i < 5)
            f /= (uint32_t) (6 - i);
    }
    for (int i = 5; i >= 0; i--) {
        s->o[i] = (uint8_t) (o % 3);
        sum += s->o[i];
        o /= 3;
    }
    s->o[6] = (uint8_t) ((3 - sum % 3) % 3);
}

int main(void)
{
    uint32_t n = 0, exp_max = 0, gen_max = 0;
    uint64_t exp_sum = 0, gen_sum = 0;
    uint8_t moves[MAX_DEPTH];

    build_dist();
    for (uint32_t r = 0; r < STATES; r++) {
        if (dist[r] != MAX_DEPTH)
            continue;
        state_t s;
        ref_unrank(r, &s);
        ida_solve(node_from_state(&s), moves);
        n++;
        exp_sum += ida_expanded;
        gen_sum += ida_generated;
        if (ida_expanded > exp_max)
            exp_max = ida_expanded;
        if (ida_generated > gen_max)
            gen_max = ida_generated;
    }
    printf("PDB=%d, %u distance-11 states, %d table reads per generated "
           "node\n",
           CORNER_PDB ? 3 : 2, n, READS_PER_CHILD);
    printf("            mean         max\n");
    printf("expanded  %10.1f  %10u\n", (double) exp_sum / n, exp_max);
    printf("generated %10.1f  %10u\n", (double) gen_sum / n, gen_max);
    printf("lookups   %10.1f  %10u\n",
           (double) gen_sum * READS_PER_CHILD / n, gen_max * READS_PER_CHILD);
    return 0;
}
