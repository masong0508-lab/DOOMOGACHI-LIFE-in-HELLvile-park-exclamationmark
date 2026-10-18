# core/Makefile - native build, tests and generators for the shared core
# (owner: foundation).
#
#   make -C core              build the headless runner (default)
#   make -C core runner       $(BUILD)/runner  (see core/native/README.md)
#   make -C core lib          compile every core object (src + gen)
#   make -C core test         unit tests + runner scripts at 240x160 and 320x240
#   make -C core gen          run every generator (tolerates missing ones)
#   make -C core check-gen    regenerate into a temp dir and diff vs the repo
#   make -C core xcheck       compile all core sources with gcc, arm and mipsel
#                             -Werror (core/tools/xcheck.sh)
#   make -C core sizes        .bss/.data/.text of each core object (arm)
#
#   BUILD=dir   build directory (default core/build)
#   SAN=1       build with -fsanitize=address,undefined
#
# GENERATOR CONVENTION (make gen / check-gen):
#   * tools/*.py containing the line "# doomo-generator" are run as
#         python3 tools/NAME.py --out-root DIR
#     and must write their outputs to DIR/<repo-relative path>
#     (e.g. DIR/core/gen/roster_data.c). Without --out-root, DIR is the
#     repository root. They read their inputs from the repository.
#     Order: tools/gen_*.py first, then everything else alphabetically.
#   * If core/tools/artgen*.c exist, artgen is built natively from
#     core/tools/artgen*.c + core/art/*.c + $(ARTGEN_CORE) and run as
#         $(BUILD)/artgen --out-root DIR
#     (it must write DIR/core/gen/art_data.c).
#   Generators must be deterministic (byte-identical output every run).

CORE  := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
ROOT  := $(abspath $(CORE)/..)
DOOMO_CORE := $(CORE)
include $(CORE)/sources.mk

BUILD ?= $(CORE)/build
override BUILD := $(abspath $(BUILD))

CC      ?= gcc
ifeq ($(CC),cc)
CC := gcc
endif
PYTHON  ?= python3
OPT     ?= -O2
CFLAGS_COMMON := -std=c99 -Wall -Wextra -Werror $(OPT) -g -MMD -MP
ifeq ($(SAN),1)
CFLAGS_COMMON += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS_SAN := -fsanitize=address,undefined
endif
CORE_CFLAGS   := $(CFLAGS_COMMON) -I$(CORE_INC)
# Native platform + tests may use the host libc.
HOST_CFLAGS   := $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200809L -I$(CORE_INC) -I$(CORE)/native

