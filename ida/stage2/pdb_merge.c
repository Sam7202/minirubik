/* pdb_merge.c - two host experiments on how the PDBs are combined, the raw
 * data for note sections 3.2 and 3.6. It links the C search and the
 * generated tables; nothing here runs on the target.
 *
 *  1. States that the permutation and the orientation PDB each put one
 *     move from solved (perm_h = ori_h = 1): their true distance and their
 *     4-corner value. Every move changes p and o at once, so fixing each
 *     half with its own move says little about fixing both with one
 *     sequence.
 *  2. IDA* with h = perm_h + ori_h instead of the max: on how many states
 *     it returns a solution longer than the true distance. The sum counts
 *     an R or B turn twice, so it is not admissible, and IDA* loses its
 *     guarantee that the first solution found is the shortest.
 *
 * True distances come from a breadth-first search over (p, o) with perm_q
 * and ori_q; verify.c checks those tables against the reference model on
 * every state and face.
 *
 * usage: ./pdb_merge   (about 30 seconds on one core)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"

#if !CORNER_PDB
#error "pdb_merge needs the 4-corner PDB (CORNER_PDB=1)"
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

static void state_digits(const state_t *s, char out[15])
{
    for (int i = 0; i < CUBIES; i++) {
        out[i] = (char) ('1' + s->p[i]);
        out[i + CUBIES] = (char) ('1' + s->o[i]);
    }
    out[14] = '\0';
}

static void print_moves(const uint8_t *moves, int len)
{
    for (int i = 0; i < len; i++)
        printf("%s%s", i ? " " : "", move_names[moves[i]]);
}

/* Print the state's input digits and an optimal solution from the real
 * search (h = max of the three PDBs). */
static void print_optimal(uint32_t r)
{
    state_t s;
    char digits[15];
    uint8_t moves[MAX_DEPTH];
    ref_unrank(r, &s);
    state_digits(&s, digits);
    int len = ida_solve(node_from_state(&s), moves);
    printf("%s, distance %u, optimal ", digits, dist[r]);
    print_moves(moves, len);
}

/* ---- 1. What max(perm_h, ori_h) misses ---- */

static void half_pdbs(void)
{
    uint32_t count[MAX_DEPTH + 1][9] = {{0}}, example[MAX_DEPTH + 1][9];
    uint32_t total = 0;

    for (uint32_t r = 0; r < STATES; r++) {
        if (perm_h[r / ORIS] != 1 || ori_h[r % ORIS] != 1)
            continue;
        state_t s;
        ref_unrank(r, &s);
        node_t n = node_from_state(&s);
        uint8_t d = dist[r], h4 = c4_row[n.co][n.cp];
        if (!count[d][h4]++)
            example[d][h4] = r;
        total++;
    }
    printf("1. perm_h = ori_h = 1 on %u states\n", total);
    printf("   distance  c4_h  states  example\n");
    for (int d = 0; d <= MAX_DEPTH; d++)
        for (int h = 0; h < 9; h++)
            if (count[d][h]) {
                state_t s;
                char digits[15];
                ref_unrank(example[d][h], &s);
                state_digits(&s, digits);
                printf("   %8d %5d %7u  %s\n", d, h, count[d][h], digits);
            }

    /* Note 3.2: the permutation after one R with the twists after one B. */
    uint32_t r = (uint32_t) perm_q[0][0] * ORIS + ori_q[1][0];
    state_t s;
    ref_unrank(r, &s);
    node_t n = node_from_state(&s);
    printf("   p after R, o after B: perm_h %u, ori_h %u, c4_h %u; ",
           perm_h[n.p], ori_h[n.o], c4_row[n.co][n.cp]);
    print_optimal(r);
    printf("\n");
}

/* ---- 2. IDA* with the sum ----
 * The same search as search.c (faces R, B, D, each 90, 180, 270 degrees,
 * the same face never twice in a row), with h = perm_h + ori_h. */

static uint8_t sum_bound, sum_len, sum_path[32];

static int sum_dfs(uint16_t p, uint16_t o, uint8_t g, int last)
{
    for (int f = 0; f < 3; f++) {
        if (f == last)
            continue;
        uint16_t np = p, no = o;
        for (int t = 0; t < 3; t++) {
            np = perm_q[f][np];
            no = ori_q[f][no];
            if (g + 1 + perm_h[np] + ori_h[no] > sum_bound)
                continue;
            sum_path[g] = (uint8_t) (f * 3 + t);
            if (np == 0 && no == 0) {
                sum_len = (uint8_t) (g + 1);
                return 1;
            }
            if (sum_dfs(np, no, (uint8_t) (g + 1), f))
                return 1;
        }
    }
    return 0;
}

/* Terminates: once the bound exceeds g + 1 + 13 nothing is pruned. */
static int sum_solve(uint32_t r)
{
    uint16_t p = (uint16_t) (r / ORIS), o = (uint16_t) (r % ORIS);
    for (sum_bound = (uint8_t) (perm_h[p] + ori_h[o]);; sum_bound++)
        if (sum_dfs(p, o, 0, -1))
            return sum_len;
}

static void sum_heuristic(void)
{
    uint32_t states[MAX_DEPTH + 1] = {0}, longer[MAX_DEPTH + 1] = {0};
    uint32_t excess[32] = {0}, total = 0, first = 0;

    for (uint32_t r = 1; r < STATES; r++) {
        int len = sum_solve(r);
        uint8_t d = dist[r];
        states[d]++;
        if (len > d) {
            longer[d]++;
            excess[len - d]++;
            total++;
            if (!first || d < dist[first])
                first = r;
        }
    }
    printf("2. IDA* with h = perm_h + ori_h: longer than optimal on %u of "
           "%u unsolved states (%.1f%%)\n",
           total, (unsigned) STATES - 1, 100.0 * total / (STATES - 1));
    printf("   moves too long:");
    for (int e = 1; e < 32; e++)
        if (excess[e])
            printf(" %d:%u", e, excess[e]);
    printf("\n   distance   longer / states\n");
    for (int d = 1; d <= MAX_DEPTH; d++)
        printf("   %8d %8u / %7u\n", d, longer[d], states[d]);

    uint16_t p = (uint16_t) (first / ORIS), o = (uint16_t) (first % ORIS);
    printf("   first at the smallest distance: ");
    print_optimal(first);
    sum_solve(first);
    printf("; sum starts at %u + %u and returns ", perm_h[p], ori_h[o]);
    print_moves(sum_path, sum_len);
    printf("\n");
}

int main(void)
{
    build_dist();
    half_pdbs();
    sum_heuristic();
    return 0;
}
