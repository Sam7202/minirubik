/* tmain.c - RV32I test harness around the target search. The tables are
 * linked in as read-only data (tables.c or tables.s); nothing is built at
 * run time. For each state the emulator placed in INBUF, it solves the
 * state between two ecall marks, so the emulator can count the
 * instructions of each query, and stores (length, expanded, generated).
 */
#include "search.h"

#define INBUF ((const volatile uint8_t *) 0x02000000u)
#define OUTBUF ((volatile uint32_t *) 0x02100000u)

static inline void mark(uint32_t tag)
{
    register uint32_t a0 __asm__("a0") = tag;
    register uint32_t a7 __asm__("a7") = 1;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

int main(void)
{
    mark(0);
    uint32_t n = *(const volatile uint32_t *) INBUF;
    const volatile uint8_t *in = INBUF + 4;
    volatile uint32_t *out = OUTBUF;
    for (uint32_t i = 0; i < n; i++, in += 14, out += 3) {
        state_t s;
        for (int k = 0; k < CUBIES; k++) {
            s.p[k] = (uint8_t) (in[k] - '1');
            s.o[k] = (uint8_t) (in[k + CUBIES] - '1');
        }
        uint8_t moves[MAX_DEPTH];
        mark(100);
        int len = ida_solve(node_from_state(&s), moves);
        mark(101);
        out[0] = (uint32_t) len;
        out[1] = ida_expanded;
        out[2] = ida_generated;
    }
    return 0;
}
