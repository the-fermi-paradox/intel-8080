#include <inttypes.h>
#include <stdlib.h>

/* register array */
enum RegCode {B = 0, C = 1, D = 2, E = 3, H = 4, L = 5, PSW = 6, A = 7, RG_COUNT};
/* flags */
enum FlagCode {CY = 0, N1 = 1, P = 2, N3 = 3, AC = 4, N5 = 5, Z = 6, S = 7, FLAG_COUNT};
#define FLAG_S  1U << S
#define FLAG_Z  1U << Z
#define FLAG_AC 1U << AC
#define FLAG_P  1U << P
#define FLAG_CY 1U << CY
#define FLAG_ON(bitset, bit)  (bitset | bit)
#define FLAG_OFF(bitset, bit) (bitset & ~(bit))

/* opcodes */
enum OpCode {
  NOP     = 0x00,
  LXI_B   = 0x01,
  STAX_B  = 0x02,
  INX_B   = 0x03,
  INR_B   = 0x04,
  DCR_B   = 0x05,
  MVI_B   = 0x06,

};

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
    case 0x00:
      break;
    }
    /* TODO: Execute instruction */
  }
  return 0;
}
