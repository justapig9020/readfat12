#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#include "fat12.h"
#include "tools.h"
#include "fat_table.h"
#include "dir_attr.h"

#define TO_PHY(logic) (logic + 31)
char *fat_err = NULL;

static struct FatTable *base_of_fat(uint8_t *base, struct Header *header) {
    // Fat1 start from sector 1 (Second sector of the disk)
    return (struct FatTable *)(base + header->byte_per_sec);
}

static struct DirEntry *base_of_rootdir(uint8_t *base, struct Header *header) {
    // Extra 1 sector for Boot sector
    size_t pre_sectors = (header->sec_per_fat * header->num_of_fat + 1);
    size_t offset = header->byte_per_sec * pre_sectors;
    return (struct DirEntry *)(base + offset);
}

static uint8_t *base_of_logical(uint8_t *base, struct Header *header) {
    const size_t start_sector = 31;
    size_t offset = 31 * header->byte_per_sec;
    return base + offset;
}

struct Fat12 *new_fat12(char *filename) {
    struct Fat12 *fs = malloc(sizeof(*fs));
    if (!fs) {
        fat_err = "Allocate memory failed";
        goto fail;
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fat_err = "Open file failed";
        goto release;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fat_err = "Get file state failed";
        goto close;
    }

    fs->size = sb.st_size;   
    uint8_t *base = mmap(NULL, fs->size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        fat_err = "Memory mapping failed";
        goto close;
    }

    close(fd);
    fs->base = base;
    fs->header = (struct Header *)(base + 3);

    if (!match(fs->header->file_sys_type, "FAT12")) {
        free_fat12(fs);
        fat_err = "Wrong file system type";
        goto fail;
    }

    fs->table = base_of_fat(base, fs->header);
    fs->root = base_of_rootdir(base, fs->header);
    fs->logical_base = base_of_logical(base, fs->header);
    return fs;
close:
    close(fd);
release:
    free(fs);
fail:
    return NULL;
}

void free_fat12(struct Fat12 *target) {
    if (!target)
        return;
    munmap(target->base, target->size);
    free(target);
}

static void _print_header(struct Header *hdr) {
    printf("Name: %s\n", hdr->name);
    printf("Byte per sector: %"PRIu16"\n", hdr->byte_per_sec);
    printf("Sector per cluster: %"PRIu8"\n", hdr->sec_per_clus);
    printf("Reserved sectors: %"PRIu16"\n", hdr->reserved_sec);
    printf("Number of fats: %"PRIu8"\n", hdr->num_of_fat);
    printf("Max number of RootDir entries: %"PRIu16"\n", hdr->max_num_of_root_dir_entries);
    printf("Total sector count: %"PRIu16"\n", hdr->total_sec_cnt);
    printf("Sector per fat: %"PRIu16"\n", hdr->sec_per_fat);
    printf("Sector per track: %"PRIu16"\n", hdr->sec_per_track);
    printf("Numbers of heads: %"PRIu16"\n", hdr->num_of_heads);
    printf("Total sector count for fat32: %"PRIu32"\n", hdr->total_sec_cnt_for_fat32);
    printf("Boot signature: 0x%"PRIx8",  ", hdr->boot_sign);
    if (hdr->boot_sign == 0x29) {
        printf("Bootable");
    }
    printf("\n");
    printf("Volume ID: %"PRIu32"\n", hdr->volume_id);
    printf("Volume label: ");
    for (int i=0; i<11; i++)
        printf("%"PRIu8, hdr->volume_label[i]);
    puts("");
    printf("File system type: %s\n", hdr->file_sys_type);
}

void print_header(struct Fat12 *image) {
    _print_header(image->header);
}

static uint16_t get_fat_entry_value(struct FatTable *entry, size_t num) {
    if (1 < num)
        return FAT_UNUSED;
    uint16_t ret = 0;
    if (num == 0) {
        ret = entry->low;
        ret |= (entry->mid & 0xf) << 8;
    } else {
        ret = entry->mid >> 4;
        ret |= (entry->top) << 4;
    }
    return ret;
}

