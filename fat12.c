#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define match(s1, s2) (strncmp(s1, s2, strlen(s2)) == 0)

#include "fat12.h"

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
    if (!fs)
        goto fail;

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
        goto release;

    struct stat sb;
    if (fstat(fd, &sb) == -1)
        goto close;
    fs->size = sb.st_size;   
    uint8_t *base = mmap(NULL, fs->size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) 
        goto close;
    close(fd);
    fs->base = base;
    fs->header = (struct Header *)(base + 3);

    if (!match(fs->header->file_sys_type, "FAT12"))
        goto release;

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
