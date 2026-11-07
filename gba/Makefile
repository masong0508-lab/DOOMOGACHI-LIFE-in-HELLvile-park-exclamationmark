# gba/Makefile - DOOMOGACHI LIFE Game Boy Advance ROM (owner: gba).
#
#   make -C gba               build/doomogachi.gba        (the game)
#   make -C gba engine-test   build/doomogachi_et.gba     (ENGINE_TEST=1:
#                             boots straight into the 3D showcase map)
#   make -C gba harness       build/harness  (libmgba headless runner)
#   make -C gba test          both ROMs + harness + smoke scripts
#   make -C gba test-speed    the speed tour: fps gates + table of every
#                             target scene (SPEED_BASE=LOG: + frame parity)
#   make -C gba test-life     the life tour (M0): the free-play screens' fps,
#                             work ms and press-to-picture latency
#   make -C gba MATURE_PACK=pack.dmpk
#                             build/doomogachi_mature.gba (E8: the mature ROM
#                             variant: the game + a DMPK pack for port gba
#                             linked into the ROM, gba/src/packrom.c)
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
#                mixer, flash routines); r3d.iwram.c is the IWRAM code
#                overlay's 3D group instead (gba.ld, src/iwram_ovl.c)
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
# E8: MATURE_PACK=/path/pack.dmpk bakes a content pack into the ROM (the mature variant). The pack
# is validated first (mpack.py: port gba, 4 MB default cap, stills raw 240x160). Not for the
# engine-test ROM.
MATURE_PACK ?=
MPACK       := $(ROOT)/tools/mpack/mpack.py
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
ifneq ($(MATURE_PACK),)
VARIANT := mature
ROM     := $(GBA)/build/doomogachi_mature.gba
PACK_ABS := $(abspath $(MATURE_PACK))
else
VARIANT := game
ROM     := $(GBA)/build/doomogachi.gba
endif
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
ifneq ($(MATURE_PACK),)
ifeq ($(ENGINE_TEST),1)
$(error MATURE_PACK is for the game ROM, not the engine test)
endif
DEFS    += -DDOOMO_PACK_ROM=1 '-DDOOMO_PACK_INCBIN="$(PACK_ABS)"'
endif
# debug cutscene viewer (hold SELECT at power-on); `make CUTVIEW=0` compiles it out
CUTVIEW ?= 1
ifeq ($(CUTVIEW),1)
DEFS    += -DDOOMO_CUTSCENE_VIEWER=1
endif
# G5: `make TILES=0` builds the overworld in Mode 4 only (no tile mode):
# the reference for frame parity checks of the tile mode
TILES ?= 1
ifeq ($(TILES),0)
DEFS    += -DGBA_TILES=0
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

.PHONY: all rom engine-test engine-test-nocore harness test test-et test-game test-save test-coresave test-home test-ow test-lang test-speed test-life profile-title profile-pause profile-invaded profile-all profile-sung clean sizes
.DEFAULT_GOAL := all

# Cartridge size (user decision 2026-11-01): the game ROM is a full 256 Mbit
# (32 MB, the GBA maximum) cartridge. gbafix pads to exactly this size and fails the build if
# the ROM outgrows it. The engine-test ROM keeps power-of-two padding.
CART_BYTES ?= 33554432
ifeq ($(ENGINE_TEST),1)
ROM_PAD := --pad
else
ROM_PAD := --pad-to $(CART_BYTES)
endif

all: rom
rom: $(ROM)

engine-test:
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile ENGINE_TEST=1 rom

engine-test-nocore:
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile ENGINE_TEST=1 NOCORE=1 rom

$(ROM): $(ELF) $(GBA)/tools/gbafix.py
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) $(GBA)/tools/gbafix.py $@ --title DOOMOGACHI --code BDME --maker DM $(ROM_PAD) || { rm -f $@; exit 1; }
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

