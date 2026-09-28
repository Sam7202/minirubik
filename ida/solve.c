/* solve.c - host command-line front end for the C search, same input
 * format as solver.c:
 *   ./solve PPPPPPPOOOOOOO
 * The solution goes to stdout; search cost and table size go to stderr.
 * It links search.c with the generated tables.c, the same data tables.s
 * gives the assembly, so its node counts are the reference for the target.
 */
#include <stdio.h>

#include "search.h"

/* Same validation as solver.c: exactly 14 digits, a permutation of 1..7,
 * twists 1..3 whose (digit - 1) sum is a multiple of 3. */
static int parse_state(const char *in, state_t *s)
{
    unsigned seen = 0, sum = 0;
    for (int i = 0; i < 14; i++) /* a short string fails here at its NUL */
        if (in[i] < '1' || in[i] > (i < CUBIES ? '7' : '3'))
            return 0;
    if (in[14] != '\0')
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

int main(int argc, char **argv)
{
    state_t s;
    uint8_t moves[MAX_DEPTH];

    if (argc != 2 || !parse_state(argv[1], &s)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solve");
        return 2;
    }
    int len = ida_solve(node_from_state(&s), moves);
    if (len < 0) {
        fprintf(stderr, "no solution within %d moves\n", MAX_DEPTH);
        return 1;
    }
    for (int i = 0; i < len; i++)
        printf("%s%s", i ? " " : "", move_names[moves[i]]);
    putchar('\n');
    fprintf(stderr, "length %d, expanded %lu, generated %lu, tables %zu B\n",
            len, (unsigned long) ida_expanded, (unsigned long) ida_generated,
            (size_t) tables_bytes);
    return fflush(stdout) || ferror(stdout) ? 1 : 0;
}
