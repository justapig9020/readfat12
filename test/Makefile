CC = gcc
LD = ld
OBJCP = objcopy
TAR = fat12
SOUR = $(TAR).S
IMG = $(TAR).img
LDFILE = script.ld

all: $(IMG)

$(TAR).o: $(SOUR)
	$(CC) -c $(SOUR) -o $@

$(TAR).elf: $(TAR).o
	$(LD) $< -o $@ -T $(LDFILE)

$(TAR).bin: $(TAR).elf
	$(OBJCP) -R .pdr -R .comment -R .note -S -O binary $< $@

$(IMG): $(TAR).bin
	dd if=$< of=$(IMG) bs=512 count=1
	dd if=/dev/zero of=$(IMG) skip=1 seek=1 bs=512 count=2879

clean:
	rm -rf *.o *.elf *.bin

cleanall:
	rm -rf *.o *.elf *.bin *.img
