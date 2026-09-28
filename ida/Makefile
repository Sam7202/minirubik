CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
DEPS := ida.c ida.h cube.h

# solve / verify:        h = max(permutation PDB, orientation PDB)
# solve_c4 / verify_c4:  also max with the 4-corner PDB (CORNER_PDB=1)
all: solve verify solve_c4 verify_c4

solve: solve.c $(DEPS)
	$(CC) $(CFLAGS) solve.c ida.c -o $@

verify: verify.c $(DEPS)
	$(CC) $(CFLAGS) verify.c ida.c -o $@

solve_c4: solve.c $(DEPS)
	$(CC) $(CFLAGS) -DCORNER_PDB=1 solve.c ida.c -o $@

verify_c4: verify.c $(DEPS)
	$(CC) $(CFLAGS) -DCORNER_PDB=1 verify.c ida.c -o $@

# Quick run: table and heuristic checks on every state, IDA* on every 97th
# state. Drop the arguments to run IDA* on all 3,674,160 states.
quick: verify verify_c4
	./verify 0 3674160 97
	./verify_c4 0 3674160 97

clean:
	$(RM) solve verify solve_c4 verify_c4

.PHONY: all quick clean
