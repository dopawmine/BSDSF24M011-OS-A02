# Makefile for ls utility - v1.0.0

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c23 -D_DEFAULT_SOURCE
SRC     = src/ls-v1.4.0.c
TARGET  = bin/ls

all: $(TARGET)

$(TARGET): $(SRC) | bin
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

bin:
	mkdir -p bin

clean:
	rm -f $(TARGET)

.PHONY: all clean
