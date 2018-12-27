.PHONY: all clean
CC=gcc
FLAG=-g -Wall
all: named_pipe
%: %.c
	$(CC) $(FLAG) -o $@ $^

clean:
	rm named_pipe
