#include "cpu.h"

/* Initialize processor state */
uint8_t memory[MEM_SIZE] = {0};
struct Registers regs = {0};
bool interrupt_enabled = 0;

static const bool parity_table[256] = {
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
};

/* Read a byte from memory */
uint8_t read_byte(uint16_t addr)
{
    return memory[addr];
}

void write_byte(uint16_t addr, uint8_t value)
{
    memory[addr] = value;
}

uint8_t read_next_byte()
{
    return read_byte(regs.pc++);
}

uint16_t merge_bytes(uint8_t lo_byte, uint8_t hi_byte)
{
    return (uint16_t) ((hi_byte << 8) | lo_byte);
}

static inline void test_pzs(uint8_t res)
{
    regs.pf = parity_table[res];
    regs.zf = (res == 0);
    regs.sf = (res & (0x80));
}

static inline void test_ac(uint8_t res, uint8_t op1, uint8_t op2)
{
    regs.acf = (res ^ op1 ^ op2) & 0x10;
}

#define R16() do {                              \
    lo_byte = read_next_byte();                 \
    hi_byte = read_next_byte();                 \
    address = merge_bytes(lo_byte, hi_byte);    \
} while(0)

#define EM_INR(rg) do {                         \
    ++rg;                                       \
    test_pzs(rg);                               \
    test_ac(rg, rg - 1, 0x01);                  \
} while(0)

#define EM_DCR(rg) do {                         \
    tmp = rg - 1;                               \
    test_pzs(tmp);                              \
    test_ac(tmp, rg, ~0x01);                    \
    rg = tmp;                                   \
} while(0)

#define EM_DAD(rg) do {                         \
    uint32_t tmp32 = regs.hl + rg;              \
    regs.hl = (uint16_t) tmp32;                 \
    regs.cf = tmp32 & 0x10000;                  \
} while(0)

#define EM_POP(regl, regh) do {                 \
    regl = read_byte(regs.sp);                  \
    regh = read_byte(regs.sp + 1);              \
    regs.sp += 2;                               \
} while(0)

#define EM_PUSH(regl, regh) do { \
    write_byte(regs.sp - 1, regh);              \
    write_byte(regs.sp - 2, regl);              \
    regs.sp -= 2;                               \
} while(0)

#define EM_RET(bl) do {                         \
    if (bl) EM_POP(regs.pcl, regs.pch);         \
} while(0)

#define EM_JUMP(bl) do {                        \
    R16();                                      \
    if (bl) {                                   \
        regs.pc = address;                      \
    }                                           \
} while (0)

#define EM_CALL(bl) do {                        \
    R16();                                      \
    if (bl) {                                   \
        EM_PUSH(regs.pcl, regs.pch);            \
        regs.pc = address;                      \
    }                                           \
} while(0)

#define EM_RST(val) do {                        \
    EM_PUSH(regs.pcl, regs.pch);                \
    regs.pc = 8 * val;                          \
} while(0)

#define EM_ADD(val, cy) do {                    \
    tmp = regs.a + (val) + (cy);                \
    test_pzs(tmp);                              \
    test_ac(tmp, regs.a, (val));                \
    regs.cf = tmp & 0x100;                      \
    regs.a = (uint8_t) tmp;                     \
} while(0)

/* This is just two's complement:
 * val is complemented, cy is the add bit */
#define EM_SUB(val, cy) do {                    \
    EM_ADD((~val) & 0xFF, !cy);                 \
    regs.cf = !regs.cf;                         \
} while(0)

#define EM_CMP(val) do {                        \
    tmp = regs.a - (val);                       \
    test_pzs(tmp);                              \
    test_ac(tmp, regs.a, ~(val));               \
    regs.cf = tmp & 0x100;                      \
} while(0)

#define EM_ANA(val) do { \
    regs.cf = 0;                                \
    regs.acf = ((regs.a | val) & 0x08);         \
    regs.a &= (val);                            \
    test_pzs(regs.a);                           \
} while(0)

