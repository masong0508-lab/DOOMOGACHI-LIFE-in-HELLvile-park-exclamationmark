# dc/Makefile - DOOMOGACHI LIFE for the Dreamcast (owner: dreamcast).
#
#   make -C dc            build/doomogachi.elf (+ .map, size report)
#   make -C dc disc       build/doomogachi.cdi (self-booting disc image), plus
#                         build/1ST_READ.BIN and build/IP.BIN
#   make -C dc all-disc   both of the above
#   make -C dc test       the ELF + disc, the symbol checks (check-elf), and
#                         the Flycast harness scripts when dc/harness exists
#   make -C dc check-elf  checks on the linked ELF only (no emulator)
#   make -C dc sizes      section sizes against the 16 MB RAM budget
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
CFLAGS := -std=gnu99 -Wall -Wextra -Werror -fno-strict-aliasing -MMD -MP \
          -I$(CORE_INC) -I$(DC)/src -I$(DC)/harness $(DEFS)
LDFLAGS := -Wl,-Map,$(MAP)

DC_SRCS   := $(sort $(wildcard $(DC)/src/*.c))
dc_obj     = $(patsubst $(DC)/%.c,$(OBJ)/dc/%.o,$(1))
core_obj   = $(patsubst $(DOOMO_CORE)/%.c,$(OBJ)/core/%.o,$(1))
DC_OBJS   := $(call dc_obj,$(DC_SRCS))
CORE_OBJS := $(call core_obj,$(CORE_SRCS))
ALL_OBJS  := $(DC_OBJS) $(CORE_OBJS)

.PHONY: all elf disc all-disc test check-elf harness-test sizes clean
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

$(CDI): $(ELF) $(DC)/ip.txt $(DC)/tools/mkcdi.sh
	sh $(DC)/tools/mkcdi.sh $(ELF) $(DC)/ip.txt $(BUILD)/disc $(CDI)

# ---- checks ------------------------------------------------------------------------
sizes: $(ELF)
	@sh $(DC)/tools/elfsize.sh $(ELF) $(SIZE) $(NM)

check-elf: $(ELF)
	@sh $(DC)/tools/check_elf.sh $(ELF) $(NM)

# The Flycast harness (DC1) lives in dc/harness. When its Makefile is in
# the tree, harness-test runs its self-test (no emulator); when the Flycast
# core has been built (make -C dc/harness flycast) it also boots the ELF and
# the CDI with dc/tests/boot.txt. Missing parts are skipped with a note.
HARNESS := $(DC)/harness
FLYCAST := $(HARNESS)/build/flycast_libretro.so
DCRUN   := $(HARNESS)/build/dcrun
SHOTS   := $(BUILD)/shots
harness-test: $(ELF) $(CDI)
	@if [ ! -f $(HARNESS)/Makefile ]; then \
	    echo "note: dc/harness (the Flycast harness, DC1) is not in this tree: boot scripts skipped"; exit 0; \
	fi; \
	$(MAKE) --no-print-directory -C $(HARNESS) test || exit 1; \
	if [ ! -f $(FLYCAST) ]; then \
	    echo "note: $(FLYCAST) not built (make -C dc/harness flycast): boot scripts skipped"; exit 0; \
	fi; \
	for img in $(ELF) $(CDI); do \
	    n=$$(basename $$img | tr . _); \
	    $(DCRUN) $(FLYCAST) $$img --nm $(BUILD)/doomogachi.nm --fresh \
	        --run-dir $(BUILD)/run --script $(DC)/tests/boot.txt --shots $(SHOTS)/$$n \
	        --timeout 300 || exit 1; \
	done

$(BUILD)/doomogachi.nm: $(ELF)
	$(NM) $(ELF) >$@
harness-test: $(BUILD)/doomogachi.nm

test: all-disc check-elf harness-test

clean:
	rm -rf $(BUILD)

-include $(ALL_OBJS:.o=.d)
endif
