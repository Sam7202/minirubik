/* search.c - IDA* over the precomputed tables; the part that runs on the
 * target. It reads the tables in tables.h and builds nothing.
 *
 * The search never builds a cube: a node is a set of coordinates, a move is
 * one table lookup per coordinate, and the heuristic is one lookup per PDB
 * followed by a max. There is no multiply, divide or modulo in the search:
 * each face is reached through pointers to its table rows, add81 rows are
 * 128 bytes wide so its index is a shift, and the 4-corner PDB is reached
 * through one row pointer per twist value.
 */
#include <stddef.h>

#include "search.h"

uint32_t ida_expanded, ida_generated;

/* ---- Nodes ---- */

node_t node_from_state(const state_t *s)
{
    node_t n;
    n.p = perm_rank(s->p);
    n.o = ori_rank(s->o);
#if CORNER_PDB
    uint8_t pos[4] = {0}, tw[4] = {0};
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < CUBIES; i++)
            if (s->p[i] == c4_cubies[k]) {
                pos[k] = (uint8_t) i;
                tw[k] = s->o[i];
            }
    n.cp = c4_rank(pos);
    n.co = tw_rank(tw);
#endif
    return n;
}

/* One quarter turn of a face is one lookup per coordinate in that face's
 * row of each table. The search holds a pointer to the face's rows rather
 * than the face index, so it never computes face * row_size. */
typedef struct {
    const uint16_t *perm, *ori;
#if CORNER_PDB
    const uint16_t *c4pos;
    const uint8_t *c4tw;
#endif
} face_rows_t;

#if CORNER_PDB
#define FACE_ROWS(f) {perm_q[f], ori_q[f], c4pos_q[f], c4tw_q[f]}
#else
#define FACE_ROWS(f) {perm_q[f], ori_q[f]}
#endif

static const face_rows_t rows[3] = {FACE_ROWS(0), FACE_ROWS(1), FACE_ROWS(2)};

static node_t step(node_t n, const face_rows_t *r)
{
    node_t c;
    c.p = r->perm[n.p];
    c.o = r->ori[n.o];
#if CORNER_PDB
    c.co = add81[n.co][r->c4tw[n.cp]]; /* twist depends on the old cp */
    c.cp = r->c4pos[n.cp];
#endif
    return c;
}

node_t node_quarter(node_t n, int face)
{
    return step(n, &rows[face]);
}

/* Each PDB is an exact distance in a smaller puzzle that every real move
 * sequence also solves, so each is a lower bound, and so is their max.
 * Their sum is not: every R or B turn changes both p and o at once. */
uint8_t node_h(node_t n)
{
    uint8_t h = perm_h[n.p], x = ori_h[n.o];
    if (x > h)
        h = x;
#if CORNER_PDB
    x = c4_row[n.co][n.cp];
    if (x > h)
        h = x;
#endif
    return h;
}

/* ---- IDA* ---- */

#ifndef DFS_LOOP
#define DFS_LOOP 1 /* 0: the recursive dfs() of tag stage3-c */
#endif

static uint8_t *path, bound, found_len;

/* dfs() searches below n, which sits at depth g, has g + h(n) <= bound and
 * is not solved. Each child is tested in the parent: most children fail the
 * bound test in the last iteration, and rejecting them there saves going
 * down a level (a call, in the recursive form) for each.
 *
 * last points at the rows of the face turned to reach n. Turning that face
 * again is never optimal (two turns of one face merge into one move or
 * none), which cuts the branching factor from 9 to 6. R, B and D share
 * corners pairwise, so no two of them commute and nothing more can be
 * pruned that way.
 * Since g + h(n) <= bound, h(n) >= 1 and bound <= MAX_DEPTH, g stays below
 * MAX_DEPTH and path[g] is in range. */
#if DFS_LOOP
/* The search is a loop over a stack of levels, as in the assembly.
 * levels[g] holds the node at depth g, the face that produced it, and the
 * loop state to resume once the child searched below it is done. Going
 * down saves the loop state, pushes the child and starts again at "down";
 * backing up pops, restores the loop state and re-enters the inner loop at
 * "up", where the recursive form returns from its call. Children, counts
 * and answers are those of the recursive form. */
typedef struct {
    node_t n;
    const face_rows_t *last; /* NULL at the root */
    const face_rows_t *r;    /* face, move index and turn of the child */
    uint8_t m, t;            /* searched below this level */
} level_t;

static level_t levels[MAX_DEPTH];

static int dfs(node_t n, uint8_t g, const face_rows_t *last)
{
    level_t *L = levels; /* ida_solve starts at g = 0 */
    const face_rows_t *r;
    uint8_t m, t;
    node_t c;

    L->n = n;
    L->last = last;
down:
    ida_expanded++;
    for (r = rows, m = 0; r != rows + 3; r++, m = (uint8_t) (m + 3)) {
        if (r == L->last)
            continue;
        c = L->n;
        for (t = 0; t < 3; t++) {
            c = step(c, r); /* 90, then 180, then 270 degrees */
            ida_generated++;
            if (g + 1 + node_h(c) > bound)
                continue;
            path[g] = (uint8_t) (m + t);
            if (c.p == 0 && c.o == 0) { /* both coordinates solved */
                found_len = (uint8_t) (g + 1);
                return 1;
            }
            L->r = r; /* the call: save the loop state, push c */
            L->m = m;
            L->t = t;
            L++;
            L->n = c;
            L->last = r;
            g++;
            goto down;
        up:; /* the return: resume with the next turn */
        }
    }
    if (L == levels)
        return 0;
    L--;
    g--;
    r = L->r;
    m = L->m;
    t = L->t;
    c = L[1].n; /* the child just searched: one more turn gives the next */
    goto up;
}
#else
/* The recursive form (tag stage3-c): a call, a stack frame and a return
 * per expanded node. */
static int dfs(node_t n, uint8_t g, const face_rows_t *last)
{
    ida_expanded++;
    uint8_t m = 0; /* move index face * 3, kept by addition */
    for (const face_rows_t *r = rows; r != rows + 3; r++, m = (uint8_t) (m + 3)) {
        if (r == last)
            continue;
        node_t c = n;
        for (uint8_t t = 0; t < 3; t++) {
            c = step(c, r); /* 90, then 180, then 270 degrees */
            ida_generated++;
            if (g + 1 + node_h(c) > bound)
                continue;
            path[g] = (uint8_t) (m + t);
            if (c.p == 0 && c.o == 0) { /* both coordinates solved */
                found_len = (uint8_t) (g + 1);
                return 1;
            }
            if (dfs(c, (uint8_t) (g + 1), r))
                return 1;
        }
    }
    return 0;
}
#endif

int ida_solve(node_t start, uint8_t moves[MAX_DEPTH])
{
    ida_expanded = ida_generated = 0;
    path = moves;
    if (start.p == 0 && start.o == 0)
        return 0;
    for (bound = node_h(start); bound <= MAX_DEPTH; bound++)
        if (dfs(start, 0, NULL))
            return found_len;
    return -1;
}
