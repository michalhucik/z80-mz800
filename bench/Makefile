CC = gcc
CFLAGS_O2 = -O2 -Wall -Wextra
CFLAGS_O3 = -O3 -Wall -Wextra

# MSYS2/MinGW fix
export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

MULTI_DIR = ../cpu-z80-multi-v0.2

# z80ex (pro porovnani) - musi byt stazen do ../tmp/
Z80EX_DIR = ../tmp/z80ex-1.1.21
Z80EX_MZ800_DIR = ../tmp/z80ex-mz800
Z80EX_DEFS = -DZ80EX_API_REVISION=1 -DZ80EX_VERSION_MAJOR=1 -DZ80EX_VERSION_MINOR=21

# ============ cpu-z80 multi-v0.2 ============

bench_multi.exe: bench_cpu_z80_multi.c bench_common.h $(MULTI_DIR)/cpu/z80.c
	$(CC) $(CFLAGS_O2) -I $(MULTI_DIR) -o $@ bench_cpu_z80_multi.c $(MULTI_DIR)/cpu/z80.c

bench_multi_o3.exe: bench_cpu_z80_multi.c bench_common.h $(MULTI_DIR)/cpu/z80.c
	$(CC) $(CFLAGS_O3) -I $(MULTI_DIR) -o $@ bench_cpu_z80_multi.c $(MULTI_DIR)/cpu/z80.c

# ============ z80ex (volitelne) ============

bench_z80ex_official.exe: bench_z80ex.c bench_common.h $(Z80EX_DIR)/z80ex.c
	$(CC) $(CFLAGS_O2) -Wno-unused-function $(Z80EX_DEFS) \
		-DBENCH_LABEL='"z80ex-official"' \
		-I $(Z80EX_DIR) -I $(Z80EX_DIR)/include \
		-o $@ bench_z80ex.c $(Z80EX_DIR)/z80ex.c

bench_z80ex_mz800.exe: bench_z80ex.c bench_common.h $(Z80EX_MZ800_DIR)/z80ex.c
	$(CC) $(CFLAGS_O2) -Wno-unused-function \
		-DBENCH_LABEL='"z80ex-mz800 "' \
		-I $(Z80EX_MZ800_DIR) -I $(Z80EX_MZ800_DIR)/include \
		-o $@ bench_z80ex.c $(Z80EX_MZ800_DIR)/z80ex.c

bench_z80ex_official_o3.exe: bench_z80ex.c bench_common.h $(Z80EX_DIR)/z80ex.c
	$(CC) $(CFLAGS_O3) -Wno-unused-function $(Z80EX_DEFS) \
		-DBENCH_LABEL='"z80ex-official"' \
		-I $(Z80EX_DIR) -I $(Z80EX_DIR)/include \
		-o $@ bench_z80ex.c $(Z80EX_DIR)/z80ex.c

bench_z80ex_mz800_o3.exe: bench_z80ex.c bench_common.h $(Z80EX_MZ800_DIR)/z80ex.c
	$(CC) $(CFLAGS_O3) -Wno-unused-function \
		-DBENCH_LABEL='"z80ex-mz800 "' \
		-I $(Z80EX_MZ800_DIR) -I $(Z80EX_MZ800_DIR)/include \
		-o $@ bench_z80ex.c $(Z80EX_MZ800_DIR)/z80ex.c

# ============ Cile ============

all: bench_multi.exe

run: all
	@echo "========================================"
	@echo "  Benchmark Z80 emulatoru (-O2)"
	@echo "========================================"
	@./bench_multi.exe
	@echo "========================================"

compare: bench_multi.exe bench_multi_o3.exe
	@echo "========================================"
	@echo "  cpu-z80 multi-v0.2"
	@echo "========================================"
	@echo ""
	@echo "--- O2 ---"
	@./bench_multi.exe
	@echo ""
	@echo "--- O3 ---"
	@./bench_multi_o3.exe
	@echo "========================================"

# Plne porovnani vcetne z80ex (vyzaduje ../tmp/z80ex-1.1.21 a ../tmp/z80ex-mz800)
compare-all: compare \
             bench_z80ex_official.exe bench_z80ex_mz800.exe \
             bench_z80ex_official_o3.exe bench_z80ex_mz800_o3.exe
	@echo ""
	@echo "========================================"
	@echo "  z80ex (reference)"
	@echo "========================================"
	@echo ""
	@echo "--- O2 ---"
	@./bench_z80ex_official.exe
	@./bench_z80ex_mz800.exe
	@echo ""
	@echo "--- O3 ---"
	@./bench_z80ex_official_o3.exe
	@./bench_z80ex_mz800_o3.exe
	@echo "========================================"

clean:
	rm -f *.exe

.PHONY: all run compare compare-all clean
