.intel_syntax noprefix

.code16

# Floppy header of FAT12 
    jmp LABEL_START
    nop
BS_OEMName:         .ascii  "jup9020 "      # OEM String, 8 bytes required
BPB_BytsPerSec:     .2byte  512             # Bytes per sector
BPB_SecPerCluster:  .byte   1               # Sector per cluster
BPB_ResvSecCnt:     .2byte  1               # Reserved sector count
BPB_NumFATs:        .byte   2               # Number of FATs
BPB_RootEntCnt:     .2byte  224             # Root entries count
BPB_TotSec16:       .2byte  2880            # Total sector number
BPB_Media:          .byte   0xf0            # Media descriptor
BPB_FATSz16:        .2byte  9               # FAT size (sectors)
BPB_SecPerTrk:      .2byte  18              # Sector per track
BPB_Numheads:       .2byte  2               # Number of magnetic heads
BPB_HiddSec:        .4byte  0               # Number of hidden sectors
BPB_TotSec32:       .4byte  0               # If Totsec 16 equal 0, this works
BS_DrvNum:          .byte   0               # Driver number of imterrupt 13
Bs_Reserved1:       .byte   0               # Reserved
BS_BootSig:         .byte   0x29            # Boot signal
BS_VolId:           .4byte  0               # Volume ID
BS_VolLab:          .ascii  "jOs 0.01   "   # Volume label, 11 bytes required
BS_FileSysType:     .ascii  "FAT12   "      # File system type, 8 bytes required

# Initial registers
LABEL_START:
