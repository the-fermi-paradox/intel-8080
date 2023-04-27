#include <stdint.h>
#include <opcodes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Define the registers */
struct Registers {
    union {
        struct {
            uint8_t c, b;
        };
        uint16_t bc;
    };
    union {
        struct {
            uint8_t e, d;
        };
        uint16_t de;
    };
    union {
        struct {
            uint8_t l, h;
        };
        uint16_t hl;
    };
    union {
        struct {
            uint8_t pcl, pch;
        };
        uint16_t pc;
    };
    union {
        struct {
            uint8_t spl, sph;
        };
        uint16_t sp;
    };
    bool cf;
    bool pf;
    bool acf;
    bool zf;
    bool sf;
    uint8_t a;
};

bool interrupt_enabled = 0;
static struct Registers regs = {0};

#   define P2(n) n, n^1, n^1, n
#   define P4(n) P2(n), P2(n^1), P2(n^1), P2(n)
#   define P6(n) P4(n), P4(n^1), P4(n^1), P4(n)
static const _Bool parity_table[256] =
{
    P6(0), P6(1), P6(1), P6(0)
};

/* Define the main memory */
#define MEM_SIZE 0x10000
static uint8_t memory[MEM_SIZE];

/* Read a byte from memory */
static uint8_t read_byte(uint16_t addr)
{
    return memory[addr];
}

static uint8_t read_next_byte()
{
    return read_byte(regs.pc++);
}

/* Write a byte to memory */
static void write_byte(uint16_t addr, uint8_t value)
{
     memory[addr] = value;
}

static uint16_t merge_bytes(uint8_t lo_byte, uint8_t hi_byte)
{
    return (uint16_t) ((hi_byte << 8) | lo_byte);
}

static inline void test_pzs(uint8_t res)
{
    regs.pf = parity_table[res];
    regs.zf = (res == 0);
    regs.sf = !!(res & (0x80));
}

static inline void test_ac(uint8_t res, uint8_t op1, uint8_t op2)
{
    regs.acf = (res ^ op1 ^ op2) & 0x10;
}

#define COMPOSE_ADDR() do {                 \
    lo_byte = read_next_byte();         \
    hi_byte = read_next_byte();         \
    address = merge_bytes(lo_byte, hi_byte);\
} while(0)

#define INR(rg) do {                        \
    ++regs.rg;                              \
    test_pzs(regs.rg);                      \
    test_ac(regs.rg, regs.rg - 1, 0x01);    \
} while(0)

#define DCR(rg) do {                        \
    --regs.rg;                              \
    test_pzs(regs.rg);                      \
    test_ac(regs.rg, regs.rg + 1, 0x01);    \
} while(0)

#define DAD(rg) do {                        \
    uint32_t tmp = regs.hl + regs.rg;       \
    regs.hl = (uint16_t) tmp;               \
    regs.cf = tmp & 0x10000;                \
} while(0)

#define POP(regl, regh) do {                \
    regl = read_byte(regs.sp);              \
    regh = read_byte(regs.sp + 1);          \
    regs.sp += 2;                           \
} while(0)

#define PUSH(regl, regh) do { \
    write_byte(regs.sp - 1, regh);          \
    write_byte(regs.sp - 2, regl);          \
    regs.sp -= 2;                           \
} while(0)

#define RET() do {                          \
    POP(regs.pcl, regs.pch);                \
} while(0)

#define JUMP() do {                         \
    COMPOSE_ADDR();                         \
    regs.pc = address;                      \
} while (0)

#define CALL() do {                         \
    COMPOSE_ADDR();                         \
    PUSH(regs.pcl, regs.pch);               \
    regs.pc = address;                      \
} while(0)

#define RST(val) do {                       \
    PUSH(regs.pcl, regs.pch);               \
    regs.pc = 8 * val;                       \
} while(0)

#define ADD(val, cy) do { \
    uint16_t tmp = regs.a + (val) + (cy);   \
    test_pzs(tmp);                          \
    test_ac(tmp, regs.a, (val));            \
    regs.cf = tmp & 0x100;                  \
    regs.a = (uint8_t) tmp;                 \
} while(0)

/* This is just two's complement:
 * val is complemented, cy is the add bit */
#define SUB(val, cy) do {                   \
    ADD(~val, !cy);                         \
    regs.cf = !regs.cf;                     \
} while(0)

