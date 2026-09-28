/* Minimal freestanding stdio for running the unmodified solver.c on Ripes. */
#ifndef STUB_STDIO_H
#define STUB_STDIO_H
typedef struct stub_file FILE;
extern FILE *stdout, *stderr;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int puts(const char *s);
int fputs(const char *s, FILE *f);
int putchar(int c);
int fflush(FILE *f);
int ferror(FILE *f);
#endif
