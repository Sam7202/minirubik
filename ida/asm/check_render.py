#!/usr/bin/env python3
"""check_render.py - check every frame of the RENDER=2 build against geometry.

Reads Ripes' output of `make check-render` on stdin. For every case, solve.s
prints the cube before its replay and after every move it replays, as the
20 x 35 net it would draw on the LED matrix. This script uses none of the
solver's tables or coordinates: it turns the 24 stickers of a cube in 3-D
(x right, y up, z front) and projects them onto the net directly. The last
frame must be the solved cube, and undoing the printed moves one by one from
there must give every earlier frame, the scrambled cube included.
"""
import re
import sys

FACES = {(0, 1, 0): 'U', (1, 0, 0): 'R', (0, 0, 1): 'F',
         (0, -1, 0): 'D', (-1, 0, 0): 'L', (0, 0, -1): 'B'}

# A clockwise quarter turn of each face, as seen from outside it: which
# stickers it moves (by the corner they sit on) and how it rotates them.
TURNS = {
    'R': (lambda v: v[0] == 1, lambda x, y, z: (x, z, -y)),
    'B': (lambda v: v[2] == -1, lambda x, y, z: (-y, x, z)),
    'D': (lambda v: v[1] == -1, lambda x, y, z: (z, y, -x)),
}

# Where each face sits in the net: (column, row) of its face slot.
SLOT = {'U': (1, 0), 'L': (0, 1), 'F': (1, 1),
        'R': (2, 1), 'B': (3, 1), 'D': (1, 2)}

CASE = re.compile(r'^(\d{14}) -> (.*?)\s+\[len \d+, expanded \d+, '
                  r'generated \d+$')
ROW = re.compile(r'^[.URFDLB]{35}$')


def solved():
    cube = {}
    for x in (-1, 1):
        for y in (-1, 1):
            for z in (-1, 1):
                for n in ((x, 0, 0), (0, y, 0), (0, 0, z)):
                    cube[(x, y, z), n] = FACES[n]
    return cube


def quarter(cube, face):
    moves, rot = TURNS[face]
    return {((rot(*v), rot(*n)) if moves(v) else (v, n)): colour
            for (v, n), colour in cube.items()}


def undo(cube, move):
    turns = {'': 1, '2': 2, "'": 3}[move[1:]]
    for _ in range(4 - turns):
        cube = quarter(cube, move[0])
    return cube


def facelet(face, x, y, z):
    """Column and row of a sticker within its face, as the net unfolds
    around F."""
    return {'U': (x > 0, z > 0),        # F edge at the bottom
            'F': (x > 0, y < 0),
            'D': (x > 0, z < 0),        # F edge at the top
            'L': (z > 0, y < 0),        # F edge on the right
            'R': (z < 0, y < 0),        # F edge on the left
            'B': (x < 0, y < 0)}[face]  # R edge on the left


def picture(cube):
    rows = [['.'] * 35 for _ in range(20)]
    for (v, n), colour in cube.items():
        face = FACES[n]
        c, r = facelet(face, *v)
        x0 = 9 * SLOT[face][0] + 4 * c
        y0 = 7 * SLOT[face][1] + 3 * r
        for y in range(y0, y0 + 3):
            for x in range(x0, x0 + 4):
                rows[y][x] = colour
    return [''.join(row) for row in rows]


def check(state, moves, rows, end):
    problems = []
    if end != '] ok':
        problems.append('self-check: ' + end)
    if len(rows) != 20 * (len(moves) + 1):
        problems.append(f'{len(rows)} rows of net for {len(moves)} moves')
    else:
        frames = [rows[i:i + 20] for i in range(0, len(rows), 20)]
        cube, bad = solved(), []
        for k in range(len(moves), -1, -1):
            if frames[k] != picture(cube):
                bad.append(k)
            if k:
                cube = undo(cube, moves[k - 1])
        if bad:
            problems.append(f'{len(bad)} of {len(frames)} frames differ from '
                            f'the 3-D model, the first is frame {min(bad)}')
    print('ok  ' if not problems else 'FAIL', f'{state}: {len(moves)} moves,',
          f'{len(moves) + 1} frames', ('- ' + '; '.join(problems))
          if problems else '')
    return not problems


def main():
    checked = failed = 0
    case = None
    for line in sys.stdin:
        line = line.rstrip('\n')
        m = CASE.match(line)
        if m:
            text = m.group(2).strip()
            case = (m.group(1), [] if text == '(solved)' else text.split(), [])
        elif case and ROW.match(line):
            case[2].append(line)
        elif case and line.startswith(']'):
            checked += 1
            failed += not check(*case, line.strip())
            case = None
    print(f'{checked} cases checked, {failed} failure(s)')
    sys.exit(1 if failed or not checked else 0)


if __name__ == '__main__':
    main()
