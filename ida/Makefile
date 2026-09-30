CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic

# PDB=3 (default): h = max(perm, orient, 4-corner PDB).
# PDB=2: perm and orient PDBs only; files get a "2" suffix.
PDB ?= 3
ifeq ($(PDB),2)
DEFS := -DCORNER_PDB=0
SUF := 2
else
DEFS := -DCORNER_PDB=1
SUF :=
endif

T := tables$(SUF)
TABLES := $(T).c $(T).s $(T)_ripes.s
HDRS := cube.h tables.h search.h build.h

all: gen_tables$(SUF) $(TABLES) solve$(SUF) verify$(SUF)

# Host generator: build.c fills the tables, gen_tables.c writes them out.
gen_tables$(SUF): gen_tables.c build.c $(HDRS)
	$(CC) $(CFLAGS) $(DEFS) gen_tables.c build.c -o $@

# One run writes tables.c (C), tables.s (.rodata) and tables_ripes.s (.data
# for Ripes' built-in assembler), so the three always hold the same bytes.
tables: $(TABLES)

$(T).c: gen_tables$(SUF)
	./gen_tables$(SUF) $(TABLES)

$(T).s $(T)_ripes.s: $(T).c
	@test -f $@ || ./gen_tables$(SUF) $(TABLES)

# Host programs linking the target search with the generated tables.
solve$(SUF): solve.c search.c $(T).c $(HDRS)
	$(CC) $(CFLAGS) $(DEFS) solve.c search.c $(T).c -o $@

verify$(SUF): verify.c search.c build.c $(T).c $(HDRS)
	$(CC) $(CFLAGS) $(DEFS) verify.c search.c build.c $(T).c -o $@

# Table checks on every state, IDA* on every 97th state: about 2 seconds
# (PDB=3) or 3 seconds (PDB=2).
quick: verify$(SUF)
	./verify$(SUF) 0 3674160 97

# Everything on every state: about 40 seconds (PDB=3) or 3 minutes (PDB=2)
# on one core.
check: verify$(SUF)
	./verify$(SUF)

clean:
	$(RM) gen_tables gen_tables2 solve solve2 verify verify2
	$(RM) tables.c tables.s tables_ripes.s tables2.c tables2.s tables2_ripes.s

.PHONY: all tables quick check clean
