#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "fat12.h"
#include "tools.h"

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
