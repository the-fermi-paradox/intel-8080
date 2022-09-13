CFLAGS = -std=c18 -Wall -Wconversion -Wsign-conversion -Wextra -O0 -g3 -I.
main : opcodes.h

.PHONY : clean
clean :
	rm main
