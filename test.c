#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "io.h"

const char *test_files[] = {"CPUTEST.COM", "TST8080.COM", "8080PRE.COM", "8080EXM.COM",};

int main(void) {
    for (size_t z = 0; z < sizeof(test_files) / sizeof(test_files[0]); ++z) {
        size_t offset = 0x100;
        uint8_t *rom = memory + offset;
        /* clear memory */
        memset(&memory[0], 0, sizeof(memory));
        /* load rom into memory */
        if (!load_rom(rom, MEM_SIZE - offset, test_files[z])) {
            return EXIT_FAILURE;
        }
        regs.pc = offset;
        /* Inject ret instruction */
        memory[0x05] = RET;
        /* Main CPU loop */
        while (1) {
            if (regs.pc == 0x05) {
                if (regs.c == 0x09) {
                    uint16_t i;
                    for (i = regs.de; read_byte(i) != '$'; ++i)
                        putc(read_byte(i), stdout);
                } else if (regs.c == 0x02)
                    putc(regs.e, stdout);
            }
            enum OpCode opcode = read_next_byte();
            if (instruction(opcode))
                break;
        }
        printf("\n\n");
    }
}
