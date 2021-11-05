#include <stdint.h>
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

#define ROOT_DIR_START 19
#define ROOT_DIR_END 32
#define DATA_START_CLUSTER 31 // first two cluster is not use
#define FIST_DATA_CLUSTER 2
#define TO_PHY(logic) (logic + DATA_START_CLUSTER)

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
    size_t offset = DATA_START_CLUSTER * header->byte_per_sec * header->sec_per_clus;
    return base + offset;
}

static uint32_t logical_to_byte_addr(uint16_t logical, uint32_t byte_per_clu) {
    uint32_t phy_cluster = TO_PHY(logical);
    return phy_cluster * byte_per_clu;
}

static uint32_t byte_per_cluster(struct Fat12 *image) {
    struct Header *h = image->header;
    return h->byte_per_sec * h->sec_per_clus;
}

static void *logic_cluster(struct Fat12 *image, size_t logic) {
    struct Header *h = image->header;
    size_t offset = logic * byte_per_cluster(image);
    return image->logical_base + offset;
}

static void *physical_cluster(struct Fat12 *image, size_t phy) {
    intptr_t base = (intptr_t)image->base;
    return (void *)(base + phy * byte_per_cluster(image));
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

uint16_t get_fat_value(struct Fat12 *image, size_t table, size_t cluster) {
    struct Header *header = image->header;
    if (table >= header->num_of_fat)
        return FAT_UNUSED;

    size_t max = header->byte_per_sec * header->sec_per_fat;
    if (cluster >= max)
        return FAT_UNUSED;

    struct FatTable *base = (struct FatTable *)(((int8_t *)image->table) + (max * table));
    /* Due to fat12 use 12 bit to represent a fat entry, that is hard to form a single
     * entry into structure.
     * Therefore, FatTable form every two entries (3 bytes) into an object.
     */
    size_t offset = cluster / 2;
    size_t num = cluster % 2;
    return get_fat_entry_value(base + offset, num);
}

static struct DirEntry *next_cluster(struct Fat12 *img, void *curr) {
    int byte_per_clu = byte_per_cluster(img);
    intptr_t base = (intptr_t)img->logical_base;
    int offset = (intptr_t)curr - base;
    int clu_num = offset / byte_per_clu;
    if (clu_num < FIST_DATA_CLUSTER) {
        int phy_num = clu_num + DATA_START_CLUSTER;
        if (phy_num > ROOT_DIR_END || phy_num < ROOT_DIR_START)
            return NULL;
        /* Root directory */
        phy_num += 1;
        if (phy_num > ROOT_DIR_END)
            return NULL;
        return physical_cluster(img, phy_num);
    }
    uint16_t next = get_fat_value(img, 0, clu_num);
    if (FAT_IS_LAST(next) ||
        next == FAT_BAD ||
        next == FAT_UNUSED) {
        return NULL;
    } else {
        return logic_cluster(img, next);
    }
}

static bool is_char(char a, char arr[], size_t size) {
    for (int i=0; i<size; i++)
        if (arr[i] == a)
            return true;
    return false;
}

static int dir_slice(char *path) {
    int i = 0;
    char block[] = {
        '/',
        '\0',
        ' ',
        '.',
    };
    while (!is_char(path[i], block, ARR_SZ(block)))
        i += 1;
    return i;
}

static int dir_per_cluster(struct Fat12 *image) {
    struct Header *h = image->header;
    uint16_t byte_per_clus = h->byte_per_sec * h->sec_per_clus;
    return byte_per_clus / sizeof(struct DirEntry);
}

static void to_upper(char *str) {
    char *ptr = str;
    while ('\0' != *ptr) {
        if (*ptr >= 'a' && *ptr <= 'z')
            *ptr &= ~(1 << 5);
        ptr += 1;
    }
}

static struct DirEntry *_find_node(struct Fat12 *image, struct DirEntry *dir, char *path) {
    int i=0;
    struct DirEntry *curr = dir;
    int slice = dir_slice(path);
    int dir_per_clus = dir_per_cluster(image);
    while (1) {
        if (i >= dir_per_clus) {
            curr -= dir_per_clus; // back to base of cluster
            curr = next_cluster(image, curr);
            if (!curr)
                return NULL;
            i = 0;
        }
        if (slice != dir_slice(curr->filename) ||
            strncmp(curr->filename, path, slice) != 0) {
            i += 1;
            curr += 1;
            continue;
        }
        path += slice;
        if (path[0] == '\0') {
            if (curr->ext[0] == ' ')
                /* Target file has no extision. Match! */
                return curr;
            else
                /* Extension not match */
                return NULL;
        } else if (path[0] == '.') {
            /* Check extension */
            path += 1;
            slice = dir_slice(path);
            if (slice == dir_slice(curr->ext) &&
                strncmp(path, curr->ext, slice) == 0)
                return curr;
            else
                return NULL;
        } else if (curr->attr & DIR_SUBDIRECTORY) {
            /* Search subdirectory */
            size_t num = curr->first_logical_cluster;
            struct DirEntry *subroot = logic_cluster(image, num);
            return _find_node(image, subroot, path + 1);
        }
    }
    return NULL;
}

struct DirEntry *find_node(struct Fat12 *image, char *path) {
    to_upper(path);
    struct DirEntry *root = image->root;
    while(match(path, "./")) {
        path += 2;
        if (strlen(path) == 0) {
            return root;
        }
    }
    return _find_node(image, root, path);
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
    if (0 == node->attr)
        return;
    printf("Name: ");
    print_file_name(node->filename, node->ext);
    puts("");

    printf("Attribute: ");
    print_node_attr(node->attr);
    puts("");

    uint16_t flc = node->first_logical_cluster;
    uint32_t byte_addr = logical_to_byte_addr(flc, 512);
    printf("First logical cluster: 0x%"PRIx16" (BYTE: 0x%"PRIx32")\n", flc, byte_addr);
    printf("File size(in byte): %"PRIu32"\n", node->file_size);
}

void print_inode(struct Fat12 *image, char *path) {
    struct DirEntry *target = find_node(image, path);
    if (!target)
        printf("%s not found\n", path);
    else
        print_node(target);
}

static void pad_tab(int n) {
    for (int i=0; i<n; i++)
        printf("|  ");
}

static void _list_inodes(struct Fat12 *image, struct DirEntry *dir, int level) {
    int n = dir_per_cluster(image);
    while (dir) {
        for (int i=0; i<n; i++) {
            struct DirEntry *curr= &dir[i];

            if (curr->attr != 0) {
                pad_tab(level);
                printf("+- ");
                print_file_name(curr->filename, curr->ext);
                puts("");
            }

            if (curr->attr & DIR_SUBDIRECTORY &&
                !match(curr->filename, ".") &&
                !match(curr->filename, "..")) {
                size_t num = curr->first_logical_cluster;
                struct DirEntry *subroot = logic_cluster(image, num);
                _list_inodes(image, subroot, level + 1);
            }
        }
        dir = next_cluster(image, dir);
    }
}

void list_inodes(struct Fat12 *image, struct DirEntry *dir) {
    if (dir == image->root) {
        /* Root directory */
        _list_inodes(image, dir, 0);
        return;
    }
    print_node(dir);
    puts("");
    if (dir->attr & DIR_SUBDIRECTORY) {
        size_t num = dir->first_logical_cluster;
        struct DirEntry *subroot = logic_cluster(image, num);
        _list_inodes(image, subroot, 0);
    }
}
