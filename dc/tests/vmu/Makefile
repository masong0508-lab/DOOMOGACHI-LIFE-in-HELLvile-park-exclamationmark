# dc/tests/vmu/Makefile - host tests of the Dreamcast controls and VMU saves
# (DC4; owner: dreamcast). Host gcc only: no KallistiOS, no emulator.
#
#   make -C dc/tests/vmu test     test_vmu (the VMS format, the VMU file
#                                 system on a simulated card pulled out at
#                                 every write, the icons, the pad and
#                                 keyboard mapping), then dc/harness/
#                                 vmu_check.py on the card image it wrote
#   make -C dc/tests/vmu SAN=1 BUILD=../../build/host-vmu-san test
#                                 the same under ASan/UBSan
#
# The core's objects (the pet's art, the palette) come from core/build
# (make -C core lib), with the native platform's objects for the plat_*
# calls the core links against.

HERE := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
ROOT := $(abspath $(HERE)/../../..)
CORE := $(ROOT)/core
DOOMO_CORE := $(CORE)
include $(CORE)/sources.mk

ifeq ($(SAN),1)
CORE_BUILD ?= $(CORE)/build-san
else
CORE_BUILD ?= $(CORE)/build
endif
BUILD ?= $(ROOT)/dc/build/host-vmu
override BUILD := $(abspath $(BUILD))
CC := gcc
OPT ?= -O2
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror $(OPT) -g -MMD -MP -D_POSIX_C_SOURCE=200809L \
          -I$(CORE_INC) -I$(ROOT)/dc/src -I$(ROOT)/dc/harness

CORE_OBJS   := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(CORE_SRCS))
NATIVE_PLAT := $(filter-out $(CORE)/native/runner_main.c,$(sort $(wildcard $(CORE)/native/*.c)))
NATIVE_OBJS := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(NATIVE_PLAT))

OWN := $(BUILD)/vms.o $(BUILD)/padmap.o $(BUILD)/save_icon.o $(BUILD)/png.o $(BUILD)/test_vmu.o

.PHONY: all test corelib clean
all: $(BUILD)/test_vmu

corelib:
	@$(MAKE) --no-print-directory -C $(CORE) lib $(NATIVE_OBJS) BUILD=$(CORE_BUILD) SAN=$(SAN)

$(CORE_OBJS) $(NATIVE_OBJS): corelib

$(BUILD)/%.o: $(ROOT)/dc/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/png.o: $(ROOT)/dc/harness/png.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_vmu.o: $(HERE)/test_vmu.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_vmu: corelib $(OWN)
	$(CC) $(LDFLAGS) -o $@ $(OWN) $(CORE_OBJS) $(NATIVE_OBJS) -lm

test: $(BUILD)/test_vmu
	@mkdir -p $(BUILD)/out
	$(BUILD)/test_vmu $(BUILD)/out
	python3 -I $(ROOT)/dc/harness/vmu_check.py $(BUILD)/out/card.bin \
	    --expect DOOMO0.SYS --expect DOOMO1.SYS --expect DOOMOCFG.SYS

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
