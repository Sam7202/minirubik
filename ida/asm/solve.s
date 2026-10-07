# solve.s - IDA* for the minirubik 2x2x2 cube in hand-written RV32I.
#
# The search is the one in ../search.c, with the same coordinates, tables and
# move order (../tables.h is the contract): faces R, B, D, each turned 90, 180
# and 270 degrees, never the same face twice in a row. So the moves and the
# expanded/generated node counts equal the C search's, and ../rv32/check_ripes
# can check the output against it on the host.
#
# What the assembly does differently from the C:
#  - The depth-first search is one loop over an array of search levels, not
#    recursion. A level is written only when the search goes down into a
#    child; while the children of a node are tried, the child's coordinates,
#    the face's table rows and the pruning limit stay in registers.
#  - A child is pruned as soon as one PDB exceeds limit = bound - g - 1,
#    4-corner PDB first; the maximum of the three is never formed.
#    EARLY_PRUNE=0 (make PRUNE=max) forms it as search.c's node_h() does,
#    to measure what the early exit alone is worth.
#  - No multiply or divide: ranks are Horner sums with x2 to x6 done as shifts
#    and adds, and numbers are printed with Ripes' print-integer call.
#
# Every case checks itself: the moves are replayed on a direct model of the
# cube (source/twist of ../cube.h), and the length is compared with the one
# in the case list. The exit code is the number of failed cases.
#
# RENDER=1 draws the cube as an unfolded net on Ripes' LED matrix before the
# replay and after every move it replays, so the frames follow the solver's
# own output. RENDER=2 draws the same frames as text, for check_render.py on
# the host. RENDER=0, the default and the measured build, has no renderer:
# every RENDER block below assembles to nothing.
#
# Build and run: see Makefile (GNU as, linked with ../tables.s, -t bin).

    .ifndef MOD3_BRANCHLESS
    .equ MOD3_BRANCHLESS, 0     # 1: twist sums mod 3 without a branch
    .endif
    .ifndef EARLY_PRUNE
    .equ EARLY_PRUNE, 1         # 0: h = max of the three PDBs, as search.c
    .endif
    .ifndef RENDER
    .equ RENDER, 0              # 1: LED matrix (GUI), 2: text frames (check)
    .endif

    # The net: faces U; L F R B; D in a 4 x 3 grid of face slots, 2 x 2
    # facelets per face, each facelet 4 LEDs wide and 3 tall, one dark LED
    # between faces: 4 * 8 + 3 = 35 wide, 3 * 6 + 2 = 20 tall. A frame
    # stores PIX bytes per LED and STRIDE LEDs per row.
    .if RENDER == 1
    # GNU as cannot see the symbols Ripes defines for its I/O devices, so the
    # Makefile defines them with the values Ripes lists for the LED Matrix.
    .if LED_MATRIX_0_WIDTH < 35 || LED_MATRIX_0_HEIGHT < 20
    .error "the net needs an LED matrix at least 35 wide and 20 tall"
    .endif
    .equ PIX, 4                 # one 32-bit RGB word per LED
    .equ STRIDE, LED_MATRIX_0_WIDTH
    .elseif RENDER == 2
    .equ PIX, 1                 # one character per LED
    .equ STRIDE, 36             # 35 and a newline
    .endif

    .equ CUBIES, 7
    .equ MAX_DEPTH, 11          # no optimal solution is longer
    .equ SLOT, 16               # bytes per search level, see ida_solve
    .equ NO_FACE, 3             # the root was not produced by any face

    # Row strides of the tables in bytes (tables.h, ../tables.s).
    .equ PERM_Q_ROW, 10080
    .equ ORI_Q_ROW, 1458
    .equ C4POS_Q_ROW, 1680
    .equ C4TW_Q_ROW, 840
    .equ C4_H_BYTES, 68040      # c4_h: 81 rows of 840
    .equ C4_ROW_BYTES, 324      # c4_row, the last table: 81 pointers

    # Ripes environment calls.
    .equ SYS_PRINT_INT, 1
    .equ SYS_WRITE, 64
    .equ SYS_EXIT, 93

