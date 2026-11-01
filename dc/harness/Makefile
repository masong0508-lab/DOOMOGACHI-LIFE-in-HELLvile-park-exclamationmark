# dc/harness/Makefile - the headless Dreamcast harness (owner: dreamcast).
#
#   make -C dc/harness            build/dcrun (the libretro frontend)
#   make -C dc/harness test       the frontend's self-test against a mock core
#                                 (no emulator needed)
#   make -C dc/harness flycast    build/flycast_libretro.so (build_flycast.sh)
#   make -C dc/harness clean      remove build/ outputs except the flycast .so
#
# Needs a host C compiler, EGL and GL headers (libegl-dev, libgl-dev; Mesa's
# EGL with EGL_MESA_platform_surfaceless at run time).
CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu99 -Wall -Wextra -Werror
B       := build

all: $(B)/dcrun

$(B)/dcrun: dcrun.c png.c png.h dc_mailbox.h libretro.h | $(B)
	$(CC) $(CFLAGS) -rdynamic -o $@ dcrun.c png.c -lEGL -ldl

$(B)/mockcore.so: tests/mockcore.c dc_mailbox.h libretro.h | $(B)
	$(CC) $(CFLAGS) -fPIC -shared -o $@ tests/mockcore.c

$(B):
	mkdir -p $@

test: $(B)/dcrun $(B)/mockcore.so
	sh tests/selftest.sh $(B)

flycast:
	./build_flycast.sh

clean:
	rm -rf $(B)/dcrun $(B)/mockcore.so $(B)/selftest $(B)/run

.PHONY: all test flycast clean
