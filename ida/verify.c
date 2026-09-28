/* verify.c - host-only harness; nothing in this file runs on the target.
 * It links the C search with the generated tables.c (the data the target
 * links) and with build.c (the generator's code).
 *
 *  0. The linked tables are byte-for-byte what build.c produces now, so
 *     tables.c (and tables.s, written in the same run) is not stale.
 *  1. Oracle: breadth-first search over all 3,674,160 states with the
 *     reference model (unrank, quarter_turn, rank, exactly as solver.c).
 *     It shares no table with the target code. 3.5 MiB of distances plus a
 *     14 MiB queue, both host only.
 *  2. Tables: on every state and every face, the target's ranking matches
 *     solver.c's, and moving the coordinates matches moving the cube. This
 *     is the condition that makes each PDB a lower bound.
 *  3. PDBs: every abstract state reached; maximum and histogram; the
 *     solved entry of each PDB is 0.
 *  4. Heuristic: admissible and consistent on every state, 0 only at the
 *     goal; and a count showing why the PDBs must be merged with max.
 *  5. IDA* on states [lo, hi): the length equals the oracle distance and
 *     the moves, replayed on the reference model, solve the cube. Reports
 *     expanded and generated nodes per depth against the 250,000 budget.
 *
 * usage: ./verify [lo hi [step]]   (default: every state)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"
#include "search.h"

enum { BUDGET = 250000 };

static int failures;

#define CHECK(cond, ...)                  \
    do {                                  \
        if (!(cond)) {                    \
            failures++;                   \
            printf("FAIL: " __VA_ARGS__); \
            putchar('\n');                \
        }                                 \
    } while (0)

/* ---- Reference model, copied from solver.c ---- */

static uint32_t ref_rank(const state_t *s)
{
    uint32_t p = 0, o = 0;
    for (int i = 0; i < CUBIES; i++) {
        uint32_t smaller = 0;
        for (int j = i + 1; j < CUBIES; j++)
            smaller += s->p[j] < s->p[i];
        p = p * (uint32_t) (CUBIES - i) + smaller;
    }
    for (int i = 0; i < 6; i++)
        o = o * 3 + s->o[i];
    return p * ORIS + o;
}

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

static state_t ref_move(state_t s, uint8_t m)
{
    for (int t = 0; t <= m % 3; t++)
        s = quarter_turn(s, m / 3);
    return s;
}

static void state_digits(const state_t *s, char out[15])
{
    for (int i = 0; i < CUBIES; i++) {
        out[i] = (char) ('1' + s->p[i]);
        out[i + CUBIES] = (char) ('1' + s->o[i]);
    }
    out[14] = '\0';
}

static int node_eq(node_t a, node_t b)
{
    return a.p == b.p && a.o == b.o
#if CORNER_PDB
           && a.cp == b.cp && a.co == b.co
#endif
        ;
}

/* ---- 0. Memory ---- */

static void print_memory(void)
{
    printf("target tables (read-only data linked into the RV32I program):\n");
    printf("  perm_q  [3][5040] u16  %6zu B\n", sizeof perm_q);
    printf("  ori_q   [3][729]  u16  %6zu B\n", sizeof ori_q);
    printf("  perm_h  [5040]    u8   %6zu B\n", sizeof perm_h);
    printf("  ori_h   [729]     u8   %6zu B\n", sizeof ori_h);
#if CORNER_PDB
    printf("  c4pos_q [3][840]  u16  %6zu B\n", sizeof c4pos_q);
    printf("  c4tw_q  [3][840]  u8   %6zu B\n", sizeof c4tw_q);
    printf("  add81   [81][128] u8   %6zu B\n", sizeof add81);
    printf("  c4_h    [81][840] u8   %6zu B\n", sizeof c4_h);
    printf("  c4_row  [81] ptr       %6d B (RV32)\n", C4ORI * 4);
#endif
    printf("  total                  %6u B (%.1f KiB of 128 KiB)\n",
           (unsigned) tables_bytes, tables_bytes / 1024.0);
    printf("host only (verify.c): oracle %u B + BFS queue %zu B\n\n",
           (unsigned) STATES, (size_t) STATES * sizeof(uint32_t));
}

/* ---- 0. Linked tables match the generator ---- */

static void check_fresh(void)
{
    static tables_t t;
    build_tables(&t);
    int stale = memcmp(t.perm_q, perm_q, sizeof perm_q) ||
                memcmp(t.ori_q, ori_q, sizeof ori_q) ||
                memcmp(t.perm_h, perm_h, sizeof perm_h) ||
                memcmp(t.ori_h, ori_h, sizeof ori_h);
#if CORNER_PDB
    stale = stale || memcmp(t.c4pos_q, c4pos_q, sizeof c4pos_q) ||
            memcmp(t.c4tw_q, c4tw_q, sizeof c4tw_q) ||
            memcmp(t.add81, add81, sizeof add81) ||
            memcmp(t.c4_h, c4_h, sizeof c4_h);
    for (int co = 0; co < C4ORI; co++)
        stale = stale || c4_row[co] != c4_h[co];
#endif
    CHECK(!stale, "tables.c differs from what build.c produces: run make tables");
    printf("0. linked tables = fresh build.c output, byte for byte\n");
}

