#include <stdio.h>
#include <string.h>
#include "fat12.h"
#include "tools.h"
#include "fat_table.h"

typedef struct {
    void (*func)(struct Fat12 *);
    char *name;
    char *help;
} Operation_t;

static void print_fat_table(struct Fat12 *image) {
    const size_t bytes = 9 * 512;
    int table;
    printf("Table: ");
    scanf("%d", &table);
    for (int i=0; i<bytes; i++) {
        uint16_t val = get_fat_value(image, table, i);
        if (val == FAT_UNUSED)
            continue;
        printf("0x%x: 0x%hx\n", i, val);
    }
}

static Operation_t ops[] = {
    { print_header, "-H" , "Display header" },
    { print_fat_table, "-t" , "list using fat table entries" },
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
