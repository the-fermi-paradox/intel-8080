CC=clang

CFLAGS = -std=c18 -Wall -Wextra -Werror -fsanitize=undefined -O0 -g3 -I.
LDLIBS = -lubsan
OBJECTS = main.o cpu.o

main : $(OBJECTS)
	$(CC) -o main $(OBJECTS) $(LDLIBS)

main.o : opcodes.h
cpu.o : cpu.h opcodes.h

.PHONY : clean
clean :
	rm main $(OBJECTS)
