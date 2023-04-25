UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
CC=gcc-12
endif

CFLAGS = -std=c18 -Wall -Wextra -O0 -g3 -I.
main : opcodes.h

.PHONY : clean
clean :
	rm main
