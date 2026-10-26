# pilot_proof - a ~70 s proof scene for the pilot cold open, built on the
# teaser trailer's toolkit (../trailer: tk.c, tk_chibi.c, main.c, palette,
# synth), which it reuses without modifying.
#   make BUILD=dir             -> dir/pilot (renderer: --list/--png/--sheet/--raw/--wav)
#   make sheet SHOT=p04        -> contact sheet of one shot
#   ./assemble.sh dir/pilot HUM.wav OUT.mp4   -> 1080p cut with the hum take
BUILD   ?= build
CC      ?= gcc
TR      := ../trailer
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -I. -I$(TR) -I../core/include -I$(BUILD)
SRCS    := $(TR)/tk.c $(TR)/tk_chibi.c $(TR)/main.c timeline.c wd.c mayor.c sets.c shots.c story.c music.c

$(BUILD)/pilot: $(SRCS) wd.h mayor.h sets.h $(TR)/tk.h $(TR)/timeline.h $(BUILD)/pal.h
	$(CC) $(CFLAGS) $(SRCS) -o $@ -lm

$(BUILD)/pal.h: ../core/src/palette.c $(TR)/pal_gen.py
	@mkdir -p $(BUILD)
	python3 $(TR)/pal_gen.py $< $@

sheet: $(BUILD)/pilot
	$(BUILD)/pilot --sheet $(SHOT) $(BUILD)/sheet_$(SHOT).png

.PHONY: sheet
