#!/usr/bin/env bash
# assemble_scene.sh - render one scene binary to a 1080p mp4 with its takes.
#   usage: assemble_scene.sh SCENE_BIN TAKES_ROOT OUT.mp4
# TAKES_ROOT is show_lib/takes (one folder per speaker, then ep101/).
# 1. The audio pass writes the cue list (line_at): "line_id seconds".
# 2. Each cue gets its best take (rec > lib > tts), the dialogue chain, and a
#    per-frame amplitude envelope for lip-sync (ENV_DIR/<id>.env).
# 3. Takes are mixed in at their cues; the video pass reads the envelopes.
set -euo pipefail
bin="$1"; root="$2"; out="$3"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/env"
CUES_OUT="$tmp/cues.txt" "$bin" --wav "$tmp/raw.wav"
ffmpeg -v error -y -i "$tmp/raw.wav" \
  -af "aresample=48000,volume=5dB,alimiter=limit=0.89:attack=3:release=60:level=disabled" -ac 2 "$tmp/synth.wav"

speaker() { local s; s=$(echo "$1" | cut -d_ -f3); case "$s" in bone) echo bonejangles;; *) echo "$s";; esac; }
pick() {  # id -> best take path
  local d="$root/$(speaker "$1")/ep101" k
  for k in rec lib tts; do ls "$d/$1_${k}_"* 2>/dev/null | head -1 && return; done
}
chain() {  # in out
  ffmpeg -nostdin -v error -y -i "$1" -af "aresample=48000,highpass=f=85,afftdn=nf=-38,\
equalizer=f=250:t=q:w=1.2:g=-2.5,equalizer=f=3200:t=q:w=1.4:g=2.5,deesser=i=0.4,\
acompressor=threshold=-22dB:ratio=3:attack=6:release=90:makeup=3,\
aecho=0.9:0.6:45:0.07,loudnorm=I=-18:TP=-3:LRA=7,aresample=48000" -ac 2 "$2"
}
inputs=(-i "$tmp/synth.wav"); filt=""; mixin="[0]"; n=1
while read -r id t; do
  src="$(pick "$id" || true)"
  if [ -z "$src" ]; then echo "no take for $id (bubble + babble only)"; continue; fi
  echo "$id <- $(basename "$src") at ${t}s"
  chain "$src" "$tmp/$id.wav"
  # lip-sync envelope: RMS per 1/30 s, normalised to the take's 95th percentile
  ffmpeg -nostdin -v error -i "$tmp/$id.wav" -ac 1 -ar 48000 -f f32le - | python3 -c "
import sys, numpy as np
x = np.frombuffer(sys.stdin.buffer.read(), dtype=np.float32)
hop = 1600; n = len(x) // hop
r = np.sqrt((x[:n*hop].reshape(n, hop) ** 2).mean(1)) if n else np.zeros(1)
r = np.clip(r / (np.percentile(r, 95) + 1e-6), 0, 1)
r[r < 0.12] = 0
print('\n'.join('%.3f' % v for v in r))" > "$tmp/env/$id.env"
  ms=$(python3 -c "print(int(round($t*1000)))")
  inputs+=(-i "$tmp/$id.wav"); filt+="[$n]adelay=${ms}|${ms}[d$n];"; mixin+="[d$n]"; n=$((n+1))
done < "$tmp/cues.txt"
ffmpeg -v error -y "${inputs[@]}" -filter_complex "${filt}${mixin}amix=inputs=$n:normalize=0:duration=first,alimiter=limit=0.89:level=disabled[o]" \
  -map "[o]" -ar 48000 -ac 2 "$tmp/mix.wav"
ENV_DIR="$tmp/env" "$bin" --raw | ffmpeg -v error -y -f rawvideo -pix_fmt rgb24 -s 240x160 -r 30 -i - -i "$tmp/mix.wav" \
  -vf "scale=1440:960:flags=neighbor,pad=1920:1080:240:60:black,format=yuv420p" \
  -c:v libx264 -crf 16 -preset medium -tune animation -r 30 -c:a aac -b:a 192k -ar 48000 -ac 2 \
  -shortest -movflags +faststart "$out"
mkdir -p "${out%.mp4}_env"; cp "$tmp/env/"*.env "${out%.mp4}_env/"
ENV_DIR="$tmp/env" "$bin" --raw > "${out%.mp4}.rgb"
echo "wrote $out"
