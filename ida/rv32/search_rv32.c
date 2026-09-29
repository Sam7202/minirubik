/* search_rv32.c - the Ripes target: IDA* over tables built on the host.
 *
 *   host: gen_tables -> ../tables.c  (const data, linked in as .rodata)
 *   Ripes: this program, ../search.c unmodified, solves cases[]
 *   host: check_ripes replays every answer and checks it is optimal
 *
 * One translation unit, freestanding: Ripes loads a flat binary and
 * implements the Linux-style ecall ABI, while the xPack newlib uses
 * semihosting, so the libc pieces needed are provided here. Nothing is
 * built or copied at run time; the search reads the linked tables directly.
 */
#include <stddef.h>
#include <stdint.h>

/* ---- libc pieces the compiler may expect ---- */

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n--)
        *p++ = (uint8_t) c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

/* ---- Ripes console I/O: SYS_write(64) / SYS_exit(93) ---- */

static void sys_write(const char *buf, int len)
{
    register int a0 __asm__("a0") = 1; /* stdout */
    register const char *a1 __asm__("a1") = buf;
    register int a2 __asm__("a2") = len;
    register int a7 __asm__("a7") = 64;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
}

static void put_str(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    sys_write(s, n);
}

/* Decimal without divide: each digit is how many times its power of ten
 * can be subtracted (at most 9 times). */
static void put_uint(uint32_t v)
{
    static const uint32_t pow10[10] = {1000000000, 100000000, 10000000,
                                       1000000,    100000,    10000,
                                       1000,       100,       10,
                                       1};
    char buf[10];
    int n = 0;
    for (int k = 0; k < 10; k++) {
        char d = '0';
        while (v >= pow10[k]) {
            v -= pow10[k];
            d++;
        }
        if (n || d != '0' || k == 9)
            buf[n++] = d;
    }
    sys_write(buf, n);
}

/* ---- the target search and the host-generated tables, unmodified ---- */

#include "../search.c"
#if CORNER_PDB
#include "../tables.c"
#else
#include "../tables2.c"
#endif

/* ---- input: same 14-digit format as solve.c ---- */

/* The twist sum is kept mod 3 as it is read: both values are at most 2, so
 * one conditional subtraction replaces the modulo. */
static int parse_state(const char *in, state_t *s)
{
    unsigned seen = 0, sum = 0;
    for (int i = 0; i < 14; i++)
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
        if (sum >= 3)
            sum -= 3;
    }
    return sum == 0;
}

/* Scrambles to solve; override with -DCASES='"...","..."'. */
static const char *const cases[] = {
#ifdef CASES
    CASES
#else
    "12345671111111", /* solved */
    "21345671111111", /* the README's example */
    "13245672131111", "41752632313211", "54721631111111",
    "51342763312223", /* worst case for PDB=3 */
#endif
};

int solver_main(void)
{
    put_str("tables ");
    put_uint(tables_bytes);
    put_str(" B, linked read-only\n");

    for (unsigned c = 0; c < sizeof cases / sizeof *cases; c++) {
        state_t s;
        uint8_t moves[MAX_DEPTH];
        put_str(cases[c]);
        if (!parse_state(cases[c], &s)) {
            put_str(" invalid\n");
            continue;
        }
        int len = ida_solve(node_from_state(&s), moves);
        put_str(" ->");
        if (len < 0) {
            put_str(" none\n");
            continue;
        }
        for (int i = 0; i < len; i++) {
            put_str(" ");
            put_str(move_names[moves[i]]);
        }
        put_str(len ? "  [len " : " (solved)  [len ");
        put_uint((uint32_t) len);
        put_str(", expanded ");
        put_uint(ida_expanded);
        put_str(", generated ");
        put_uint(ida_generated);
        put_str("]\n");
    }
    return 0;
}

/* ---- startup: no crt0 under -nostdlib ---- */

__attribute__((naked, section(".text.init"))) void _start(void)
{
    __asm__ volatile(
        ".option push\n"
        ".option norelax\n"
        "la gp, __global_pointer$\n"
        ".option pop\n"
        "la sp, __stack_top\n"
        "la t0, __bss_start\n"
        "la t1, __bss_end\n"
        "1: bgeu t0, t1, 2f\n"
        "sw zero, 0(t0)\n"
        "addi t0, t0, 4\n"
        "j 1b\n"
        "2: call solver_main\n"
        "mv a0, zero\n"
        "li a7, 93\n"
        "ecall\n"
        "3: j 3b\n");
}
