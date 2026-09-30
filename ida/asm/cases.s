# cases.s - the states solve.s solves, each checked inside the program.
#
# One case is 16 bytes: the 14 characters of solver.c's input format, the
# expected optimal length (255 if unknown) and a pad byte. A zero byte ends
# the list. `make run CASE=... EXPECT=...` replaces this file with one case.
    .section .rodata
    .globl cases
cases:
    .ascii "12345671111111"
    .byte 0, 0                  # solved
    .ascii "35621472211121"
    .byte 3, 0                  # short scramble: R D R
    .ascii "62345713133111"
    .byte 8, 0                  # tests/solutions.txt of minirubik
    .ascii "24316572122213"
    .byte 8, 0
    .ascii "25713642221111"
    .byte 8, 0
    .ascii "24513763133333"
    .byte 9, 0
    .ascii "43752611332133"
    .byte 9, 0
    .ascii "25416373331111"
    .byte 10, 0
    .ascii "21345671111111"
    .byte 11, 0                 # distance 11, also reported on its own
    .ascii "51342763312223"
    .byte 11, 0                 # the costliest distance-11 state (three PDBs)
    .byte 0
