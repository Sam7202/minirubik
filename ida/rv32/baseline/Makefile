# The original minirubik solver.c on Ripes, as the baseline for --iret.
#
#   make run                    rv32i, one scramble (default: README example)
#   make run CASE=41752632313211
#   make run ARCH=rv32im ISAEXTS=M

RIPES     ?= /Applications/Ripes.app/Contents/MacOS/Ripes
RVCC      ?= riscv-none-elf-gcc
RVSIZE    ?= riscv-none-elf-size
RVOBJCOPY ?= riscv-none-elf-objcopy
ARCH      ?= rv32i
ABI       ?= ilp32
PROC      ?= RV32_ISS
ISAEXTS   ?=
TIMEOUT   ?= 3600000
CASE      ?= 21345671111111

GCCINC   := $(shell $(RVCC) -print-file-name=include)
RVCFLAGS ?= -march=$(ARCH) -mabi=$(ABI) -O2 -std=c99 -Wall -Wextra \
            -ffreestanding -nostdlib -fno-builtin \
            -fno-tree-loop-distribute-patterns -nostdinc -isystem $(GCCINC) -Iinclude -g
SRC := ../../../solver.c

all: baseline_rv32.bin

baseline_rv32.elf: baseline_rv32.c link.ld $(SRC) include/*.h
	$(RVCC) $(RVCFLAGS) -DCASE='"$(CASE)"' -T link.ld baseline_rv32.c -o $@ -lgcc
	@$(RVSIZE) $@

baseline_rv32.bin: baseline_rv32.elf
	$(RVOBJCOPY) -O binary $< $@

run: baseline_rv32.bin
	$(RIPES) --mode cli --src $< -t bin --proc $(PROC) \
	  $(if $(ISAEXTS),--isaexts $(ISAEXTS),) --timeout $(TIMEOUT) \
	  --iret --cycles --exectime

clean:
	$(RM) baseline_rv32.elf baseline_rv32.bin

# Rebuild whenever CASE / ARCH change.
.PHONY: all run clean baseline_rv32.elf
