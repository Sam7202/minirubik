/* emu.c - minimal RV32I interpreter that counts retired instructions.
 *
 * usage: ./emu prog.elf states.txt
 *   states.txt: one 14-digit state per line, written to INBUF before start.
 * The target marks points of interest with ecall a7=1, a0=tag:
 *   tags 0..7   record the instruction count at that point
 *   tag 100/101 start/end of one query; the difference is kept per query
 * Instructions executed at 0x18..0xb3 are the helpers in start.S
 * (__mulsi3, __udivsi3, __umodsi3, memset) and are also counted apart.
 * ecall a7=93 exits. After exit, OUTBUF holds (len, expanded, generated)
 * per query, which is printed next to the instruction count.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE (64u << 20)
#define INBUF 0x02000000u
#define OUTBUF 0x02100000u
#define MAXQ 100000

static uint8_t *mem;

static void fault(const char *what, uint32_t pc, uint32_t addr)
{
    fprintf(stderr, "fault: %s at pc=%08x addr=%08x\n", what, pc, addr);
    exit(3);
}

static uint32_t load_elf(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t) n);
    if (fread(b, 1, (size_t) n, f) != (size_t) n)
        exit(2);
    fclose(f);
    uint32_t entry, phoff;
    uint16_t phentsize, phnum, machine;
    if (memcmp(b, "\x7f" "ELF", 4) || b[4] != 1 || b[5] != 1)
        fault("not a 32-bit little-endian ELF", 0, 0);
    memcpy(&machine, b + 18, 2);
    if (machine != 0xF3)
        fault("not RISC-V", 0, 0);
    memcpy(&entry, b + 24, 4);
    memcpy(&phoff, b + 28, 4);
    memcpy(&phentsize, b + 42, 2);
    memcpy(&phnum, b + 44, 2);
    for (int i = 0; i < phnum; i++) {
        const uint8_t *ph = b + phoff + (uint32_t) i * phentsize;
        uint32_t type, off, vaddr, filesz, memsz;
        memcpy(&type, ph, 4);
        memcpy(&off, ph + 4, 4);
        memcpy(&vaddr, ph + 8, 4);
        memcpy(&filesz, ph + 16, 4);
        memcpy(&memsz, ph + 20, 4);
        if (type != 1)
            continue;
        if ((uint64_t) vaddr + memsz > MEM_SIZE)
            fault("segment outside memory", 0, vaddr);
        memcpy(mem + vaddr, b + off, filesz);
    }
    free(b);
    return entry;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s prog.elf states.txt\n", argv[0]);
        return 2;
    }
    mem = calloc(MEM_SIZE, 1);
    uint32_t pc = load_elf(argv[1]);

    /* queries */
    static char line[64];
    static char names[MAXQ][15];
    uint32_t nq = 0;
    FILE *sf = fopen(argv[2], "r");
    if (!sf) {
        perror(argv[2]);
        return 2;
    }
    while (fgets(line, sizeof line, sf) && nq < MAXQ) {
        if (strlen(line) < 14)
            continue;
        memcpy(mem + INBUF + 4 + 14 * nq, line, 14);
        memcpy(names[nq], line, 14);
        names[nq][14] = 0;
        nq++;
    }
    fclose(sf);
    memcpy(mem + INBUF, &nq, 4);

    static uint64_t qins[MAXQ];
    uint64_t stage[8] = {0}, icount = 0, qstart = 0, helper = 0, hq = 0, hqstart = 0, first = 0;
    int seen[8] = {0};
    uint32_t qi = 0;
    uint32_t x[32] = {0};

    for (;;) {
        if (pc > MEM_SIZE - 4 || (pc & 3))
            fault("fetch", pc, pc);
        uint32_t ins;
        memcpy(&ins, mem + pc, 4);
        icount++;
        if (pc >= 0x18 && pc < 0xb4)
            helper++;
        uint32_t op = ins & 0x7f, rd = (ins >> 7) & 31, f3 = (ins >> 12) & 7;
        uint32_t rs1 = (ins >> 15) & 31, rs2 = (ins >> 20) & 31, f7 = ins >> 25;
        int32_t immI = (int32_t) ins >> 20;
        int32_t immS = ((int32_t) ins >> 25 << 5) | (int32_t) ((ins >> 7) & 31);
        int32_t immB = (int32_t) (((ins >> 31) & 1) << 12 | ((ins >> 7) & 1) << 11 |
                                  ((ins >> 25) & 0x3f) << 5 | ((ins >> 8) & 0xf) << 1);
        immB = immB << 19 >> 19;
        int32_t immJ = (int32_t) (((ins >> 31) & 1) << 20 | ((ins >> 12) & 0xff) << 12 |
                                  ((ins >> 20) & 1) << 11 | ((ins >> 21) & 0x3ff) << 1);
        immJ = immJ << 11 >> 11;
        uint32_t a = x[rs1], bb = x[rs2], npc = pc + 4, v = 0;
        int wr = 1;
        switch (op) {
        case 0x37: v = ins & 0xfffff000u; break;                 /* LUI */
        case 0x17: v = pc + (ins & 0xfffff000u); break;          /* AUIPC */
        case 0x6f: v = pc + 4; npc = pc + (uint32_t) immJ; break; /* JAL */
        case 0x67: v = pc + 4; npc = (a + (uint32_t) immI) & ~1u; break; /* JALR */
        case 0x63: {                                             /* branches */
            int t;
            switch (f3) {
            case 0: t = a == bb; break;
            case 1: t = a != bb; break;
            case 4: t = (int32_t) a < (int32_t) bb; break;
            case 5: t = (int32_t) a >= (int32_t) bb; break;
            case 6: t = a < bb; break;
            case 7: t = a >= bb; break;
            default: fault("bad branch", pc, ins); t = 0;
            }
            if (t)
                npc = pc + (uint32_t) immB;
            wr = 0;
            break;
        }
        case 0x03: {                                             /* loads */
            uint32_t ad = a + (uint32_t) immI;
            if (ad > MEM_SIZE - 4)
                fault("load", pc, ad);
            switch (f3) {
            case 0: v = (uint32_t) (int32_t) (int8_t) mem[ad]; break;
            case 1: { int16_t h; memcpy(&h, mem + ad, 2); v = (uint32_t) (int32_t) h; break; }
            case 2: memcpy(&v, mem + ad, 4); break;
            case 4: v = mem[ad]; break;
            case 5: { uint16_t h; memcpy(&h, mem + ad, 2); v = h; break; }
            default: fault("bad load", pc, ins);
            }
            break;
        }
        case 0x23: {                                             /* stores */
            uint32_t ad = a + (uint32_t) immS;
            if (ad > MEM_SIZE - 4)
                fault("store", pc, ad);
            switch (f3) {
            case 0: mem[ad] = (uint8_t) bb; break;
            case 1: { uint16_t h = (uint16_t) bb; memcpy(mem + ad, &h, 2); break; }
            case 2: memcpy(mem + ad, &bb, 4); break;
            default: fault("bad store", pc, ins);
            }
            wr = 0;
            break;
        }
        case 0x13: {                                             /* OP-IMM */
            uint32_t sh = rs2;
            switch (f3) {
            case 0: v = a + (uint32_t) immI; break;
            case 2: v = (int32_t) a < immI; break;
            case 3: v = a < (uint32_t) immI; break;
            case 4: v = a ^ (uint32_t) immI; break;
            case 6: v = a | (uint32_t) immI; break;
            case 7: v = a & (uint32_t) immI; break;
            case 1: v = a << sh; break;
            case 5: v = (f7 & 0x20) ? (uint32_t) ((int32_t) a >> sh) : a >> sh; break;
            }
            break;
        }
        case 0x33:                                               /* OP */
            if (f7 == 0x01)
                fault("M extension instruction", pc, ins);
            switch (f3) {
            case 0: v = (f7 & 0x20) ? a - bb : a + bb; break;
            case 1: v = a << (bb & 31); break;
            case 2: v = (int32_t) a < (int32_t) bb; break;
            case 3: v = a < bb; break;
            case 4: v = a ^ bb; break;
            case 5: v = (f7 & 0x20) ? (uint32_t) ((int32_t) a >> (bb & 31)) : a >> (bb & 31); break;
            case 6: v = a | bb; break;
            case 7: v = a & bb; break;
            }
            break;
        case 0x0f: wr = 0; break;                                /* FENCE */
        case 0x73:                                               /* ECALL */
            wr = 0;
            if (x[17] == 93)
                goto done;
            if (x[17] == 1) {
                uint32_t tag = x[10];
                if (tag < 8) {
                    stage[tag] = icount;
                    seen[tag] = 1;
                }
                else if (tag == 100) {
                    qstart = icount;
                    hqstart = helper;
                    if (!first)
                        first = icount;
                }
                else if (tag == 101 && qi < MAXQ) {
                    qins[qi++] = icount - qstart;
                    if (helper - hqstart > hq)
                        hq = helper - hqstart;
                }
            }
            break;
        default:
            fault("illegal instruction", pc, ins);
        }
        if (wr && rd)
            x[rd] = v;
        pc = npc;
    }
