/* check_ripes.c - host side, last step of the flow:
 *   host builds tables -> Ripes runs the search -> host checks the answers.
 *
 * Reads the Ripes CLI output of search_rv32 on stdin and, for every
 * "STATE -> moves  [len N, ...]" line, checks independently of ida.c:
 *   1. replaying the moves on STATE with cube.h's reference quarter_turn
 *      gives the solved cube;
 *   2. the number of moves is the true optimal distance, from a full BFS
 *      over all 3,674,160 states (same model and rank as solver.c);
 *   3. N printed by the target equals the number of moves;
 *   4. the moves and the expanded/generated node counts equal those of the
 *      host build of ../search.c on the same state. The search order is
 *      fixed (tables.h), so any indexing slip on the target that still
 *      happens to give an optimal length almost always changes the counts.
 * Checks 1-2 use only cube.h and a BFS; check 4 uses search.c as the
 * reference, so it compares the target against the host build.
 * It also requires the target's "tables N B" banner and exit code 0.
 * Exit status 0 only if every check passes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../cube.h"
#include "../search.h"

static uint32_t rank_state(const state_t *s) /* identical to solver.c */
{
    uint32_t p = 0, o = 0;
    for (int i = 0; i < CUBIES; i++) {
        int smaller = 0;
        for (int j = i + 1; j < CUBIES; j++)
            smaller += s->p[j] < s->p[i];
        p = p * (uint32_t) (CUBIES - i) + (uint32_t) smaller;
    }
    for (int i = 0; i < 6; i++)
        o = o * 3 + s->o[i];
    return p * ORIS + o;
}

static uint8_t *bfs_distance(void)
{
    uint8_t *dist = malloc(STATES);
    state_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint32_t head = 0, tail = 1;
    if (!dist || !queue)
        return NULL;
    memset(dist, 0xFF, STATES);
    queue[0] = (state_t) {{0, 1, 2, 3, 4, 5, 6}, {0}};
    dist[0] = 0;
    while (head < tail) {
        state_t s = queue[head++];
        uint8_t d = dist[rank_state(&s)];
        for (int f = 0; f < 3; f++) {
            state_t t = s;
            for (int n = 0; n < 3; n++) {
                t = quarter_turn(t, f);
                uint32_t r = rank_state(&t);
                if (dist[r] == 0xFF) {
                    dist[r] = (uint8_t) (d + 1);
                    queue[tail++] = t;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(dist);
        return NULL;
    }
    return dist;
}

static int parse_state(const char *in, state_t *s)
{
    unsigned seen = 0, sum = 0;
    for (int i = 0; i < 14; i++)
        if (in[i] < '1' || in[i] > (i < CUBIES ? '7' : '3'))
            return 0;
    for (int i = 0; i < CUBIES; i++) {
        unsigned p = (unsigned) (in[i] - '1');
        unsigned o = (unsigned) (in[i + CUBIES] - '1');
        if ((seen >> p) & 1)
            return 0;
        seen |= 1u << p;
        s->p[i] = (uint8_t) p;
        s->o[i] = (uint8_t) o;
        sum += o;
    }
    return sum % 3 == 0;
}

static int move_index(const char *tok)
{
    for (int m = 0; m < MOVES; m++)
        if (!strcmp(tok, move_names[m]))
            return m;
    return -1;
}

int main(void)
{
    uint8_t *dist = bfs_distance();
    char line[512];
    int banner_ok = 0, exit_ok = 0, checked = 0, failed = 0;

    if (!dist) {
        fputs("check_ripes: BFS oracle failed\n", stderr);
        return 1;
    }
    while (fgets(line, sizeof line, stdin)) {
        if (!strncmp(line, "tables ", 7))
            banner_ok = 1;
        if (!strncmp(line, "Program exited with code: 0", 27))
            exit_ok = 1;
        char *arrow = strstr(line, " -> ");
        if (!arrow || arrow - line != 14)
            continue;

        state_t s;
        char name[15];
        memcpy(name, line, 14);
        name[14] = '\0';
        if (!parse_state(line, &s)) {
            printf("FAIL %s: not a valid state\n", name);
            failed++;
            continue;
        }
        uint32_t start = rank_state(&s);

        /* Host reference: the same search on the same state. */
        uint8_t ref_moves[MAX_DEPTH];
        int ref_len = ida_solve(node_from_state(&s), ref_moves);
        uint32_t ref_exp = ida_expanded, ref_gen = ida_generated;

        /* Moves run up to "[" (or "(solved)");
         * "[len N, expanded E, generated G]" follows. */
        char *bracket = strchr(arrow, '[');
        int printed_len = -1;
        unsigned long exp = 0, gen = 0;
        int have_counts = bracket &&
            sscanf(bracket, "[len %d, expanded %lu, generated %lu",
                   &printed_len, &exp, &gen) == 3;
        if (bracket)
            *bracket = '\0';
        uint8_t got_moves[MAX_DEPTH];
        int len = 0, bad_token = 0;
        for (char *tok = strtok(arrow + 4, " \n"); tok;
             tok = strtok(NULL, " \n")) {
            if (!strcmp(tok, "(solved)"))
                continue;
            int m = move_index(tok);
            if (m < 0 || len == MAX_DEPTH) {
                bad_token = 1;
                break;
            }
            got_moves[len] = (uint8_t) m;
            for (int t = 0; t <= m % 3; t++)
                s = quarter_turn(s, m / 3);
            len++;
        }

        int solved = rank_state(&s) == 0;
        int optimal = len == dist[start];
        int same_moves = !bad_token && len == ref_len &&
                         !memcmp(got_moves, ref_moves, (size_t) len);
        int same_counts = have_counts && exp == ref_exp && gen == ref_gen;
        int ok = !bad_token && solved && optimal && printed_len == len &&
                 same_moves && same_counts;
        printf("%s %s: %d moves, optimal %d, expanded %lu/%u, generated "
               "%lu/%u (target/host)%s%s%s%s%s%s\n",
               ok ? "ok  " : "FAIL", name, len, dist[start], exp, ref_exp,
               gen, ref_gen, solved ? "" : ", NOT SOLVED",
               bad_token ? ", bad move token" : "",
               printed_len == len ? "" : ", len field mismatch",
               have_counts ? "" : ", no node counts in output",
               same_moves ? "" : ", moves differ from host search.c",
               !have_counts || same_counts ? "" : ", node counts differ");
        checked++;
        failed += !ok;
    }
    free(dist);

    if (!banner_ok) {
        puts("FAIL: no \"tables N B\" banner - the target program did not start");
        failed++;
    }
    if (!exit_ok) {
        puts("FAIL: Ripes did not report exit code 0");
        failed++;
    }
    if (!checked) {
        puts("FAIL: no solutions found in the Ripes output");
        failed++;
    }
    printf("%d solutions checked on host, %d failure(s)\n", checked, failed);
    return failed != 0;
}
