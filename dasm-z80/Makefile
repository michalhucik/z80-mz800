CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -I../ai2-z80/utils
AR = ar

SRCS = z80_dasm.c z80_dasm_tables.c z80_dasm_format.c \
       z80_dasm_symtab.c z80_dasm_analysis.c z80_dasm_compat.c
OBJS = $(SRCS:.c=.o)

all: libdasm_z80.a

libdasm_z80.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c z80_dasm.h z80_dasm_internal.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: libdasm_z80.a ../tests/test_dasm.c
	$(CC) $(CFLAGS) -o test_dasm ../tests/test_dasm.c -L. -ldasm_z80
	./test_dasm

clean:
	rm -f $(OBJS) libdasm_z80.a test_dasm test_dasm.exe

.PHONY: all test clean
