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
#                                    the same under ASan/UBSan (any UBSan
#                                    report fails the run)
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
# SAN=1: a UBSan report stops the run (this directory's objects are built
# with -fno-sanitize-recover; UBSAN_OPTIONS makes the core's objects, built
# by core/Makefile without it, stop as well), so it fails the test instead
# of scrolling past in the log
RUN_ENV :=
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
RUN_ENV := UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
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

.PHONY: all test unit compare tier corelib clean
all: $(BUILD)/test_audio $(BUILD)/dcsnd_runner $(BUILD)/tier_irq $(BUILD)/tier_bridge

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

# DC5b: test_tier twice (see its header): the IRQ tier with musvoice.c (its
# three plat_ calls renamed so test_tier can tap them), and the legacy bridge
TIER_REN := -Dplat_voice=dc_plat_voice -Dplat_audio_bus=dc_plat_audio_bus -Dplat_audio_fx=dc_plat_audio_fx -DDC_TAPE_OUT=dc_music_tape -DDC_BUS_OUT=dc_music_bus
$(BUILD)/tier_mv.o: $(ROOT)/dc/src/musvoice.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TIER_REN) -c $< -o $@
$(BUILD)/tier_irq.o: $(HERE)/test_tier.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DTIER_IRQ -c $< -o $@
$(BUILD)/tier_bridge.o: $(HERE)/test_tier.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/voiceref.o: $(HERE)/voiceref.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/tier_irq: corelib $(BUILD)/plat_native_ref.o $(SND_OBJS) $(BUILD)/tier_mv.o $(BUILD)/tier_irq.o $(BUILD)/voiceref.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(BUILD)/plat_native_ref.o $(SND_OBJS) $(BUILD)/tier_mv.o \
	    $(BUILD)/tier_irq.o $(BUILD)/voiceref.o -lm
$(BUILD)/tier_bridge: corelib $(BUILD)/plat_native_ref.o $(SND_OBJS) $(BUILD)/tier_bridge.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(BUILD)/plat_native_ref.o $(SND_OBJS) $(BUILD)/tier_bridge.o -lm

tier: $(BUILD)/tier_irq $(BUILD)/tier_bridge
	$(RUN_ENV) $(BUILD)/tier_irq unit
	$(RUN_ENV) sh $(HERE)/run_tier.sh $(BUILD)/tier_irq $(BUILD)/tier_bridge $(BUILD)/tier

unit: $(BUILD)/test_audio
	$(RUN_ENV) $(BUILD)/test_audio

compare: $(BUILD)/dcsnd_runner
	$(RUN_ENV) sh $(HERE)/run_compare.sh $(BUILD)/dcsnd_runner $(CORE)/tests/scripts $(BUILD)/wav

test: unit tier compare

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
