#ifndef EMU8080_CPUH
#define EMU8080_CPUH
#include <stdint.h>
#include <stdbool.h>
#include "opcodes.h"

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
#define MEM_SIZE 0x10000
extern uint8_t memory[MEM_SIZE];
extern struct Registers regs;
extern bool interrupt_enabled;

extern void write_byte(uint16_t addr, uint8_t value);
extern uint8_t read_byte(uint16_t addr);
extern uint8_t read_next_byte();
extern uint16_t merge_bytes(uint8_t lo_byte, uint8_t hi_byte);
extern void instruction(enum OpCode opcode);

#endif