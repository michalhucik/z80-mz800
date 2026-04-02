CC = gcc
CFLAGS = -O2 -Wall -Wextra

export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

MULTI_DIR = ../cpu-z80-multi-v0.2

SRC = zexall_runner.c

zexall_multi.exe: $(SRC) $(MULTI_DIR)/cpu/z80.c
	$(CC) $(CFLAGS) -I $(MULTI_DIR) -o $@ $(SRC) $(MULTI_DIR)/cpu/z80.c

all: zexall_multi.exe

# ZEXDOC (rychly test, jen dokumentovane instrukce)
zexdoc: all
	@./zexall_multi.exe zexdoc.com

# ZEXALL (kompletni test, vcetne nedokumentovanych)
zexall: all
	@./zexall_multi.exe zexall.com

clean:
	rm -f *.exe

.PHONY: all zexdoc zexall clean
