# Count the RV32I instructions of the target search.
# ../search.c is compiled for rv32i by clang, linked at address 0 with the
# generated tables as read-only data, and run on emu, a small RV32I
# interpreter that counts every instruction. The input is all 2,644 states
# at distance 11, where the worst cases are.
#
#   progB  search + tables.c   three PDBs
#   progS  search + tables.s   the assembly data file itself
#   progA  search + tables2.c  two PDBs (PDB=2)
#
# progS must give exactly the same lengths and node counts as progB, which
# checks tables.s against the C tables.
CLANG ?= clang
LLD ?= ld.lld
SIZE ?= llvm-size
RES := $(shell $(CLANG) -print-resource-dir)/include
RVFLAGS := --target=riscv32 -march=rv32i -mabi=ilp32 -O2 -ffreestanding \
	-nostdinc -isystem stub -isystem $(RES) -mno-relax -fno-pic -I..
RVAS := --target=riscv32 -march=rv32i -mabi=ilp32
LINK := $(LLD) -T link.ld --no-relax

all: emu d11.txt progA.elf progB.elf progS.elf

emu: emu.c
	$(CC) -O2 -o $@ emu.c

gen11: gen11.c ../cube.h
	$(CC) -O2 -I.. -o $@ gen11.c

d11.txt: gen11
	./gen11 > $@

../tables.c ../tables.s:
	$(MAKE) -C .. tables

../tables2.c:
	$(MAKE) -C .. tables PDB=2

start.o: start.S
	$(CLANG) $(RVFLAGS) -c start.S -o $@

SRC := tmain.c ../search.c ../search.h ../tables.h ../cube.h start.o link.ld

progB.elf: $(SRC) ../tables.c
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=1 -c tmain.c -o tmainB.o
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=1 -c ../search.c -o searchB.o
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=1 -c ../tables.c -o tablesB.o
	$(LINK) start.o tmainB.o searchB.o tablesB.o -o $@

progS.elf: progB.elf ../tables.s
	$(CLANG) $(RVAS) -c ../tables.s -o tablesS.o
	$(LINK) start.o tmainB.o searchB.o tablesS.o -o $@

progA.elf: $(SRC) ../tables2.c
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=0 -c tmain.c -o tmainA.o
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=0 -c ../search.c -o searchA.o
	$(CLANG) $(RVFLAGS) -DCORNER_PDB=0 -c ../tables2.c -o tablesA.o
	$(LINK) start.o tmainA.o searchA.o tablesA.o -o $@

# About 3 minutes in total.
run: all
	./emu progB.elf d11.txt > progB.out
	./emu progS.elf d11.txt > progS.out
	./emu progA.elf d11.txt > progA.out
	@for p in progB progS progA; do \
		echo "== $$p"; grep -v '^Q ' $$p.out; \
		$(SIZE) -A $$p.elf | awk '$$1 ~ /^\.s?(rodata|data|bss)/ { n += $$2 } \
			END { printf "data: .rodata + .data + .bss = %d of 131072 bytes\n", n }'; \
	done
	@grep '^Q ' progB.out > qB.txt; grep '^Q ' progS.out > qS.txt; \
	if cmp -s qB.txt qS.txt; then \
		echo "tables.s gives the same result as tables.c on every state"; \
	else echo "MISMATCH between tables.s and tables.c"; exit 1; fi

clean:
	$(RM) emu gen11 d11.txt *.o *.elf *.out qB.txt qS.txt

.PHONY: all run clean
