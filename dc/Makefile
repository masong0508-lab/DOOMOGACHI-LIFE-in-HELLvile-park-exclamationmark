# dc/Makefile - BOREagotchi LIFE! for the Dreamcast (owner: dreamcast).
#
#   make -C dc            build/doomogachi.elf (+ .map, size report)
#   make -C dc disc       build/doomogachi.cdi (self-booting disc image), plus
#                         build/1ST_READ.BIN and build/IP.BIN
#   make -C dc all-disc   both of the above
#   make -C dc test       the ELF + disc, the symbol checks (check-elf), and
#                         the Flycast harness scripts when dc/harness exists
#   make -C dc check-elf  checks on the linked ELF only (no emulator)
#   make -C dc host-test  the 2D layer on the host against the native runner
#                         (dc/tests/gfx2d), DC4's VMU file system, icons
#                         and controls (dc/tests/vmu) and DC5's sound mixer
#                         against musrender (dc/tests/audio); `make
#                         dc-host-test` needs no KOS
#   make -C dc vmu-test   DC4 on Flycast: saves over a power cycle, a full VMU,
#                         no VMU, the first-boot config record, and the stick /
#                         trigger / keyboard against the D-pad (dc/tests/vmu/
#                         run_flycast.sh; results in build/shots/vmu); part of
#                         `test` when Flycast is there
#   make -C dc disc-test  DC9C on Flycast: the CDI boots, the boot FMV (INTRO.DMV) plays and
#                         ends in the title, START skips it, a game is saved to the VMU
#                         through the menus and loaded after a restart, an old VMU image
#                         loads; each twice, identical (dc/tests/disc/run_flycast.sh);
#                         part of `make -C dc test`. INTRO=none builds the disc without
#                         the movie, INTRO_REQUIRED=1 fails when ffmpeg / numpy are missing
#   make -C dc audio-test DC5 on Flycast: the title, the invaded town and a
#                         combat level with sound (expect-audio, no
#                         underruns or late AICA refills, the effects), each
#                         compared with `runner --wav` (envelope r >= 0.9),
#                         and the title's content hash twice (dc/tests/audio/
#                         run_flycast.sh; results in build/shots/audio);
#                         part of `test` when Flycast is there
#   make -C dc ui-compare the six DC3 UI scripts on Flycast against the native
#                         runner (dc/harness/ui_compare.sh; shots and diffs in
#                         build/shots/ui); part of `test` when Flycast is there
#   make -C dc ow-test    DC7 on Flycast: the overworld in 3D in story_ch1/ch2,
#                         story_invasion, fp_island, side_sample_a and the
#                         Hellvile walks (dc/tests/overworld), each shot with a
#                         3D view against the host software PVR's (compare3d.py),
#                         no polygon dropped, no tick over 33.3 ms; results in
#                         build/shots/ow; part of `test` when Flycast is there
#   make -C dc family-test  DC9A on Flycast: every story_*, fp_* and side_* script of
#                         core/tests/scripts twice in dcrun (SCRIPTS="a b" for some),
#                         each must pass and repeat byte for byte (dc/tests/families;
#                         results in build/shots/families)
#   make -C dc family-test2  DC9B: the same for the minigame, house, creator, studio, fishing,
#                         cooking, library, dream, concert and exorcism scripts (results in
#                         build/shots/families2)
#   make -C dc v3d-test   DC6 part 1 on the host: the first-person view's polygons
#                         (software PVR) against the reference raycaster
#   make -C dc room-test  DC8 on Flycast: the house. The 58 room_ref golden
#                         frames drawn by the game's plat_room (identical to
#                         room_ref, pixel for pixel) and the house scripts
#                         (core/tests/scripts/home_*.txt) against the host
#                         software PVR's pictures of the same ticks
#                         (compare_house.py), no prim dropped, no tick over
#                         33.3 ms (dc/tests/house/run_flycast.sh); results in
#                         build/shots/house; part of `test` when Flycast is there
#   make -C dc sizes      section sizes against the 16 MB RAM budget
#   make -C dc PACK=/path/pack.dmpk disc
#                         the mature disc: the pack (built with `mpack.py build`, port dc) goes on
#                         the disc as PACK.DMP and dc/src/packdisc.c is linked (E10)
#   make -C dc check-talkdisc
#                         TD1 on the host: dc/src/talkdisc.c and core/src/talk_db.c over a real TALK_en.BIN
#   make -C dc talk-test  TD1 on Flycast: the CDI's TALK_EN.BIN opens and verifies, a greeting is generated
#                         and the Talk screen opens (dc/tests/talk, twice, byte-identical shots)
#   make -C dc clean
#
# Toolchain: KallistiOS and its sh-elf gcc (dc/tools/setup_kos.sh, see
# docs/DREAMCAST.md). The KOS environment (KOS_BASE, KOS_CFLAGS, ...) comes
# from <prefix>/kos/environ.sh. When it is not sourced, this Makefile
# sources it itself from DC_PREFIX (default /opt/toolchains/dc, then
# $HOME/dc) and runs again, so `make -C dc` works from any shell.
#
# Every core source listed by core/sources.mk is compiled and the objects
# are linked directly (never through an archive), so the core's weak stubs
# are overridden by the strong definitions. -ffunction-sections and
# -fdata-sections (in KOS_CFLAGS) with --gc-sections (KOS_LDFLAGS) drop the
# overridden weak defaults.

