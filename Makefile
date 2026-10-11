# Makefile - top-level entry points for DOOMOGACHI LIFE in HELLvile Park!
# (owner: ci). Every target delegates to the module builds (core/, gba/,
# ps1/, trailer/); see docs/BUILDING.md for toolchain setup.
#
#   make            same as `make all`
#   make all        core tests + GBA ROMs + PS1 discs (the PS1 part is skipped,
#                   with a note, when PSn00bSDK or a MIPS compiler is missing)
#   make test       everything that can run on this machine: core tests,
#                   core tests under ASan/UBSan, xcheck (gcc/arm/mipsel -Werror),
#                   check-gen, the GBA mGBA harness and the PS1 PCSX-Redux harness
#                   (each platform part is skipped, with a note, if its tools are missing)
#   make core       native unit tests + headless playthrough (make -C core test)
#   make gba        gba/build/doomogachi.gba + doomogachi_et.gba (engine test)
#   make ps1        ps1/build/doomogachi.{exe,bin,cue} + ps1/build-enginetest/
#   make trailer    trailer/build/trailer.mp4 (needs ffmpeg)
#   make dist       copy the ROMs / zip the discs into dist/ (release names)
#   make doctor     show which toolchains and tools were found
#   make clean      remove every build directory
#
# Variables:
#   PSN00BSDK_LIBS=<prefix>/lib/libpsn00b   PSn00bSDK install (auto-detected in
#                   /opt/psn00bsdk, /usr/local, ~/psn00bsdk, ~/.local)
#   PS1_TARGET=mipsel-linux-gnu|mipsel-none-elf   (auto-detected)
#   PCSX_REDUX=/path/to/pcsx-redux  PS1_BIOS=/path/to/openbios.bin   (PS1 harness)
#   JOBS=N          parallel jobs for the sub-builds (default: CPU count)

.NOTPARALLEL:
.DEFAULT_GOAL := all

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
# -jJOBS for the sub-makes, unless `make -jN` already shares a jobserver with them
J = $(if $(findstring jobserver,$(MAKEFLAGS)),,-j$(JOBS))

have = $(shell command -v $(1) 2>/dev/null)

# ---- toolchain detection ------------------------------------------------------
HOST_CC := $(call have,gcc)
PYTHON3 := $(call have,python3)
ARM_GCC := $(call have,arm-none-eabi-gcc)
# newlib: without it -print-file-name echoes the bare name back
ARM_LIBC := $(if $(ARM_GCC),$(filter /%,$(shell arm-none-eabi-gcc -print-file-name=libc.a 2>/dev/null)))
MGBA_HDR := $(if $(HOST_CC),$(shell printf '\043include <mgba/core/core.h>\n' | gcc -E -x c - >/dev/null 2>&1 && echo yes))
CMAKE    := $(call have,cmake)
NINJA    := $(or $(call have,ninja),$(call have,ninja-build))
FFMPEG   := $(call have,ffmpeg)
ZIP      := $(call have,zip)

PS1_TARGET ?= $(if $(call have,mipsel-linux-gnu-gcc),mipsel-linux-gnu,$(if $(call have,mipsel-none-elf-gcc),mipsel-none-elf))
SDK_PROBE  := /opt/psn00bsdk /usr/local $(HOME)/psn00bsdk $(HOME)/.local /usr/local/psn00bsdk
PSN00BSDK_LIBS ?= $(patsubst %/cmake/sdk.cmake,%,$(firstword $(wildcard $(addsuffix /lib/libpsn00b/cmake/sdk.cmake,$(SDK_PROBE)))))
PSN00BSDK_OK   := $(if $(PSN00BSDK_LIBS),$(wildcard $(PSN00BSDK_LIBS)/cmake/sdk.cmake))
PSN00BSDK_ABS  := $(abspath $(PSN00BSDK_LIBS))
PSN00BSDK_BIN  := $(if $(PSN00BSDK_OK),$(abspath $(PSN00BSDK_LIBS)/../../bin))

PCSX_REDUX ?= /opt/ps1emu/pcsx-redux
PS1_BIOS   ?= /opt/ps1emu/openbios.bin
XVFB_RUN   := $(call have,xvfb-run)

# Why the PS1 build cannot run (empty = it can)
PS1_WHY := $(strip \
  $(if $(PS1_TARGET),,no mipsel-linux-gnu-gcc / mipsel-none-elf-gcc;) \
  $(if $(PSN00BSDK_OK),,PSn00bSDK not found (set PSN00BSDK_LIBS=<prefix>/lib/libpsn00b);) \
  $(if $(CMAKE),,cmake not found;) \
  $(if $(NINJA),,ninja not found;) \
  $(if $(PYTHON3),,python3 not found;))
