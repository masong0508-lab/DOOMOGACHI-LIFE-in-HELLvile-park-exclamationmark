# core/Makefile - native build, tests and generators for the shared core
# (owner: foundation).
#
#   make -C core              build the headless runner (default)
#   make -C core runner       $(BUILD)/runner  (see core/native/README.md)
#   make -C core lib          compile every core object (src + gen)
#   make -C core test         unit tests + runner scripts at 240x160 and 320x240 + parts-check
#                             + the journey budgets (journeys)
#   make -C core journeys     the journey and tap meter: core/tests/journeys/
#                             against budgets.tsv at both sizes (M0)
#   make -C core gen          run every generator (tolerates missing ones)
#   make -C core check-gen    regenerate into a temp dir and diff vs the repo
#   make -C core xcheck       compile all core sources with gcc, arm, mipsel and
#                             sh4 -Werror (core/tools/xcheck.sh)
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
#   * Last, ulgen (core/tools/ulgen.c, linked with the native core objects)
#     writes DIR/core/gen/unlock_index.c from the unlock registry.
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
CORE_CFLAGS   := $(CFLAGS_COMMON) -DDOOMO_HITCHECK -DDOOMO_PARTS_LZ_TEST -DDOOMO_PART_AGE_TEST -I$(CORE_INC)
# Native platform + tests may use the host libc.
HOST_CFLAGS   := $(CFLAGS_COMMON) -DDOOMO_PARTS_LZ_TEST -DDOOMO_PART_AGE_TEST -D_POSIX_C_SOURCE=200809L -I$(CORE_INC) -I$(CORE)/native \
                 -DNAT_TALK_DIR='"$(BUILD)/talk"'