DC   := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
ROOT := $(abspath $(DC)/..)

ifeq ($(strip $(KOS_BASE)),)
# ---- no KOS environment: source environ.sh and run again --------------------
DC_PREFIX ?= $(firstword $(patsubst %/kos/environ.sh,%,$(wildcard /opt/toolchains/dc/kos/environ.sh $(HOME)/dc/kos/environ.sh)))
KOS_ENVIRON := $(if $(DC_PREFIX),$(wildcard $(DC_PREFIX)/kos/environ.sh))

.DEFAULT_GOAL := all
.PHONY: all $(MAKECMDGOALS)
all $(filter-out all,$(MAKECMDGOALS)):
ifeq ($(KOS_ENVIRON),)
	@if [ "$@" = clean ]; then rm -rf "$(DC)/build"; exit 0; fi; \
	printf '\n*** KallistiOS not found (no /opt/toolchains/dc/kos/environ.sh or ~/dc/kos/environ.sh).\n    Install it with dc/tools/setup_kos.sh, or source your own environ.sh first,\n    or set DC_PREFIX=<prefix>. Details: docs/DREAMCAST.md section 1.\n\n' >&2; exit 1
else
	@bash -c '. "$(KOS_ENVIRON)" && exec "$(MAKE)" --no-print-directory -f "$(DC)/Makefile" $@'
endif

else
# ---- KOS environment present ---------------------------------------------------
DOOMO_CORE := $(ROOT)/core
include $(ROOT)/core/sources.mk

