CC = gcc
CFLAGS = -O2 -Wall -Wextra

export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

CPU_Z80_DIR = ../cpu-z80

SRC = zexall_runner.c

zexall_z80.exe: $(SRC) $(CPU_Z80_DIR)/cpu/z80.c
	$(CC) $(CFLAGS) -I $(CPU_Z80_DIR) -o $@ $(SRC) $(CPU_Z80_DIR)/cpu/z80.c

all: zexall_z80.exe

# ZEXDOC (rychly test, jen dokumentovane instrukce)
zexdoc: all
	@./zexall_z80.exe zexdoc.com

# ZEXALL (kompletni test, vcetne nedokumentovanych)
zexall: all
	@./zexall_z80.exe zexall.com

clean:
	rm -f *.exe

.PHONY: all zexdoc zexall clean
