#!/usr/bin/env python3
"""batch.py - run every distance-11 state on Ripes, one run per state.

Each state gets its own image, and the runs go in parallel:
- asm: the image is assembled once with a placeholder input, and each run
  gets a copy with the 14 placeholder bytes replaced by the state: the same
  bytes the assembler would emit for it. The case expects length 11, so the
  program checks that itself.
- rv32: the C harness is compiled once per state, with the compile command
  that `make -n` prints for rv32/. Patching the bytes would not work: the
  input is a compile-time constant, and gcc checks it while compiling.

Run from ida/, with the xPack toolchain on PATH, after
`stage4/d11 > stage4/d11.txt`:
  stage4/batch.py asm            > stage4/asm.csv
  stage4/batch.py rv32           > stage4/rv32_pdb3.csv
  stage4/batch.py rv32 --pdb 2   > stage4/rv32_pdb2.csv
Options: -j N parallel runs (default: CPU count); --raw FILE keeps every
run's output, which rv32/check_ripes can check. The summary goes to stderr.
"""
import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

RIPES = os.environ.get('RIPES', '/Applications/Ripes.app/Contents/MacOS/Ripes')
PLACEHOLDER = 'XXXXXXXXXXXXXX'
LIMIT = 50_000_000  # instructions per distance-11 state, the pass mark


def asm_template():
    """Assemble the image once with the placeholder input."""
    subprocess.run(['make', '-s', '-C', 'asm', 'solve.bin',
                    f'CASE={PLACEHOLDER}', 'EXPECT=11'],
                   check=True, stdout=subprocess.DEVNULL)
    with open('asm/solve.bin', 'rb') as f:
        template = f.read()
    if template.count(PLACEHOLDER.encode()) != 1:
        sys.exit('placeholder not found exactly once in the image')
    return template


def asm_image(template, state, path):
    data = bytearray(template)
    offset = template.find(PLACEHOLDER.encode())
    data[offset:offset + len(PLACEHOLDER)] = state.encode()
    with open(path, 'wb') as f:
        f.write(data)


def rv32_commands(pdb):
    """The compile and objcopy commands make would run for one case."""
    out = subprocess.run(['make', '-n', '-C', 'rv32', 'search_rv32.bin',
                          f'PDB={pdb}', f'CASES="{PLACEHOLDER}"'],
                         capture_output=True, text=True, check=True).stdout
    lines = out.replace('\\\n', ' ').splitlines()
    cmds = [l for l in lines if 'gcc ' in l or 'objcopy ' in l]
    if len(cmds) != 2 or PLACEHOLDER not in cmds[0]:
        sys.exit('unexpected output from make -n in rv32/')
    return cmds


def rv32_image(cmds, state, path):
    elf = path[:-4] + '.elf'
    for c in cmds:
        c = c.replace(PLACEHOLDER, state)
        c = c.replace('search_rv32.elf', elf).replace('search_rv32.bin', path)
        subprocess.run(c, shell=True, cwd='rv32', check=True,
                       capture_output=True)
    os.remove(elf)


def run_one(make_image, state, tmpdir):
    path = os.path.join(tmpdir, state + '.bin')
    make_image(state, path)
    proc = subprocess.run([RIPES, '--mode', 'cli', '--src', path, '-t', 'bin',
                           '--proc', 'RV32_ISS', '--timeout', '600000',
                           '--iret'], capture_output=True)
    os.remove(path)
    out = proc.stdout.decode('latin-1')
    iret = re.search(r'instructions retired\s*\n\s*(\d+)', out)
    code = re.search(r'Program exited with code: (-?\d+)', out)
    line = next((l for l in out.splitlines()
                 if l.startswith(state + ' -> ')), '')
    counts = re.search(r'\[len (\d+), expanded (\d+), generated (\d+)\](.*)',
                       line)
    return {
        'state': state,
        'iret': int(iret.group(1)) if iret else -1,
        'len': int(counts.group(1)) if counts else -1,
        'expanded': int(counts.group(2)) if counts else -1,
        'generated': int(counts.group(3)) if counts else -1,
        'exit': int(code.group(1)) if code else -1,
        'check': counts.group(4).strip() if counts else '',
        'raw': out,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('kind', choices=['asm', 'rv32'])
    ap.add_argument('--pdb', type=int, default=3, choices=[2, 3])
    ap.add_argument('-j', type=int, default=os.cpu_count())
    ap.add_argument('--states', default='stage4/d11.txt')
    ap.add_argument('--raw')
    args = ap.parse_args()

    with open(args.states) as f:
        states = [s.strip() for s in f if s.strip()]
    if args.kind == 'asm':
        template = asm_template()
        def make_image(state, path):
            asm_image(template, state, path)
    else:
        cmds = rv32_commands(args.pdb)
        def make_image(state, path):
            rv32_image(cmds, state, path)

    results = []
    with tempfile.TemporaryDirectory() as tmpdir, \
            concurrent.futures.ThreadPoolExecutor(args.j) as pool:
        jobs = [pool.submit(run_one, make_image, s, tmpdir) for s in states]
        for i, job in enumerate(concurrent.futures.as_completed(jobs), 1):
            results.append(job.result())
            if i % 200 == 0 or i == len(states):
                print(f'{i}/{len(states)}', file=sys.stderr)
    results.sort(key=lambda r: states.index(r['state']))

    print('state,iret,len,expanded,generated,exit,check')
    for r in results:
        print(f"{r['state']},{r['iret']},{r['len']},{r['expanded']},"
              f"{r['generated']},{r['exit']},{r['check'] or '-'}")
    if args.raw:
        with open(args.raw, 'w', encoding='latin-1') as f:
            f.write(''.join(r['raw'] for r in results))

    bad = [r for r in results
           if r['len'] != 11 or r['exit'] != 0 or r['iret'] < 0
           or (args.kind == 'asm' and r['check'] != 'ok')]
    irets = [r['iret'] for r in results]
    worst = max(results, key=lambda r: r['iret'])
    best = min(results, key=lambda r: r['iret'])
    name = 'asm' if args.kind == 'asm' else f'rv32 PDB={args.pdb}'
    print(f'{name}: {len(results)} states, {len(bad)} failed, '
          f'{sum(i > LIMIT for i in irets)} over {LIMIT:,} instructions\n'
          f'  mean {sum(irets) / len(irets):,.1f}  '
          f'max {worst["iret"]:,} ({worst["state"]})  '
          f'min {best["iret"]:,} ({best["state"]})', file=sys.stderr)
    for r in bad[:10]:
        print(f'  FAILED {r["state"]}: len {r["len"]}, exit {r["exit"]}, '
              f'check {r["check"]!r}', file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