/* ---- 1. Oracle ---- */

static uint8_t *dist;

static void build_oracle(void)
{
    /* The distance table in minirubik report.md, section 4. */
    static const uint32_t expected[12] = {
        1, 9, 54, 321, 1847, 9992, 50136, 227536, 870072, 1887748, 623800, 2644,
    };
    uint32_t *queue = malloc(sizeof *queue * STATES);
    dist = malloc(STATES);
    if (!queue || !dist) {
        puts("out of memory");
        exit(1);
    }
    memset(dist, 0xFF, STATES);
    uint32_t head = 0, tail = 1;
    queue[0] = 0;
    dist[0] = 0;
    while (head < tail) {
        uint32_t r = queue[head++];
        state_t s;
        ref_unrank(r, &s);
        for (int f = 0; f < 3; f++) {
            state_t t = s;
            for (int k = 0; k < 3; k++) {
                t = quarter_turn(t, f);
                uint32_t r2 = ref_rank(&t);
                if (dist[r2] == 0xFF) {
                    dist[r2] = (uint8_t) (dist[r] + 1);
                    queue[tail++] = r2;
                }
            }
        }
    }
    free(queue);

    uint32_t hist[16] = {0};
    for (uint32_t r = 0; r < STATES; r++)
        hist[dist[r] & 15]++;
    CHECK(tail == STATES, "oracle reached %u of %u states", tail,
          (unsigned) STATES);
    for (int d = 0; d < 12; d++)
        CHECK(hist[d] == expected[d], "oracle depth %d: %u states, report %u",
              d, hist[d], expected[d]);
    printf("1. oracle: %u states, depth histogram matches report.md\n", tail);
}

/* ---- 2. Tables against the reference model ---- */

static void check_tables(void)
{
    uint32_t bad_rank = 0, bad_move = 0;
    for (uint32_t r = 0; r < STATES; r++) {
        state_t s;
        ref_unrank(r, &s);
        node_t n = node_from_state(&s);
        if (n.p != r / ORIS || n.o != r % ORIS)
            bad_rank++;
        for (int f = 0; f < 3; f++) {
            state_t t = quarter_turn(s, f);
            if (!node_eq(node_quarter(n, f), node_from_state(&t)))
                bad_move++;
        }
    }
    CHECK(bad_rank == 0, "%u states ranked differently from solver.c",
          bad_rank);
    CHECK(bad_move == 0, "%u (state, face) pairs where tables disagree",
          bad_move);
    printf("2. tables: ranking = solver.c on every state; coordinate moves "
           "= cube moves on all %u x 3 quarter turns\n",
           (unsigned) STATES);
}

/* ---- 3. PDB coverage ---- */

static void pdb_stats(const char *name, const uint8_t *h, uint32_t n)
{
    uint32_t hist[16] = {0}, unreached = 0;
    int max = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (h[i] > 15) {
            unreached++;
            continue;
        }
        hist[h[i]]++;
        if (h[i] > max)
            max = h[i];
    }
    CHECK(unreached == 0, "%s PDB: %u entries unreached", name, unreached);
    printf("3. %-7s PDB: %5u/%u reached, max %d, histogram:", name,
           n - unreached, n, max);
    for (int d = 0; d <= max; d++)
        printf(" %u", hist[d]);
    putchar('\n');
}

static void check_solved_entries(void)
{
    int ok = perm_h[0] == 0 && ori_h[0] == 0;
#if CORNER_PDB
    ok = ok && c4_row[0][c4_rank(c4_cubies)] == 0;
#endif
    CHECK(ok, "a PDB entry for the solved state is not 0");
    printf("3. solved entries: perm_h[0] = ori_h[0]%s = 0\n",
           CORNER_PDB ? " = c4_h[0][goal]" : "");
}

/* ---- 4. Admissibility and consistency, exhaustively ---- */