PS1_HARNESS_WHY := $(strip $(PS1_WHY) \
  $(if $(wildcard $(PCSX_REDUX)),,PCSX-Redux not found at $(PCSX_REDUX) (set PCSX_REDUX=...);) \
  $(if $(wildcard $(PS1_BIOS)),,BIOS image not found at $(PS1_BIOS) (set PS1_BIOS=...);) \
  $(if $(XVFB_RUN),,xvfb-run not found;))

HINT_HOST := Ubuntu/Debian: sudo apt install build-essential python3   macOS: xcode-select --install && brew install python
HINT_ARM  := Ubuntu/Debian: sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi   macOS: brew install --cask gcc-arm-embedded
HINT_MGBA := Ubuntu/Debian: sudo apt install libmgba-dev (0.10)   macOS: build libmgba from source (docs/BUILDING.md)
HINT_FFMPEG := Ubuntu/Debian: sudo apt install ffmpeg   macOS: brew install ffmpeg

# fail NAME HINT - print a friendly "missing tool" message and stop
define fail
	@printf '\n*** %s\n    %s\n    (details: docs/BUILDING.md, or run `make doctor`)\n\n' "$(1)" "$(2)" >&2; exit 1
endef

# PS1 environment for cmake: the SDK's toolchain file + mkpsxiso on PATH
PS1_ENV := PSN00BSDK_LIBS="$(PSN00BSDK_ABS)" PATH="$(PSN00BSDK_BIN):$$PATH"
PS1_CFG := -DPSN00BSDK_TARGET=$(PS1_TARGET) -DDOOMO_EMU=$(PCSX_REDUX) -DDOOMO_BIOS=$(PS1_BIOS)

.PHONY: all test core core-test core-san xcheck check-gen gba gba-test ps1 ps1-test \
        ps1-optional gba-test-optional ps1-test-optional trailer dist doctor clean \
        need-host need-arm need-mgba need-ps1

# ---- aggregate targets --------------------------------------------------------------
all: core gba ps1-optional
	@echo ""
	@echo "make all: done. GBA ROM: gba/build/doomogachi.gba$(if $(PS1_WHY),,  PS1 disc: ps1/build/doomogachi.cue)"

test: core-test core-san xcheck check-gen gba-test-optional ps1-test-optional
	@echo ""
	@echo "make test: all available tests passed."

# ---- core ------------------------------------------------------------------------------
core: core-test

core-test: need-host
	$(MAKE) -C core $(J) test

core-san: need-host
	$(MAKE) -C core $(J) test SAN=1 BUILD=build-san

xcheck: need-host
	$(if $(PSN00BSDK_OK),PSN00BSDK_LIBS="$(PSN00BSDK_ABS)") sh core/tools/xcheck.sh

check-gen: need-host
	$(MAKE) -C core check-gen

need-host:
ifeq ($(HOST_CC),)
	$(call fail,No host C compiler (gcc) found.,$(HINT_HOST))
endif
ifeq ($(PYTHON3),)
	$(call fail,python3 not found.,$(HINT_HOST))
endif

# ---- GBA ---------------------------------------------------------------------------------
gba: need-arm
	$(MAKE) -C gba $(J)
	$(MAKE) -C gba $(J) engine-test

gba-test: need-arm need-mgba
	$(MAKE) -C gba test

gba-test-optional:
ifeq ($(ARM_GCC)$(ARM_LIBC),)
	@echo "note: skipping the GBA harness (arm-none-eabi-gcc not found; $(HINT_ARM))"
else ifeq ($(ARM_LIBC),)
	@echo "note: skipping the GBA harness (newlib for arm-none-eabi not found; $(HINT_ARM))"
else ifeq ($(MGBA_HDR),)
	@echo "note: skipping the GBA harness (libmgba headers not found; $(HINT_MGBA))"
else
	$(MAKE) -C gba test
endif

need-arm: need-host
ifeq ($(ARM_GCC),)
	$(call fail,arm-none-eabi-gcc not found (needed for the GBA ROM).,$(HINT_ARM))
endif
ifeq ($(ARM_LIBC),)
	$(call fail,newlib for arm-none-eabi not found (libc.a).,$(HINT_ARM))
endif

need-mgba:
ifeq ($(MGBA_HDR),)
	$(call fail,libmgba development headers not found (needed for the headless GBA harness).,$(HINT_MGBA))
endif

# ---- PS1 ---------------------------------------------------------------------------------
ps1: need-ps1
	cd ps1 && $(PS1_ENV) cmake --preset ps1 $(PS1_CFG) && $(PS1_ENV) cmake --build --preset ps1 -j $(JOBS)
	cd ps1 && $(PS1_ENV) cmake --preset ps1-enginetest $(PS1_CFG) && $(PS1_ENV) cmake --build --preset ps1-enginetest -j $(JOBS)

