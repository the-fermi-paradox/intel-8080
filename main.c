#include <inttypes.h>
#include <stdlib.h>
#include <opcodes.h>

/* register array */
enum RegCode {B = 0, C = 1, D = 2, E = 3, H = 4, L = 5, PSW = 6, A = 7, RG_COUNT};

/* flags */
#define FLAG_S  1U << 7U
#define FLAG_Z  1U << 6U
#define FLAG_AC 1U << 4U
#define FLAG_P  1U << 2U
#define FLAG_CY 1U << 0U
#define FLAG_ON(bitset, bit)  (bitset | bit)
#define FLAG_OFF(bitset, bit) (bitset & ~(bit))

int main(int argc, char** argv)
{
  uint8_t registers[RG_COUNT] = {
    [B] = 0,
    [C] = 0,
    [D] = 0,
    [E] = 0,
    [H] = 0,
    [L] = 0,
    [PSW] = 0,
    [A] = 0,
  };
  uint16_t sp = 0;
  uint16_t pc = 0;
  /* registers */
  uint8_t memory[(uint16_t) -1] = {0};
  /* TODO: Load program into memory */
  /* TODO: Initialize program counter to first instruction of program */
  /* Processor Cycle */
  for (;;) {
    /* TODO: Fetch instruction */
    enum OpCode opcode = *(memory + pc);
    /* TODO: Op Code switch statement */
    switch (opcode) {
      case NOP:
        break;
      case LXI_B:
        break;
      case STAX_B:
        break;
      case INX_B:
        break;
      case INR_B:
        break;
      case DCR_B:
        break;
      case MVI_B:
        break;
      case RLC:
        break;
      case DSUB:
        break;
      case DAD_B:
        break;
      case LDAX_B:
        break;
      case DCX_B:
        break;
      case INR_C:
        break;
      case DCR_C:
        break;
      case MVI_C:
        break;
      case RRC:
        break;

      case AHRL:
        break;
      case LXI_D:
        break;
      case STAX_D:
        break;
      case INX_D:
        break;
      case INR_D:
        break;
      case DCR_D:
        break;
      case MVI_D:
        break;
      case RAL:
        break;
      case RDEL:
        break;
      case DAD_D:
        break;
      case LDAX_D:
        break;
      case DCX_D:
        break;
      case INR_E:
        break;
      case DCR_E:
        break;
      case MVI_E:
        break;
      case RAR:
        break;

      case RIM:
        break;
      case LXI_H:
        break;
      case SHLD:
        break;
      case INX_H:
        break;
      case INR_H:
        break;
      case DCR_H:
        break;
      case MVI_H:
        break;
      case DAA:
        break;
      case LDHI:
        break;
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
