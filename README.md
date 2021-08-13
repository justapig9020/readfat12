# readfat12
A tool to interpret fat12 format image

## Usage
```
./readfat <Command> <Image name> <Argument>
```

## Quick test
```
cd test
make
```
Then  you can get a fat12 image file `fat12.img`.

Mount it and do some operation to it.

After that use `readfat` to observe those modification.

## TODO
Parse FAT to link multi-cluster file