# x = x mod 3 for 0 <= x <= 4 (a twist plus a twist); three holds 3.
    .macro MOD3 x, three, tmp
    .if MOD3_BRANCHLESS
    sltiu \tmp, \x, 3           # 1 if x < 3
    addi  \tmp, \tmp, -1        # 0 if x < 3, else all ones
    andi  \tmp, \tmp, 3         # 0 or 3
    sub   \x, \x, \tmp
    .else
    bltu  \x, \three, .Lmod3_\@
    addi  \x, \x, -3
.Lmod3_\@:
    .endif
    .endm

# Print the string constant \label; \label\()_len is its length.
    .macro PRINT label
    la    a0, \label
    li    a1, \label\()_len
    jal   print
    .endm

    .section .rodata
banner:      .ascii "tables "
    .equ banner_len, . - banner
bytes_txt:   .ascii " B\n"
    .equ bytes_txt_len, . - bytes_txt
arrow:       .ascii " ->"
    .equ arrow_len, . - arrow
solved_txt:  .ascii " (solved)"
    .equ solved_txt_len, . - solved_txt
len_txt:     .ascii "  [len "
    .equ len_txt_len, . - len_txt
exp_txt:     .ascii ", expanded "
    .equ exp_txt_len, . - exp_txt
gen_txt:     .ascii ", generated "
    .equ gen_txt_len, . - gen_txt
ok_txt:      .ascii "] ok\n"
    .equ ok_txt_len, . - ok_txt
fail_txt:    .ascii "] FAIL\n"
    .equ fail_txt_len, . - fail_txt
invalid_txt: .ascii " invalid FAIL\n"
    .equ invalid_txt_len, . - invalid_txt

# Move m = face * 3 + turns - 1: R R2 R' B B2 B' D D2 D'.
move_txt:    .ascii " R  R2 R' B  B2 B' D  D2 D'"  # 3 bytes per move
move_len:    .byte 2, 3, 3, 2, 3, 3, 2, 3, 3
move_face:   .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
move_turns:  .byte 1, 2, 3, 1, 2, 3, 1, 2, 3

# cube.h: a quarter turn of face f fills position i with the cubie from
# source[f][i] and adds twist[f][i] to its twist. Used only by replay.
source:      .byte 1, 4, 2, 0, 3, 5, 6
             .byte 0, 1, 2, 4, 5, 6, 3
             .byte 0, 2, 5, 3, 1, 4, 6
twist:       .byte 1, 2, 0, 2, 1, 0, 0
             .byte 0, 0, 0, 1, 2, 1, 2
             .byte 0, 0, 0, 0, 0, 0, 0

    .align 2
# The rows of each face in perm_q, ori_q, c4pos_q, c4tw_q.
face_rows:
    .word perm_q, ori_q, c4pos_q, c4tw_q
    .word perm_q + PERM_Q_ROW, ori_q + ORI_Q_ROW
    .word c4pos_q + C4POS_Q_ROW, c4tw_q + C4TW_Q_ROW
    .word perm_q + 2 * PERM_Q_ROW, ori_q + 2 * ORI_Q_ROW
    .word c4pos_q + 2 * C4POS_Q_ROW, c4tw_q + 2 * C4TW_Q_ROW

    .if RENDER
# Faces: 0 U, 1 R, 2 F, 3 D, 4 L, 5 B. slot_face[j] lists the faces of the
# three facelets of position j, starting at its U or D facelet and going
# counterclockwise as seen from outside the corner. A cubie of twist t in
# position j has its U/D sticker t facelets on, so facelet m shows sticker
# (m - t) mod 3 of that cubie, whose colours are slot_face of its home
# position (the cubie number). Derived from cube.h's twist[]; see the README.
slot_face:   .byte 0, 4, 2          # 0 ULF, the fixed corner
             .byte 0, 2, 1          # 1 UFR
             .byte 3, 1, 2          # 2 DRF
             .byte 3, 2, 4          # 3 DFL
             .byte 0, 1, 5          # 4 URB
             .byte 3, 5, 1          # 5 DBR
             .byte 3, 4, 5          # 6 DLB
             .byte 0, 5, 4          # 7 UBL

    .align 1