#define CMP(val) do { \
    uint16_t tmp = regs.a - (val);          \
    test_pzs(tmp);                          \
    test_ac(tmp, regs.a, ~(val));           \
    regs.cf = tmp & 0x100;                  \
} while(0)

#define ANA(val) do {                       \
    regs.a &= (val);                        \
    regs.cf = 0;                            \
    regs.acf = ((regs.a | val) & 0x08);     \
    test_pzs(regs.a);                       \
} while(0)

#define XRA(val) do {                       \
    regs.a ^= (val);                        \
    regs.cf = 0;                            \
    regs.acf = 0;                           \
    test_pzs(regs.a);                       \
} while(0)

#define ORA(val) do {                       \
    regs.a |= (val);                        \
    regs.cf = 0;                            \
    regs.acf = 0;                           \
    test_pzs(regs.a);                       \
} while(0)

unsigned char *load_rom(unsigned char bufptr[static 1], const char filename[static 1])
{
    /* get true path */
    const char* base = "../rom/";
    const size_t file_size = 0x800;
    const size_t len = strlen(base) + strlen(filename) + 1;
    char *path = malloc(len);
    snprintf(path, len, "%s%s", base, filename);

    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "failed to open %s\n", path);
        free(path);
        return NULL;
    }
    if (!fread(bufptr, file_size, 1, file)) {
        fprintf(stderr, "failed to read %s\n", path);
        free(path);
        fclose(file);
        return NULL;
    }
    fclose(file);
    free(path);
    return bufptr + file_size;
}

