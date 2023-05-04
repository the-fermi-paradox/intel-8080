#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include "cpu.h"
#include "io.h"

int main(int argc, char **argv)
{
    const char *program_name = argv[0];
    static struct option const long_options[] = {
            {"offset", required_argument, NULL, 'o'},
            {"version", no_argument, NULL, 'v'},
            {"help", no_argument, NULL, 'h'},
            {NULL, 0, NULL, 0},
    };
    /* parse options */
    int c;
    size_t offset = 0;
    while ((c = getopt_long(argc, argv, "vho:", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                errno = 0;
                offset = strtol(optarg, NULL, 0);
                switch(errno) {
                    case EINVAL:
                        /* handle unsupported */
                        fprintf(stderr, "%s: offset %s is an unsupported value\n", program_name, optarg);
                        return EINVAL;
                    case ERANGE:
                        /* handle overflow */
                        fprintf(stderr, "%s: offset %s is out of range for long\n", program_name, optarg);
                        return ERANGE;
                }
                /* handle valid long but too short for memory */
                if (offset >= MEM_SIZE) {
                    fprintf(stderr, "%s: offset %s is bigger than the cpu memory\n", program_name, optarg);
                    return EXIT_FAILURE;
                }
                break;
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "%s: expected arguments\n", program_name);
        return EXIT_FAILURE;
    }
    unsigned char *rom = memory + offset;
    for (int argind = optind; argind < argc; ++argind) {
        /* load rom into memory; files are loaded left to right */
        size_t bytes_read;
        if (!(bytes_read = load_rom(rom, MEM_SIZE - offset,argv[argind]))) {
            return EXIT_FAILURE;
        }
        rom = rom + bytes_read;
    }
    regs.pc = offset;
    while(1) {
        enum OpCode opcode = read_next_byte();
        if (instruction(opcode))
            break;
    }
}