#define EM_XRA(val) do {                        \
    regs.a ^= (val);                            \
    regs.cf = 0;                                \
    regs.acf = 0;                               \
    test_pzs(regs.a);                           \
} while(0)

#define EM_ORA(val) do {                        \
    regs.a |= (val);                            \
    regs.cf = 0;                                \
    regs.acf = 0;                               \
    test_pzs(regs.a);                           \
} while(0)


int instruction(enum OpCode opcode)
{
    uint8_t lo_byte, hi_byte, res;
    uint16_t address, tmp;

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
            EM_INR(regs.b);
            break;
        case DCR_B:
            EM_DCR(regs.b);
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
            EM_DAD(regs.bc);
            break;
        case LDAX_B:
            regs.a = read_byte(regs.bc);
            break;
        case DCX_B:
            --regs.bc;
            break;
        case INR_C:
            EM_INR(regs.c);
            break;
        case DCR_C:
            EM_DCR(regs.c);
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
            EM_INR(regs.d);
            break;
        case DCR_D:
            EM_DCR(regs.d);
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
            EM_DAD(regs.de);
            break;
        case LDAX_D:
            regs.a = read_byte(regs.de);
            break;
        case DCX_D:
            --regs.de;
            break;
        case INR_E:
            EM_INR(regs.e);
            break;
        case DCR_E:
            EM_DCR(regs.e);
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
            R16();
            write_byte(address, regs.l);
            write_byte(address + 1, regs.h);
            break;
        }
        case INX_H:
            ++regs.hl;
            break;
        case INR_H:
            EM_INR(regs.h);
            break;
        case DCR_H:
            EM_DCR(regs.h);
            break;
        case MVI_H:
            regs.h = read_next_byte();
            break;
        case DAA: {
            uint8_t old_cf = regs.cf;
            uint8_t hi_nib = regs.a >> 4;
            uint8_t lo_nib = regs.a & 0x0F;
            uint8_t add = 0;

            if (lo_nib > 9 || regs.acf) {
                add += 0x06;
            }

            if (hi_nib > 9 || old_cf || ((hi_nib >= 9) && lo_nib > 9)) {
                add += 0x60;
                old_cf = 1;
            }
            EM_ADD(add, 0);
            regs.cf = old_cf;
            break;
        }
        case LDHI:
            /* not implemented in 8080 */
            break;
        case DAD_H:
            EM_DAD(regs.hl);
            break;
        case LHLD: {
            R16();
            regs.l = read_byte(address);
            regs.h = read_byte(address + 1);
            break;
        }
        case DCX_H:
            --regs.hl;
            break;
        case INR_L:
            EM_INR(regs.l);
            break;
        case DCR_L:
            EM_DCR(regs.l);
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
            R16();
            write_byte(address, regs.a);
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
            test_ac(res, res + 1, ~0x1);
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
            EM_DAD(regs.sp);
            break;
        case LDA:
            R16();
            regs.a = read_byte(address);
            break;
        case DCX_SP:
            --regs.sp;
            break;
        case INR_A:
            EM_INR(regs.a);
            break;
        case DCR_A:
            EM_DCR(regs.a);
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
            return EXIT_HLT;
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
            EM_ADD(regs.b, 0);
            break;
        case ADD_C:
            EM_ADD(regs.c, 0);
            break;
        case ADD_D:
            EM_ADD(regs.d, 0);
            break;
        case ADD_E:
            EM_ADD(regs.e, 0);
            break;
        case ADD_H:
            EM_ADD(regs.h, 0);
            break;
        case ADD_L:
            EM_ADD(regs.l, 0);
            break;
        case ADD_M:
            res = read_byte(regs.hl);
            EM_ADD(res, 0);
            break;
        case ADD_A:
            EM_ADD(regs.a, 0);
            break;
        case ADC_B:
            EM_ADD(regs.b, regs.cf);
            break;
        case ADC_C:
            EM_ADD(regs.c, regs.cf);
            break;
        case ADC_D:
            EM_ADD(regs.d, regs.cf);
            break;
        case ADC_E:
            EM_ADD(regs.e, regs.cf);
            break;
        case ADC_H:
            EM_ADD(regs.h, regs.cf);
            break;
        case ADC_L:
            EM_ADD(regs.l, regs.cf);
            break;
        case ADC_M:
            res = read_byte(regs.hl);
            EM_ADD(res, regs.cf);
            break;

        case ADC_A:
            EM_ADD(regs.a, regs.cf);
            break;
        case SUB_B:
            EM_SUB(regs.b, 0);
            break;
        case SUB_C:
            EM_SUB(regs.c, 0);
            break;
        case SUB_D:
            EM_SUB(regs.d, 0);
            break;
        case SUB_E:
            EM_SUB(regs.e, 0);
            break;
        case SUB_H:
            EM_SUB(regs.h, 0);
            break;
        case SUB_L:
            EM_SUB(regs.l, 0);
            break;
        case SUB_M:
            res = read_byte(regs.hl);
            EM_SUB(res, 0);
            break;
        case SUB_A:
            EM_SUB(regs.a, 0);
            break;
        case SBB_B:
            EM_SUB(regs.b, regs.cf);
            break;
        case SBB_C:
            EM_SUB(regs.c, regs.cf);
            break;
        case SBB_D:
            EM_SUB(regs.d, regs.cf);
            break;
        case SBB_E:
            EM_SUB(regs.e, regs.cf);
            break;
        case SBB_H:
            EM_SUB(regs.h, regs.cf);
            break;
        case SBB_L:
            EM_SUB(regs.l, regs.cf);
            break;
        case SBB_M:
            res = read_byte(regs.hl);
            EM_SUB(res, regs.cf);
            break;

        case SBB_A:
            EM_SUB(regs.a, regs.cf);
            break;
        case ANA_B:
            EM_ANA(regs.b);
            break;
        case ANA_C:
            EM_ANA(regs.c);
            break;
        case ANA_D:
            EM_ANA(regs.d);
            break;
        case ANA_E:
            EM_ANA(regs.e);
            break;
        case ANA_H:
            EM_ANA(regs.h);
            break;
        case ANA_L:
            EM_ANA(regs.l);
            break;
        case ANA_M:
            res = read_byte(regs.hl);
            EM_ANA(res);
            break;
        case ANA_A:
            EM_ANA(regs.a);
            break;
        case XRA_B:
            EM_XRA(regs.b);
            break;
        case XRA_C:
            EM_XRA(regs.c);
            break;
        case XRA_D:
            EM_XRA(regs.d);
            break;
        case XRA_E:
            EM_XRA(regs.e);
            break;
        case XRA_H:
            EM_XRA(regs.h);
            break;
        case XRA_L:
            EM_XRA(regs.l);
            break;
        case XRA_M:
            res = read_byte(regs.hl);
            EM_XRA(res);
            break;

        case XRA_A:
            EM_XRA(regs.a);
            break;
        case ORA_B:
            EM_ORA(regs.b);
            break;
        case ORA_C:
            EM_ORA(regs.c);
            break;
        case ORA_D:
            EM_ORA(regs.d);
            break;
        case ORA_E:
            EM_ORA(regs.e);
            break;
        case ORA_H:
            EM_ORA(regs.h);
            break;
        case ORA_L:
            EM_ORA(regs.l);
            break;
        case ORA_M:
            res = read_byte(regs.hl);
            EM_ORA(res);
            break;
        case ORA_A:
            EM_ORA(regs.a);
            break;
        case CMP_B:
            EM_CMP(regs.b);
            break;
        case CMP_C:
            EM_CMP(regs.c);
            break;
        case CMP_D:
            EM_CMP(regs.d);
            break;
        case CMP_E:
            EM_CMP(regs.e);
            break;
        case CMP_H:
            EM_CMP(regs.h);
            break;
        case CMP_L:
            EM_CMP(regs.l);
            break;
        case CMP_M:
            lo_byte = read_byte(regs.hl);
            EM_CMP(lo_byte);
            break;

        case CMP_A:
            EM_CMP(regs.a);
            break;
        case RNZ:
            EM_RET(!regs.zf);
            break;
        case POP_B:
            EM_POP(regs.c, regs.b);
            break;
        case JNZ:
            EM_JUMP(!regs.zf);
            break;
        case JMP:
            EM_JUMP(1);
            break;
        case CNZ:
            EM_CALL(!regs.zf);
            break;
        case PUSH_B:
            EM_PUSH(regs.c, regs.b);
            break;
        case ADI:
            lo_byte = read_next_byte();
            EM_ADD(lo_byte, 0);
            break;
        case RST_0:
            break;
        case RZ:
            EM_RET(regs.zf);
            break;
        case RET:
            EM_RET(1);
            break;
        case JZ:
            EM_JUMP(regs.zf);
            break;
        case RSTV:
            /* not implemented in 8080 */
            break;
        case CZ:
            EM_CALL(regs.zf);
            break;
        case CALL:
            EM_CALL(1);
            break;
        case ACI:
            lo_byte = read_next_byte();
            EM_ADD(lo_byte, regs.cf);
            break;

        case RST_1:
            EM_RST(1);
            break;
        case RNC:
            EM_RET(!regs.cf);
            break;
        case POP_D:
            EM_POP(regs.e, regs.d);
            break;
        case JNC:
            EM_JUMP(!regs.cf);
            break;
        case OUT:
            read_next_byte();
            break;
        case CNC:
            EM_CALL(!regs.cf);
            break;
        case PUSH_D:
            EM_PUSH(regs.e, regs.d);
            break;
        case SUI:
            lo_byte = read_next_byte();
            EM_SUB(lo_byte, 0);
            break;
        case RST_2:
            EM_RST(2);
            break;
        case RC:
            EM_RET(regs.cf);
            break;
        case SHLX:
            /* not implemented in 8080 */
            break;
        case JC:
            EM_JUMP(regs.cf);
            break;
        case IN:
            read_next_byte();
            break;
        case CC:
            EM_CALL(regs.cf);
            break;
        case JNUI:
            break;
        case SBI:
            lo_byte = read_next_byte();
            EM_SUB(lo_byte, regs.cf);
            break;

        case RST_3:
            EM_RST(3);
            break;
        case RPO:
            EM_RET(!regs.pf);
            break;
        case POP_H:
            EM_POP(regs.l, regs.h);
            break;
        case JPO:
            EM_JUMP(!regs.pf);
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
            EM_CALL(!regs.pf);
            break;
        case PUSH_H:
            EM_PUSH(regs.l, regs.h);
            break;
        case ANI:
            lo_byte = read_next_byte();
            EM_ANA(lo_byte);
            break;
        case RST_4:
            EM_RST(4);
            break;
        case RPE:
            EM_RET(regs.pf);
            break;
        case PCHL:
            regs.pcl = regs.l;
            regs.pch = regs.h;
            break;
        case JPE:
            EM_JUMP(regs.pf);
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
            EM_CALL(regs.pf);
            break;
        case LHLX:
            /* not implemented in 8080 */
            break;
        case XRI:
            lo_byte = read_next_byte();
            EM_XRA(lo_byte);
            break;

        case RST_5:
            EM_RST(5);
            break;
        case RP:
            EM_RET(!regs.sf);
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
            EM_JUMP(!regs.sf);
            break;
        case DI:
            interrupt_enabled = 0;
            break;
        case CP:
            EM_CALL(!regs.sf);
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
            EM_ORA(lo_byte);
            break;
        case RST_6:
            EM_RST(6);
            break;
        case RM:
            EM_RET(regs.sf);
            break;
        case SPHL:
            regs.sp = regs.hl;
            break;
        case JM:
            EM_JUMP(regs.sf);
            break;
        case EI:
            interrupt_enabled = 1;
            break;
        case CM:
            EM_CALL(regs.sf);
            break;
        case JUI:
            /* not implemented in 8080 */
            break;
        case CPI:
            lo_byte = read_next_byte();
            EM_CMP(lo_byte);
            break;

        case RST_7:
            EM_RST(7);
            break;
    }
    if (regs.pc == 0) {
        return EXIT_RST;
    }
    return EXIT_OK;
}

