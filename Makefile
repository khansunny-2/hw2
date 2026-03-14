CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -ggdb

.PHONY: all clean

all: fpmul

fpmul: fpmul.c
	$(CC) $(CFLAGS) fpmul.c -o fpmul