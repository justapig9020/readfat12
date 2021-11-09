#ifndef _FAT12_
#define _FAT12_

#include <stdint.h>
#include <unistd.h>

char *fat_err;

struct __attribute__((__packed__)) Header {
    char name[8];
    uint16_t byte_per_sec;
    uint8_t sec_per_clus;
    uint16_t reserved_sec;
    uint8_t num_of_fat;
    uint16_t max_num_of_root_dir_entries;
    uint16_t total_sec_cnt;
    uint8_t ignore0;
    uint16_t sec_per_fat;
    uint16_t sec_per_track;
    uint16_t num_of_heads;
    uint32_t ignore1;
    uint32_t total_sec_cnt_for_fat32;
    uint16_t ignore2;
    uint8_t boot_sign;
    uint32_t volume_id;
    uint8_t volume_label[11];
    char file_sys_type[8];
};

struct __attribute__((__packed__)) DirEntry{
    char filename[8];
    char ext[3];
    uint8_t attr;
    uint16_t reserved0;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t ignore_in_fat12;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_logical_cluster;
    uint32_t file_size;
};

struct __attribute__((__packed__)) FatTable {
    uint8_t low;
    uint8_t mid;
    uint8_t top;
};

struct Fat12 {
    uint8_t *base;
    size_t size;
    struct Header *header;
    struct FatTable *table;
    struct DirEntry *root;
    uint8_t *logical_base;
};

struct Fat12 *new_fat12(char *filename);

void free_fat12(struct Fat12 *target);

void print_header(struct Fat12 *image);

uint16_t get_fat_value(struct Fat12 *image, size_t table, size_t sector);

void print_inode(struct Fat12 *image, char *path);

struct DirEntry *find_node(struct Fat12 *image, char *path);

typedef void (*Display_t)(struct Fat12 *img, struct DirEntry *dir, int level);
void list_inodes(struct Fat12 *image, struct DirEntry *root, Display_t display);

void print_file_name(char name[], char ext[]);

void print_node(struct DirEntry *node);

#endif
