# The hand-written RV32I search on Ripes: GNU as, linked with ../tables.s.
#
#   make run                 every case in cases.s on Ripes, print --iret
#   make verify              run, then check every answer on the host
#   make run CASE=21345671111111 EXPECT=11    one state, for its own count
#   make run MOD3=branchless twist sums mod 3 without a branch
#   make run PROC=RV32_5S    pipeline model (much slower)
#   make size                section sizes of the image

RIPES   ?= /Applications/Ripes.app/Contents/MacOS/Ripes
RVAS      ?= riscv-none-elf-as
RVLD      ?= riscv-none-elf-ld
RVOBJCOPY ?= riscv-none-elf-objcopy
RVSIZE    ?= riscv-none-elf-size
PROC    ?= RV32_ISS
TIMEOUT ?= 600000
MOD3    ?= branch
CASE    ?=
EXPECT  ?= 255

# -march=rv32i: a mul, div or rem in the source is an assembly error.
ASFLAGS := -march=rv32i -mabi=ilp32
ifeq ($(MOD3),branchless)
ASFLAGS += --defsym MOD3_BRANCHLESS=1
endif

ifeq ($(CASE),)
CASES := cases.s
else
CASES := case_one.s
endif

RIPESRUN = $(RIPES) --mode cli --src solve.bin -t bin --proc $(PROC) \
           --timeout $(TIMEOUT) --iret --cycles

all: solve.bin

../tables.s:
	$(MAKE) -C .. tables

tables.o: ../tables.s
	$(RVAS) $(ASFLAGS) $< -o $@

case_one.s: FORCE
	printf '    .section .rodata\n    .globl cases\ncases:\n    .ascii "%s"\n    .byte %s, 0\n    .byte 0\n' '$(CASE)' '$(EXPECT)' > $@

# Rebuilt every time so CASE, EXPECT and MOD3 apply. --no-relax: every la
# stays auipc + addi as written, so instruction counts do not depend on where
# the linker places symbols (relaxing la to one instruction when an address
# fits in 12 bits made two builds differ by 18,693 instructions), and no
# code is made gp-relative (gp is never set).
solve.elf: solve.s $(CASES) tables.o ../rv32/link.ld FORCE
	$(RVAS) $(ASFLAGS) solve.s -o solve.o
	$(RVAS) $(ASFLAGS) $(CASES) -o cases.o
	$(RVLD) --no-relax --no-warn-rwx-segments -T ../rv32/link.ld \
	  solve.o cases.o tables.o -o $@

# FORCE too: make 3.81 compares whole seconds, so a solve.elf relinked in the
# same second as the last solve.bin would not refresh it.
solve.bin: solve.elf FORCE
	$(RVOBJCOPY) -O binary $< $@

run: solve.bin
	$(RIPESRUN)

# Ripes -> host: replay, BFS distance, and moves and node counts against the
# host build of ../search.c (../rv32/check_ripes).
verify: solve.bin
	$(MAKE) -s -C ../rv32 check_ripes
	$(RIPESRUN) > ripes_out.txt
	@cat ripes_out.txt
	@echo "---- host check ----"
	../rv32/check_ripes < ripes_out.txt

size: solve.elf
	$(RVSIZE) -A solve.elf

clean:
	$(RM) solve.o cases.o tables.o solve.elf solve.bin case_one.s ripes_out.txt

.PHONY: all run verify size clean FORCE
