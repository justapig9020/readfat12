#ifndef _FAT_TABLE_H_
#define _FAT_TABLE_H_

#define FAT_UNUSED 0x000
#define FAT_BAD 0xff7
#define FAT_IS_RESERVED(val) (((val & FAT_BAD) == val) && val != FAT_BAD)
#define FAT_LAST 0xff8
#define FAT_IS_LAST(val) ((val & FAT_LAST) == FAT_LAST)

#endif
