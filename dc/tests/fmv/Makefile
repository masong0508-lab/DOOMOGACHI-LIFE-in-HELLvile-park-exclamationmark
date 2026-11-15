# dc/tests/fmv/Makefile - host test of the boot FMV (DC9C; owner: dreamcast).
# Host gcc, python3 and numpy only: no KallistiOS, no ffmpeg, no emulator.
#
#   make -C dc/tests/fmv test     gen.py builds a synthetic clip with dc/tools/mkfmv.py,
#                                 test_dmv decodes it (dc/src/dmv.c) and checks it
#   make -C dc/tests/fmv SAN=1 BUILD=../../build/host-fmv-san test    under ASan/UBSan

HERE := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
ROOT := $(abspath $(HERE)/../../..)
BUILD ?= $(ROOT)/dc/build/host-fmv
override BUILD := $(abspath $(BUILD))
CC := gcc
OPT ?= -O2
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror $(OPT) -g -I$(ROOT)/dc/src

.PHONY: test clean
test: $(BUILD)/test_dmv
	python3 -I $(HERE)/gen.py $(BUILD)/clip
	$(BUILD)/test_dmv $(BUILD)/clip

$(BUILD)/test_dmv: $(HERE)/test_dmv.c $(ROOT)/dc/src/dmv.c $(ROOT)/dc/src/dmv.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(HERE)/test_dmv.c $(ROOT)/dc/src/dmv.c -lm

clean:
	rm -rf $(BUILD)
