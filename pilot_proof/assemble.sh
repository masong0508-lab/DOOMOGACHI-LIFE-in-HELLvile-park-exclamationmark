#!/usr/bin/env bash
# assemble.sh - final 1080p cut of the pilot proof.
#   usage: assemble.sh PILOT_BIN TAKES_DIR OUT.mp4
# PILOT_BIN: the built renderer (make BUILD=dir -> dir/pilot).
# TAKES_DIR: the show's locked Mayor takes (show_lib/takes/mayor/ep101), named
#   as in show/ep101/voice.lock: co12_hum (the creator's recorded hum),
#   co13_heehee, co15_perfect, co18_stay.
# The pixel frames are scaled 6x nearest-neighbour to 1440x960 and
# pillarboxed in 1920x1080 (like trailer/assemble.sh); audio is AAC 48 kHz.
set -euo pipefail
bin="$1"; takes="$2"; out="$3"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

# synth score + sfx, mastered like the teaser (+5 dB into a -1 dBFS limiter)
"$bin" --wav "$tmp/raw.wav"
ffmpeg -v error -y -i "$tmp/raw.wav" \
  -af "aresample=48000,volume=5dB,alimiter=limit=0.89:attack=3:release=60:level=disabled" -ac 2 "$tmp/synth.wav"

# dialogue chain for each take: rumble cut, gentle denoise, de-mud, presence,
# de-ess, 3:1 compression, a touch of outdoor slap so it sits in the plaza,
# then loudness-matched (-19 LUFS for lines, -21 for the hum under the scene)
chain() {  # in out lufs
  ffmpeg -v error -y -i "$1" -af "aresample=48000,highpass=f=85,afftdn=nf=-38,\
equalizer=f=250:t=q:w=1.2:g=-2.5,equalizer=f=3200:t=q:w=1.4:g=2.5,deesser=i=0.4,\
acompressor=threshold=-22dB:ratio=3:attack=6:release=90:makeup=3,\
aecho=0.9:0.6:45:0.07,loudnorm=I=$3:TP=-3:LRA=7,aresample=48000" -ac 2 "$2"
}
pick() { ls "$takes"/$1_* | head -1; }
chain "$(pick co12_hum_rec)"   "$tmp/hum.wav"    -21
chain "$(pick co13_heehee_lib)" "$tmp/heehee.wav" -18
chain "$(pick co15_perfect_tts)" "$tmp/perfect.wav" -18
chain "$(pick co18_stay_tts)"  "$tmp/stay.wav"   -18

# placements (s), from timeline.c and the shot frames; minus the 30 ms take preroll
d_hum=3.35; d_hee=13.137; d_perf=33.437; d_stay=44.57
ms() { python3 -c "print(int(round($1*1000)))"; }
ffmpeg -v error -y -i "$tmp/synth.wav" -i "$tmp/hum.wav" -i "$tmp/heehee.wav" -i "$tmp/perfect.wav" -i "$tmp/stay.wav" \
  -filter_complex "[1]adelay=$(ms $d_hum)|$(ms $d_hum)[h];[2]adelay=$(ms $d_hee)|$(ms $d_hee)[a];\
[3]adelay=$(ms $d_perf)|$(ms $d_perf)[b];[4]adelay=$(ms $d_stay)|$(ms $d_stay)[c];\
[0][h][a][b][c]amix=inputs=5:normalize=0:duration=first,alimiter=limit=0.89:level=disabled[o]" \
  -map "[o]" -ar 48000 -ac 2 "$tmp/mix.wav"

"$bin" --raw | ffmpeg -v error -y -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - -i "$tmp/mix.wav" \
  -vf "scale=1440:960:flags=neighbor,pad=1920:1080:240:60:black,format=yuv420p" \
  -c:v libx264 -crf 16 -preset medium -tune animation -r 30 -c:a aac -b:a 192k -ar 48000 -ac 2 \
  -shortest -movflags +faststart "$out"
cp "$tmp/mix.wav" "${out%.mp4}_mix.wav"
echo "wrote $out"
