# dc/tests/audio/Makefile - host tests of the Dreamcast sound mixer (DC5;
# owner: dreamcast). Host gcc only: no KallistiOS, no emulator.
#
#   make -C dc/tests/audio test      test_audio (the mixer's units: chunking
#                                    and gaps give the same content stream,
#                                    pitch, samples, pan, priority, the queue),
#                                    then run_compare.sh: three runner
#                                    scripts on dcsnd_runner, the Dreamcast
#                                    mixer's WAV against musrender's
#                                    (compare.py: envelope and pitch)
#   make -C dc/tests/audio unit      the unit tests only
#   make -C dc/tests/audio SAN=1 BUILD=../../build/host-audio-san test
#                                    the same under ASan/UBSan
#
# dcsnd_runner is the native runner (core/native) whose plat_tone,
# plat_sample and plat_end_frame also drive dc/src's mixer (sndtee.c); the
# core's objects come from core/build (make -C core lib).

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
BUILD ?= $(ROOT)/dc/build/host-audio
override BUILD := $(abspath $(BUILD))
CC := gcc
OPT ?= -O2
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror $(OPT) -g -MMD -MP -D_POSIX_C_SOURCE=200809L \
          -I$(CORE_INC) -I$(CORE)/native -I$(ROOT)/dc/src -I$(HERE)

CORE_OBJS   := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(CORE_SRCS))
NATIVE_ALL  := $(sort $(wildcard $(CORE)/native/*.c))
NATIVE_OWN  := $(CORE)/native/plat_native.c $(CORE)/native/runner_main.c
NATIVE_KEEP := $(filter-out $(NATIVE_OWN),$(NATIVE_ALL))
NATIVE_OBJS := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(NATIVE_KEEP))

SND_OBJS := $(BUILD)/mixer.o $(BUILD)/sfx.o
TEE_OBJS := $(BUILD)/sndtee.o $(BUILD)/plat_native_ref.o $(BUILD)/runner_main.o

# the native sound calls become the reference (ref_*); sndtee.c defines plat_*
REN_NAT := -Dplat_tone=ref_tone -Dplat_sample=ref_sample -Dplat_end_frame=ref_end_frame

.PHONY: all test unit compare corelib clean
all: $(BUILD)/test_audio $(BUILD)/dcsnd_runner

# test_audio links the native platform as it is (plat_native.o), the
# runner its renamed copy: both are built here, never assumed
NATIVE_PLAT := $(CORE_BUILD)/obj/native/plat_native.o
corelib:
	@$(MAKE) --no-print-directory -C $(CORE) lib $(NATIVE_OBJS) $(NATIVE_PLAT) BUILD=$(CORE_BUILD) SAN=$(SAN)

$(CORE_OBJS) $(NATIVE_OBJS) $(NATIVE_PLAT): corelib

$(BUILD)/%.o: $(ROOT)/dc/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sndtee.o: $(HERE)/sndtee.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_audio.o: $(HERE)/test_audio.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/plat_native_ref.o: $(CORE)/native/plat_native.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REN_NAT) -c $< -o $@

$(BUILD)/runner_main.o: $(CORE)/native/runner_main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/dcsnd_runner: corelib $(SND_OBJS) $(TEE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(SND_OBJS) $(TEE_OBJS) -lm

# the unit test links the native platform as it is (plat_native.c's own
# plat_tone etc. are never called; the mixer is driven directly)
$(BUILD)/test_audio: corelib $(SND_OBJS) $(BUILD)/test_audio.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(NATIVE_PLAT) \
	    $(SND_OBJS) $(BUILD)/test_audio.o -lm

unit: $(BUILD)/test_audio
	$(BUILD)/test_audio

compare: $(BUILD)/dcsnd_runner
	sh $(HERE)/run_compare.sh $(BUILD)/dcsnd_runner $(CORE)/tests/scripts $(BUILD)/wav

test: unit compare

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
