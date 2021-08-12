all: readfat

readfat: fat12.o main.o
	gcc -o readfat fat12.o main.o

%.o: %.c
	gcc -c $< -o $@

clean:
	rm -f *.o readfat
