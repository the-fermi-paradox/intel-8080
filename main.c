#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <opcodes.h>
// #define NDEBUG
#include <assert.h>

/* main memory */
static unsigned memory[(unsigned) -1] = {0};
/* conditional bit flags */
enum Flags {FLAG_CY = 0, FLAG_P = 2, FLAG_AC = 4, FLAG_Z = 6, FLAG_S = 7};
static unsigned flags = 0;

/* register array */
/* stored in order Hi/Lo */
enum RegCode {rB = 0, rC = 1, rD = 2, rE = 3, rH = 4, rL = 5, rPSW = 6, rA = 7, RG_COUNT};
static unsigned reg[RG_COUNT] = {0};
static unsigned *B = reg + rB;
static unsigned *C = reg + rC;
static unsigned *D = reg + rD;
static unsigned *E = reg + rE;
static unsigned *H = reg + rH;
static unsigned *L = reg + rL;
static unsigned *PSW = reg + rPSW;
static unsigned *A = reg + rA;
static unsigned *sp = NULL;

/* masks */
/* position of the most significant bit */
#define PMSB_8 7U
/* position of the least significant bit */
#define PLSB_8 0U
/* most significant bit */
#define MSB_8 (1U << PMSB_8)
/* least significant bit */
#define LSB_8 (1U << PLSB_8)
/* position of the "overflow" bit in certain operations */
#define POVERFLOW_8 8U
/* the overflow bit */
#define OVERFLOW_8 (1U << POVERFLOW_8)
/* bitmasks for keeping our bits right */
#define BITMASK_8  0xFF
#define BITMASK_16 0xFFFF

/* set the bit at pos in bitfield to x, return the modified bitfield */
static inline unsigned e80_set_bit(unsigned bitfield, unsigned pos, unsigned x) {
  assert(pos <= PMSB_8 && pos >= 0);
  assert(bitfield <= UINT8_MAX);
  return (bitfield & ~(1U << pos)) | (!!x << pos);
}

/* get the bit at pos in bitfield */
static inline unsigned e80_get_bit(unsigned bitfield, unsigned pos) {
  assert(pos <= PMSB_8 && pos >= 0);
  assert(bitfield <= UINT8_MAX);
  return !!(bitfield & (1U << pos));
}

/* set the bit at pos in flags to x */
static inline void e80_set_flag(unsigned pos, unsigned x) {
  flags = e80_set_bit(flags, pos, x);
}

/* get the bit at pos in flags */
static inline unsigned e80_get_flag(unsigned pos) {
  return !!(e80_get_bit(flags, pos));
}

static inline unsigned e80_check_parity(unsigned value) {
  unsigned parity = 1;
  while (value) {
    parity = !parity;
    value &= (value - 1U);
  }
  return parity;
}

static inline unsigned e80_check_zero(unsigned value) {
  return !value;
}

static inline unsigned e80_check_sign(unsigned value) {
  return !!(value & 0x80U);
}

static inline unsigned e80_check_carry(unsigned value) {
  return !!(value & 0x100U);
}

static inline unsigned e80_check_half_carry(unsigned v1, unsigned v2) {
  return !!(((v1 & 0x0fU) + (v2 & 0x0fU)) & 0x10U);
}


static inline void e80_increment(unsigned* p) {
  assert(p);
  assert(*p <= UINT8_MAX);
  unsigned sum = *p + 1;
  e80_set_flag(FLAG_P,  e80_check_parity(sum));
  e80_set_flag(FLAG_AC, e80_check_half_carry(*p, 1));
  e80_set_flag(FLAG_Z,  e80_check_zero(sum));
  e80_set_flag(FLAG_S,  e80_check_sign(sum));
  *p = sum & BITMASK_8;
}

static inline void e80_add(unsigned value) {
  unsigned carry = e80_get_flag(FLAG_CY);
  unsigned sum = *A + value + carry;

  /* Handle condition bits */

  e80_set_flag(FLAG_CY, e80_check_carry(sum));
  e80_set_flag(FLAG_AC, e80_check_half_carry(*A, value));
  e80_set_flag(FLAG_Z,  e80_check_zero(sum));
  e80_set_flag(FLAG_P,  e80_check_parity(sum));
  e80_set_flag(FLAG_S,  e80_check_sign(sum));
  *A = sum & BITMASK_8;
}

static inline void e80_subtract(unsigned value) {
  
}

/* performs a left rotation with the carry bit set to the LSB's new value */
static inline unsigned e80_lrotate(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg <= UINT8_MAX);

  e80_set_flag(FLAG_CY, !!(*reg & MSB_8));
  *reg = (*reg << 1U) | (*reg >> (PMSB_8));
  return *reg = *reg & BITMASK_8;
}

