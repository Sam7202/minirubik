# Stage 1 measurement A: touch N bytes of guest memory, then exit.
# Change N and compare Ripes' host RSS across runs.
.equ N, 1048576            # bytes to write

.text
main:
    li   t0, 0x10000000     # start: data area, away from the program at 0
    li   t1, N
    add  t1, t0, t1         # t1 = end address (exclusive)
    li   t2, 0x5A           # non-zero value
loop:
    beq  t0, t1, done
    sw   t2, 0(t0)          # write four bytes
    addi t0, t0, 4
    j    loop
done:
    li   a7, 10             # exit (Ripes/MARS convention)
    ecall

