# core/sources.mk - the portable core source list (owner: foundation).
#
# Included by core/Makefile, gba/Makefile and (mirrored by) the ps1 build.
# Every *.c in core/src and core/gen is part of the core, so new files
# from any module are picked up automatically - never list files by hand.
#
#   include path/to/core/sources.mk
#   -> DOOMO_CORE  absolute path of core/
#      CORE_INC    include dir   (use -I$(CORE_INC))
#      CORE_SRCS   all portable core sources (absolute paths, sorted)
#
# Link the resulting objects DIRECTLY (never through a .a archive): the
# weak placeholders in src/stubs.c must be overridden by the owners'
# strong definitions, and an archive member is not pulled in to replace a
# weak symbol that is already defined. Embedded targets should compile
# with -ffunction-sections -fdata-sections and link with --gc-sections so
# overridden weak stub data (and save_scratch.c's default buffer) costs
# no RAM.
ifndef DOOMO_CORE
DOOMO_CORE := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
endif
CORE_INC  := $(DOOMO_CORE)/include
CORE_SRCS := $(sort $(wildcard $(DOOMO_CORE)/src/*.c) $(wildcard $(DOOMO_CORE)/gen/*.c))