# facelet_off[j][m]: offset of the top-left LED of facelet m of position j,
# at column x and row y of the net.
    .macro FACELET x, y
    .half PIX * (\y * STRIDE + \x)
    .endm
facelet_off: FACELET  9, 3;  FACELET  4,  7; FACELET  9,  7
             FACELET 13, 3;  FACELET 13,  7; FACELET 18,  7
             FACELET 13, 14; FACELET 18, 10; FACELET 13, 10
             FACELET  9, 14; FACELET  9, 10; FACELET  4, 10
             FACELET 13, 0;  FACELET 22,  7; FACELET 27,  7
             FACELET 13, 17; FACELET 27, 10; FACELET 22, 10
             FACELET  9, 17; FACELET  0, 10; FACELET 31, 10
             FACELET  9, 0;  FACELET 31,  7; FACELET  0,  7

    .align 2
    .if RENDER == 1
colors:      .word 0xFFFFFF         # U white
             .word 0xFF0000         # R red
             .word 0x00C000         # F green
             .word 0xFFFF00         # D yellow
             .word 0xFF8000         # L orange
             .word 0x0040FF         # B blue
    .else
colors:      .word 'U', 'R', 'F', 'D', 'L', 'B'
    .endif
    .endif

    .if RENDER == 2
    .data
# One frame as text: a newline, then 20 rows of 35 LEDs ('.' when dark).
net_txt:     .byte 10
net_rows:    .rept 20
             .ascii "..................................."
             .byte 10
             .endr
    .equ net_txt_len, . - net_txt
    .endif

    .bss
    .align 2
slots:       .zero SLOT * MAX_DEPTH  # search levels 0..10, see ida_solve
n_expanded:  .zero 4            # node counts of the last search, as
n_generated: .zero 4            # ida_expanded/ida_generated in search.c
path:        .zero 12           # moves of the last solution
cube_a:      .zero 16           # p[7] then o[7] of the case being solved
cube_b:      .zero 16           # replay's second buffer
lehmer:      .zero 8            # c_i of the permutation's Lehmer code
c4pos:       .zero 12           # pos[4], tw[4], r[4] of cubies 0..3

    .section .text.init, "ax"
    .globl _start
_start:
    la    sp, __stack_top
    PRINT banner                # "tables N B": bytes of the linked tables:
    la    a0, perm_q            # perm_q (the first) to the end of c4_h,
    la    t0, c4_h + C4_H_BYTES # plus c4_row; the padding that aligns
    sub   a0, t0, a0            # c4_row to a word is not a table byte
    addi  a0, a0, C4_ROW_BYTES
    jal   print_int
    PRINT bytes_txt
    la    s0, cases             # current case, see cases.s
    li    s1, 0                 # failed cases

next_case:
    lbu   t0, 0(s0)
    beqz  t0, all_done
    mv    a0, s0
    li    a1, 14
    jal   print
    PRINT arrow
    mv    a0, s0
    jal   parse
    beqz  a0, invalid
    jal   start_node
    jal   ida_solve
    mv    s2, a0                # length

    bnez  s2, 1f
    PRINT solved_txt
1:  li    s3, 0                 # moves printed
    j     3f
2:  la    t0, path
    add   t0, t0, s3
    lbu   t0, 0(t0)             # move m
    la    t1, move_len
    add   t1, t1, t0
    lbu   a1, 0(t1)
    slli  t1, t0, 1             # m * 3 = (m << 1) + m
    add   t1, t1, t0
    la    a0, move_txt
    add   a0, a0, t1
    jal   print
    addi  s3, s3, 1
3:  bne   s3, s2, 2b

    PRINT len_txt
    mv    a0, s2
    jal   print_int
    PRINT exp_txt
    la    t0, n_expanded
    lw    a0, 0(t0)
    jal   print_int
    PRINT gen_txt
    la    t0, n_generated
    lw    a0, 0(t0)
    jal   print_int

    mv    a0, s2                # check: the moves solve the cube ...
    jal   replay
    lbu   t0, 14(s0)            # ... in the expected number (255: unknown)
    li    t1, 255
    beq   t0, t1, 4f
    beq   t0, s2, 4f
    li    a0, 0
