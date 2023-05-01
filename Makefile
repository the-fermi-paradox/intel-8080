CC=clang

CFLAGS = -std=c18 -Wall -Wextra -Werror -fsanitize=undefined -O0 -g3 -I.
LDLIBS = -lubsan
OBJECTS = cpu.o io.o

main : main.c $(OBJECTS)
	$(CC) $(CFLAGS) main.c -o main $(OBJECTS) $(LDLIBS)
test : test.c $(OBJECTS)
	$(CC) $(CFLAGS) test.c -o test $(OBJECTS) $(LDLIBS)

cpu.o  : cpu.h opcodes.h
io.o   : io.h

.PHONY : clean
clean :
	rm -f main test $(OBJECTS)
