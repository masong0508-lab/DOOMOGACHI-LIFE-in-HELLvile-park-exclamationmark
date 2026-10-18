# gba/Makefile - DOOMOGACHI LIFE Game Boy Advance ROM (owner: gba).
#
#   make -C gba               build/doomogachi.gba        (the game)
#   make -C gba engine-test   build/doomogachi_et.gba     (ENGINE_TEST=1:
#                             boots straight into the 3D showcase map)
#   make -C gba harness       build/harness  (libmgba headless runner)
#   make -C gba test          both ROMs + harness + smoke scripts
#   make -C gba clean
#
# Toolchain: arm-none-eabi-gcc + newlib from apt (no devkitPro). Our own
# crt0.s, gba.ld and tools/gbafix.py. Every core source listed by
# core/sources.mk is compiled and the objects are linked directly (never
# through an archive) so the core's weak stubs are overridden.
#
# Code generation:
#   *.c          THUMB, ROM (-mthumb -O2)
#   *.iwram.c    ARM, copied to IWRAM at boot (renderer inner loops, IRQ,
#                mixer, flash routines)
#   IWRAM_CODE   single ARM functions inside THUMB files (gba_hw.h)

GBA  := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
ROOT := $(abspath $(GBA)/..)
DOOMO_CORE := $(ROOT)/core
include $(ROOT)/core/sources.mk

PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size
PYTHON  ?= python3
HOSTCC  ?= gcc

ENGINE_TEST ?= 0
# NOCORE=1 (engine test only): link just the stable core basics (palette,
# font, fixmath) plus enginetest/standalone/ stand-ins for art and maps,
# so the renderer can be developed while the core is still landing.
NOCORE ?= 0
ifeq ($(ENGINE_TEST),1)
ifeq ($(NOCORE),1)
VARIANT := et-nocore
ROM     := $(GBA)/build/doomogachi_et_nocore.gba
else
VARIANT := et
ROM     := $(GBA)/build/doomogachi_et.gba
endif
else
VARIANT := game
ROM     := $(GBA)/build/doomogachi.gba
endif

BUILD := $(GBA)/build
OBJ   := $(BUILD)/obj-$(VARIANT)
ELF   := $(ROM:.gba=.elf)
MAP   := $(ROM:.gba=.map)

ARCH    := -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb-interwork
COMMON  := -std=c99 -O2 -g -Wall -Wextra -ffunction-sections -fdata-sections \
           -fno-strict-aliasing -fomit-frame-pointer -MMD -MP \
           -I$(CORE_INC) -I$(GBA)/src -I$(GBA)/enginetest
DEFS    := -DDOOMO_PLATFORM_GBA=1
ifeq ($(ENGINE_TEST),1)
DEFS    += -DENGINE_TEST=1
endif
# debug cutscene viewer (hold SELECT at power-on); `make CUTVIEW=0` compiles it out
CUTVIEW ?= 1
ifeq ($(CUTVIEW),1)
DEFS    += -DDOOMO_CUTSCENE_VIEWER=1
endif
GBA_CFLAGS  := $(ARCH) $(COMMON) $(DEFS) -Werror
# Core code must also be warning-free (ARCHITECTURE.md), but a warning in
# a module that is still landing must not stop the ROM from building.
CORE_CFLAGS := $(ARCH) $(COMMON) $(DEFS)
ASFLAGS     := $(ARCH) -g -x assembler-with-cpp

LDFLAGS := $(ARCH) -mthumb -nostartfiles -T $(GBA)/gba.ld -Wl,--gc-sections \
           -Wl,-Map,$(MAP) -Wl,--no-warn-rwx-segments
LIBS    := -lc -lgcc

