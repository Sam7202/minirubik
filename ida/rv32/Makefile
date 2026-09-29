# Host builds the tables, Ripes runs the search, host checks the answers.
#
#   make verify           full flow: ../tables.c -> Ripes -> check_ripes
#   make run              just run on Ripes and print --iret
#   make run CASES='"21345671111111"'     one scramble, for a per-query count
#   make run PDB=2        two PDBs only (../tables2.c)
#   make run ARCH=rv32im ISAEXTS=M
#   make run PROC=RV32_5S                pipeline model (much slower)
#   make size             .text / .rodata / .bss of the Ripes image
#
# The baseline (original minirubik solver.c on Ripes) is in baseline/.

RIPES     ?= /Applications/Ripes.app/Contents/MacOS/Ripes
RVCC      ?= riscv-none-elf-gcc
RVSIZE    ?= riscv-none-elf-size
RVOBJCOPY ?= riscv-none-elf-objcopy
ARCH      ?= rv32i
ABI       ?= ilp32
PROC      ?= RV32_ISS      # functional ISA sim: ~120x faster wall-clock.
                          # Use RV32_5S for pipeline cycle counts.
ISAEXTS   ?=
TIMEOUT   ?= 600000
PDB       ?= 3
CASES     ?=

ifeq ($(PDB),2)
DEFS   := -DCORNER_PDB=0
TABLES := ../tables2.c
else
DEFS   := -DCORNER_PDB=1
TABLES := ../tables.c
endif

HOSTCFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
RVCFLAGS   ?= -march=$(ARCH) -mabi=$(ABI) -O2 -std=c99 -Wall -Wextra \
              -ffreestanding -nostdlib -fno-builtin \
              -fno-tree-loop-distribute-patterns -g -I..
DEPS := ../search.c ../search.h ../tables.h ../cube.h
RIPESRUN = $(RIPES) --mode cli --src search_rv32.bin -t bin --proc $(PROC) \
           $(if $(ISAEXTS),--isaexts $(ISAEXTS),) --timeout $(TIMEOUT)

all: search_rv32.bin

# Step 1 on the host: the top-level Makefile generates the tables.
../tables.c:
	$(MAKE) -C .. tables
../tables2.c:
	$(MAKE) -C .. tables PDB=2

# Step 2: the Ripes image. Rebuilt every time so CASES/PDB/ARCH apply.
# No -lgcc: RV32I has no multiply or divide, so any a * b, a / b or a % b
# that gcc cannot turn into shifts becomes a call to __mulsi3/__udivsi3...
# and must fail to link rather than slip in.
search_rv32.elf: search_rv32.c $(TABLES) link.ld $(DEPS)
	$(RVCC) $(RVCFLAGS) $(DEFS) $(if $(CASES),-DCASES='$(CASES)') \
	  -T link.ld search_rv32.c -o $@
	@$(RVSIZE) $@

search_rv32.bin: search_rv32.elf
	$(RVOBJCOPY) -O binary $< $@

run: search_rv32.bin
	$(RIPESRUN) --iret --cycles --cpi

# Step 3 on the host: replay each Ripes answer on the reference cube and
# compare its length with a full BFS. ripes_out.txt keeps the raw log.
# Links the host build of the same search and tables as the reference for
# the node-count check; rebuilt every time so PDB applies.
check_ripes: check_ripes.c $(TABLES) $(DEPS)
	$(CC) $(HOSTCFLAGS) $(DEFS) check_ripes.c ../search.c $(TABLES) -o $@

verify: search_rv32.bin check_ripes
	$(RIPESRUN) --iret > ripes_out.txt 2>&1
	@cat ripes_out.txt
	@echo "---- host check ----"
	./check_ripes < ripes_out.txt

size: search_rv32.elf
	$(RVSIZE) -A $< | awk '$$1 ~ /^\./ && $$2 > 0'

clean:
	$(RM) search_rv32.elf search_rv32.bin check_ripes ripes_out.txt

.PHONY: all run verify size clean search_rv32.elf check_ripes
