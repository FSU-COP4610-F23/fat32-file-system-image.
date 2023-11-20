# Define the compiler
# Define the compiler
CC = gcc

CFLAGS = -Wall -Wextra -g -std=c99
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
