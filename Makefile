# Define the compiler
# Define the compiler
CC = gcc

CFLAGS = -Wall -Wextra -g -std=c99 -D_POSIX_C_SOURCE=200809L
TARGET = filesys
SRC = filesys.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
