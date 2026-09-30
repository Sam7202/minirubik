/* d11.c - print every state at distance 11 (2,644 of them), one per line in
 * solver.c's 14-character input format, in rank order. Input for batch.py.
 *
 * Distances come from a breadth-first search over (p, o) with perm_q and
 * ori_q, as in stage2/pdb_merge.c; verify.c checks those tables against the
 * reference model on every state and face.
 *
 * usage: ./d11 > d11.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tables.h"

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
    uint8_t *dist = malloc(STATES);
    if (!dist) {
        puts("out of memory");
        return 1;
    }
    memset(dist, 0xFF, STATES);
    dist[0] = 0;
    for (uint8_t d = 0;; d++) { /* level sweep, 0xFF meaning unvisited */
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
    for (uint32_t r = 0; r < STATES; r++) {
        if (dist[r] != 11)
            continue;
        state_t s;
        ref_unrank(r, &s);
        char out[15];
        for (int i = 0; i < CUBIES; i++) {
            out[i] = (char) ('1' + s.p[i]);
            out[i + CUBIES] = (char) ('1' + s.o[i]);
        }
        out[14] = '\0';
        puts(out);
    }
    return 0;
}
