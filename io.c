#include <stdlib.h>
#include <string.h>
#include "io.h"

extern size_t load_rom(unsigned char *bufptr, size_t buflen, const char *filename)
{
    /* get true path */
    const char *base = "../rom/";
    const size_t len = snprintf(NULL, 0, "%s%s", base, filename) + 1;
    char *path = malloc(len);
    snprintf(path, len, "%s%s", base, filename);

    FILE *file = fopen(path, "r");

    free(path);
    if (!file) {
        perror("fopen");
        return 0;
    }

    if (fseek(file, 0L, SEEK_END) == EOF) {
        perror("fseek");
        fclose(file);
        return 0;
    }
    const size_t bytes_read = ftell(file);
    if (bytes_read >= buflen) {
        fprintf(stderr, "file size %lu is bigger than the buffer\n", bytes_read);
        return EXIT_FAILURE;
    }
    rewind(file);

    if (!fread(bufptr, bytes_read, 1, file)) {
        perror("fread");
        fclose(file);
        return 0;
    }
    fclose(file);
    return bytes_read;
}