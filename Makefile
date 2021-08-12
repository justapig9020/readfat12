Header = tools.h fat12.h

all: readfat

readfat: fat12.o main.o
	gcc -o readfat fat12.o main.o

%.o: %.c $(Header)
	gcc -c $< -o $@

clean:
	rm -f *.o readfat