4:  beqz  a0, 5f
    PRINT ok_txt
    j     6f
5:  PRINT fail_txt
    addi  s1, s1, 1
6:  addi  s0, s0, 16
    j     next_case

invalid:
    PRINT invalid_txt
    addi  s1, s1, 1
    addi  s0, s0, 16
    j     next_case

all_done:
    mv    a0, s1                # exit code: failed cases
    li    a7, SYS_EXIT
    ecall

    .text

# print: write(1, a0, a1). Clobbers a0-a2, a7.
print:
    mv    a2, a1
    mv    a1, a0
    li    a0, 1
    li    a7, SYS_WRITE
    ecall
    ret

# print_int: a0 in decimal. Clobbers a7.
print_int:
    li    a7, SYS_PRINT_INT
    ecall
    ret

# parse: a0 = the 14 characters of a case. Writes p[7] and o[7] to cube_a and
# returns a0 = 1 if they are a valid state (labels 1-7 each once, twists 1-3,
# twist sum a multiple of 3), otherwise a0 = 0.
parse:
    la    t0, cube_a
    li    t1, 0                 # bit p set once cubie p is placed
    li    t2, 0                 # twist sum mod 3
    li    t3, 3
    addi  a1, a0, CUBIES        # end of the position labels
1:  lbu   t4, 0(a0)
    addi  t4, t4, -49           # '1' -> 0
    li    t5, CUBIES
    bgeu  t4, t5, 2f            # not 1-7 (below '1' wraps around)
    lbu   t5, CUBIES(a0)
    addi  t5, t5, -49
    bgeu  t5, t3, 2f            # not 1-3
    li    t6, 1
    sll   t6, t6, t4
    and   a2, t1, t6
    bnez  a2, 2f                # cubie placed twice
    or    t1, t1, t6
    sb    t4, 0(t0)
    sb    t5, CUBIES(t0)
    add   t2, t2, t5
    MOD3  t2, t3, t6
    addi  a0, a0, 1
    addi  t0, t0, 1
    bne   a0, a1, 1b
    seqz  a0, t2
    ret
2:  li    a0, 0
    ret

# start_node: the coordinates (tables.h) of the cube in cube_a:
# a0 = p, a1 = o, a2 = cp, a3 = co.
start_node:
    la    t0, cube_a
    # c_i = #{j > i: p[j] < p[i]} for i = 0..5, into lehmer[]
    la    t6, lehmer
    mv    t1, t0                # &p[i]
    addi  t5, t0, CUBIES        # &p[7]
    addi  a5, t0, CUBIES - 1    # &p[6]: one past the last i
1:  lbu   t3, 0(t1)
    li    t4, 0
    addi  t2, t1, 1             # &p[j]
2:  lbu   a4, 0(t2)
    sltu  a4, a4, t3
    add   t4, t4, a4
    addi  t2, t2, 1
    bne   t2, t5, 2b
    sb    t4, 0(t6)
    addi  t6, t6, 1
    addi  t1, t1, 1
    bne   t1, a5, 1b
    # p = ((((c0 * 6 + c1) * 5 + c2) * 4 + c3) * 3 + c4) * 2 + c5
    la    t6, lehmer
    lbu   a0, 0(t6)
    slli  t1, a0, 1             # x6 = (x << 1) + (x << 2)
    slli  a0, a0, 2
    add   a0, a0, t1
    lbu   t1, 1(t6)
    add   a0, a0, t1
    slli  t1, a0, 2             # x5 = (x << 2) + x
    add   a0, a0, t1
    lbu   t1, 2(t6)
    add   a0, a0, t1
    slli  a0, a0, 2             # x4
    lbu   t1, 3(t6)
    add   a0, a0, t1
    slli  t1, a0, 1             # x3 = (x << 1) + x
    add   a0, a0, t1
    lbu   t1, 4(t6)
    add   a0, a0, t1
    slli  a0, a0, 1             # x2
    lbu   t1, 5(t6)
    add   a0, a0, t1
    # o = base 3 of the twists of positions 0..5, position 0 first
    addi  t1, t0, CUBIES        # &o[0]
    addi  t2, t0, CUBIES + 6    # &o[6]
    li    a1, 0
