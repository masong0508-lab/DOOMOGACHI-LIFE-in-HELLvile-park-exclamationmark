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

# Experimental real game target: link shared C99 core directly. This
# remains separate from the fully green PS2-0 diagnostic until its title,
# portraits, scene navigation and performance run on PCSX2.
DOOMO_CORE := $(abspath ../core)
include ../core/sources.mk
GAME_CORE_OBJS := $(patsubst $(DOOMO_CORE)/%.c,build/core/%.o,$(CORE_SRCS))
GAME_OBJS := build/game_main.o build/game2d.o build/padmap.o $(GAME_CORE_OBJS)
GAME_BIN := build/doomogachi_ps2_game.elf

.PHONY: game
game: $(GAME_BIN)
$(GAME_BIN): $(GAME_OBJS)
	@mkdir -p $(@D)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $(GAME_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)

build/game_main.o: src/game_main.c src/padmap.h src/game2d.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/game2d.o: src/game2d.c src/game2d.h
	@mkdir -p build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/core/%.o: $(DOOMO_CORE)/%.c
	@mkdir -p $(@D)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