# ---- sources -----------------------------------------------------------------
GBA_SRCS  := $(sort $(wildcard $(GBA)/src/*.c))
GBA_ASMS  := $(sort $(wildcard $(GBA)/src/*.s))
ET_SRCS   :=
ifeq ($(ENGINE_TEST),1)
ET_SRCS   := $(sort $(wildcard $(GBA)/enginetest/*.c))
ifeq ($(NOCORE),1)
ET_SRCS   += $(sort $(wildcard $(GBA)/enginetest/standalone/*.c))
CORE_SRCS := $(wildcard $(addprefix $(DOOMO_CORE)/src/,palette.c font.c fixmath.c fixmath_table.c))
endif
endif
# generated per variant (no races between the ROM builds)
GEN_TABLES := $(OBJ)/gen/gba_tables.c

# object naming: gba/src/x.c -> OBJ/gba/x.o, core/src/y.c -> OBJ/core/src/y.o
gba_obj  = $(patsubst $(GBA)/%.c,$(OBJ)/gba/%.o,$(1))
core_obj = $(patsubst $(DOOMO_CORE)/%.c,$(OBJ)/core/%.o,$(1))

GBA_OBJS  := $(call gba_obj,$(GBA_SRCS) $(ET_SRCS)) \
             $(patsubst $(GBA)/%.s,$(OBJ)/gba/%.o,$(GBA_ASMS)) \
             $(OBJ)/gen/gba_tables.o
CORE_OBJS := $(call core_obj,$(CORE_SRCS))

# gba objects first: the engine test's weak fallbacks (enginetest/) must win
# over the core's weak stubs, and the core's strong definitions win over both.
ALL_OBJS  := $(GBA_OBJS) $(CORE_OBJS)

.PHONY: all rom engine-test engine-test-nocore harness test test-et test-game test-save test-coresave test-home clean sizes
.DEFAULT_GOAL := all

all: rom
rom: $(ROM)

engine-test:
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile ENGINE_TEST=1 rom

engine-test-nocore:
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile ENGINE_TEST=1 NOCORE=1 rom

$(ROM): $(ELF) $(GBA)/tools/gbafix.py
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) $(GBA)/tools/gbafix.py $@ --title DOOMOGACHI --code BDME --maker DM --pad
	@$(PYTHON) $(GBA)/tools/memreport.py $(MAP) $(ELF)

$(ELF): $(ALL_OBJS) $(GBA)/gba.ld
	$(CC) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LIBS)

# ---- compile rules -----------------------------------------------------------------
$(OBJ)/gba/%.iwram.o: $(GBA)/%.iwram.c
	@mkdir -p $(dir $@)
	$(CC) $(GBA_CFLAGS) -marm -c $< -o $@

$(OBJ)/gba/%.o: $(GBA)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(GBA_CFLAGS) -mthumb -c $< -o $@

$(OBJ)/gba/%.o: $(GBA)/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJ)/core/%.o: $(DOOMO_CORE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -mthumb -c $< -o $@

$(GEN_TABLES): $(GBA)/tools/mktables.py
	@mkdir -p $(dir $@)
	$(PYTHON) $< $@

$(OBJ)/gen/gba_tables.o: $(GEN_TABLES)
	@mkdir -p $(dir $@)
	$(CC) $(GBA_CFLAGS) -mthumb -c $< -o $@

# ---- headless harness (host, libmgba) ------------------------------------------
HARNESS := $(BUILD)/harness
harness: $(HARNESS)
$(HARNESS): $(GBA)/harness/harness.c $(GBA)/harness/png.c $(GBA)/harness/png.h
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=gnu99 -O2 -g -Wall -Wextra -o $@ $(GBA)/harness/harness.c \
	    $(GBA)/harness/png.c -lmgba

# ---- tests ---------------------------------------------------------------------
SHOTS ?= $(BUILD)/shots
# one after the other: test-et and test-save rebuild the same engine-test
# objects, so they must not run in parallel (each still builds with -j)
test:
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-et
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-save
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-game
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-coresave
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-story
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-home

test-et: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile engine-test
	@mkdir -p $(SHOTS)/et
	$(HARNESS) $(BUILD)/doomogachi_et.gba --script $(GBA)/harness/scripts/enginetest.txt \
	    --shots $(SHOTS)/et

test-save: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile engine-test
	@mkdir -p $(SHOTS)/save && rm -f $(BUILD)/savetest.sav
	$(HARNESS) $(BUILD)/doomogachi_et.gba --script $(GBA)/harness/scripts/save1.txt \
	    --shots $(SHOTS)/save --save $(BUILD)/savetest.sav
	$(HARNESS) $(BUILD)/doomogachi_et.gba --script $(GBA)/harness/scripts/save2.txt \
	    --shots $(SHOTS)/save --save $(BUILD)/savetest.sav

test-game: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/game
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/smoke.txt \
	    --shots $(SHOTS)/game

# the core's streamed save_game / load_game on flash, across two runs
test-coresave: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@rm -f $(BUILD)/coresave.sav
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/coresave1.txt \
	    --save $(BUILD)/coresave.sav --quiet
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/coresave2.txt \
	    --save $(BUILD)/coresave.sav --quiet

# --- S2 (for H2, SPEC sec.12): the story's scripts, every
# harness/scripts/story_*.txt (Ch1: P0, Ep 1.1, Ep 1.6, cutscene fps) ---
test-story: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/story
	@for s in $(GBA)/harness/scripts/story_*.txt; do \
	    echo "test-story: $$s"; \
	    $(HARNESS) $(BUILD)/doomogachi.gba --script $$s --shots $(SHOTS)/story --quiet || exit 1; \
	done
# --- end S2 ---

# --- H2 (SPEC sec.7 H2, C2): the dream house renderer, gba/src/room.c:
# parity with the room_ref golden on every path, then SCENE_HOME fps ---
test-home: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/home
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/home.txt \
	    --shots $(SHOTS)/home --golden $(ROOT)/core/tests/golden/room_ref.txt \
	    --nm $(BUILD)/doomogachi.nm --quiet
# --- end H2 ---

sizes: $(ELF)
	$(SIZE) -A $(ELF)

clean:
	rm -rf $(BUILD)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