3:  slli  t3, a1, 1             # x3
    add   a1, a1, t3
    lbu   t3, 0(t1)
    add   a1, a1, t3
    addi  t1, t1, 1
    bne   t1, t2, 3b
    # pos[k] and tw[k]: where cubie k = 0..3 is, and its twist
    la    t6, c4pos
    li    t1, 0                 # position i
    li    t5, 4
4:  add   t2, t0, t1
    lbu   t3, 0(t2)             # the cubie at i
    bgeu  t3, t5, 5f            # not one of the four
    add   t4, t6, t3
    sb    t1, 0(t4)             # pos[cubie] = i
    lbu   t2, CUBIES(t2)
    sb    t2, 4(t4)             # tw[cubie] = o[i]
5:  addi  t1, t1, 1
    li    t2, CUBIES
    bne   t1, t2, 4b
    # r_k = pos[k] - #{j < k: pos[j] < pos[k]} for k = 1..3, into r[]
    li    t1, 1                 # k
6:  add   t2, t6, t1            # &pos[k]
    lbu   t3, 0(t2)
    mv    t4, t3
    mv    a4, t6                # &pos[j]
7:  lbu   a5, 0(a4)
    sltu  a5, a5, t3
    sub   t4, t4, a5
    addi  a4, a4, 1
    bne   a4, t2, 7b
    sb    t4, 8(t2)             # r[k]
    addi  t1, t1, 1
    bne   t1, t5, 6b
    # cp = ((r0 * 6 + r1) * 5 + r2) * 4 + r3, with r0 = pos[0]
    lbu   a2, 0(t6)
    slli  t1, a2, 1             # x6
    slli  a2, a2, 2
    add   a2, a2, t1
    lbu   t1, 9(t6)
    add   a2, a2, t1
    slli  t1, a2, 2             # x5
    add   a2, a2, t1
    lbu   t1, 10(t6)
    add   a2, a2, t1
    slli  a2, a2, 2             # x4
    lbu   t1, 11(t6)
    add   a2, a2, t1
    # co = base 3 of tw[0..3], cubie 0 first
    addi  t1, t6, 4
    addi  t2, t6, 8
    li    a3, 0
8:  slli  t3, a3, 1             # x3
    add   a3, a3, t3
    lbu   t3, 0(t1)
    add   a3, a3, t3
    addi  t1, t1, 1
    bne   t1, t2, 8b
    ret

# ida_solve: a0..a3 = the start node (p, o, cp, co). Finds an optimal solution:
# a0 = its length, the moves in path[], the node counts in n_expanded and
# n_generated (counted as in search.c).
#
# Search level g = 0..10 is slots + 16 g:
#   +0 p, +2 o, +4 cp (halfwords), +6 co, +7 the face that produced the node,
#   +8 face and +9 turn (0..2) of the child being searched below it (bytes).
# Registers in the search loop:
#   s0 level being expanded      s9 limit = bound - g - 1      a6 bound
#   s1 perm_h  s2 ori_h  s3 add81  s4 c4_row   a7 face_rows   t5 slots
#   s5..s8 the face's rows in perm_q, ori_q, c4pos_q, c4tw_q   t6 = 3
#   s10 generated  s11 expanded
#   a0..a3 the current child (p, o, cp, co)  a4 face  a5 turn
ida_solve:
    addi  sp, sp, -64
    sw    ra, 0(sp)
    sw    s0, 4(sp)
    sw    s1, 8(sp)
    sw    s2, 12(sp)
    sw    s3, 16(sp)
    sw    s4, 20(sp)
    sw    s5, 24(sp)
    sw    s6, 28(sp)
    sw    s7, 32(sp)
    sw    s8, 36(sp)
    sw    s9, 40(sp)
    sw    s10, 44(sp)
    sw    s11, 48(sp)
    li    s10, 0
    li    s11, 0
    li    t0, 0                 # length, if the start is solved
    or    t1, a0, a1
    beqz  t1, .Lreturn
    la    s1, perm_h
    la    s2, ori_h
    la    s3, add81
    la    s4, c4_row
    la    a7, face_rows
    la    t5, slots
    li    t6, 3
    mv    s0, t5
    sh    a0, 0(s0)
    sh    a1, 2(s0)
    sh    a2, 4(s0)
    sb    a3, 6(s0)
    li    t0, NO_FACE
    sb    t0, 7(s0)
    # bound = h(start), the largest of the three PDBs
    slli  t0, a3, 2
    add   t0, s4, t0
    lw    t0, 0(t0)
    add   t0, t0, a2
    lbu   a6, 0(t0)
    add   t0, s1, a0
    lbu   t0, 0(t0)
    bgeu  a6, t0, 1f
    mv    a6, t0
