#!/usr/bin/env bash
# compose_logo.sh ORIGINAL.mp4 WORKDIR OUT.mp4 - assemble the enhanced logo
# clip from WORKDIR/up/*.png + WORKDIR/title_x4plus.png (see enhance_logo.sh).
set -euo pipefail
src="$1"; work="$2"; out="$3"
n=$(ls "$work"/up/*.png | wc -l)
moving=$(awk "BEGIN{print $n/30}")
title=2.6
total=$(awk "BEGIN{print $moving+$title}")
fade_st=$(awk "BEGIN{print $total-0.6}")
ffmpeg -v error -y \
  -framerate 30 -i "$work/up/%04d.png" \
  -loop 1 -framerate 30 -t $title -i "$work/title_x4plus.png" \
  -ss 0.5 -t $total -i "$src" \
  -filter_complex "\
[0:v]scale=1920:1080:flags=lanczos,hqdn3d=0:0:4:4,cas=0.35,eq=saturation=1.06:contrast=1.02,setsar=1,format=yuv420p[a];\
[1:v]scale=1920:1080:flags=lanczos,cas=0.2,setsar=1,format=yuv420p[b];\
[a][b]concat=n=2:v=1:a=0,fade=t=out:st=$fade_st:d=0.6[v];\
[2:a]afftdn=nf=-30,volume=12dB,afade=t=out:st=$fade_st:d=0.6,aresample=48000[au]" \
  -map "[v]" -map "[au]" -r 30 -c:v libx264 -crf 18 -preset slow -tune film \
  -c:a aac -b:a 192k -ac 2 -movflags +faststart "$out"
echo "wrote $out ($total s)"
