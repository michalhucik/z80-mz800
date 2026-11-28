CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-variable

# MSYS2/MinGW fix - GCC potrebuje zapisovat temp soubory
export TMPDIR = /tmp
export TMP = /tmp
export TEMP = /tmp

CPU_Z80_DIR = ../cpu-z80

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
            test_timing.c \
            test_cpu_ctrl_event.c \
            test_call_ret_hooks.c \
            test_halt_pc.c \
            test_ei_delay.c \
            test_r_register.c \
            test_step_cycles.c \
            test_push_order.c \
            test_reset_state.c \
            test_iff_change.c \
            test_block_repeat.c \
            test_read16_order.c \
            test_im2_vector.c

# Emulator zdrojovy soubor
Z80_SRC = $(CPU_Z80_DIR)/cpu/z80.c

all: test_z80.exe test_fastpath.exe

test_z80.exe: $(TEST_SRCS) test_framework.h $(Z80_SRC)
	$(CC) $(CFLAGS) \
		-I $(CPU_Z80_DIR) \
		-o $@ $(TEST_SRCS) $(Z80_SRC)

# Samostatný test RAM fast-path (jádro přeložené s -DCPU_Z80_RAM_FASTPATH)
test_fastpath.exe: test_fastpath_latch.c test_framework.h $(Z80_SRC)
	$(CC) $(CFLAGS) -DCPU_Z80_RAM_FASTPATH \
		-I $(CPU_Z80_DIR) \
		-o $@ test_fastpath_latch.c $(Z80_SRC)

run: test_z80.exe test_fastpath.exe
	@./test_z80.exe
	@./test_fastpath.exe

clean:
	rm -f test_z80.exe test_fastpath.exe

.PHONY: all run clean
