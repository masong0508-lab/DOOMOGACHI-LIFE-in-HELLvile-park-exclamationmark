# DOOMOGACHI LIFE teaser trailer - pixel animation rendered with the game's
# real palette.  make -> build/trailer.mp4 (+ .gif)   make preview SCENE=s04
BUILD   ?= build
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -I. -I../core/include -I$(BUILD)
SCENE_SRCS ?= $(wildcard scenes/*.c)
MUSIC_SRCS ?= $(wildcard music.c)
SRCS    := tk.c tk_chibi.c timeline.c main.c $(MUSIC_SRCS) $(SCENE_SRCS)
FFMPEG  ?= ffmpeg

all: $(BUILD)/trailer.mp4 $(BUILD)/trailer.gif

$(BUILD)/pal.h: ../core/src/palette.c pal_gen.py
	@mkdir -p $(BUILD)
	python3 pal_gen.py $< $@

$(BUILD)/trailer: $(SRCS) tk.h timeline.h $(BUILD)/pal.h
	$(CC) $(CFLAGS) $(SRCS) -o $@ -lm

$(BUILD)/trailer.wav: $(BUILD)/trailer
	$(BUILD)/trailer --wav $@

# mastered soundtrack: 48 kHz stereo, +5 dB into a -1 dBFS limiter (about -16 LUFS,
# true peak about -1.6 dBFS; the raw synth mix sits at -21 LUFS)
$(BUILD)/trailer_master.wav: $(BUILD)/trailer.wav
	$(FFMPEG) -y -loglevel error -i $< \
	  -af "aresample=48000,volume=5dB,alimiter=limit=0.89:attack=3:release=60:level=disabled" -ac 2 $@

$(BUILD)/trailer.mp4: $(BUILD)/trailer $(BUILD)/trailer_master.wav
	$(BUILD)/trailer --raw | $(FFMPEG) -y -loglevel error -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - \
	  -i $(BUILD)/trailer_master.wav -vf scale=960:640:flags=neighbor -c:v libx264 -crf 16 -preset slow \
	  -tune animation -pix_fmt yuv420p -c:a aac -b:a 192k -ar 48000 -shortest -movflags +faststart $@

$(BUILD)/trailer.gif: $(BUILD)/trailer
	$(BUILD)/trailer --raw | $(FFMPEG) -y -loglevel error -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - \
	  -vf "fps=15,scale=480:320:flags=neighbor,split[a][b];[a]palettegen=max_colors=256:stats_mode=full[p];[b][p]paletteuse=dither=none" $@

preview: $(BUILD)/trailer
	$(BUILD)/trailer --sheet $(SCENE) $(BUILD)/sheet_$(SCENE).png

list: $(BUILD)/trailer
	$(BUILD)/trailer --list

clean:
	rm -rf $(BUILD)

.PHONY: all preview list clean
