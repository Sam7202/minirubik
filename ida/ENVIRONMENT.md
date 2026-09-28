# Environment

Every instruction count in this directory depends on the versions below.

## Fork

| | |
| :--- | :--- |
| Upstream | https://github.com/sysprog21/minirubik |
| Fork | https://github.com/Sam7202/minirubik |
| Starting commit | `3811ad0a87bd490e45099c3cb179ec33caf46cb5` ("Import from internal tree") |

On 2026-09-28 the starting commit was identical to upstream `HEAD`.

## Simulator

| | |
| :--- | :--- |
| Ripes | **v2.2.6-106-g5b8a616** (continuous build; the version string is embedded in the binary) |
| Models used | `RV32_ISS` for counts, `RV32_5S` for pipeline data |
| Mode | CLI (`--mode cli`), no renderer |

The v2.2.6 release has no `RV32_ISS`, so it cannot reproduce these numbers.

Environment calls checked on this build, from a `-t asm` program:

| a7 | Effect |
| :---: | :--- |
| 1 | print integer in a0 |
| 4 | print string at a0 |
| 11 | print character in a0 |
| 10 | exit |
| 64 | Linux-style write(fd=a0, buf=a1, len=a2); used by the C harnesses |
| 93 | exit with code a0 |

## Toolchain

| | |
| :--- | :--- |
| Target compiler | xPack GNU RISC-V Embedded GCC 15.2.0 (`riscv-none-elf-gcc`), `-O2 -march=rv32i -mabi=ilp32` |
| Host compiler | Apple clang 16.0.0 |
| Host | Apple M1, macOS 15.0 |

`riscv-none-elf-gcc` is the xPack distribution of the same GNU toolchain as
`riscv64-unknown-elf-gcc`. Both accept `-march=rv32i -mabi=ilp32`.

## Measurement conventions

- Instruction count: the pinned Ripes build above, CLI, renderer off, the same
  input, `--iret`.
- Code size: linked `.text` bytes with the renderer off.
