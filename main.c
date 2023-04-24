#include <stdint.h>
#include <opcodes.h>
#include <stdbool.h>

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
    bool cf;
    bool pf;
    bool acf;
    bool zf;
    bool sf;
    uint8_t a;
    uint16_t pc;
    union {
        struct {
            uint8_t spl, sph;
        };
        uint16_t sp;
    };
};

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
    lo_byte = read_byte(++regs.pc);         \
    hi_byte = read_byte(++regs.pc);         \
    address = merge_bytes(lo_byte, hi_byte);\
} while(0)

#define INCR_REG(rg) do {                   \
    ++regs.rg;                              \
    test_pzs(regs.rg);                      \
    test_ac(regs.rg, regs.rg - 1, 0x01);    \
} while(0)

#define DECR_REG(rg) do {                   \
    --regs.rg;                              \
    test_pzs(regs.rg);                      \
    test_ac(regs.rg, regs.rg + 1, 0x01);    \
} while(0)

#define DBLE_ADD(rg) do {                   \
    uint32_t tmp = regs.hl + regs.rg;       \
    regs.hl = (uint16_t) tmp;               \
    regs.cf = tmp & 0x10000;                \
} while(0)

int main(void)
{
    uint8_t lo_byte, hi_byte, res;
    uint16_t address;
    regs.pc = 0x0000; // set initial program counter

    // main loop
    for (regs.pc = 0x100;; ++regs.pc) {
        enum OpCode opcode = read_byte(regs.pc);
        switch (opcode) {
            case NOP:
                /* do nothing */
                break;
            case LXI_B:
                regs.c = read_byte(++regs.pc);
                regs.b = read_byte(++regs.pc);
                break;
            case STAX_B:
                write_byte(regs.bc, regs.a);
                break;
            case INX_B:
                ++regs.bc;
                break;
            case INR_B:
                INCR_REG(b);
                break;
            case DCR_B:
                DECR_REG(b);
                break;
            case MVI_B:
                regs.b = read_byte(++regs.pc);
                break;
            case RLC:
                regs.a = (regs.a << 1) | (regs.a >> 7);
                regs.cf = regs.a & 0x01; /* the rotated out bit, now the LSB, is copied into the carry */
                break;
            case DSUB:
                /* not implemented in 8080 */
                break;
            case DAD_B:
                DBLE_ADD(bc);
                break;
            case LDAX_B:
                regs.a = read_byte(regs.bc);
                break;
            case DCX_B:
                --regs.bc;
                break;
            case INR_C:
                INCR_REG(c);
                break;
            case DCR_C:
                DECR_REG(c);
                break;
            case MVI_C:
                regs.c = read_byte(++regs.pc);
                break;

            case RRC:
                regs.a = (regs.a >> 1) | (regs.a << 7);
                regs.cf = regs.a & 0x80; /* the rotated out bit, now the MSB, is copied into the carry */
                break;
            case AHRL:
                /* not implemented in 8080 */
                break;
            case LXI_D:
                regs.e = read_byte(++regs.pc);
                regs.d = read_byte(++regs.pc);
                regs.pc += 2;
                break;
            case STAX_D:
                write_byte(regs.de, regs.a);
                break;
            case INX_D:
                ++regs.de;
                break;
            case INR_D:
                INCR_REG(d);
                break;
            case DCR_D:
                DECR_REG(d);
                break;
            case MVI_D:
                regs.d = read_byte(++regs.pc);
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
                DBLE_ADD(de);
                break;
            case LDAX_D:
                regs.a = read_byte(regs.de);
                break;
            case DCX_D:
                --regs.de;
                break;
            case INR_E:
                INCR_REG(e);
                break;
            case DCR_E:
                DECR_REG(e);
                break;
            case MVI_E:
                regs.e = read_byte(++regs.pc);
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
                regs.l = read_byte(++regs.pc);
                regs.h = read_byte(++regs.pc);
                regs.pc += 2;
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
                INCR_REG(h);
                break;
            case DCR_H:
                DECR_REG(h);
                break;
            case MVI_H:
                regs.h = read_byte(++regs.pc);
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
                DBLE_ADD(hl);
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
                INCR_REG(l);
                break;
            case DCR_L:
                DECR_REG(l);
                break;
            case MVI_L:
                regs.l = read_byte(++regs.pc);
                break;
            case CMA:
                regs.a = ~regs.a;
                break;
            case SIM:
                /* not implemented in 8080 */
                break;
            case LXI_SP:
                regs.spl = read_byte(++regs.pc);
                regs.sph = read_byte(++regs.pc);
                break;
            case STA:
                COMPOSE_ADDR();
                break;
            case INX_SP:
                ++regs.sp;
                break;
            case INR_M:
                address = merge_bytes(regs.l, regs.h);
                res = read_byte(address) + 1;
                write_byte(address, res);
                test_pzs(res);
                test_ac(res, res - 1, 0x1);
                break;
            case DCR_M:
                address = merge_bytes(regs.l, regs.h);
                res = read_byte(address) - 1;
                write_byte(address, res);
                test_pzs(res);
                test_ac(res, res + 1, 0x1);
                break;
            case MVI_M:
                address = merge_bytes(regs.l, regs.h);
                res = read_byte(++regs.pc);
                write_byte(address, res);
                break;
            case STC:
                regs.cf = 1;
                break;
            case LDSI:
                /* not implemented in 8080 */
                break;
            case DAD_SP:
                DBLE_ADD(sp);
                break;
            case LDA:
                break;
            case DCX_SP:
                --regs.sp;
                break;
            case INR_A:
                INCR_REG(a);
                break;
            case DCR_A:
                DECR_REG(a);
                break;
            case MVI_A:
                break;

            case CMC:
                break;
            case MOV_B_B:
                break;
            case MOV_B_C:
                break;
            case MOV_B_D:
                break;
            case MOV_B_E:
                break;
            case MOV_B_H:
                break;
            case MOV_B_L:
                break;
            case MOV_B_M:
                break;
            case MOV_B_A:
                break;
            case MOV_C_B:
                break;
            case MOV_C_C:
                break;
            case MOV_C_D:
                break;
            case MOV_C_E:
                break;
            case MOV_C_H:
                break;
            case MOV_C_L:
                break;
            case MOV_C_M:
                break;


            case MOV_C_A:
                break;
            case MOV_D_B:
                break;
            case MOV_D_C:
                break;
            case MOV_D_D:
                break;
            case MOV_D_E:
                break;
            case MOV_D_H:
                break;
            case MOV_D_L:
                break;
            case MOV_D_M:
                break;
            case MOV_D_A:
                break;
            case MOV_E_B:
                break;
            case MOV_E_C:
                break;
            case MOV_E_D:
                break;
            case MOV_E_E:
                break;
            case MOV_E_H:
                break;
            case MOV_E_L:
                break;
            case MOV_E_M:
                break;

            case MOV_E_A:
                break;
            case MOV_H_B:
                break;
            case MOV_H_C:
                break;
            case MOV_H_D:
                break;
            case MOV_H_E:
                break;
            case MOV_H_H:
                break;
            case MOV_H_L:
                break;
            case MOV_H_M:
                break;
            case MOV_H_A:
                break;
            case MOV_L_B:
                break;
            case MOV_L_C:
                break;
            case MOV_L_D:
                break;
            case MOV_L_E:
                break;
            case MOV_L_H:
                break;
            case MOV_L_L:
                break;
            case MOV_L_M:
                break;

            case MOV_L_A:
                break;
            case MOV_M_B:
                break;
            case MOV_M_C:
                break;
            case MOV_M_D:
                break;
            case MOV_M_E:
                break;
            case MOV_M_H:
                break;
            case MOV_M_L:
                break;
            case HLT:
                break;
            case MOV_M_A:
                break;
            case MOV_A_B:
                break;
            case MOV_A_C:
                break;
            case MOV_A_D:
                break;
            case MOV_A_E:
                break;
            case MOV_A_H:
                break;
            case MOV_A_L:
                break;
            case MOV_A_M:
                break;

            case MOV_A_A:
                break;
            case ADD_B:
                break;
            case ADD_C:
                break;
            case ADD_D:
                break;
            case ADD_E:
                break;
            case ADD_H:
                break;
            case ADD_L:
                break;
            case ADD_M:
                break;
            case ADD_A:
                break;
            case ADC_B:
                break;
            case ADC_C:
                break;
            case ADC_D:
                break;
            case ADC_E:
                break;
            case ADC_H:
                break;
            case ADC_L:
                break;
            case ADC_M:
                break;

            case ADC_A:
                break;
            case SUB_B:
                break;
            case SUB_C:
                break;
            case SUB_D:
                break;
            case SUB_E:
                break;
            case SUB_H:
                break;
            case SUB_L:
                break;
            case SUB_M:
                break;
            case SUB_A:
                break;
            case SBB_B:
                break;
            case SBB_C:
                break;
            case SBB_D:
                break;
            case SBB_E:
                break;
            case SBB_H:
                break;
            case SBB_L:
                break;
            case SBB_M:
                break;

            case SBB_A:
                break;
            case ANA_B:
                break;
            case ANA_C:
                break;
            case ANA_D:
                break;
            case ANA_E:
                break;
            case ANA_H:
                break;
            case ANA_L:
                break;
            case ANA_M:
                break;
            case ANA_A:
                break;
            case XRA_B:
                break;
            case XRA_C:
                break;
            case XRA_D:
                break;
            case XRA_E:
                break;
            case XRA_H:
                break;
            case XRA_L:
                break;
            case XRA_M:
                break;

            case XRA_A:
                break;
            case ORA_B:
                break;
            case ORA_C:
                break;
            case ORA_D:
                break;
            case ORA_E:
                break;
            case ORA_H:
                break;
            case ORA_L:
                break;
            case ORA_M:
                break;
            case ORA_A:
                break;
            case CMP_B:
                break;
            case CMP_C:
                break;
            case CMP_D:
                break;
            case CMP_E:
                break;
            case CMP_H:
                break;
            case CMP_L:
                break;
            case CMP_M:
                break;

            case CMP_A:
                break;
            case RNZ:
                break;
            case POP_B:
                break;
            case JNZ:
                break;
            case JMP:
                break;
            case CNZ:
                break;
            case PUSH_B:
                break;
            case ADI:
                break;
            case RST_0:
                break;
            case RZ:
                break;
            case RET:
                break;
            case JZ:
                break;
            case RSTV:
                break;
            case CZ:
                break;
            case CALL:
                break;
            case ACI:
                break;

            case RST_1:
                break;
            case RNC:
                break;
            case POP_D:
                break;
            case JNC:
                break;
            case OUT:
                break;
            case CNC:
                break;
            case PUSH_D:
                break;
            case SUI:
                break;
            case RST_2:
                break;
            case RC:
                break;
            case SHLX:
                break;
            case JC:
                break;
            case IN:
                break;
            case CC:
                break;
            case JNUI:
                break;
            case SBI:
                break;

            case RST_3:
                break;
            case RPO:
                break;
            case POP_H:
                break;
            case JPO:
                break;
            case XTHL:
                break;
            case CPO:
                break;
            case PUSH_H:
                break;
            case ANI:
                break;
            case RST_4:
                break;
            case RPE:
                break;
            case PCHL:
                break;
            case JPE:
                break;
            case XCHG:
                break;
            case CPE:
                break;
            case LHLX:
                break;
            case XRI:
                break;

            case RST_5:
                break;
            case RP:
                break;
            case POP_PSW:
                break;
            case JP:
                break;
            case DI:
                break;
            case CP:
                break;
            case PUSH_PSW:
                break;
            case ORI:
                break;
            case RST_6:
                break;
            case RM:
                break;
            case SPHL:
                break;
            case JM:
                break;
            case EI:
                break;
            case CM:
                break;
            case JUI:
                break;
            case CPI:
                break;

            case RST_7:
                break;
        }
        if (regs.pc == UINT16_MAX)
            break;
    }

    return 0;
}