1:  add   t0, s2, a1
    lbu   t0, 0(t0)
    bgeu  a6, t0, .Literation
    mv    a6, t0

.Literation:                    # one depth-first search within the bound
    mv    s0, t5
    addi  s9, a6, -1
.Lexpand:                       # try the children of the node at s0
    addi  s11, s11, 1
    li    a4, 0
.Lface:
    lbu   t0, 7(s0)
    beq   a4, t0, .Lnext_face   # never the same face twice in a row
    slli  t0, a4, 4
    add   t0, t0, a7
    lw    s5, 0(t0)
    lw    s6, 4(t0)
    lw    s7, 8(t0)
    lw    s8, 12(t0)
    lhu   a0, 0(s0)             # the first child starts from the node
    lhu   a1, 2(s0)
    lhu   a2, 4(s0)
    lbu   a3, 6(s0)
    li    a5, 0
.Lturn:                         # one more quarter turn gives the next child
    slli  t0, a0, 1
    add   t0, s5, t0
    lhu   a0, 0(t0)             # p = perm_q[f][p]
    slli  t0, a1, 1
    add   t0, s6, t0
    lhu   a1, 0(t0)             # o = ori_q[f][o]
    add   t0, s8, a2
    lbu   t1, 0(t0)             # twist added, from the old cp
    slli  t0, a2, 1
    add   t0, s7, t0
    lhu   a2, 0(t0)             # cp = c4pos_q[f][cp]
    slli  t0, a3, 7
    add   t0, s3, t0
    add   t0, t0, t1
    lbu   a3, 0(t0)             # co = add81[co][twist]
    addi  s10, s10, 1
    .if EARLY_PRUNE
    slli  t0, a3, 2             # the 4-corner PDB first: it prunes most
    add   t0, s4, t0
    lw    t0, 0(t0)
    add   t0, t0, a2
    lbu   t0, 0(t0)
    bltu  s9, t0, .Lnext_turn   # g + 1 + h > bound
    add   t0, s1, a0
    lbu   t0, 0(t0)
    bltu  s9, t0, .Lnext_turn
    add   t0, s2, a1
    lbu   t0, 0(t0)
    bltu  s9, t0, .Lnext_turn
    .else
    add   t0, s1, a0            # h = max(perm_h, ori_h, c4_h), in the order
    lbu   t0, 0(t0)             # of search.c's node_h()
    add   t1, s2, a1
    lbu   t1, 0(t1)
    bgeu  t0, t1, .Lmax_ori
    mv    t0, t1
.Lmax_ori:
    slli  t1, a3, 2
    add   t1, s4, t1
    lw    t1, 0(t1)
    add   t1, t1, a2
    lbu   t1, 0(t1)
    bgeu  t0, t1, .Lmax_c4
    mv    t0, t1
.Lmax_c4:
    bltu  s9, t0, .Lnext_turn   # g + 1 + h > bound
    .endif
    or    t0, a0, a1
    beqz  t0, .Lfound           # p = o = 0: solved
    sb    a4, 8(s0)             # go down into this child
    sb    a5, 9(s0)
    addi  s0, s0, SLOT
    sh    a0, 0(s0)
    sh    a1, 2(s0)
    sh    a2, 4(s0)
    sb    a3, 6(s0)
    sb    a4, 7(s0)
    addi  s9, s9, -1
    j     .Lexpand
