/* baseline_rv32.c - the original minirubik solver.c, unmodified, on Ripes (baseline for v2).
 *
 * Two things are required to run solver.c on Ripes at all: Ripes has no
 * command line, so the input is given with -DCASE='"..."' and passed as
 * argv; and newlib's malloc, which grows the heap with brk, cannot get the
 * 3.5 MB table and the 14 MB queue there, so malloc is a bump allocator.
 * The rest (stub headers in include/, %s-only printf, Ripes ecalls) only
 * keeps newlib out of the image so that --iret is solver.c's own work.
 * main is renamed to solver_main. Each run - exactly like the host binary -
 * rebuilds the full 3,674,160-state BFS table first.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "include/stdio.h"
#include "include/stdlib.h"
#include "include/string.h"

#ifndef CASE
#define CASE "21345671111111"
#endif

/* ---- Ripes console I/O: SYS_write(64) ---- */

static void sys_write(int fd, const char *buf, int len)
{
    register int a0 __asm__("a0") = fd;
    register const char *a1 __asm__("a1") = buf;
    register int a2 __asm__("a2") = len;
    register int a7 __asm__("a7") = 64;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
}

struct stub_file {
    int fd;
};
static struct stub_file out = {1}, err = {2};
FILE *stdout = &out, *stderr = &err;

static int put_s(FILE *f, const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    sys_write(f->fd, s, n);
    return n;
}

/* Only "%s" is used by solver.c. */
static int vput(FILE *f, const char *fmt, va_list ap)
{
    int n = 0;
    for (; *fmt; fmt++) {
        if (fmt[0] == '%' && fmt[1] == 's') {
            n += put_s(f, va_arg(ap, const char *));
            fmt++;
        } else {
            sys_write(f->fd, fmt, 1);
            n++;
        }
    }
    return n;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vput(stdout, fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vput(f, fmt, ap);
    va_end(ap);
    return n;
}

int fputs(const char *s, FILE *f) { return put_s(f, s); }
int puts(const char *s) { put_s(stdout, s); sys_write(1, "\n", 1); return 0; }
int putchar(int c) { char ch = (char) c; sys_write(1, &ch, 1); return c; }
int fflush(FILE *f) { (void) f; return 0; }
int ferror(FILE *f) { (void) f; return 0; }

/* ---- memory: bump allocator over the (sparse) space past .bss ---- */

extern char __heap_start[];
static char *heap = __heap_start;

void *malloc(size_t n)
{
    void *p = heap;
    heap += (n + 7) & ~(size_t) 7;
    return p;
}
void free(void *p) { (void) p; }

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

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n--; p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return (uint8_t) *a - (uint8_t) *b;
}

/* ---- the original solver, unmodified ---- */

#define main solver_main
#include "../../../solver.c"
#undef main

int baseline_main(void)
{
    static char arg[] = CASE;
    char *argv[] = {"solver", arg, NULL};
    return solver_main(2, argv);
}

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
        "2: call baseline_main\n"
        "li a7, 93\n"
        "ecall\n"
        "3: j 3b\n");
}