/* performs a left rotation with the carry flag acting as a 9th bit */
/* in other words, the rotation is done THROUGH the carry */
static inline unsigned e80_lrotate_c(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg <= UINT8_MAX);

  unsigned tmp, cbit;
  cbit = !!(*reg & MSB_8);
  tmp = *reg | (e80_get_flag(FLAG_CY) << POVERFLOW_8);
  tmp = (tmp << 1) | (tmp >> POVERFLOW_8);
  e80_set_flag(FLAG_CY, cbit);
  return *reg = tmp & BITMASK_8;
}

/* performs a right rotation with the carry bit set to the MSB's new value */
static inline unsigned e80_rrotate(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg <= UINT8_MAX);

  e80_set_flag(FLAG_CY, !!(*reg & LSB_8));
  *reg = (*reg >> 1U) | (*reg << PMSB_8);
  return *reg = *reg & BITMASK_8;
}

/* performs a right rotation with the carry flag acting as a 9th bit */
/* in other words, the rotation is done THROUGH the carry */
static inline unsigned e80_rrotate_c(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg <= UINT8_MAX);

  unsigned tmp, cbit;
  cbit = *reg & LSB_8;
  tmp = *reg | (e80_get_flag(FLAG_CY) << POVERFLOW_8);
  tmp = (tmp >> 1);
  return *reg = tmp & BITMASK_8;
}

/* increments a register and returns the incremented register's value */
static inline unsigned e80_incr_reg(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg >= 0U && *reg <= UINT8_MAX);
  return ++(*reg);
}

/* decrements a register and returns the decremented register's value */
static inline unsigned e80_decr_reg(unsigned* reg) {
  assert(reg != NULL);
  assert(*reg >= 0U && *reg <= UINT8_MAX);
  return --(*reg);
}

/* assigns val to register reg */
static inline unsigned e80_assn_reg(unsigned* reg, unsigned val) {
  assert(reg != NULL);
  return *reg = val;
}

/* returns the hi part; sets the value of the lo part */
static inline unsigned e80_split16(unsigned num, unsigned* lo_reg) {
  unsigned hi_val = (0xf0 & num) >> 8;
  *lo_reg = 0x0f & num;
  return hi_val;
}

/* merge two bytes (hibyte, lobyte) to form a 2-byte (16-bit) integer */
static inline unsigned e80_merge16(unsigned hibyte, unsigned lobyte) {
  return (hibyte << 8) | (lobyte);
}

/* takes in the hi part of a register pair and returns the result
   of merging it with its lo part */
static inline unsigned e80_merge_rp(unsigned* hi_reg) {
  return e80_merge16(*hi_reg, *(hi_reg + 1));
}

/* assigns two bytes (b1, b2) to the register pair pointed to by hi */
static inline unsigned e80_assn_rp(unsigned* hi_reg, unsigned hibyte, unsigned lobyte) {
  *hi_reg = hibyte;
  *(hi_reg + 1) = lobyte;
  return *hi_reg;
}

/* increments each part of a register pair by one */
static inline unsigned e80_incr_rp(unsigned* hi) {
  unsigned rp = e80_merge_rp(hi);
  *hi = e80_split16(rp + 1, hi + 1);
  return *hi;
}

/* decrements each part of a register pair by one */
static inline unsigned e80_decr_rp(unsigned* hi_reg) {
  unsigned rp = e80_merge_rp(hi_reg);
  *hi_reg = e80_split16(rp - 1, hi_reg + 1);
  return *hi_reg;
}

/* adds two register pairs, storing the result in dest (hi and lo)
 * src and dest should be the hi parts of their pairs */
static inline unsigned e80_add_rp(unsigned* src, unsigned* dest) {
  unsigned r1 = e80_merge_rp(src);
  unsigned r2 = e80_merge_rp(dest);
  unsigned sum = r1 + r2;
  /* there's a carry */
  if (sum < r1)
    flags |= (1 << FLAG_CY);
  *dest = e80_split16(sum, (dest + 1));
  return sum;
}

static inline unsigned e80_read_mem(uint16_t address) {
  return memory[address];
}

static inline unsigned e80_write_mem(uint16_t address, unsigned val) {
  return memory[address] = val;
}

static inline unsigned e80_write_mem_rpaddr(unsigned* hi_reg, unsigned val) {
  return e80_write_mem(e80_merge_rp(hi_reg), val);
}

/* using the register pair as data, write to the address */
static inline unsigned e80_write_mem_rpdata(unsigned* hi_reg, unsigned hibyte, unsigned lobyte) {
  unsigned addr = e80_merge16(hibyte, lobyte);
  e80_write_mem(addr, *(hi_reg + 1));
  e80_write_mem(addr + 1, *hi_reg);
  return *hi_reg;
}

static inline unsigned e80_read_mem_rpaddr(unsigned* hi_reg) {
  return e80_read_mem(e80_merge_rp(hi_reg));
}

