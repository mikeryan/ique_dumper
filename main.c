#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "ique.h"

int main(int argc, char **argv) {
    int r;

    if (argc < 3) {
        printf("Usage: %s <filename> <block1> [ <block2> ... ]\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];

    r = ique_init();
    if (r < 0)
        return 1;

    int ique_status(void);
    // ique_status();

    int ique_get_led(void);
    // int ret = ique_get_led();

    // printf("get led %d\n", ret);

    void blink_led(void);
    // blink_led();

    int ique_get_block(uint32_t block, unsigned char *out);
    int ique_get_block2(uint16_t block, unsigned char *out);
    // ique_get_block(0x0fe4, buf);


    int ique_bulk_receive(unsigned char *out, int size);

    FILE *out = fopen(filename, "w");
    if (out == NULL) err(1, "Can't open %s", filename);

    // flush the buffer
    int ique_flush(void);
    // ique_flush();

    if (argc < 3) abort();
    int start = atoi(argv[2]);
    int end = atoi(argv[3]);

    unsigned char buf[0x4000];
    int i;
    for (i = start; i < end; ++i) {
        // int block = atoi(argv[i]);
        ique_get_block2(i, buf);
        fwrite(buf, 0x4000, 1, out);
        fflush(out);
    }
    fclose(out);

    return 0;
}
