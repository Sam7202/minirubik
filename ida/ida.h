/* ida.h - target-side solver: coordinate move tables, pattern databases
 * (PDBs) and IDA*. Everything declared here is what would later run on
 * RV32I; verify.c is the host-only harness that checks it.
 */
#ifndef IDA_H
#define IDA_H

#include <stddef.h>
#include <stdint.h>

#include "cube.h"

/* 0: h = max(permutation PDB, orientation PDB).
 * 1: additionally max with a PDB over 4 corners' positions and twists,
 *    the fallback if the node budget turns out too tight. */

enum { MAX_DEPTH = 11 }; /* God's number in HTM; no optimal solution is longer */

/* A search node is just the coordinates; the cube itself is never built. */
typedef struct {
    uint16_t p; /* permutation coordinate, Lehmer rank 0..5039 */
    uint16_t o; /* orientation coordinate, base-3 rank 0..728 */
} node_t;

void ida_init(void); /* build move tables and PDBs; call once */
node_t node_from_state(const state_t *s);
node_t node_quarter(node_t n, int face);
uint8_t node_h(node_t n);

/* Writes an optimal solution into moves[] and returns its length, or -1 if
 * none exists within MAX_DEPTH (impossible for a valid state). */
int ida_solve(node_t start, uint8_t moves[MAX_DEPTH]);

/* Search cost of the last ida_solve, summed over all IDA* iterations:
 * expanded  = nodes that passed the bound test and had children generated,
 * generated = children produced (one per node_quarter in the search). */
extern uint32_t ida_expanded, ida_generated;

/* The tables are exported so that verify.c can inspect them. */
extern uint16_t perm_q[3][PERMS], ori_q[3][ORIS];
extern uint8_t perm_h[PERMS], ori_h[ORIS];

size_t ida_table_bytes(void); /* total size of the tables above */

#endif
