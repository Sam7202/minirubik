/* cube.h - the 2x2x2 model shared by the target solver and the host verifier.
 *
 * source[][] and twist[][] are copied from sysprog21/minirubik solver.c, so
 * this program describes the same cube and uses the same rank encoding:
 *   rank = permutation_rank * 729 + orientation_rank, solved state = rank 0.
 */
#ifndef CUBE_H
#define CUBE_H

#include <stdint.h>

enum {
    CUBIES = 7,            /* corner 0 is fixed; seven corners move */
    PERMS = 5040,          /* 7! */
    ORIS = 729,            /* 3^6: the 7th twist is implied by the other six */
    STATES = PERMS * ORIS, /* 3,674,160 */
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES]; /* p[i]: which cubie sits in position i */
    uint8_t o[CUBIES]; /* o[i]: its twist, 0..2 */
} state_t;

/* A quarter turn fills destination position i with the cubie from
 * source[face][i] and adds twist[face][i]. Faces: 0 = R, 1 = B, 2 = D. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* Move m = face * 3 + turn; turn 0, 1, 2 = one, two, three quarter turns. */
static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};

/* Reference quarter turn, identical to solver.c. The target code does not
 * call it in the search; it only builds tables from source[] and twist[]. */
static inline state_t quarter_turn(state_t s, int face)
{
    state_t r;
    for (int i = 0; i < CUBIES; i++) {
        int from = source[face][i];
        r.p[i] = s.p[from];
        r.o[i] = (uint8_t) ((s.o[from] + twist[face][i]) % 3);
    }
    return r;
}

#endif
