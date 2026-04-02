CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-variable

# MSYS2/MinGW fix - GCC potrebuje zapisovat temp soubory
export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

CPU_Z80_DIR = ../cpu-z80-single-v0.2

# Vsechny testovaci zdrojove soubory
TEST_SRCS = test_main.c \
            test_ld.c \
            test_alu.c \
            test_rotate_bit.c \
            test_jump_call.c \
            test_block_io.c \
            test_ix_iy.c \
            test_interrupts.c \
            test_flags_daa.c \
            test_timing.c

# Emulator zdrojovy soubor
Z80_SRC = $(CPU_Z80_DIR)/cpu/z80.c

all: test_z80.exe

test_z80.exe: $(TEST_SRCS) test_framework.h $(Z80_SRC)
	$(CC) $(CFLAGS) \
		-I $(CPU_Z80_DIR) \
		-o $@ $(TEST_SRCS) $(Z80_SRC)

run: test_z80.exe
	@./test_z80.exe

clean:
	rm -f test_z80.exe

.PHONY: all run clean