ps1-optional:
ifeq ($(PS1_WHY),)
	$(MAKE) ps1
else
	@echo "note: skipping the PS1 disc: $(PS1_WHY) See docs/BUILDING.md."
endif

ps1-test: ps1
ifneq ($(PS1_HARNESS_WHY),)
	$(call fail,Cannot run the PS1 harness: $(PS1_HARNESS_WHY),See docs/BUILDING.md (PCSX-Redux).)
endif
	cd ps1 && $(PS1_ENV) cmake --build --preset ps1 --target harness
	cd ps1 && $(PS1_ENV) cmake --build --preset ps1-enginetest --target harness

ps1-test-optional:
ifeq ($(PS1_HARNESS_WHY),)
	$(MAKE) ps1-test
else
	@echo "note: skipping the PS1 harness: $(PS1_HARNESS_WHY)"
endif

need-ps1:
ifneq ($(PS1_WHY),)
	$(call fail,Cannot build the PS1 disc: $(PS1_WHY),Ubuntu/Debian: sudo apt install gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu cmake ninja-build - then build PSn00bSDK (docs/BUILDING.md).)
endif

# ---- trailer ----------------------------------------------------------------------------
trailer: need-host
ifeq ($(FFMPEG),)
	$(call fail,ffmpeg not found (needed to encode the trailer).,$(HINT_FFMPEG))
endif
	$(MAKE) -C trailer build/trailer.mp4

# ---- dist (same file names as the CI artifacts / GitHub Releases) ----------------------
dist: all
	@mkdir -p dist
	cp gba/build/doomogachi.gba dist/doomogachi.gba
	cp gba/build/doomogachi_et.gba dist/doomogachi-enginetest.gba
ifeq ($(PS1_WHY),)
ifeq ($(ZIP),)
	@echo "note: zip not found - copying the PS1 discs unzipped"
	@mkdir -p dist/doomogachi-ps1 dist/doomogachi-ps1-enginetest
	cp ps1/build/doomogachi.exe ps1/build/doomogachi.bin ps1/build/doomogachi.cue dist/doomogachi-ps1/
	cp ps1/build-enginetest/doomogachi.exe ps1/build-enginetest/doomogachi.bin ps1/build-enginetest/doomogachi.cue dist/doomogachi-ps1-enginetest/
else
	rm -f dist/doomogachi-ps1.zip dist/doomogachi-ps1-enginetest.zip
	cd ps1/build && zip -9 -q $(ROOT)/dist/doomogachi-ps1.zip doomogachi.exe doomogachi.bin doomogachi.cue
	cd ps1/build-enginetest && zip -9 -q $(ROOT)/dist/doomogachi-ps1-enginetest.zip doomogachi.exe doomogachi.bin doomogachi.cue
endif
endif
	@ls -l dist

# ---- doctor -----------------------------------------------------------------------------
found = $(if $(1),found   $(1),MISSING)
doctor:
	@echo "DOOMOGACHI LIFE build doctor"
	@echo "  host gcc            $(call found,$(HOST_CC))"
	@echo "  python3             $(call found,$(PYTHON3))"
	@echo "  arm-none-eabi-gcc   $(call found,$(ARM_GCC))"
	@echo "  newlib (arm)        $(call found,$(ARM_LIBC))"
	@echo "  libmgba headers     $(if $(MGBA_HDR),found,MISSING (GBA harness only))"
	@echo "  MIPS compiler       $(if $(PS1_TARGET),found   $(PS1_TARGET)-gcc,MISSING)"
	@echo "  PSn00bSDK           $(if $(PSN00BSDK_OK),found   $(PSN00BSDK_LIBS),MISSING (set PSN00BSDK_LIBS=<prefix>/lib/libpsn00b))"
	@echo "  cmake               $(call found,$(CMAKE))"
	@echo "  ninja               $(call found,$(NINJA))"
	@echo "  PCSX-Redux          $(if $(wildcard $(PCSX_REDUX)),found   $(PCSX_REDUX),MISSING (PS1 harness only))"
	@echo "  xvfb-run            $(if $(XVFB_RUN),found   $(XVFB_RUN),MISSING (PS1 harness only))"
	@echo "  ffmpeg              $(if $(FFMPEG),found   $(FFMPEG),MISSING (trailer only))"
	@echo ""
	@echo "  make gba : $(if $(ARM_GCC),$(if $(ARM_LIBC),ready,needs newlib),needs arm-none-eabi-gcc)"
	@echo "  make ps1 : $(if $(PS1_WHY),$(PS1_WHY),ready)"

# ---- clean ------------------------------------------------------------------------------
clean:
	$(MAKE) -C core clean
	$(MAKE) -C gba clean
	$(MAKE) -C trailer clean
	rm -rf core/build-san ps1/build ps1/build-enginetest ps1/build-pal ps1/build-plat dist