KOS_WRAP := $(KOS_BASE)/utils/build_wrappers
CC       := $(KOS_WRAP)/kos-cc
OBJCOPY  := $(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-objcopy
NM       := $(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-nm
SIZE     := $(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-size
STRIP    := $(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-strip
PYTHON   ?= python3

BUILD := $(DC)/build
OBJ   := $(BUILD)/obj
ELF   := $(BUILD)/doomogachi.elf
MAP   := $(BUILD)/doomogachi.map
CDI   := $(BUILD)/doomogachi.cdi

# kos-cc adds KOS_CFLAGS (-O2 -m4-single -ml -ffunction-sections
# -fdata-sections -g -Wall, the KOS include paths, ...) to every compile and
# KOS_LDFLAGS + KOS_LIBS to the link.
# The core keeps to C99 (ARCHITECTURE.md); gnu99 is for the KOS headers.
DEFS   := -DDOOMO_PLATFORM_DC=1
# TD1: Talk is built in; its database TALK_en.BIN goes on the disc (dc/src/talkdisc.c)
DEFS   += -DDOOMO_HAS_TALK=1
# TD2: the read-only views of the Talk screen that dcrun's talk-* commands use (dc/src/talkhc.c)
DEFS   += -DDOOMO_TALK_DEBUG=1
# DC5b: MUSIC_IRQ=0 builds the music engine's legacy bridge instead of its IRQ tier
MUSIC_IRQ ?= 1
DEFS   += -DDC_MUSIC_IRQ=$(MUSIC_IRQ)
# E10: an optional mature content pack on the disc (PACK=/path/pack.dmpk)
PACK   ?=
ifneq ($(strip $(PACK)),)
DEFS   += -DDOOMO_PACK_ON_DISC=1
endif
CFLAGS := -std=gnu99 -Wall -Wextra -Werror -fno-strict-aliasing -MMD -MP \
          -I$(CORE_INC) -I$(DC)/src -I$(DC)/harness $(DEFS)
LDFLAGS := -Wl,-Map,$(MAP)

DC_SRCS   := $(sort $(wildcard $(DC)/src/*.c))
dc_obj     = $(patsubst $(DC)/%.c,$(OBJ)/dc/%.o,$(1))
core_obj   = $(patsubst $(DOOMO_CORE)/%.c,$(OBJ)/core/%.o,$(1))
DC_OBJS   := $(call dc_obj,$(DC_SRCS))
CORE_OBJS := $(call core_obj,$(CORE_SRCS))
ALL_OBJS  := $(DC_OBJS) $(CORE_OBJS)

.PHONY: all elf disc all-disc test check-elf check-packdisc check-talkdisc talk-test host-test harness-test ui-compare vmu-test disc-test audio-test ow-test room-test v3d-test family-test family-test2 sizes clean
.DEFAULT_GOAL := all

all: elf
elf: $(ELF)
	@$(MAKE) --no-print-directory -f $(DC)/Makefile sizes

$(ELF): $(ALL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(ALL_OBJS)

$(OBJ)/dc/%.o: $(DC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/core/%.o: $(DOOMO_CORE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- disc --------------------------------------------------------------------------
# 1ST_READ.BIN (scrambled binary), IP.BIN (from ip.txt), an ISO with the
# boot sector, then cdi4dc's self-booting CDI (dc/tools/mkcdi.sh)
disc: $(CDI)
all-disc: elf disc

# E10: validate the pack for port dc first; a changed PACK rebuilds packdisc.o (stamp) and the disc
PACKSTAMP := $(BUILD)/pack.stamp
.PHONY: FORCE
FORCE:
$(PACKSTAMP): FORCE
	@mkdir -p $(BUILD); p="$(strip $(PACK))"; c=""; [ -z "$$p" ] || c=$$(cksum < "$$p" 2>/dev/null); \
	echo "$$p $$c" | cmp -s - $@ || echo "$$p $$c" > $@
$(OBJ)/dc/src/packdisc.o: $(PACKSTAMP)

# TD1: Talk's database (the full English bank, core/tools/talkc.py) goes on the disc root as TALK_EN.BIN
TALK_DIR := $(ROOT)/core/build/talk
TALK_BIN := $(TALK_DIR)/TALK_en.BIN
TALK_SRC := $(wildcard $(ROOT)/core/tools/talkc*.py) $(ROOT)/core/include/doomo/talk.h $(ROOT)/core/gen/txt_dict.c \
            $(ROOT)/core/data/story/register.tsv $(shell find $(ROOT)/core/data/talk -type f 2>/dev/null)
$(TALK_BIN): $(TALK_SRC)
	@$(MAKE) --no-print-directory -C $(ROOT)/core $(TALK_BIN)

# DC9C: the boot FMV, INTRO.DMV on the disc root (dc/tools/mkfmv.py encodes
# trailer/assets/dippinn_logo.mp4; needs ffmpeg and numpy). Without them the
# disc is built without an intro, with a warning; INTRO_REQUIRED=1 fails instead.
# INTRO=/path/x.dmv uses a ready-made file, INTRO=none builds no intro.
INTRO          ?=
INTRO_REQUIRED ?= 0
INTRO_SRC      := $(ROOT)/trailer/assets/dippinn_logo.mp4
INTRO_DMV      := $(BUILD)/INTRO.DMV
FMV_TOOLS      := $(shell command -v ffmpeg >/dev/null 2>&1 && $(PYTHON) -I -c "import numpy" >/dev/null 2>&1 && echo yes)
ifeq ($(strip $(INTRO)),none)
INTRO_FILE :=
else ifneq ($(strip $(INTRO)),)
INTRO_FILE := $(strip $(INTRO))
else ifeq ($(FMV_TOOLS),yes)
INTRO_FILE := $(INTRO_DMV)
else ifeq ($(INTRO_REQUIRED),1)
$(error INTRO_REQUIRED=1 but ffmpeg or python3 numpy is missing (apt install ffmpeg python3-numpy), or pass INTRO=file.dmv)
else
$(warning no ffmpeg or numpy: the disc is built without the boot FMV (INTRO.DMV))
INTRO_FILE :=
endif

$(INTRO_DMV): $(INTRO_SRC) $(DC)/tools/mkfmv.py
	@mkdir -p $(BUILD)
	$(PYTHON) -I $(DC)/tools/mkfmv.py $(INTRO_SRC) $@

$(CDI): $(ELF) $(DC)/ip.txt $(DC)/tools/mkcdi.sh $(PACKSTAMP) $(INTRO_FILE) $(TALK_BIN)
	@if [ -n "$(strip $(PACK))" ]; then $(PYTHON) -I $(ROOT)/tools/mpack/mpack.py validate "$(strip $(PACK))" --port dc || exit 1; fi
	sh $(DC)/tools/mkcdi.sh $(ELF) $(DC)/ip.txt $(BUILD)/disc $(CDI) "$(strip $(PACK))" "$(INTRO_FILE)" $(TALK_DIR)

# ---- checks ------------------------------------------------------------------------
sizes: $(ELF)
	@sh $(DC)/tools/elfsize.sh $(ELF) $(SIZE) $(NM)

check-elf: $(ELF)
	@sh $(DC)/tools/check_elf.sh $(ELF) $(NM)

# The Flycast harness (DC1) lives in dc/harness. When its Makefile is in
# the tree, harness-test runs its self-test (no emulator); when the Flycast
# core is there (make -C dc/harness flycast, or flycast_libretro.so in the
# toolchain prefix next to kos/) it also boots the ELF and the CDI with
# dc/tests/boot.txt and runs ui-compare. Missing parts are skipped with a
# note. Flycast cannot open an ELF over 16 MiB (the debug info makes ours
# about 17 MB), so the harness boots a copy without debug sections; the
# symbols come from doomogachi.nm.
HARNESS := $(DC)/harness
FLYCAST ?= $(firstword $(wildcard $(HARNESS)/build/flycast_libretro.so \
                                  $(abspath $(KOS_BASE)/..)/flycast_libretro.so))
DCRUN   := $(HARNESS)/build/dcrun
SHOTS   := $(BUILD)/shots
ELF_RUN := $(BUILD)/doomogachi-nodebug.elf
RUNNER  := $(ROOT)/core/build/runner
harness-test: $(ELF) $(CDI) $(ELF_RUN)
	@if [ ! -f $(HARNESS)/Makefile ]; then \
	    echo "note: dc/harness (the Flycast harness, DC1) is not in this tree: boot scripts skipped"; exit 0; \
	fi; \
	$(MAKE) --no-print-directory -C $(HARNESS) test || exit 1; \
	if [ -z "$(FLYCAST)" ]; then \
	    echo "note: no Flycast core (make -C dc/harness flycast): boot scripts and ui-compare skipped"; exit 0; \
	fi; \
	for img in $(ELF_RUN) $(CDI); do \
	    n=$$(basename $$img | tr . _); \
	    $(DCRUN) $(FLYCAST) $$img --nm $(BUILD)/doomogachi.nm --fresh \
	        --run-dir $(BUILD)/run --script $(DC)/tests/boot.txt --shots $(SHOTS)/$$n \
	        --timeout 300 || exit 1; \
	done; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile ui-compare || exit 1; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile vmu-test || exit 1; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile disc-test || exit 1; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile audio-test || exit 1; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile ow-test || exit 1; \
	$(MAKE) --no-print-directory -f $(DC)/Makefile room-test

$(ELF_RUN): $(ELF)
	$(STRIP) --strip-debug -o $@ $(ELF)

# DC3: the six UI scripts on Flycast against the native runner (host build)
ui-compare: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "ui-compare: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	@$(MAKE) --no-print-directory -C $(ROOT)/core runner
	sh $(HARNESS)/ui_compare.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(RUNNER) $(SHOTS)/ui

# DC4: the VMU saves and the controls on Flycast
vmu-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "vmu-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	sh $(DC)/tests/vmu/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(SHOTS)/vmu

# DC9C: the disc image, the boot FMV and a game saved and loaded after a restart, each twice
disc-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "disc-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@if [ ! -f $(BUILD)/disc/cd/INTRO.DMV ]; then echo "disc-test: the disc has no INTRO.DMV (needs ffmpeg and numpy, or INTRO=file.dmv)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	sh $(DC)/tests/disc/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(SHOTS)/disc

AUDIO_IRQ_RUNNER := $(BUILD)/host-audio/dcsnd_runner_irq
# DC5: the sound on Flycast against the native runner's musrender
audio-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "audio-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	@$(MAKE) --no-print-directory -C $(ROOT)/core runner
	@if [ "$(MUSIC_IRQ)" = 1 ]; then $(MAKE) --no-print-directory -C $(DC)/tests/audio $(AUDIO_IRQ_RUNNER); fi
	sh $(DC)/tests/audio/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(RUNNER) $(SHOTS)/audio \
	    $(if $(filter 1,$(MUSIC_IRQ)),$(AUDIO_IRQ_RUNNER))

# DC7: the overworld on Flycast against the host software PVR (dc/tests/gfx2d)
HOSTRUN := $(BUILD)/host/dc2d_runner
ow-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "ow-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	@$(MAKE) --no-print-directory -C $(ROOT)/core runner
	@$(MAKE) --no-print-directory -C $(DC)/tests/gfx2d $(HOSTRUN)
	sh $(DC)/tests/overworld/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(RUNNER) $(HOSTRUN) $(SHOTS)/ow

# DC9A: the story, free-play and side-mission scripts on Flycast, each twice, identical
family-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "family-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	sh $(DC)/tests/families/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(SHOTS)/families $(SCRIPTS)

# DC9B: the minigames, house (home_visit plays the Talk screen since TD1; home_talk_* need dcrun's talk-* commands, TD3), creator, studio, fishing and cooking, library, dream, concert and exorcism scripts, each twice, identical
FAMILY2 := mg_*.txt home_defend.txt home_editor.txt home_flats.txt home_gallery.txt home_haunt.txt home_mart.txt home_rooms.txt home_tutorial.txt home_visit.txt creator*.txt studio*.txt fishing.txt cooking.txt library_*.txt dream*.txt concert_*.txt life_exorcism.txt
family-test2: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "family-test2: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	FAMILY_GLOBS="$(FAMILY2)" sh $(DC)/tests/families/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(SHOTS)/families2 $(SCRIPTS)

# DC8: the house on Flycast: room_ref's golden frames and the house scripts
ROOMTEST := $(BUILD)/host/dcroom_test
room-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ]; then echo "room-test: no Flycast core (make -C dc/harness flycast)" >&2; exit 1; fi
	@$(MAKE) --no-print-directory -C $(HARNESS) all
	@$(MAKE) --no-print-directory -C $(ROOT)/core runner
	@$(MAKE) --no-print-directory -C $(DC)/tests/gfx2d $(HOSTRUN) $(ROOMTEST)
	sh $(DC)/tests/house/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(RUNNER) $(HOSTRUN) $(ROOMTEST) $(SHOTS)/house

$(BUILD)/doomogachi.nm: $(ELF)
	$(NM) $(ELF) >$@
harness-test: $(BUILD)/doomogachi.nm

# E10: the disc pack reader against a real pack, host cc only (KOS file calls mocked)
check-packdisc:
	@sh $(DC)/tests/packdisc_host.sh

# TD1: the disc Talk reader against a real TALK_en.BIN (talk_db.c on top), host cc only
check-talkdisc:
	@sh $(DC)/tests/talkdisc_host.sh

# TD1 on Flycast: the CDI's Talk bank opens and verifies, one greeting is generated, the Talk screen opens
talk-test: $(CDI) $(BUILD)/doomogachi.nm
	@if [ -z "$(FLYCAST)" ] || [ ! -x $(DCRUN) ]; then echo "note: no Flycast core or dcrun: talk-test skipped"; exit 0; fi; \
	sh $(DC)/tests/talk/run_flycast.sh $(FLYCAST) $(DCRUN) $(CDI) $(BUILD)/doomogachi.nm $(SHOTS)/talk

# DC3: the portable 2D layer against the native runner, on the host
# (dc/tests/gfx2d: unit tests + the UI scripts through the software PVR);
# DC4: the VMU file system, the icons and the controls (dc/tests/vmu);
# DC5: the sound mixer's units and its WAVs against musrender (dc/tests/audio)
v3d-test:
	@$(MAKE) --no-print-directory -C $(DC)/tests/gfx2d v3d-test

host-test:
	@$(MAKE) --no-print-directory -C $(DC)/tests/gfx2d test
	@$(MAKE) --no-print-directory -C $(DC)/tests/vmu test
	@$(MAKE) --no-print-directory -C $(DC)/tests/audio test

test: all-disc check-elf check-packdisc check-talkdisc host-test harness-test talk-test

clean:
	rm -rf $(BUILD)

-include $(ALL_OBJS:.o=.d)
endif
