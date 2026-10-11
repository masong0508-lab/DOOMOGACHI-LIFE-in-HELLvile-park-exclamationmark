#!/usr/bin/env bash
# assemble.sh - final 1080p cut: Dippinn Productions logo, then the pixel
# trailer scaled 6x (1440x960, nearest neighbour) pillarboxed in 1920x1080.
#   usage: assemble.sh TRAILER_BIN OUT.mp4 [OUT.gif]
# TRAILER_BIN is the built trailer renderer (make build/trailer).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
bin="$1"; out="$2"; gif="${3:-}"
logo="$here/assets/dippinn_logo.mp4"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

"$bin" --wav "$tmp/raw.wav"
# master the synth mix the same way the Makefile does (+5 dB into a -1 dBFS limiter, ~-16 LUFS)
ffmpeg -v error -y -i "$tmp/raw.wav" \
  -af "aresample=48000,volume=5dB,alimiter=limit=0.89:attack=3:release=60:level=disabled" -ac 2 "$tmp/trailer.wav"
# 1) the pixel trailer at 1080p (30 fps, 48 kHz stereo)
"$bin" --raw | ffmpeg -v error -y -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - -i "$tmp/trailer.wav" \
  -vf "scale=1440:960:flags=neighbor,pad=1920:1080:240:60:black,format=yuv420p" \
  -c:v libx264 -crf 16 -preset slow -r 30 -c:a aac -b:a 192k -ar 48000 -ac 2 -shortest "$tmp/body.mp4"
# 2) logo first, then the trailer
ffmpeg -v error -y -i "$logo" -i "$tmp/body.mp4" -filter_complex \
  "[0:v]fps=30,scale=1920:1080,setsar=1,format=yuv420p[v0];[1:v]setsar=1,format=yuv420p[v1];\
   [0:a]aresample=48000,loudnorm=I=-21:TP=-2:LRA=11,aresample=48000,aformat=channel_layouts=stereo[a0];[1:a]aresample=48000,aformat=channel_layouts=stereo[a1];\
   [v0][a0][v1][a1]concat=n=2:v=1:a=1[v][a]" \
  -map "[v]" -map "[a]" -c:v libx264 -crf 17 -preset slow -c:a aac -b:a 192k -movflags +faststart "$out"
# 3) optional GIF: logo letterboxed into 480x320, then the trailer at 2x
if [ -n "$gif" ]; then
  ffmpeg -v error -y -i "$logo" -vf "fps=12,scale=480:270,pad=480:320:0:25:black" "$tmp/logo_gif.mp4" -an
  "$bin" --raw | ffmpeg -v error -y -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - \
    -vf "fps=15,scale=480:320:flags=neighbor" -c:v libx264 -crf 12 "$tmp/body_gif.mp4"
  ffmpeg -v error -y -i "$tmp/logo_gif.mp4" -i "$tmp/body_gif.mp4" -filter_complex \
    "[0:v]fps=15,setsar=1[a];[1:v]fps=15,setsar=1[b];[a][b]concat=n=2:v=1[v];\
     [v]split[x][y];[x]palettegen=max_colors=256:stats_mode=full[p];[y][p]paletteuse=dither=none" "$gif"
fi
echo "wrote $out ${gif}"
