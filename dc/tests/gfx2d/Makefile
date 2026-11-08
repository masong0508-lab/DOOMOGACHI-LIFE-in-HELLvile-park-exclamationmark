# dc/tests/gfx2d/Makefile - host tests of the Dreamcast 2D layer (DC3; owner:
# dreamcast). Host gcc only: no KallistiOS, no emulator.
#
#   make -C dc/tests/gfx2d            build/host/dc2d_test and dc2d_runner
#   make -C dc/tests/gfx2d test       the unit tests + the runner comparison
#                                     of the UI scripts (run_compare.sh)
#   make -C dc/tests/gfx2d unit       the unit tests only (2D: test_dc2d.c;
#                                     DC7's 3D batches and overworld: test_dc3d.c)
#   make -C dc/tests/gfx2d SAN=1 BUILD=../../build/host-san test   under ASan/UBSan
#
# dc2d_runner is the native runner (core/native) whose 2D calls also drive
# dc/src's portable 2D layer; tee.c compares the two at every shot.
# The core's own objects come from core/build (make -C core lib).

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
BUILD ?= $(ROOT)/dc/build/host
override BUILD := $(abspath $(BUILD))
CC := gcc
OPT ?= -O2
# SAN=1: everything under ASan/UBSan (core objects from core/build-san, the
# core's own SAN build directory)
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror $(OPT) -g -MMD -MP -DDOOMO_PARTS_LZ_TEST \
          -D_POSIX_C_SOURCE=200809L -DDC2D_HOST -I$(CORE_INC) -I$(CORE)/native \
          -I$(ROOT)/dc/src -I$(ROOT)/dc/harness -I$(HERE)

CORE_OBJS   := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(CORE_SRCS))
NATIVE_ALL  := $(sort $(wildcard $(CORE)/native/*.c))
NATIVE_OWN  := $(CORE)/native/plat_native.c $(CORE)/native/runner_main.c \
               $(CORE)/native/raycast.c $(CORE)/native/overworld.c
NATIVE_KEEP := $(filter-out $(NATIVE_OWN),$(NATIVE_ALL))
NATIVE_OBJS := $(patsubst $(CORE)/%.c,$(CORE_BUILD)/obj/%.o,$(NATIVE_KEEP))

DC2D_SRCS := $(addprefix $(ROOT)/dc/src/,gfx2d.c texcache.c font.c pal.c dc3d.c overworld.c)
DC2D_OBJS := $(patsubst $(ROOT)/dc/src/%.c,$(BUILD)/dc2d/%.o,$(DC2D_SRCS))
TEE_OBJS  := $(BUILD)/tee.o $(BUILD)/swpvr.o $(BUILD)/png.o $(BUILD)/plat_native_ref.o $(BUILD)/weak_fx_ref.o \
             $(BUILD)/raycast_ref.o $(BUILD)/overworld_ref.o $(BUILD)/weak_fx_sw.o

# the native 2D calls become the reference (ref_*); tee.c defines plat_*
REN_NAT := -Dplat_begin_frame=ref_begin_frame -Dplat_end_frame=ref_end_frame \
           -Dplat_clear=ref_clear -Dplat_rect=ref_rect -Dplat_shade_rect=ref_shade_rect \
           -Dplat_checker=ref_checker -Dplat_line=ref_line -Dplat_text=ref_text \
           -Dplat_image=ref_image -Dplat_hold_frame=ref_hold_frame
REN_FX  := -Dplat_image_xform=ref_image_xform -Dplat_post_offset=ref_post_offset \
           -Dplat_post_scale=ref_post_scale -Dplat_post_blend=ref_post_blend \
           -Dplat_post_iris=ref_post_iris -Dplat_pal_lerp=ref_pal_lerp \
           -Dplat_room_ui_opaque=ref_room_ui_opaque \
           -Dplat_rect=ref_rect -Dplat_image=ref_image -Dplat_shade_rect=ref_shade_rect \
           -Dplat_checker=ref_checker

.PHONY: all test unit compare corelib clean
all: $(BUILD)/dc2d_test $(BUILD)/dc3d_test $(BUILD)/dc2d_runner

corelib:
	@$(MAKE) --no-print-directory -C $(CORE) lib $(NATIVE_OBJS) BUILD=$(CORE_BUILD) SAN=$(SAN)

$(CORE_OBJS) $(NATIVE_OBJS): corelib

$(BUILD)/dc2d/%.o: $(ROOT)/dc/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/tee.o: $(HERE)/tee.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/swpvr.o: $(HERE)/swpvr.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_dc2d.o: $(HERE)/test_dc2d.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_dc3d.o: $(HERE)/test_dc3d.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/png.o: $(ROOT)/dc/harness/png.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/plat_native_ref.o: $(CORE)/native/plat_native.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REN_NAT) -c $< -o $@

$(BUILD)/raycast_ref.o: $(CORE)/native/raycast.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Dplat_view3d=ref_view3d -c $< -o $@

$(BUILD)/overworld_ref.o: $(CORE)/native/overworld.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Dplat_overworld=ref_overworld -c $< -o $@

# DC3_FX=sw: the software post effects drawn through the tee (both sides)
REN_SW := -Dplat_image_xform=sw_image_xform -Dplat_post_offset=sw_post_offset \
          -Dplat_post_scale=sw_post_scale -Dplat_post_blend=sw_post_blend \
          -Dplat_post_iris=sw_post_iris -Dplat_pal_lerp=sw_pal_lerp \
          -Dplat_room_ui_opaque=sw_room_ui_opaque
$(BUILD)/weak_fx_sw.o: $(CORE)/src/plat_weak_fx.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REN_SW) -c $< -o $@

$(BUILD)/weak_fx_ref.o: $(CORE)/src/plat_weak_fx.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REN_FX) -c $< -o $@

$(BUILD)/runner_main_tee.o: $(CORE)/native/runner_main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Dnat_png_write=tee_png_write -c $< -o $@

$(BUILD)/dc2d_runner: corelib $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/runner_main_tee.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/runner_main_tee.o -lm

$(BUILD)/dc2d_test: corelib $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/test_dc2d.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/test_dc2d.o -lm

# DC7: the 3D batches and the overworld (test_dc3d.c)
$(BUILD)/dc3d_test: corelib $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/test_dc3d.o
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJS) $(NATIVE_OBJS) $(DC2D_OBJS) $(TEE_OBJS) $(BUILD)/test_dc3d.o -lm

unit: $(BUILD)/dc2d_test $(BUILD)/dc3d_test
	$(BUILD)/dc2d_test
	$(BUILD)/dc3d_test

compare: $(BUILD)/dc2d_runner
	sh $(HERE)/run_compare.sh $(BUILD)/dc2d_runner $(CORE)/tests/scripts $(BUILD)/shots

test: unit compare

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d $(BUILD)/dc2d/*.d)