.Lnext_turn:
    addi  a5, a5, 1
    bne   a5, t6, .Lturn
.Lnext_face:
    addi  a4, a4, 1
    bne   a4, t6, .Lface
    beq   s0, t5, .Lbound_up    # all children tried: back up one level
    addi  s0, s0, -SLOT
    addi  s9, s9, 1
    lbu   a4, 8(s0)
    lbu   a5, 9(s0)
    slli  t0, a4, 4
    add   t0, t0, a7
    lw    s5, 0(t0)
    lw    s6, 4(t0)
    lw    s7, 8(t0)
    lw    s8, 12(t0)
    lhu   a0, SLOT + 0(s0)      # continue turning the child searched below
    lhu   a1, SLOT + 2(s0)
    lhu   a2, SLOT + 4(s0)
    lbu   a3, SLOT + 6(s0)
    j     .Lnext_turn
.Lbound_up:                     # no solution within the bound
    addi  a6, a6, 1
    li    t0, MAX_DEPTH
    bgeu  t0, a6, .Literation
    li    t0, -1                # unreachable for a valid state
    j     .Lreturn

.Lfound:                        # moves: each level's child, then this one
    mv    t0, t5
    la    t1, path
1:  beq   t0, s0, 2f
    lbu   t2, 8(t0)             # face * 3 + turn
    slli  t3, t2, 1
    add   t2, t2, t3
    lbu   t3, 9(t0)
    add   t2, t2, t3
    sb    t2, 0(t1)
    addi  t0, t0, SLOT
    addi  t1, t1, 1
    j     1b
2:  slli  t3, a4, 1
    add   t3, t3, a4
    add   t3, t3, a5
    sb    t3, 0(t1)
    la    t0, path
    sub   t0, t1, t0
    addi  t0, t0, 1             # length

.Lreturn:                       # t0 = length
    la    t1, n_expanded
    sw    s11, 0(t1)
    sw    s10, 4(t1)            # n_generated
    mv    a0, t0
    lw    ra, 0(sp)
    lw    s0, 4(sp)
    lw    s1, 8(sp)
    lw    s2, 12(sp)
    lw    s3, 16(sp)
    lw    s4, 20(sp)
    lw    s5, 24(sp)
    lw    s6, 28(sp)
    lw    s7, 32(sp)
    lw    s8, 36(sp)
    lw    s9, 40(sp)
    lw    s10, 44(sp)
    lw    s11, 48(sp)
    addi  sp, sp, 64
    ret

# replay: a0 = number of moves in path[]. Applies them to cube_a with the
# direct model (cube.h's quarter_turn, alternating with cube_b) and returns
# a0 = 1 if the cube ends solved (p[i] = i, o[i] = 0), otherwise 0.
replay:
    .if RENDER
    addi  sp, sp, -16
    sw    ra, 12(sp)
    .endif
    la    a1, path
    add   a2, a1, a0            # end of the moves
    la    a3, cube_a            # current cube
    la    a4, cube_b            # next cube
    li    t6, 3
    .if RENDER
    jal   draw                  # the cube as given
    .endif
1:  beq   a1, a2, 5f
    lbu   t0, 0(a1)             # move
    la    t1, move_face
    add   t1, t1, t0
    lbu   t2, 0(t1)             # face
    la    t1, move_turns
    add   t1, t1, t0
    lbu   a5, 0(t1)             # quarter turns
    slli  t3, t2, 3             # face * 7 = (face << 3) - face
    sub   t3, t3, t2
    la    t4, source
    add   t4, t4, t3
    la    t5, twist
    add   t5, t5, t3