uint16_t get_fat_value(struct Fat12 *image, size_t table, size_t sector) {
    struct Header *header = image->header;
    if (table >= header->num_of_fat)
        return FAT_UNUSED;

    size_t max = header->byte_per_sec * header->num_of_fat;
    if (sector >= max)
        return FAT_UNUSED;

    struct FatTable *base = &image->table[max * table];
    /* Due to fat12 use 12 bit to represent a fat entry, that is hard to form a single
     * entry into structure.
     * Therefore, FatTable form every two entries (3 bytes) into an object.
     */
    size_t offset = sector / 2;
    size_t num = sector % 2;
    return get_fat_entry_value(base + offset, num);
}

static int dir_slice(char *path) {
    int i = 0;
    while ('/' != path[i] && '\0' != path[i] && ' ' != path[i])
        i += 1;
    return i;
}

static void *logic_sector(struct Fat12 *image, size_t logic) {
    size_t offset = logic * image->header->byte_per_sec;
    return image->logical_base + offset;
}

static struct DirEntry *find_node(struct Fat12 *image, struct DirEntry *base, char *path) {
    int i=0;
    struct DirEntry *ptr = base;
    int slice = dir_slice(path);
    while (ptr->filename[0] != '\0') {
        if (slice != dir_slice(ptr->filename) ||
            strncmp(ptr->filename, path, slice) != 0) {
            ptr += 1;
            continue;
        }
        path += slice;
        if (path[0] == '\0')
            return ptr;
        else if (ptr->attr & DIR_SUBDIRECTORY) {
            size_t num = ptr->first_logical_cluster;
            struct DirEntry *subroot = logic_sector(image, num);
            return find_node(image, subroot, path + 1);
        }
    }
    return NULL;
}

static bool is_end_char(char c) {
    return c == ' ' ||
           c == '\0' ||
           c == '\n';
}

static bool is_empty(char str[], int size) {
    for (int i=0; i<size; i++)
        if (str[i] != ' ')
            return false;
    return true;
}

static void print_string(char *base, int max) {
    int i = 0;
    while (i < max && !is_end_char(base[i])) {
        printf("%c", base[i]);
        i += 1;
    }
}

static void print_file_name(char name[], char ext[]) {
    print_string(name, 8);
    if (!is_empty(ext, 3)) {
        printf(".");
        print_string(ext, 3);
    }
}

static void print_node_attr(uint8_t val) {
    const struct {
        uint8_t val;
        char *name;
    } attrs[] = {
        { DIR_READ_ONLY, "READONLY" },
        { DIR_HIDDEN, "HIDDEN" },
        { DIR_SYSTEM,  "SYSTEM" },
        { DIR_Volume_LABEL,  "VOLUME_LABEL" },
        { DIR_SUBDIRECTORY,  "SUBDIRECTORY" },
        { DIR_ARCHIVE ,  "ARCHIVE " },
    };
    for (int i=0; i<ARR_SZ(attrs); i++) {
        if (val & attrs[i].val)
            printf("%s ", attrs[i].name);
    }
}

static void print_node(struct DirEntry *node) {
    printf("Name: ");
    print_file_name(node->filename, node->ext);
    puts("");

    printf("Attribute: ");
    print_node_attr(node->attr);
    puts("");

    printf("First logical cluster: 0x%"PRIx16"\n", node->first_logical_cluster);
    printf("File size(in byte): %"PRIu32"\n", node->file_size);
}

static void to_upper(char *str) {
    char *ptr = str;
    while ('\0' != *ptr) {
        if (*ptr >= 'a' && *ptr <= 'z')
            *ptr &= ~(1 << 5);
        ptr += 1;
    }
}

void print_inode(struct Fat12 *image, char *path) {
    to_upper(path);
    struct DirEntry *root = image->root;
    struct DirEntry *target = find_node(image, root, path);
    if (!target)
        printf("%s not found\n", path);
    else
        print_node(target);
}
