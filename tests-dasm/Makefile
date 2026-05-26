CC = gcc
CFLAGS = -O2 -Wall -Wextra -I../dasm-z80 -I../cpu-z80/utils

# MSYS2/MinGW: bez nastaveneho TEMP padne GCC na zapis tmp do C:\WINDOWS
export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

DASM_DIR = ../dasm-z80
DASM_LIB = $(DASM_DIR)/libdasm_z80.a

TESTS = test_dasm_sym.exe test_motorola_ix.exe

all: $(TESTS)

$(DASM_LIB):
	$(MAKE) -C $(DASM_DIR)

test_dasm_sym.exe: test_dasm_sym.c $(DASM_LIB)
	$(CC) $(CFLAGS) -o $@ test_dasm_sym.c $(DASM_LIB)

test_motorola_ix.exe: test_motorola_ix.c $(DASM_LIB)
	$(CC) $(CFLAGS) -o $@ test_motorola_ix.c $(DASM_LIB)

run: $(TESTS)
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

.PHONY: all run clean