NATIVE_SRCS := $(sort $(wildcard $(CORE)/native/*.c))
NATIVE_MAIN := $(CORE)/native/runner_main.c
NATIVE_PLAT := $(filter-out $(NATIVE_MAIN),$(NATIVE_SRCS))
TEST_SRCS   := $(sort $(wildcard $(CORE)/tests/*.c))

obj = $(patsubst $(CORE)/%.c,$(BUILD)/obj/%.o,$(1))
CORE_OBJS   := $(call obj,$(CORE_SRCS))
PLAT_OBJS   := $(call obj,$(NATIVE_PLAT))
MAIN_OBJ    := $(call obj,$(NATIVE_MAIN))
TEST_OBJS   := $(call obj,$(TEST_SRCS))

.PHONY: all lib runner test unit scripts gen check-gen xcheck sizes clean
.DEFAULT_GOAL := runner

all: runner $(BUILD)/tests

lib: $(CORE_OBJS)

runner: $(BUILD)/runner

$(BUILD)/obj/src/%.o: $(CORE)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

$(BUILD)/obj/gen/%.o: $(CORE)/gen/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

$(BUILD)/obj/native/%.o: $(CORE)/native/%.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD)/obj/tests/%.o: $(CORE)/tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -I$(CORE)/tests -c $< -o $@

# Objects are linked directly (no archives) so strong definitions always
# override the weak stubs.
$(BUILD)/runner: $(CORE_OBJS) $(PLAT_OBJS) $(MAIN_OBJ)
	$(CC) $(LDFLAGS_SAN) -o $@ $^

$(BUILD)/tests: $(CORE_OBJS) $(PLAT_OBJS) $(TEST_OBJS)
	$(CC) $(LDFLAGS_SAN) -o $@ $^

# ---- tests ------------------------------------------------------------------
test: unit scripts

unit: $(BUILD)/tests
	$(BUILD)/tests --golden $(CORE)/tests/golden --tmp $(BUILD)/testtmp

scripts: $(BUILD)/runner
	sh $(CORE)/tests/run_scripts.sh $(BUILD)/runner $(CORE)/tests/scripts $(BUILD)/shots

# ---- generators ---------------------------------------------------------------
GEN_ROOT ?= $(ROOT)
ARTGEN_SRCS := $(sort $(wildcard $(CORE)/tools/artgen*.c) $(wildcard $(CORE)/art/*.c))
ARTGEN_CORE ?= $(wildcard $(addprefix $(CORE)/src/,palette.c fixmath.c fixmath_table.c rng.c))
PY_GENS := $(shell grep -l '^\# doomo-generator' $(ROOT)/tools/*.py 2>/dev/null | sort)
PY_GENS_FIRST := $(filter $(ROOT)/tools/gen_%,$(PY_GENS))
PY_GENS_REST  := $(filter-out $(PY_GENS_FIRST),$(PY_GENS))

$(BUILD)/artgen: $(ARTGEN_SRCS) $(ARTGEN_CORE)
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -g -D_POSIX_C_SOURCE=200809L \
	    -I$(CORE_INC) -I$(CORE)/art -I$(CORE)/tools -o $@ $^

# --- H4: furngen, the dream house's furniture (SPEC R19, C3; HF sec.6.2) ---
FURNGEN_SRCS := $(CORE)/tools/furngen.c $(CORE)/native/png.c $(CORE)/src/palette.c $(CORE)/gen/art_data.c
$(BUILD)/furngen: $(FURNGEN_SRCS) $(wildcard $(CORE_INC)/doomo/*.h)
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -g -D_POSIX_C_SOURCE=200809L \
	    -DFURNGEN_REPO='"$(ROOT)"' -I$(CORE_INC) -I$(CORE)/native -o $@ $(FURNGEN_SRCS)
# --- end H4 ---

gen:
	@set -e; \
	for g in $(PY_GENS_FIRST); do \
	    echo "gen: $$g"; $(PYTHON) $$g --out-root "$(GEN_ROOT)"; \
	done
ifneq ($(strip $(wildcard $(CORE)/tools/artgen*.c)),)
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) $(BUILD)/artgen
	@echo "gen: artgen"; mkdir -p "$(GEN_ROOT)/core/gen"; $(BUILD)/artgen --out-root "$(GEN_ROOT)"
else
	@echo "gen: (no core/tools/artgen*.c yet - skipped)"
endif
	@# --- H4: furngen (after artgen: it links art_data.c's shade tables) ---
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) $(BUILD)/furngen
	@echo "gen: furngen"; mkdir -p "$(GEN_ROOT)/core/gen"; $(BUILD)/furngen --out-root "$(GEN_ROOT)"
	@# --- end H4 ---
	@# --- S2: textpack, the story text packs (SPEC sec.5; reads core/data/story/) ---
	@echo "gen: textpack"; mkdir -p "$(GEN_ROOT)/core/gen"; $(PYTHON) $(CORE)/tools/textpack.py --out-root "$(GEN_ROOT)"
	@# --- end S2 ---
	@set -e; \
	for g in $(PY_GENS_REST); do \
	    echo "gen: $$g"; $(PYTHON) $$g --out-root "$(GEN_ROOT)"; \
	done

check-gen:
	@rm -rf $(BUILD)/gencheck && mkdir -p $(BUILD)/gencheck
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) gen GEN_ROOT=$(BUILD)/gencheck
	@cd $(BUILD)/gencheck && status=0; \
	for f in $$(find . -type f | sort); do \
	    if ! cmp -s "$$f" "$(ROOT)/$$f"; then echo "STALE generated file: $${f#./}"; status=1; fi; \
	done; \
	if [ $$status -eq 0 ]; then echo "check-gen: all generated files up to date"; fi; \
	exit $$status

# ---- cross compile check --------------------------------------------------------
xcheck:
	sh $(CORE)/tools/xcheck.sh

sizes:
	@mkdir -p $(BUILD)/sizes
	@for f in $(CORE_SRCS); do \
	    o=$(BUILD)/sizes/$$(basename $$f .c).o; \
	    arm-none-eabi-gcc -std=c99 -O2 -mthumb -mcpu=arm7tdmi -ffunction-sections \
	        -fdata-sections -I$(CORE_INC) -c $$f -o $$o || exit 1; \
	done; arm-none-eabi-size -t $(BUILD)/sizes/*.o

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD)/obj -name '*.d' 2>/dev/null)