int main(void)
{
  /* TODO: Load program into memory */
  /* TODO: Initialize program counter to first instruction of program */

  /* Processor Cycle */

  /* pc (program counter) as a 16-bit integer instead of a pointer has some nice features
   * memory[pc] can never access bad memory. If incremented past (1 << 16) - 1,
   * it will just loop around to 0, which guarantees it will be in range.
   */
  for (uint16_t pc = 0;;pc++) {
    enum OpCode opcode = e80_read_mem(pc);
    /* These operations are always safe */
    unsigned lo_data = e80_read_mem(pc);
    unsigned hi_data = e80_read_mem(pc + 1);
    /* TODO: Op Code switch statement */
    switch (opcode) {
    case NOP:
      break;
    case LXI_B:
      e80_assn_rp(B, hi_data, lo_data);
      pc++; pc++;
      break;
    case STAX_B:
      e80_write_mem_rpaddr(B, *A);
      break;
    case INX_B:
      e80_incr_rp(B);
      break;
    case INR_B:
      e80_incr_reg(B);
      break;
    case DCR_B:
      e80_decr_reg(B);
      break;
    case MVI_B:
      e80_assn_reg(B, lo_data);
      pc++;
      break;
    case RLC:
      e80_lrotate(A);
      break;
    case DSUB:
      break; /* Not implemented in 8080 */
    case DAD_B:
      e80_add_rp(B, H);
      break;
    case LDAX_B:
      e80_assn_reg(A, e80_read_mem_rpaddr(B));
      break;
    case DCX_B:
      e80_decr_rp(B);
      break;
    case INR_C:
      e80_incr_reg(C);
      break;
    case DCR_C:
      e80_decr_reg(C);
      break;
    case MVI_C:
      e80_assn_reg(C, lo_data);
      pc++;
      break;
    case RRC:
      e80_rrotate(A);
      break;
    case AHRL:
      break; /* Not implemented in 8080 */
    case LXI_D:
      e80_assn_rp(D, hi_data, lo_data);
      pc++;
      pc++;
      break;
    case STAX_D:
      e80_write_mem_rpaddr(D, *A);
      break;
    case INX_D:
      e80_incr_rp(D);
      break;
    case INR_D:
      e80_incr_reg(D);
      break;
    case DCR_D:
      e80_decr_reg(D);
      break;
    case MVI_D:
      e80_assn_reg(D, lo_data);
      pc++;
      break;
    case RAL:    e80_lrotate_c(A); break;
    case RDEL:   break; /* Not implemented in 8080 */
    case DAD_D:  e80_add_rp(D, H); break;
    case LDAX_D: e80_assn_reg(A, e80_read_mem_rpaddr(D)); break;
    case DCX_D:  e80_decr_rp(D); break;
    case INR_E:  e80_incr_reg(E); break;
    case DCR_E:  e80_decr_reg(E); break;
    case MVI_E:  e80_assn_reg(E, lo_data); pc++; break;
    case RAR:    e80_rrotate_c(A); break;
 
    case RIM:    break; /* not implemented in 8080 */
    case LXI_H:  e80_assn_rp(H, hi_data, lo_data); pc++; pc++; break;
    case SHLD:   e80_write_mem_rpdata(H, hi_data, lo_data); pc++; pc++; break;
    case INX_H:  e80_incr_rp(H); break;
    case INR_H:  e80_incr_reg(H); break;
    case DCR_H:  e80_decr_reg(H); break;
    case MVI_H:  e80_assn_reg(H, lo_data); pc++; break;
    case DAA: {
      unsigned lsbs = *A & 0x0f;
      if (e80_get_flag(FLAG_AC) || lsbs > 9) {
        e80_add(6U);
      }
      unsigned msbs = *A >> 4;
      if (e80_get_flag(FLAG_CY) || msbs > 9) {
	msbs = (msbs + 6U) << 4;
      }
      *A = (*A & 0x0fU) | msbs;
      break;
    }
    case LDHI:   break; /* not implemented in 8080 */
    case DAD_H:
      break;
    case LHLD:
      break;
    case DCX_H:
      break;
    case INR_L:
      break;
    case DCR_L:
      break;
    case MVI_L:
      break;
    case CMA:
      break;

    case SIM:
      break;
    case LXI_SP:
      break;
    case STA:
      break;
    case INX_SP:
      break;
    case INR_M:
      break;
    case DCR_M:
      break;
    case MVI_M:
      break;
    case STC:
      break;
    case LDSI:
      break;
    case DAD_SP:
      break;
    case LDA:
      break;
    case DCX_SP:
      break;
    case INR_A:
      break;
    case DCR_A:
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
    /* TODO: Execute instruction */
  }
  return 0;
}