2:  li    a6, 0                 # one quarter turn, position a6
3:  add   t0, t4, a6
    lbu   t0, 0(t0)             # source[f][i]
    add   t0, a3, t0
    lbu   t1, 0(t0)             # its cubie ...
    lbu   t2, CUBIES(t0)        # ... and twist
    add   t0, t5, a6
    lbu   t0, 0(t0)
    add   t2, t2, t0            # plus twist[f][i], mod 3
    MOD3  t2, t6, t0
    add   t0, a4, a6
    sb    t1, 0(t0)
    sb    t2, CUBIES(t0)
    addi  a6, a6, 1
    li    t0, CUBIES
    bne   a6, t0, 3b
    mv    t0, a3                # the next cube becomes the current one
    mv    a3, a4
    mv    a4, t0
    addi  a5, a5, -1
    bnez  a5, 2b
    addi  a1, a1, 1
    .if RENDER
    jal   draw                  # the cube after this move
    .endif
    j     1b
5:  li    a6, 0                 # solved: p[i] = i and o[i] = 0
6:  add   t0, a3, a6
    lbu   t1, 0(t0)
    bne   t1, a6, 7f
    lbu   t1, CUBIES(t0)
    bnez  t1, 7f
    addi  a6, a6, 1
    li    t0, CUBIES
    bne   a6, t0, 6b
    li    a0, 1
    .if RENDER
    j     8f
    .else
    ret
    .endif
7:  li    a0, 0
    .if RENDER
8:  lw    ra, 12(sp)
    addi  sp, sp, 16
    .endif
    ret

    .if RENDER
# draw: one frame of the cube at a3 (p[7] then o[7]): RENDER=1 writes the
# LED matrix, RENDER=2 prints the frame as text. Keeps every register
# replay uses (a1-a4, t6).
draw:
    addi  sp, sp, -32
    sw    ra, 28(sp)
    sw    a1, 24(sp)
    sw    a2, 20(sp)
    sw    a3, 16(sp)
    sw    a4, 12(sp)
    sw    t6, 8(sp)
    .if RENDER == 1
    li    a7, LED_MATRIX_0_BASE
    .else
    la    a7, net_rows
    .endif
    li    a5, 0                 # position j
1:  li    a6, 0                 # position 0 holds cubie 0, never twisted
    li    a4, 0
    beqz  a5, 2f
    add   t0, a3, a5
    lbu   a6, -1(t0)            # p[j - 1] + 1: the cubie's home position
    addi  a6, a6, 1
    lbu   a4, CUBIES - 1(t0)    # its twist
2:  slli  t0, a6, 1             # home * 3
    add   t0, t0, a6
    la    t1, slot_face
    add   a6, t1, t0            # its colours, sticker 0 first
    slli  t0, a5, 1             # j * 3 halfwords
    add   t0, t0, a5
    slli  t0, t0, 1
    la    t1, facelet_off
    add   a2, t1, t0            # this position's facelets
    li    a0, 0                 # facelet m
3:  sub   t0, a0, a4            # sticker (m - twist) mod 3
    bgez  t0, 4f
    addi  t0, t0, 3
4:  add   t0, a6, t0
    lbu   t0, 0(t0)             # its face
    slli  t0, t0, 2
    la    t1, colors
    add   t0, t1, t0
    lw    t4, 0(t0)             # its colour
    slli  t0, a0, 1
    add   t0, a2, t0
    lhu   t2, 0(t0)
    add   t2, a7, t2            # top-left LED of the facelet
    li    t5, 3                 # 3 rows of 4 LEDs
5:
    .if RENDER == 1
    sw    t4, 0(t2)
    sw    t4, 4(t2)
    sw    t4, 8(t2)
    sw    t4, 12(t2)
    .else
    sb    t4, 0(t2)
    sb    t4, 1(t2)
    sb    t4, 2(t2)
    sb    t4, 3(t2)
    .endif
    addi  t2, t2, PIX * STRIDE
    addi  t5, t5, -1
    bnez  t5, 5b
    addi  a0, a0, 1
    li    t0, 3
    bne   a0, t0, 3b
    addi  a5, a5, 1
    li    t0, 8
    bne   a5, t0, 1b
    .if RENDER == 2
    PRINT net_txt
    .endif
    lw    ra, 28(sp)
    lw    a1, 24(sp)
    lw    a2, 20(sp)
    lw    a3, 16(sp)
    lw    a4, 12(sp)
    lw    t6, 8(sp)
    addi  sp, sp, 32
    ret
    .endif