# E8: the baked pack is checked before it is linked, and a changed pack rebuilds packrom.o
ifneq ($(MATURE_PACK),)
$(OBJ)/pack.ok: $(PACK_ABS) $(MPACK)
	@mkdir -p $(dir $@)
	@$(PYTHON) $(MPACK) validate $(PACK_ABS) --port gba --gba-default-cap || { echo "gba: $(PACK_ABS) is not a valid GBA pack"; exit 1; }
	@touch $@
$(OBJ)/gba/src/packrom.o: $(OBJ)/pack.ok $(PACK_ABS)
endif

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
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-ow
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile test-lang

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

# --- G2: the overworld sprite blit against the G1 baseline's per-pixel
# blit, 2,400 random cases in the game ROM (gba/src/owtest.c) ---
test-ow: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/owsprite.txt --quiet
# --- end G2 ---

# --- L2a: one text path: the Spanish Options help marquee and the Unlocks
# header are translated on the GBA too (harness expect-text) ---
test-lang: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/lang
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/lang_es.txt \
	    --shots $(SHOTS)/lang --quiet
# --- end L2a ---

# --- G1: the speed tour (docs/GBA.md "Speed pass 4"): every target scene's
# fps (expect-fps floors), screenshots and output-frame hashes, printed as a
# table by tools/speedtable.py. SPEED_BASE=LOG (another ROM's
# build/speed_tour.log, e.g. the merge base's, made with the same script)
# adds its numbers beside and fails unless every scene hashed the same
# ticks with identical frames. SPEED_SCRIPT=FILE plays another tour script
# (a package's, on its merge base), SPEED_GATES=0 only reports the floors.
# Not part of `test`. ---
SPEED_LOG ?= $(BUILD)/speed_tour.log
SPEED_BASE ?=
SPEED_SCRIPT ?= $(GBA)/harness/scripts/speed_tour.txt
SPEED_GATES ?= 1
test-speed: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/speed
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(SPEED_SCRIPT) \
	    $(if $(filter 0,$(SPEED_GATES)),--no-fps-gates) \
	    --shots $(SHOTS)/speed --quiet > $(SPEED_LOG) 2>&1 || rc=1; \
	$(PYTHON) $(GBA)/tools/speedtable.py $(SPEED_LOG) \
	    $(if $(SPEED_BASE),--base $(SPEED_BASE) --parity) || rc=1; \
	echo "test-speed: log $(SPEED_LOG), shots $(SHOTS)/speed"; \
	exit $$rc
# --- end G1 ---

# --- profile-title: where the title screen's time goes (harness/scripts/title_profile.txt):
# drawn fps + CPU work per tick and the hottest functions (harness `profile`, needs the ELF's
# symbols), idle and with the menu open. Reports, never gates. Output: build/title_profile.log ---
TITLE_PROFILE_LOG ?= $(BUILD)/title_profile.log
profile-title: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/title
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/title_profile.txt \
	    --shots $(SHOTS)/title --nm $(BUILD)/doomogachi.nm --no-fps-gates --quiet > $(TITLE_PROFILE_LOG) 2>&1 || rc=1; \
	cat $(TITLE_PROFILE_LOG); \
	echo "profile-title: log $(TITLE_PROFILE_LOG)"; \
	exit $$rc
# --- end profile-title ---

# --- M0: the life tour (docs/GBA.md "Life tour"): the island hub, news,
# island menu, roster, resident and apartments: fps and work ms (expect-fps
# floors) and the latency of the press that opens each one. Not part of
# `test` (like test-speed). ---
LIFE_LOG ?= $(BUILD)/life_speed.log
test-life: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/life
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/life_speed.txt \
	    --shots $(SHOTS)/life --quiet > $(LIFE_LOG) 2>&1 || rc=1; \
	grep -E "harness: (fps|latency)|expected|HANG|FAIL" $(LIFE_LOG); \
	echo "test-life: log $(LIFE_LOG), shots $(SHOTS)/life"; \
	exit $$rc
# --- end M0 ---

sizes: $(ELF)
	$(SIZE) -A $(ELF)

clean:
	rm -rf $(BUILD)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)

