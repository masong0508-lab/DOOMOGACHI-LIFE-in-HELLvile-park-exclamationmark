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
EE_CFLAGS += -std=c99 -ffunction-sections -fdata-sections
EE_LDFLAGS += -L$(GSKIT)/lib -Wl,--gc-sections
EE_LIBS := -lgskit_toolkit -lgskit -ldmakit -lpad -lm
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
