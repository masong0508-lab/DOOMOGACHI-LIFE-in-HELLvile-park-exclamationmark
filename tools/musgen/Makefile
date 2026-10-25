# tools/musgen/Makefile - the build-time composer (owner: MU1T;
# docs/design/music/SPEC.md sec.8.3, 16.2). Host only; never part of CI's
# check-gen (the committed core/music/gen/**/*.dms are the source).
#
#   make -C tools/musgen            build/musgen/musgen (with $(CC))
#   make -C tools/musgen check      build with gcc AND clang; both must print the
#                                   reference fingerprint 37d71b12 8d898ef0 c33cfa26 bdcb0bd2
#   make -C tools/musgen renders    the listening renders of the four hubs into
#                                   build/renders/MU1T/ (git-ignored, CF-36)
HERE  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOT  := $(abspath $(HERE)/../..)
OUT   ?= $(ROOT)/build/musgen
CC    ?= gcc
ifeq ($(CC),cc)
CC := gcc
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2
SRCS  := $(HERE)/musgen.c $(HERE)/mg_gen.c $(HERE)/mg_themes.c
FP    := 37d71b12 8d898ef0 c33cfa26 bdcb0bd2
CUES  := title hahaland invasion home
REN   := $(ROOT)/build/renders/MU1T

.PHONY: all check renders clean
all: $(OUT)/musgen

$(OUT)/musgen: $(SRCS) $(HERE)/mg.h
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(OUT)/musgen-%: $(SRCS) $(HERE)/mg.h
	@mkdir -p $(OUT)
	$* $(CFLAGS) -o $@ $(SRCS)

check: $(OUT)/musgen-gcc $(OUT)/musgen-clang
	@for c in gcc clang; do \
	    fp=$$($(OUT)/musgen-$$c fingerprint); \
	    if [ "$$fp" = "$(FP)" ]; then echo "musgen ($$c): fingerprint $$fp OK"; \
	    else echo "musgen ($$c): fingerprint $$fp != $(FP)"; exit 1; fi; \
	done

renders: $(OUT)/musgen
	@mkdir -p $(REN)
	@for c in $(CUES); do \
	    $(OUT)/musgen wav $$c 180 $(REN)/$$c\_morph_180s_ref.wav morph; \
	    $(OUT)/musgen reel $$c $(REN)/$$c\_genreel_ref.wav; \
	done

clean:
	rm -rf $(OUT)
