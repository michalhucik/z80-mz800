CC = gcc
CFLAGS = -O2 -Wall -Wextra

export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

V01_DIR = ../cpu-z80
MULTI_DIR = ../cpu-z80-multi-v0.2

SRC = zexall_runner.c

# Build vsech variant
zexall_v01.exe: $(SRC) $(V01_DIR)/cpu/z80.c
	$(CC) $(CFLAGS) -DCORE_V01 -I $(V01_DIR) -o $@ $(SRC) $(V01_DIR)/cpu/z80.c

zexall_multi.exe: $(SRC) $(MULTI_DIR)/cpu/z80.c
	$(CC) $(CFLAGS) -I $(MULTI_DIR) -o $@ $(SRC) $(MULTI_DIR)/cpu/z80.c

all: zexall_v01.exe zexall_multi.exe

# ZEXDOC (rychly test, jen dokumentovane instrukce)
zexdoc: all
	@echo "=== cpu-z80 v0.1 ===" && ./zexall_v01.exe zexdoc.com
	@echo "" && echo "=== cpu-z80 multi-v0.2 ===" && ./zexall_multi.exe zexdoc.com

# ZEXALL (kompletni test, vcetne nedokumentovanych)
zexall: all
	@echo "=== cpu-z80 v0.1 ===" && ./zexall_v01.exe zexall.com
	@echo "" && echo "=== cpu-z80 multi-v0.2 ===" && ./zexall_multi.exe zexall.com

clean:
	rm -f *.exe

.PHONY: all zexdoc zexall clean
