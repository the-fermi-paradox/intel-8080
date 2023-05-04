CC=clang

CFLAGS  := -std=c18 -Wall -Wextra -Werror -O3 -g3 -I.
LDLIBS  :=
OBJECTS := cpu.o io.o

main : main.c $(OBJECTS)
	$(CC) $(CFLAGS) main.c -o main $(OBJECTS) $(LDLIBS)
test : test.c $(OBJECTS)
	$(CC) $(CFLAGS) test.c -o test $(OBJECTS) $(LDLIBS)

cpu.o  : opcodes.h Makefile
io.o   : Makefile

.PHONY : clean
clean :
	rm -f main test $(OBJECTS)
