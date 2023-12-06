# Define the compiler
# Define the compiler
CC = gcc

CFLAGS = -Wall -Wextra -g -std=c99
SRC = 

filesys: filesys.c
	$(CC) $(CFLAGS) -c filesys.c -o filesys.o
	$(CC) $(CFLAGS) -o filesys filesys.c

clean:
	rm -f filesys filesys.o
