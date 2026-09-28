CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
DEPS := ida.c ida.h cube.h

# h = max(permutation PDB, orientation PDB)
all: solve verify

solve: solve.c $(DEPS)
	$(CC) $(CFLAGS) solve.c ida.c -o $@

verify: verify.c $(DEPS)
	$(CC) $(CFLAGS) verify.c ida.c -o $@

# Quick run: table and heuristic checks on every state, IDA* on every 97th
# state. Drop the arguments to run IDA* on all 3,674,160 states.
quick: verify
	./verify 0 3674160 97

clean:
	$(RM) solve verify

.PHONY: all quick clean
