# PS2SDK's native GNU Make route, deliberately not requiring CMake:
#   make -C ps2
# Uses installed ps2dev EE toolchain, gsKit/dmaKit and PADMAN.
ifndef PS2SDK
$(error Set PS2SDK to your PS2SDK install directory)
endif
ifndef GSKIT
$(error Set GSKIT to your gsKit install directory)
endif

EE_BIN := build/doomogachi_ps2.elf
EE_OBJS := build/main.o build/padmap.o
EE_INCS += -I../core/include -Isrc -I$(GSKIT)/include
EE_CFLAGS += -std=c99 -DDOOMO_PLATFORM_PS2=1 -DDOOMO_HAS_TALK=1 -ffunction-sections -fdata-sections
EE_LDFLAGS += -L$(GSKIT)/lib -Wl,--gc-sections
EE_LIBS := -lgskit_toolkit -lgskit -ldmakit -lmc -lpad -lm
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

.PHONY: all clean
all: $(EE_BIN)

build/main.o: src/main.c src/padmap.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/padmap.o: src/padmap.c src/padmap.h ../core/include/doomo/input.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -rf build

# Experimental real game target: link shared C99 core directly. This
# remains separate from the fully green PS2-0 diagnostic until its title,
# portraits, scene navigation and performance run on PCSX2.
DOOMO_CORE := $(abspath ../core)
include ../core/sources.mk
GAME_CORE_OBJS := $(patsubst $(DOOMO_CORE)/%.c,build/core/%.o,$(CORE_SRCS))
GAME_OBJS := build/game_main.o build/game2d.o build/memcard.o build/audio.o build/synth.o build/talkdisc.o build/ref_overworld.o build/ref_view3d.o build/padmap.o $(GAME_CORE_OBJS)
GAME_LIBS := $(EE_LIBS) -laudsrv -lcdvd
GAME_BIN := build/doomogachi_ps2_game.elf
# Preserve GAME_BIN (DWARF) for source-level debugging, but put a
# separately verified no-DWARF image on the disc to save storage.
GAME_DISC_BIN := build/doomogachi_ps2_disc.elf
PS2_EE_STRIP ?= mips64r5900el-ps2-elf-strip

.PHONY: game
game: $(GAME_BIN)
$(GAME_BIN): $(GAME_OBJS)
	@mkdir -p $(@D)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $(GAME_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(GAME_LIBS)

# Strip debugging symbols only, never the program's loaded sections.
# check_disc_elf.py compares the *actual bytes* and entry point that the
# PS2 EE loader sees. Any discrepancy fails the ISO build closed.
$(GAME_DISC_BIN): $(GAME_BIN) tools/check_disc_elf.py
	$(PS2_EE_STRIP) --strip-debug -o $@ $<
	python3 tools/check_disc_elf.py $< $@

build/game_main.o: src/game_main.c src/padmap.h src/game2d.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/game2d.o: src/game2d.c src/game2d.h src/indexed_blit.h src/indexed_present.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/memcard.o: src/memcard.c src/save_journal.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/audio.o: src/audio.c src/synth.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/synth.o: src/synth.c src/synth.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/talkdisc.o: src/talkdisc.c src/talk_path.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/core/%.o: $(DOOMO_CORE)/%.c
	@mkdir -p $(@D)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# Portable reference geometry backed by the same GS staging surface.
build/ref_overworld.o: src/ref_overworld.c src/frame_ref.h src/game2d.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
build/ref_view3d.o: src/ref_view3d.c src/frame_ref.h src/game2d.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# PS2-4 CD image for PCSX2 / PS2 homebrew disc launch. The English
# language bank comes from the SAME generator as PS1 and Dreamcast, not
# a new hand-maintained dialogue file. audsrv.irx ships from PS2SDK.
TALK_BIN := ../core/build/talk/TALK_en.BIN
AUDSRV_IRX := $(PS2SDK)/iop/irx/audsrv.irx
DISC_ROOT := build/disc
DISC_ISO := build/doomogachi_ps2_game.iso
# Default: compact DVD ISO with no unnecessary mkisofs 300 KiB tail pad.
# Set PS2_ISO_PAD=1 when making an image specifically for physical burning.
PS2_ISO_PAD ?= 0
ISO_PAD_OPT := $(if $(filter 1,$(PS2_ISO_PAD)),-pad,-no-pad)
PS2_ZSTD_LEVEL ?= 15

$(TALK_BIN):
	$(MAKE) -C ../core talk
.PHONY: disc
disc: $(GAME_DISC_BIN) $(TALK_BIN) $(AUDSRV_IRX)
	@command -v xorriso >/dev/null || (echo 'Install xorriso to build the PS2 ISO' && false)
	@rm -rf $(DISC_ROOT)
	@mkdir -p $(DISC_ROOT)
	cp $(GAME_DISC_BIN) $(DISC_ROOT)/DOOMOG.ELF
	cp $(TALK_BIN) $(DISC_ROOT)/TALK_EN.BIN
	cp $(AUDSRV_IRX) $(DISC_ROOT)/AUDSRV.IRX
	cp SYSTEM.CNF $(DISC_ROOT)/SYSTEM.CNF
	xorriso -as mkisofs -iso-level 1 $(ISO_PAD_OPT) -V DOOMOGACHI -o $(DISC_ISO) $(DISC_ROOT)
	@echo "PS2 disc image ready: $(DISC_ISO)"

# BIOS-free Play! emulator smoke; runs on a desktop Linux host with
# Flatpak, Xvfb and ImageMagick import installed. DOES NOT certify the
# game is playable; the two screenshots and log are review evidence.
.PHONY: play-smoke
play-smoke: disc
	@command -v xvfb-run >/dev/null || (echo 'Install xvfb-run/xauth' && false)
	xvfb-run -a -s '-screen 0 1280x720x24' \
		python3 tools/play_biosfree_smoke.py $(DISC_ISO) \
		--out build/play-smoke --seconds 35

# Compact storage pipeline, independent of the game's memory/runtime:
#   make -C ps2 compact
# Keep the native ISO, plus byte-exact checked DVD CHD (PCSX2) and
# ISO.ZST archival file. No separate game assets or dummy filler.
.PHONY: compact
compact: disc
	@command -v chdman >/dev/null || (echo 'Install mame-tools (chdman)' && false)
	@command -v zstd >/dev/null || (echo 'Install zstd' && false)
	python3 tools/compact_iso.py --iso $(DISC_ISO) --out-dir build/compact --zstd-level $(PS2_ZSTD_LEVEL)
