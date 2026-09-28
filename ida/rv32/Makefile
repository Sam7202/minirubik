# Build the tables and run the search, both on Ripes.
#
#   make run              rv32i on Ripes' functional model
#   make run ARCH=rv32im ISAEXTS=M
#   make run PROC=RV32_5S pipeline model (much slower)
#   make run CORNER_PDB=1 also build and use the 4-corner PDB

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
CORNER_PDB ?= 0

RVCFLAGS   ?= -march=$(ARCH) -mabi=$(ABI) -O2 -std=c99 -Wall -Wextra \
              -ffreestanding -nostdlib -fno-builtin \
              -fno-tree-loop-distribute-patterns -g \
              -DCORNER_PDB=$(CORNER_PDB)
DEPS := ../ida.c ../ida.h ../cube.h

all: search_rv32.bin

search_rv32.elf: search_rv32.c link.ld $(DEPS)
	$(RVCC) $(RVCFLAGS) -T link.ld search_rv32.c -o $@ -lgcc
	@$(RVSIZE) $@

search_rv32.bin: search_rv32.elf
	$(RVOBJCOPY) -O binary $< $@
	@ls -l $@

run: search_rv32.bin
	$(RIPES) --mode cli --src $< -t bin --proc $(PROC) \
	  $(if $(ISAEXTS),--isaexts $(ISAEXTS),) --timeout $(TIMEOUT) \
	  --cycles --iret --cpi

# The search code on its own, as the report counts it: no main, no printing.
size-ida:
	$(RVCC) -march=$(ARCH) -mabi=$(ABI) -O2 -std=c99 -ffreestanding -c ../ida.c -o ida_rv32.o
	$(RVSIZE) ida_rv32.o

clean:
	$(RM) search_rv32.elf search_rv32.bin ida_rv32.o

.PHONY: all run size-ida clean search_rv32.elf
