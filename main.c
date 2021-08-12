#include <stdio.h>
#include <string.h>
#include "fat12.h"
#include "tools.h"

typedef struct {
    void (*func)(struct Fat12 *);
    char *name;
    char *help;
} Operation_t;

static Operation_t ops[] = {
    { print_header, "-H" , "Display header" },
};

static void print_help() {
    printf("usage: readfat <Operation> <Image name>\n\n");
    printf("Operations:\n");
    for (int i=0; i<ARR_SZ(ops); i++) {
        printf("    %s\t%s\n", ops[i].name, ops[i].help);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_help();
        return 1;
    }

    char *cmd = argv[1];
    char *file = argv[2];
    struct Fat12 *image = new_fat12(file);
    if (!image) {
        printf("[FAILED] Unable to parse the image\n");
        printf("\t%s\n", fat_err);
        return 1;
    }

    for (int i=0; i<ARR_SZ(ops); i++) {
        if (match(cmd, ops[i].name)) {
            ops[i].func(image);
            break;
        }
    }
    free_fat12(image);
    return 0;
}
