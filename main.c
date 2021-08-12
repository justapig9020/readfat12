#include <stdio.h>

#include "fat12.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: readfat <image name>\n");
        return 1;
    }

    struct Fat12 *image = new_fat12(argv[1]);
    if (!image)
        return 1;
    free_fat12(image);
    return 0;
}
