#include <stdio.h>

int
main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s CONFIG_FILE\n", argv[0]);
    }
    return 0;
}
