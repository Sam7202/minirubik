/* Startup and the runtime helpers clang may call on RV32I (no M extension).
 * __mulsi3 / __udivsi3 / __umodsi3 follow libgcc's RISC-V algorithms. */
    .section .text.start, "ax"
    .globl _start
_start:
    li   sp, 0x03FFFFF0
    call main
    li   a7, 93
    ecall

    .text
    .globl __mulsi3
__mulsi3:
    mv   a2, a0
    li   a0, 0
1:  andi a3, a1, 1
    beqz a3, 2f
    add  a0, a0, a2
2:  srli a1, a1, 1
    slli a2, a2, 1
    bnez a1, 1b
    ret

    .globl __udivsi3
__udivsi3:
    mv   a2, a1
    mv   a1, a0
    li   a0, -1
    beqz a2, 5f
    li   a3, 1
    bgeu a2, a1, 2f
1:  blez a2, 2f
    slli a2, a2, 1
    slli a3, a3, 1
    bgtu a1, a2, 1b
2:  li   a0, 0
3:  bltu a1, a2, 4f
    sub  a1, a1, a2
    or   a0, a0, a3
4:  srli a3, a3, 1
    srli a2, a2, 1
    bnez a3, 3b
5:  ret

    .globl __umodsi3
__umodsi3:
    mv   t0, ra
    call __udivsi3
    mv   a0, a1
    jr   t0

    .globl memset
memset:
    mv   t0, a0
    beqz a2, 2f
1:  sb   a1, 0(t0)
    addi t0, t0, 1
    addi a2, a2, -1
    bnez a2, 1b
2:  ret
