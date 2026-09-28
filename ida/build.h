/* build.h - host-side construction of every table in tables.h. Used by the
 * generator (gen_tables.c) and by verify.c to check that tables.c is current.
 * None of this runs on the target. */
#ifndef BUILD_H
#define BUILD_H

#include "tables.h"

typedef struct {
    uint16_t perm_q[3][PERMS];
    uint16_t ori_q[3][ORIS];
    uint8_t perm_h[PERMS];
    uint8_t ori_h[ORIS];
#if CORNER_PDB
    uint16_t c4pos_q[3][C4POS];
    uint8_t c4tw_q[3][C4POS];
    uint8_t add81[C4ORI][ADD81_COLS];
    uint8_t c4_h[C4ORI][C4POS];
#endif
} tables_t;

void build_tables(tables_t *t);

#endif
