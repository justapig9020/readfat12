#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat12.h"
#include "tools.h"
#include "fat_table.h"

typedef struct {
    void (*func)(struct Fat12 *, char *argv[]);
    char *name;
    char *arg;
    char *help;
} Operation_t;

static void _print_header(struct Fat12 *image, char *argv[]) {
    print_header(image);
}

static void print_fat_table(struct Fat12 *image, char *argv[]) {
    const size_t entries = 9 * 512 / 3 * 2;
    if (!argv[0])
        return;
    int table = atoi(argv[0]);
    for (int i=0; i<entries; i++) {
        uint16_t val = get_fat_value(image, table, i);
        if (val == FAT_UNUSED)
            continue;
        printf("0x%x: 0x%hx\n", i, val);
    }
}

static void print_node_info(struct Fat12 *image, char *argv[]) {
    char *path = argv[0];
    if (!path)
        return;
    print_inode(image, path);
}

static void pad_tab(int n) {
    for (int i=0; i<n; i++)
        printf("|  ");
}

static void filename_only(struct Fat12 *_, struct DirEntry *node, int level) {
    pad_tab(level);
    printf("+- ");
    print_file_name(node->filename, node->ext);
    puts("");
}

static void full_info(struct Fat12 *img, struct DirEntry *node, int _) {
    print_node(node);
    puts("");
}

static Display_t mode_select(int mode) {
    Display_t ret;
    switch (mode) {
    case 0:
        ret = filename_only;
        break;
    case 1:
        ret = full_info;
        break;
    default:
        ret = filename_only;
    }
    return ret;
}

static void print_all_content(struct Fat12 *image, char*argv[]) {
    char *path = argv[0];
    struct DirEntry *root= find_node(image, path);

    if (!root) {
        printf("%s not found\n", path);
        return;
    }

    int mode = atoi(argv[1]);
    Display_t display = mode_select(mode);
    list_inodes(image, root, display);
}

static Operation_t ops[] = {
    { _print_header, "-H", "None\t", "Display header"},
    { print_fat_table, "-t", "<table: uint>", "List using fat table entries"},
    { print_node_info, "-n", "<path: string>", "Display info of given path" },
    { print_all_content, "-l", "<path: string>", "List all contents under the path"},
};

static void print_help() {
    printf("usage: readfat <Command> <Image name> <Argument>\n\n");
    printf("Command\tArgument\n\n");
    for (int i=0; i<ARR_SZ(ops); i++) {
        printf("%s\t%s\t%s\n", ops[i].name, ops[i].arg, ops[i].help);
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
            ops[i].func(image, argv + 3);
            break;
        }
    }
    free_fat12(image);
    return 0;
}