NATIVE_SRCS := $(sort $(wildcard $(CORE)/native/*.c))
NATIVE_MAIN := $(CORE)/native/runner_main.c
NATIVE_PLAT := $(filter-out $(NATIVE_MAIN),$(NATIVE_SRCS))
TEST_SRCS   := $(sort $(wildcard $(CORE)/tests/*.c))

obj = $(patsubst $(CORE)/%.c,$(BUILD)/obj/%.o,$(1))
CORE_OBJS   := $(call obj,$(CORE_SRCS))
PLAT_OBJS   := $(call obj,$(NATIVE_PLAT))
MAIN_OBJ    := $(call obj,$(NATIVE_MAIN))
TEST_OBJS   := $(call obj,$(TEST_SRCS))

.PHONY: all lib runner test unit scripts journeys gen check-gen xcheck sizes clean story-check talk talk-check parts-check part-icons-sheet
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
test: story-check talk-check parts-check unit scripts journeys

unit: $(BUILD)/tests talk
	$(BUILD)/tests --golden $(CORE)/tests/golden --tmp $(BUILD)/testtmp

# --- T1/T2: Talk, the word network (docs/TALK.md) ---
# talk        $(BUILD)/talk/TALK_en.BIN (the native runner and test_talk_db
#             read it; never committed; the disc builds make their own)
# talk-check  every talkc.py lint and its seeded bad inputs (in test)
TALK_DEPS := $(CORE)/tools/talkc.py $(CORE)/tools/talkc_parse.py $(CORE)/tools/talkc_safe.py $(CORE)/tools/talkc_gen.py \
             $(CORE)/include/doomo/talk.h $(CORE)/gen/txt_dict.c $(CORE)/data/story/register.tsv \
             $(shell find $(CORE)/data/talk -type f 2>/dev/null)
TALK_GENTEST := $(wildcard $(CORE)/tests/talk_gen/*.tlk)
talk: $(BUILD)/talk/TALK_en.BIN $(BUILD)/talk/TALK_gentest.BIN
$(BUILD)/talk/TALK_en.BIN: $(TALK_DEPS)
	@mkdir -p $(dir $@)
	@$(PYTHON) $(CORE)/tools/talkc.py --bin --lang en --out $@
# T5a: the generator tests' grammar (core/tests/talk_gen/) with the English lexicon
$(BUILD)/talk/TALK_gentest.BIN: $(TALK_DEPS) $(TALK_GENTEST)
	@mkdir -p $(dir $@)
	@$(PYTHON) $(CORE)/tools/talkc.py --bin --lang en --grammar $(CORE)/tests/talk_gen --out $@
talk-check:
	@$(PYTHON) $(CORE)/tools/talkc.py --check
	@$(PYTHON) $(CORE)/tools/talkc.py --selftest
# --- end T1/T2 ---

# --- T7: the census (docs/TALK.md "The census"; design 3.7). The full run:
# 2,000,000 contexts and 2,000,000 bot conversations against the design's
# targets (exit 1 on a miss). The unit tests run a smoke census
# (core/tests/test_talk_census.c). ---
.PHONY: talk-census
$(BUILD)/talk_census: $(CORE)/tools/talk_census.c $(CORE)/tools/talk_census.h $(CORE_OBJS) $(PLAT_OBJS)
	$(CC) $(HOST_CFLAGS) -I$(CORE)/tools -o $@ $(CORE)/tools/talk_census.c $(CORE_OBJS) $(PLAT_OBJS) $(LDFLAGS_SAN)
talk-census: $(BUILD)/talk_census talk
	$(BUILD)/talk_census --full --grid $(CORE)/data/talk/census_en.tsv
# --- end T7 ---

scripts: $(BUILD)/runner
	sh $(CORE)/tests/run_scripts.sh $(BUILD)/runner $(CORE)/tests/scripts $(BUILD)/shots

# --- M0: the journey and tap meter (docs/GAME_DESIGN.md "UX budgets") ---
journeys: $(BUILD)/runner
	sh $(CORE)/tests/run_journeys.sh $(BUILD)/runner $(CORE)/tests/journeys $(BUILD)/journeys
# --- end M0 ---

# --- CR1/CR2: the part tools' seeded rejects (parts_build.py, partcat_build.py) ---
parts-check:
	@$(PYTHON) $(ROOT)/tools/parts_build.py --selftest
	@$(PYTHON) $(ROOT)/tools/partcat_build.py --selftest
# --- end CR1/CR2 ---

# --- P0: the script-rewrite guardrails (docs/STORY_ENGINE.md "Script tools") ---
# The Spanish story TSVs against the packs, the save and flow locks, and the
# pack sizes. STORY_ES=rewrite (the default while the English is rewritten,
# DECISIONS: Spanish is redone after the rewrite) reports stale and missing
# Spanish rows without failing; STORY_ES=strict fails on them. Overflow,
# a Spanish pack that cannot load and any lock break always fail.
STORY_ES ?= rewrite
STORY_ES_FLAGS_rewrite := --allow-missing --allow-stale
STORY_ES_FLAGS_strict  :=
story-check:
	@$(PYTHON) $(CORE)/tools/textpack.py --check-es $(STORY_ES_FLAGS_$(STORY_ES))
	@$(PYTHON) $(CORE)/tools/story_opslock.py --check
	@# --- CL6 (clarity C7 + C16): every objective names where, fits the pill; one name per place ---
	@$(PYTHON) $(CORE)/tools/story_wayfind.py
	@$(PYTHON) $(CORE)/tools/story_wayfind.py --selftest
	@$(PYTHON) $(CORE)/tools/textpack.py --sizes
	@# --- E5: the PG-21 register lint (nothing adult near a kid) and its seeded bad tree ---
	@$(PYTHON) $(CORE)/tools/story_register.py
	@$(PYTHON) $(CORE)/tools/story_register.py --selftest
	@# --- E0: user packs (textpack --user): the SFW fixture rebuilds byte for byte,
	@# every seeded bad one fails, `show` stays in role=mslot|ending packs ---
	@mkdir -p "$(BUILD)/e0pack"
	@$(PYTHON) $(CORE)/tools/textpack.py --user $(CORE)/tests/fixtures/e0_user.dsc --out "$(BUILD)/e0pack/e0_user.pack" >/dev/null
	@cmp "$(BUILD)/e0pack/e0_user.pack" $(CORE)/tests/fixtures/e0_user.pack || \
	    { echo "story-check: core/tests/fixtures/e0_user.pack is stale (textpack.py --user)"; exit 1; }
	@for f in $(CORE)/tests/fixtures/e0_bad/*.dsc; do \
	    if $(PYTHON) $(CORE)/tools/textpack.py --user $$f --out "$(BUILD)/e0pack/bad.pack" >/dev/null 2>&1; then \
	        echo "story-check: $$f compiled, but a user pack may not do that"; exit 1; fi; done
	@if $(PYTHON) $(CORE)/tools/textpack.py --data $(CORE)/tests/fixtures/e0_show_builtin \
	    --out-root "$(BUILD)/e0pack/neg" >/dev/null 2>&1; then \
	    echo "story-check: show compiled in a pack without role=mslot|ending"; exit 1; fi
	@$(PYTHON) $(CORE)/tools/textpack.py --data $(CORE)/tests/fixtures/e0_builtin_ok \
	    --out-root "$(BUILD)/e0pack/ok" >/dev/null
	@# --- E7: the content pack tool (tools/mpack): format read from pack.h, seeded refusals ---
	@$(PYTHON) -I $(ROOT)/tools/mpack/test_mpack.py | tail -n 1
	@$(PYTHON) -I $(ROOT)/tools/mpack/mpack.py example --out "$(BUILD)/e0pack/example.dmpk" >/dev/null
	@cmp "$(BUILD)/e0pack/example.dmpk" $(CORE)/tests/fixtures/example.dmpk || \
	    { echo "story-check: core/tests/fixtures/example.dmpk is stale (tools/mpack/mpack.py example)"; exit 1; }
	@echo "story-check: user packs OK (fixture byte-identical, $$(ls $(CORE)/tests/fixtures/e0_bad/*.dsc | wc -l) seeded refusals)"
# --- end P0 ---

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

# --- SP1 (X3): ulgen, unlock_find's key index (core/gen/unlock_index.c). It links the native core (the unlock
# tables are every team's, weak defaults and all), so it is built from the same objects as the tests. ---
$(BUILD)/obj/tools/%.o: $(CORE)/tools/%.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -c $< -o $@
$(BUILD)/ulgen: $(CORE_OBJS) $(PLAT_OBJS) $(BUILD)/obj/tools/ulgen.o
	$(CC) $(LDFLAGS_SAN) -o $@ $^
# --- end SP1 ---

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
	@# --- CR7: the creator's part icons (links the core: the real renderer and catalog) ---
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) $(BUILD)/part_icons
	@echo "gen: part_icons"; mkdir -p "$(GEN_ROOT)/core/gen"; $(BUILD)/part_icons --out-root "$(GEN_ROOT)" >/dev/null
	@# --- end CR7 ---
	@# --- S2: textpack, the story text packs (SPEC sec.5; reads core/data/story/) ---
	@echo "gen: textpack"; mkdir -p "$(GEN_ROOT)/core/gen"; $(PYTHON) $(CORE)/tools/textpack.py --out-root "$(GEN_ROOT)"
	@# --- end S2 ---
	@# --- T1: talkc, the Talk word network (after textpack: it encodes with
	@# txt_dict.c's code): gen_game.tsv, concepts.lock, talk_fixture.c ---
	@echo "gen: talkc"; $(PYTHON) $(CORE)/tools/talkc.py --out-root "$(GEN_ROOT)"
	@# --- end T1 ---
	@set -e; \
	for g in $(PY_GENS_REST); do \
	    echo "gen: $$g"; $(PYTHON) $$g --out-root "$(GEN_ROOT)"; \
	done
	@# --- SP1 (X3): ulgen last (it links the core as committed, other generators' outputs included) ---
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) $(BUILD)/ulgen
	@echo "gen: ulgen"; mkdir -p "$(GEN_ROOT)/core/gen"; $(BUILD)/ulgen --out-root "$(GEN_ROOT)"
	@# --- end SP1 ---

# --- CR7: the part icon tool (core/tools/part_icons.c): `make -C core part-icons-sheet` writes the
# icon contact sheet to $(BUILD)/sheets/part_icons.png ---
$(BUILD)/obj/tools/part_icons.o: $(CORE)/tools/part_icons.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -c $< -o $@
$(BUILD)/part_icons: $(CORE_OBJS) $(PLAT_OBJS) $(BUILD)/obj/tools/part_icons.o
	$(CC) $(LDFLAGS_SAN) -o $@ $^
part-icons-sheet: $(BUILD)/part_icons
	@mkdir -p $(BUILD)/sheets
	$(BUILD)/part_icons --sheet $(BUILD)/sheets
# --- end CR7 ---

check-gen:
	@rm -rf $(BUILD)/gencheck && mkdir -p $(BUILD)/gencheck
	@$(MAKE) --no-print-directory -f $(CORE)/Makefile BUILD=$(BUILD) gen GEN_ROOT=$(BUILD)/gencheck
	@cd $(BUILD)/gencheck && status=0; \
	for f in $$(find . -type f | sort); do \
	    if ! cmp -s "$$f" "$(ROOT)/$$f"; then echo "STALE generated file: $${f#./}"; status=1; fi; \
	done; \
	if [ $$status -eq 0 ]; then echo "check-gen: all generated files up to date"; fi; \
	exit $$status
	@# --- L1: also no left-over language table (a language that lost its strings) ---
	@$(PYTHON) $(ROOT)/tools/langpack.py --check

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