static void check_heuristic(void)
{
    uint32_t over_p = 0, over_o = 0, over_c = 0, over_h = 0, over_sum = 0;
    uint32_t inconsistent = 0, zero_elsewhere = 0, gap[12] = {0};
    uint64_t hsum = 0, dsum = 0;

    for (uint32_t r = 0; r < STATES; r++) {
        state_t s;
        ref_unrank(r, &s);
        node_t n = node_from_state(&s);
        uint8_t d = dist[r], hp = perm_h[n.p], ho = ori_h[n.o], h = node_h(n);
        over_p += hp > d;
        over_o += ho > d;
#if CORNER_PDB
        over_c += c4_row[n.co][n.cp] > d;
#endif
        over_sum += hp + ho > d;
        zero_elsewhere += h == 0 && r != 0;
        if (h > d)
            over_h++;
        else
            gap[d - h]++;
        hsum += h;
        dsum += d;
        for (int f = 0; f < 3; f++) { /* all 9 moves: |h(n) - h(child)| <= 1 */
            node_t c = n;
            for (int t = 0; t < 3; t++) {
                c = node_quarter(c, f);
                int h2 = node_h(c);
                inconsistent += h2 > h + 1 || h > h2 + 1;
            }
        }
    }
    CHECK(over_p == 0, "perm PDB exceeds true distance on %u states", over_p);
    CHECK(over_o == 0, "orient PDB exceeds true distance on %u states",
          over_o);
    CHECK(over_c == 0, "corner PDB exceeds true distance on %u states",
          over_c);
    CHECK(over_h == 0, "h exceeds true distance on %u states", over_h);
    CHECK(inconsistent == 0, "h inconsistent on %u edges", inconsistent);
    CHECK(zero_elsewhere == 0, "h = 0 on %u unsolved states", zero_elsewhere);
    printf("4. h = max of PDBs: admissible and consistent on all states, "
           "0 only at the goal; mean h %.3f vs mean distance %.3f\n",
           (double) hsum / STATES, (double) dsum / STATES);
    printf("   distance - h:");
    for (int i = 0; i < 12; i++)
        if (gap[i])
            printf(" %d:%u", i, gap[i]);
    printf("\n   (for contrast, h_perm + h_orient overestimates on %u states, "
           "so the merge must be max)\n",
           over_sum);
}

/* ---- 5. IDA* against the oracle ---- */

static void check_ida(uint32_t lo, uint32_t hi, uint32_t step)
{
    struct {
        uint32_t n, exp_max, gen_max;
        uint64_t exp_sum;
    } by[MAX_DEPTH + 1];
    memset(by, 0, sizeof by);
    uint32_t tested = 0, wrong = 0, replay = 0, over_exp = 0, over_gen = 0;
    uint32_t worst_exp = 0, worst_gen = 0, worst_rank = 0;
    uint8_t moves[MAX_DEPTH];

    printf("5. IDA* on states [%u, %u) step %u ...\n", lo, hi, step);
    fflush(stdout);
    for (uint32_t r = lo; r < hi; r += step) {
        state_t s;
        ref_unrank(r, &s);
        int len = ida_solve(node_from_state(&s), moves);
        tested++;
        if (len != dist[r]) {
            wrong++;
            continue;
        }
        state_t t = s;
        for (int i = 0; i < len; i++)
            t = ref_move(t, moves[i]);
        replay += ref_rank(&t) != 0;

        by[len].n++;
        by[len].exp_sum += ida_expanded;
        if (ida_expanded > by[len].exp_max)
            by[len].exp_max = ida_expanded;
        if (ida_generated > by[len].gen_max)
            by[len].gen_max = ida_generated;
        over_exp += ida_expanded > BUDGET;
        over_gen += ida_generated > BUDGET;
        if (ida_expanded > worst_exp) {
            worst_exp = ida_expanded;
            worst_gen = ida_generated;
            worst_rank = r;
        }
    }
    CHECK(wrong == 0, "%u states solved with a non-optimal length", wrong);
    CHECK(replay == 0, "%u solutions do not solve the cube", replay);
    printf("   %u states: every length equals the oracle distance, every "
           "solution replays to solved\n",
           tested);
    printf("   depth   states   mean expanded   max expanded   max generated\n");
    for (int d = 0; d <= MAX_DEPTH; d++)
        if (by[d].n)
            printf("   %5d %8u %15.1f %14u %15u\n", d, by[d].n,
                   (double) by[d].exp_sum / by[d].n, by[d].exp_max,
                   by[d].gen_max);
    state_t w;
    char digits[15];
    ref_unrank(worst_rank, &w);
    state_digits(&w, digits);
    printf("   worst: %s (distance %u) expanded %u, generated %u\n", digits,
           dist[worst_rank], worst_exp, worst_gen);
    printf("   states over %d: %u by expanded, %u by generated\n", BUDGET,
           over_exp, over_gen);
}

int main(int argc, char **argv)
{
    uint32_t lo = 0, hi = STATES, step = 1;
    if (argc == 3 || argc == 4) {
        lo = (uint32_t) strtoul(argv[1], NULL, 10);
        hi = (uint32_t) strtoul(argv[2], NULL, 10);
        if (argc == 4)
            step = (uint32_t) strtoul(argv[3], NULL, 10);
    }
    if (argc == 2 || argc > 4 || hi > STATES || lo >= hi || step == 0) {
        fprintf(stderr, "usage: %s [lo hi [step]]\n", argv[0]);
        return 2;
    }

    print_memory();
    check_fresh();
    build_oracle();
    check_tables();
    pdb_stats("perm", perm_h, PERMS);
    pdb_stats("orient", ori_h, ORIS);
#if CORNER_PDB
    pdb_stats("corner4", &c4_h[0][0], C4ORI * C4POS);
#endif
    check_solved_entries();
    check_heuristic();
    check_ida(lo, hi, step);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
