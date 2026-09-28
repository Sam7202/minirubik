/* search.h - IDA* over the precomputed tables. This is the part that must
 * execute on the target; search.c builds nothing and only reads tables.h. */
#ifndef SEARCH_H
#define SEARCH_H

#include "tables.h"

enum { MAX_DEPTH = 11 }; /* God's number in HTM; no optimal solution is longer */

/* A search node is just the coordinates; the cube itself is never built. */
typedef struct {
    uint16_t p, o;
#if CORNER_PDB
    uint16_t cp;
    uint8_t co;
#endif
} node_t;

node_t node_from_state(const state_t *s); /* once per query */
node_t node_quarter(node_t n, int face);  /* one quarter turn of face */
uint8_t node_h(node_t n);                 /* heuristic */

/* Writes an optimal solution into moves[] and returns its length, or -1 if
 * none exists within MAX_DEPTH (impossible for a valid state). */
int ida_solve(node_t start, uint8_t moves[MAX_DEPTH]);

/* Search cost of the last ida_solve, summed over all IDA* iterations:
 * expanded  = nodes that passed the bound test and had children generated,
 * generated = children produced, one per quarter-turn step in the search. */
extern uint32_t ida_expanded, ida_generated;

#endif