done:
    printf("exit code %u, total instructions %llu\n", x[10], (unsigned long long) icount);
    for (int k = 0; k < 8; k++)
        if (seen[k])
            printf("mark %d at instruction %llu\n", k, (unsigned long long) stage[k]);
    uint64_t init = first ? first : icount;
    printf("before the first query (startup and any table building): %llu\n",
           (unsigned long long) init);
    printf("helper instructions (mul/div/memset) in the worst query: %llu\n",
           (unsigned long long) hq);
    uint64_t sum = 0, max = 0;
    uint32_t worst = 0;
    for (uint32_t i = 0; i < qi; i++) {
        uint32_t r[3];
        memcpy(r, mem + OUTBUF + 12 * i, 12);
        printf("Q %s %u %u %u %llu\n", names[i], r[0], r[1], r[2], (unsigned long long) qins[i]);
        sum += qins[i];
        if (qins[i] > max) {
            max = qins[i];
            worst = i;
        }
    }
    if (qi) {
        uint32_t r[3];
        memcpy(r, mem + OUTBUF + 12 * worst, 12);
        printf("summary: %u queries, mean %.0f instructions per query, max %llu (%s: length %u, expanded %u, generated %u)\n",
               qi, (double) sum / qi, (unsigned long long) max, names[worst], r[0], r[1], r[2]);
        printf("worst single run = before first query %llu + worst query %llu = %llu\n",
               (unsigned long long) init, (unsigned long long) max,
               (unsigned long long) (init + max));
    }
    return 0;
}