# --- profile-invaded: where the invaded Hahaland map's time goes (docs/INVADED_SPEED.md) ---
INVADED_PROFILE_LOG ?= $(BUILD)/invaded_profile.log
profile-invaded: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/invaded
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/invaded_profile.txt \
	    --shots $(SHOTS)/invaded --nm $(BUILD)/doomogachi.nm --no-fps-gates --worklog --quiet > $(INVADED_PROFILE_LOG) 2>&1 || rc=1; \
	cat $(INVADED_PROFILE_LOG); \
	exit $$rc

# --- profile-pause: the overworld pause panel vs the visit under it ---
PAUSE_PROFILE_LOG ?= $(BUILD)/pause_profile.log
profile-pause: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/pause
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/pause_profile.txt \
	    --shots $(SHOTS)/pause --nm $(BUILD)/doomogachi.nm --no-fps-gates --quiet > $(PAUSE_PROFILE_LOG) 2>&1 || rc=1; \
	cat $(PAUSE_PROFILE_LOG); \
	exit $$rc

# --- profile-all: the speed tour with a PC profile of every scene (the road to 30 fps everywhere).
# The tour script with its vhash-seq lines dropped and `profile N` after every expect-fps; reports,
# never gates. Output: build/profile_all.log and a ranked markdown table (tools/profile_report.py) ---
PROFILE_ALL_LOG ?= $(BUILD)/profile_all.log
PROFILE_ALL_N ?= 8000000
profile-all: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/profile_all
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	sed -e '/^vhash-seq/d' -e '/^expect-fps/a profile $(PROFILE_ALL_N)' $(SPEED_SCRIPT) > $(BUILD)/profile_all.txt
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(BUILD)/profile_all.txt \
	    --shots $(SHOTS)/profile_all --nm $(BUILD)/doomogachi.nm --no-fps-gates --quiet > $(PROFILE_ALL_LOG) 2>&1 || rc=1; \
	$(PYTHON) $(GBA)/tools/profile_report.py $(PROFILE_ALL_LOG); \
	echo "profile-all: log $(PROFILE_ALL_LOG)"; \
	exit $$rc
# --- end profile-all ---

# --- profile-studio: where the Music Studio's time goes (MS-P0, docs/MUSIC_STUDIO_ROADMAP.md):
# every tab and the PERFORM show, fps + work per tick and the hottest functions, plus the
# slow-frame report (--worklog). Reports, never gates. Output: build/studio_profile.log ---
STUDIO_PROFILE_LOG ?= $(BUILD)/studio_profile.log
profile-studio: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	@mkdir -p $(SHOTS)/studio
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	@rc=0; \
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/studio_profile.txt \
	    --shots $(SHOTS)/studio --nm $(BUILD)/doomogachi.nm --no-fps-gates --worklog --quiet > $(STUDIO_PROFILE_LOG) 2>&1 || rc=1; \
	cat $(STUDIO_PROFILE_LOG); \
	exit $$rc
# --- end profile-studio ---
# --- profile-sung: the sung PERFORM window (gba/harness/scripts/studio_sung.txt, 330 ticks after the curtain) as a
# per-tick work log, and the slowest ticks in it (tools/worklog_spikes.py): which tick is the 50.8 ms one ---
SUNG_LOG ?= $(BUILD)/sung.log
profile-sung: harness
	@$(MAKE) --no-print-directory -f $(GBA)/Makefile rom ENGINE_TEST=0
	$(PREFIX)nm -n $(BUILD)/doomogachi.elf > $(BUILD)/doomogachi.nm
	@rc=0; \\
	$(HARNESS) $(BUILD)/doomogachi.gba --script $(GBA)/harness/scripts/studio_sung.txt \\
	    --nm $(BUILD)/doomogachi.nm --no-fps-gates --worklog --quiet > $(SUNG_LOG) 2>&1 || rc=1; \\
	python3 $(GBA)/../tools/worklog_median.py $(SUNG_LOG) || rc=1; \\
	python3 $(GBA)/../tools/worklog_spikes.py $(SUNG_LOG) 14 || rc=1; \\
	exit $$rc
# --- end profile-sung ---
