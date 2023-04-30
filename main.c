#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cpu.h>
#include <getopt.h>
#include <errno.h>

size_t load_rom(unsigned char *bufptr, const char *filename)
{
    /* get true path */
    const char *base = "../rom/";
    const size_t len = snprintf(NULL, 0, "%s%s", base, filename) + 1;
    char *path = malloc(len);
    snprintf(path, len, "%s%s", base, filename);

    FILE *file = fopen(path, "r");
    free(path);
    if (!file) {
        fprintf(stderr, "failed to open %s\n", filename);
        return 0;
    }

    if (fseek(file, 0L, SEEK_END) == EOF) {
        fprintf(stderr, "failed to seek on %s\n", filename);
        fclose(file);
        return 0;
    }
    const size_t bytes_read = ftell(file);
    rewind(file);

    if (!fread(bufptr, bytes_read, 1, file)) {
        fprintf(stderr, "failed to read %s\n", filename);
        fclose(file);
        return 0;
    }
    fclose(file);
    return bytes_read;
}

int main(int argc, char **argv)
{
    const char *program_name = argv[0];
    static struct option const long_options[] = {
            {"offset", required_argument, NULL, 'o'},
            {"version", no_argument, NULL, 'v'},
            {"help", no_argument, NULL, 'h'},
            {NULL, 0, NULL, 0},
            {0, 0, 0, 0},
    };
    /* parse options */
    int c;
    size_t offset = 0;
    while ((c = getopt_long(argc, argv, "vho:", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                errno = 0;
                offset = strtol(optarg, NULL, 0);
                if (offset >= MEM_SIZE) {
                    fprintf(stderr, "%s: offset %s is bigger than the cpu memory\n", program_name, optarg);
                    return EXIT_FAILURE;
                }
                switch(errno) {
                    case EINVAL:
                        fprintf(stderr, "%s: offset %s is an unsupported value\n", program_name, optarg);
                        return EINVAL;
                    case ERANGE:
                        fprintf(stderr, "%s: offset %s is out of range for long\n", program_name, optarg);
                        return ERANGE;
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
        if (!(bytes_read = load_rom(rom, argv[argind]))) {
            return EXIT_FAILURE;
        }
        rom = rom + bytes_read;
    }
    regs.pc = offset;
    /* Inject ret instruction */
    memory[0x05] = RET;
    while(1) {
        if (regs.pc == 0x05) {
            if (regs.c == 0x09)  {
                uint16_t i;
                for (i = regs.de; read_byte(i) != '$'; ++i)
                    putc(read_byte(i), stdout);
            }
            else if (regs.c == 0x02)
                putc(regs.e, stdout);
        }
        enum OpCode opcode = read_next_byte();
        instruction(opcode);
    }
}