int main(void)
{
    /* load rom into memory */
    unsigned char *rom = memory;
    if (!(rom = load_rom(rom, "invaders.h")))
        return 1;
    if (!(rom = load_rom(rom, "invaders.g")))
        return 1;
    if (!(rom = load_rom(rom, "invaders.f")))
        return 1;
    if (!(rom = load_rom(rom, "invaders.e")))
        return 1;

    uint8_t lo_byte, hi_byte, res;
    uint16_t address;
    lo_byte = hi_byte = res = address = 0;
    regs.pc = 0;
    regs.bc = 0;

    size_t times_run = 0;
    while(1) {
        times_run++;
        enum OpCode opcode = read_next_byte();
        int x = 1;
        switch (opcode) {
            case NOP:
                /* do nothing */
                break;
            case LXI_B:
                regs.c = read_next_byte();
                regs.b = read_next_byte();
                break;
            case STAX_B:
                write_byte(regs.bc, regs.a);
                break;
            case INX_B:
                ++regs.bc;
                break;
            case INR_B:
                INR(b);
                break;
            case DCR_B:
                DCR(b);
                break;
            case MVI_B:
                regs.b = read_next_byte();
                break;
            case RLC:
                regs.a = (regs.a << 1) | (regs.a >> 7);
                regs.cf = regs.a & 0x01; /* the rotated out bit, now the LSB, is copied into the carry */
                break;
            case DSUB:
                /* not implemented in 8080 */
                break;
            case DAD_B:
                DAD(bc);
                break;
            case LDAX_B:
                regs.a = read_byte(regs.bc);
                break;
            case DCX_B:
                --regs.bc;
                break;
            case INR_C:
                INR(c);
                break;
            case DCR_C:
                DCR(c);
                break;
            case MVI_C:
                regs.c = read_next_byte();
                break;

            case RRC:
                regs.a = (regs.a >> 1) | (regs.a << 7);
                regs.cf = regs.a & 0x80; /* the rotated out bit, now the MSB, is copied into the carry */
                break;
            case AHRL:
                /* not implemented in 8080 */
                break;
            case LXI_D:
                regs.e = read_next_byte();
                regs.d = read_next_byte();
                break;
            case STAX_D:
                write_byte(regs.de, regs.a);
                break;
            case INX_D:
                ++regs.de;
                break;
            case INR_D:
                INR(d);
                break;
            case DCR_D:
                DCR(d);
                break;
            case MVI_D:
                regs.d = read_next_byte();
                break;
            case RAL: {
                /* we rotate left through the carry */
                res = (regs.a << 1) | regs.cf;
                regs.cf = regs.a & 0x80;
                regs.a = res;
                break;
            }
            case RDEL:
                /* not implemented in 8080 */
                break;
            case DAD_D:
                DAD(de);
                break;
            case LDAX_D:
                regs.a = read_byte(regs.de);
                break;
            case DCX_D:
                --regs.de;
                break;
            case INR_E:
                INR(e);
                break;
            case DCR_E:
                DCR(e);
                break;
            case MVI_E:
                regs.e = read_next_byte();
                break;

            case RAR:
                /* we rotate right through the carry */
                res = (regs.a >> 1) | (regs.cf << 7);
                regs.cf = (regs.a & 0x1);
                regs.a = res;
                break;
            case RIM:
                /* not implemented in 8080 */
                break;
            case LXI_H:
                regs.l = read_next_byte();
                regs.h = read_next_byte();
                break;
            case SHLD: {
                COMPOSE_ADDR();
                write_byte(address, regs.l);
                write_byte(address + 1, regs.h);
                break;
            }
            case INX_H:
                ++regs.hl;
                break;
            case INR_H:
                INR(h);
                break;
            case DCR_H:
                DCR(h);
                break;
            case MVI_H:
                regs.h = read_next_byte();
                break;
            case DAA: {
                uint8_t old_cf = regs.cf;
                uint8_t old_a  = regs.a;
                regs.cf = 0;

                if ((old_a & 0x0F) > 9 || regs.acf) {
                    /* extract a carry bit from the operation */
                    regs.cf = 0x100 & (regs.a += 0x06);
                    regs.cf |= old_cf;
                    regs.acf = 1;
                }

                if (old_a > 0x99 || old_cf) {
                    regs.a += 0x60;
                    regs.cf = 1;
                } else {
                    regs.cf = 0;
                }
                test_pzs(regs.a);
                break;
            }
            case LDHI:
                /* not implemented in 8080 */
                break;
            case DAD_H:
                DAD(hl);
                break;
            case LHLD: {
                COMPOSE_ADDR();
                regs.l = read_byte(address);
                regs.h = read_byte(address + 1);
                break;
            }
            case DCX_H:
                --regs.hl;
                break;
            case INR_L:
                INR(l);
                break;
            case DCR_L:
                DCR(l);
                break;
            case MVI_L:
                regs.l = read_next_byte();
                break;
            case CMA:
                regs.a = ~regs.a;
                break;
            case SIM:
                /* not implemented in 8080 */
                break;
            case LXI_SP:
                regs.spl = read_next_byte();
                regs.sph = read_next_byte();
                break;
            case STA:
                COMPOSE_ADDR();
                break;
            case INX_SP:
                ++regs.sp;
                break;
            case INR_M:
                res = read_byte(regs.hl) + 1;
                write_byte(regs.hl, res);
                test_pzs(res);
                test_ac(res, res - 1, 0x1);
                break;
            case DCR_M:
                res = read_byte(regs.hl) - 1;
                write_byte(regs.hl, res);
                test_pzs(res);
                test_ac(res, res + 1, 0x1);
                break;
            case MVI_M:
                res = read_next_byte();
                write_byte(regs.hl, res);
                break;
            case STC:
                regs.cf = 1;
                break;
            case LDSI:
                /* not implemented in 8080 */
                break;
            case DAD_SP:
                DAD(sp);
                break;
            case LDA:
                break;
            case DCX_SP:
                --regs.sp;
                break;
            case INR_A:
                INR(a);
                break;
            case DCR_A:
                DCR(a);
                break;
            case MVI_A:
                regs.a = read_next_byte();
                break;

            case CMC:
                regs.cf = !regs.cf;
                break;
            case MOV_B_B:
                regs.b = regs.b;
                break;
            case MOV_B_C:
                regs.b = regs.c;
                break;
            case MOV_B_D:
                regs.b = regs.d;
                break;
            case MOV_B_E:
                regs.b = regs.e;
                break;
            case MOV_B_H:
                regs.b = regs.h;
                break;
            case MOV_B_L:
                regs.b = regs.l;
                break;
            case MOV_B_M:
                address = merge_bytes(regs.l, regs.h);
                regs.b = read_byte(address);
                break;
            case MOV_B_A:
                regs.b = regs.a;
                break;
            case MOV_C_B:
                regs.c = regs.b;
                break;
            case MOV_C_C:
                regs.c = regs.c;
                break;
            case MOV_C_D:
                regs.c = regs.d;
                break;
            case MOV_C_E:
                regs.c = regs.e;
                break;
            case MOV_C_H:
                regs.c = regs.h;
                break;
            case MOV_C_L:
                regs.c = regs.l;
                break;
            case MOV_C_M:
                regs.c = read_byte(regs.hl);
                break;


            case MOV_C_A:
                regs.c = regs.a;
                break;
            case MOV_D_B:
                regs.d = regs.b;
                break;
            case MOV_D_C:
                regs.d = regs.c;
                break;
            case MOV_D_D:
                regs.d = regs.d;
                break;
            case MOV_D_E:
                regs.d = regs.e;
                break;
            case MOV_D_H:
                regs.d = regs.h;
                break;
            case MOV_D_L:
                regs.d = regs.l;
                break;
            case MOV_D_M:
                regs.d = read_byte(regs.hl);
                break;
            case MOV_D_A:
                regs.d = regs.a;
                break;
            case MOV_E_B:
                regs.e = regs.b;
                break;
            case MOV_E_C:
                regs.e = regs.c;
                break;
            case MOV_E_D:
                regs.e = regs.d;
                break;
            case MOV_E_E:
                regs.e = regs.e;
                break;
            case MOV_E_H:
                regs.e = regs.h;
                break;
            case MOV_E_L:
                regs.e = regs.l;
                break;
            case MOV_E_M:
                regs.e = read_byte(regs.hl);
                break;

            case MOV_E_A:
                regs.e = regs.a;
                break;
            case MOV_H_B:
                regs.h = regs.b;
                break;
            case MOV_H_C:
                regs.h = regs.c;
                break;
            case MOV_H_D:
                regs.h = regs.d;
                break;
            case MOV_H_E:
                regs.h = regs.e;
                break;
            case MOV_H_H:
                regs.h = regs.h;
                break;
            case MOV_H_L:
                regs.h = regs.l;
                break;
            case MOV_H_M:
                regs.h = read_byte(regs.hl);
                break;
            case MOV_H_A:
                regs.h = regs.a;
                break;
            case MOV_L_B:
                regs.l = regs.b;
                break;
            case MOV_L_C:
                regs.l = regs.c;
                break;
            case MOV_L_D:
                regs.l = regs.d;
                break;
            case MOV_L_E:
                regs.l = regs.e;
                break;
            case MOV_L_H:
                regs.l = regs.h;
                break;
            case MOV_L_L:
                regs.l = regs.l;
                break;
            case MOV_L_M:
                regs.l = read_byte(regs.hl);
                break;

            case MOV_L_A:
                regs.l = regs.a;
                break;
            case MOV_M_B:
                write_byte(regs.hl, regs.b);
                break;
            case MOV_M_C:
                write_byte(regs.hl, regs.c);
                break;
            case MOV_M_D:
                write_byte(regs.hl, regs.d);
                break;
            case MOV_M_E:
                write_byte(regs.hl, regs.e);
                break;
            case MOV_M_H:
                write_byte(regs.hl, regs.h);
                break;
            case MOV_M_L:
                write_byte(regs.hl, regs.l);
                break;
            case HLT:
                exit(0);
            case MOV_M_A:
                write_byte(regs.hl, regs.a);
                break;
            case MOV_A_B:
                regs.a = regs.b;
                break;
            case MOV_A_C:
                regs.a = regs.c;
                break;
            case MOV_A_D:
                regs.a = regs.d;
                break;
            case MOV_A_E:
                regs.a = regs.e;
                break;
            case MOV_A_H:
                regs.a = regs.h;
                break;
            case MOV_A_L:
                regs.a = regs.l;
                break;
            case MOV_A_M:
                regs.a = read_byte(regs.hl);
                break;

            case MOV_A_A:
                regs.a = regs.a;
                break;
            case ADD_B:
                ADD(regs.b, 0);
                break;
            case ADD_C:
                ADD(regs.c, 0);
                break;
            case ADD_D:
                ADD(regs.d, 0);
                break;
            case ADD_E:
                ADD(regs.e, 0);
                break;
            case ADD_H:
                ADD(regs.h, 0);
                break;
            case ADD_L:
                ADD(regs.l, 0);
                break;
            case ADD_M:
                res = read_byte(regs.hl);
                ADD(res, 0);
                break;
            case ADD_A:
                ADD(regs.a, 0);
                break;
            case ADC_B:
                ADD(regs.b, regs.cf);
                break;
            case ADC_C:
                ADD(regs.c, regs.cf);
                break;
            case ADC_D:
                ADD(regs.d, regs.cf);
                break;
            case ADC_E:
                ADD(regs.e, regs.cf);
                break;
            case ADC_H:
                ADD(regs.h, regs.cf);
                break;
            case ADC_L:
                ADD(regs.l, regs.cf);
                break;
            case ADC_M:
                res = read_byte(regs.hl);
                ADD(res, regs.cf);
                break;

            case ADC_A:
                ADD(regs.a, regs.cf);
                break;
            case SUB_B:
                SUB(regs.b, 0);
                break;
            case SUB_C:
                SUB(regs.c, 0);
                break;
            case SUB_D:
                SUB(regs.d, 0);
                break;
            case SUB_E:
                SUB(regs.e, 0);
                break;
            case SUB_H:
                SUB(regs.h, 0);
                break;
            case SUB_L:
                SUB(regs.l, 0);
                break;
            case SUB_M:
                res = read_byte(regs.hl);
                SUB(res, 0);
                break;
            case SUB_A:
                SUB(regs.a, 0);
                break;
            case SBB_B:
                SUB(regs.b, 1);
                break;
            case SBB_C:
                SUB(regs.c, 1);
                break;
            case SBB_D:
                SUB(regs.d, 1);
                break;
            case SBB_E:
                SUB(regs.e, 1);
                break;
            case SBB_H:
                SUB(regs.h, 1);
                break;
            case SBB_L:
                SUB(regs.l, 1);
                break;
            case SBB_M:
                res = read_byte(regs.hl);
                SUB(res, 1);
                break;

            case SBB_A:
                SUB(regs.a, 1);
                break;
            case ANA_B:
                ANA(regs.b);
                break;
            case ANA_C:
                ANA(regs.c);
                break;
            case ANA_D:
                ANA(regs.d);
                break;
            case ANA_E:
                ANA(regs.e);
                break;
            case ANA_H:
                ANA(regs.h);
                break;
            case ANA_L:
                ANA(regs.l);
                break;
            case ANA_M:
                res = read_byte(regs.hl);
                ANA(res);
                break;
            case ANA_A:
                ANA(regs.a);
                break;
            case XRA_B:
                XRA(regs.b);
                break;
            case XRA_C:
                XRA(regs.c);
                break;
            case XRA_D:
                XRA(regs.d);
                break;
            case XRA_E:
                XRA(regs.e);
                break;
            case XRA_H:
                XRA(regs.h);
                break;
            case XRA_L:
                XRA(regs.l);
                break;
            case XRA_M:
                res = read_byte(regs.hl);
                XRA(res);
                break;

            case XRA_A:
                XRA(regs.a);
                break;
            case ORA_B:
                ORA(regs.b);
                break;
            case ORA_C:
                ORA(regs.c);
                break;
            case ORA_D:
                ORA(regs.d);
                break;
            case ORA_E:
                ORA(regs.e);
                break;
            case ORA_H:
                ORA(regs.h);
                break;
            case ORA_L:
                ORA(regs.l);
                break;
            case ORA_M:
                res = read_byte(regs.hl);
                ORA(res);
                break;
            case ORA_A:
                ORA(regs.a);
                break;
            case CMP_B:
                CMP(regs.b);
                break;
            case CMP_C:
                CMP(regs.c);
                break;
            case CMP_D:
                CMP(regs.d);
                break;
            case CMP_E:
                CMP(regs.e);
                break;
            case CMP_H:
                CMP(regs.h);
                break;
            case CMP_L:
                CMP(regs.l);
                break;
            case CMP_M:
                lo_byte = read_byte(regs.hl);
                CMP(lo_byte);
                break;

            case CMP_A:
                CMP(regs.a);
                break;
            case RNZ:
                if (!regs.zf) RET();
                break;
            case POP_B:
                POP(regs.c, regs.b);
                break;
            case JNZ:
                if (!regs.zf) JUMP();
                break;
            case JMP:
                JUMP();
                break;
            case CNZ:
                if (!regs.zf) CALL();
                break;
            case PUSH_B:
                PUSH(regs.c, regs.b);
                break;
            case ADI:
                lo_byte = read_next_byte();
                ADD(lo_byte, 0);
                break;
            case RST_0:
                break;
            case RZ:
                if (regs.zf) RET();
                break;
            case RET:
                RET();
                break;
            case JZ:
                if (regs.zf) JUMP();
                break;
            case RSTV:
                /* not implemented in 8080 */
                break;
            case CZ:
                if (regs.zf) CALL();
                break;
            case CALL:
                CALL();
                break;
            case ACI:
                lo_byte = read_next_byte();
                ADD(lo_byte, regs.cf);
                break;

            case RST_1:
                RST(1);
                break;
            case RNC:
                if (!regs.cf) RET();
                break;
            case POP_D:
                POP(regs.e, regs.d);
                break;
            case JNC:
                if (!regs.cf) JUMP();
                break;
            case OUT:
                break;
            case CNC:
                if (!regs.cf) CALL();
                break;
            case PUSH_D:
                PUSH(regs.e, regs.d);
                break;
            case SUI:
                lo_byte = read_next_byte();
                SUB(lo_byte, 0);
                break;
            case RST_2:
                RST(2);
                break;
            case RC:
                if (regs.cf) RET();
                break;
            case SHLX:
                /* not implemented in 8080 */
                break;
            case JC:
                if (regs.cf) JUMP();
                break;
            case IN:
                break;
            case CC:
                if (regs.cf) CALL();
                break;
            case JNUI:
                break;
            case SBI:
                lo_byte = read_next_byte();
                SUB(lo_byte, 1);
                break;

            case RST_3:
                RST(3);
                break;
            case RPO:
                if (regs.pf) RET();
                break;
            case POP_H:
                POP(regs.l, regs.h);
                break;
            case JPO:
                if (!regs.pf) JUMP();
                break;
            case XTHL:
                lo_byte = regs.l;
                hi_byte = regs.h;
                regs.l = read_byte(regs.sp);
                regs.h = read_byte(regs.sp + 1);
                write_byte(regs.sp, lo_byte);
                write_byte(regs.sp + 1, hi_byte);
                break;
            case CPO:
                if (!regs.pf) CALL();
                break;
            case PUSH_H:
                PUSH(regs.l, regs.h);
                break;
            case ANI:
                lo_byte = read_next_byte();
                ANA(lo_byte);
                break;
            case RST_4:
                RST(4);
                break;
            case RPE:
                if (regs.pf) RET();
                break;
            case PCHL:
                regs.pcl = regs.l;
                regs.pch = regs.h;
                break;
            case JPE:
                if (regs.pf) JUMP();
                break;
            case XCHG:
                lo_byte = regs.l;
                hi_byte = regs.h;
                regs.l = regs.e;
                regs.h = regs.d;
                regs.e = lo_byte;
                regs.d = hi_byte;
                break;
            case CPE:
                if (regs.pf) CALL();
                break;
            case LHLX:
                /* not implemented in 8080 */
                break;
            case XRI:
                lo_byte = read_next_byte();
                XRA(lo_byte);
                break;

            case RST_5:
                RST(050);
                break;
            case RP:
                if (!regs.sf) RET();
                break;
            case POP_PSW:
                lo_byte = read_byte(regs.sp);
                hi_byte = read_byte(regs.sp + 1);
                regs.cf  = 0x01 & lo_byte;
                regs.pf  = 0x04 & lo_byte;
                regs.acf = 0x10 & lo_byte;
                regs.zf  = 0x40 & lo_byte;
                regs.sf  = 0x80 & lo_byte;

                regs.a = hi_byte;
                regs.sp += 2;
                break;
            case JP:
                if (!regs.sf) JUMP();
                break;
            case DI:
                interrupt_enabled = 0;
                break;
            case CP:
                if (!regs.sf) CALL();
                break;
            case PUSH_PSW:
                lo_byte = 0x02;
                lo_byte |= regs.cf;
                lo_byte |= regs.pf << 2;
                lo_byte |= regs.acf << 4;
                lo_byte |= regs.zf << 6;
                lo_byte |= regs.sf << 7;
                write_byte(regs.sp - 1, regs.a);
                write_byte(regs.sp - 2, lo_byte);
                regs.sp -= 2;
                break;
            case ORI:
                lo_byte = read_next_byte();
                ORA(lo_byte);
                break;
            case RST_6:
                RST(6);
                break;
            case RM:
                if (regs.sf) RET();
                break;
            case SPHL:
                regs.sp = regs.hl;
                break;
            case JM:
                if (regs.sf) JUMP();
                break;
            case EI:
                interrupt_enabled = 1;
                break;
            case CM:
                if (regs.sf) CALL();
                break;
            case JUI:
                /* not implemented in 8080 */
                break;
            case CPI:
                lo_byte = read_next_byte();
                CMP(lo_byte);
                break;

            case RST_7:
                RST(7);
                break;
        }
    }

    return 0;
}
