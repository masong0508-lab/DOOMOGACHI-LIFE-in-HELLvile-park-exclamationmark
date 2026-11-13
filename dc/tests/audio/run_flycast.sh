#!/bin/sh
# run_flycast.sh - DC5's checks on Flycast (owner: dreamcast): the game's
# sound through the AICA stream, in dcrun.
#
#   sh dc/tests/audio/run_flycast.sh FLYCAST.so DCRUN IMAGE NMFILE RUNNER OUTDIR [IRQ_RUNNER]
#
# IRQ_RUNNER (DC5b, an image built with the music engine's IRQ tier): the
# host runner with the same tier (dcsnd_runner_irq); its DCSND_WAV is the
# reference. The native runner's WAV is the bridge's sound, which the IRQ
# tier is not meant to match (60 Hz control frames: faster envelopes on the
# instruments that are not MIF_ENV30, exact pitch, any duty); the pair is
# only reported ("vs bridge" in the summary), not gated. With that tier the
# gaps are not cut (--drop-gaps): the music's own rests are exact silence
# too, and the cut mistook them for stream gaps (title r 0.99 -> 0.63). Combat's
# bar is 0.8 there (measured 0.83; the music lines up tick for tick, the
# scene's own effects differ between dcrun and the runner, see combat.txt).
#
#   title / invasion / combat
#              the script in dcrun with --wav (sound there: expect-audio;
#              no underruns while the game runs at its pace), then the same
#              lines on the native runner (`runner --wav`, the DC-only lines
#              filtered out) and compare.py on the pair: envelope r >= 0.9
#              after lining the two up (Flycast's WAV starts at power on);
#              combat also needs the effects it fires (expect-sfx) and is
#              compared on envelopes clipped at the 97th percentile;
#              the stream's silent gaps while the harness held the game are
#              cut from the Dreamcast WAV first (compare.py --drop-gaps)
#   repeat     title.txt a second time: the content hash after 600 tick
#              slots (dc_mailbox.snd_hash600) must be the same (the sound
#              is a function of the game's ticks)
# OUTDIR/<check>/ gets the logs, the WAVs and the envelope plots;
# OUTDIR/summary.txt one line per check. Exit status 1 when one fails.
set -u
[ $# -eq 6 ] || [ $# -eq 7 ] || { echo "usage: run_flycast.sh FLYCAST DCRUN IMAGE NMFILE RUNNER OUTDIR [IRQ_RUNNER]" >&2; exit 2; }
FLY=$1 DCRUN=$2 IMG=$3 NM=$4 RUNNER=$5 OUT=$6 IRQR=${7:-}
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
sum=$OUT/summary.txt
: > "$sum"
bad=0
res() {   # name status detail
    printf '%-9s %s %s\n' "$1" "$2" "$3" | tee -a "$sum"
    [ "$2" = ok ] || bad=1
}
hash600() { grep -o 'hash600 0x[0-9a-f]*' "$1" | tail -1 | cut -d' ' -f2; }

for name in title invasion combat; do
    d=$OUT/$name
    rm -rf "$d"; mkdir -p "$d"
    "$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
        --shots "$d/shots" --script "$here/$name.txt" --wav "$d/dc.wav" --timeout 600 \
        > "$d/dcrun.log" 2>&1
    r=$?
    # the reference: the lines the native runner knows
    grep -E '^(wait|seed|scene|push|press|hold|expect-scene)( |$)' "$here/$name.txt" > "$d/ref_script.txt"
    "$RUNNER" --quiet --script "$d/ref_script.txt" --shots "$d/ref_shots" --saves "$d/ref_saves" \
        --w 320 --h 240 --wav "$d/ref.wav" > "$d/runner.log" 2>&1
    nr=$?
    vsb=
    if [ -n "$IRQR" ]; then
        mv "$d/ref.wav" "$d/bridge.wav"
        DCSND_WAV="$d/ref.wav" "$IRQR" --quiet --script "$d/ref_script.txt" --shots "$d/ref_shots" --saves "$d/ref_saves" \
            --w 320 --h 240 > "$d/runner_irq.log" 2>&1
        [ $? -eq 0 ] || nr=1
        vsb=$(python3 -I "$here/compare.py" "$d/bridge.wav" "$d/dc.wav" --label "$name" --max-lag 5 \
              --drop-gaps 256 2>&1 | sed 's/^[A-Za-z]* //')
        vsb="; vs bridge (not gated): $vsb"
    fi
    rm -rf "$d/run" "$d/ref_shots" "$d/ref_saves"
    clip=; [ $name = combat ] && clip="--clip-pct 97"   # see combat.txt
    # --drop-gaps: the AICA's silent gaps while the harness held the game
    # (sync stops) are not the game's sound (compare.py, DC8)
    gaps="--drop-gaps 256"; bar=
    [ -n "$IRQR" ] && { gaps=; [ $name = combat ] && bar="--min-corr 0.8"; }
    c=$(python3 -I "$here/compare.py" "$d/ref.wav" "$d/dc.wav" --label "$name" --max-lag 5 \
        $gaps $bar $clip --plot "$d/envelope.png" 2>&1)
    cr=$?
    stats=$(grep '^dcrun: sound' "$d/dcrun.log" | sed 's/^dcrun: //')
    p2p=$(grep -o 'peak-to-peak [0-9]* (min' "$d/dcrun.log" | head -1)
    st=ok; [ $r -eq 0 ] && [ $nr -eq 0 ] && [ $cr -eq 0 ] || st=FAIL
    res "$name" "$st" "(dcrun $r, runner $nr) $p2p; $c; $stats$vsb"
done

# ---- repeat: the same ticks give the same sound ----
d=$OUT/repeat
rm -rf "$d"; mkdir -p "$d"
"$DCRUN" "$FLY" "$IMG" --nm "$NM" --fresh --native-seed 0x5EED1234 --run-dir "$d/run" \
    --shots "$d/shots" --script "$here/title.txt" --timeout 600 > "$d/dcrun.log" 2>&1
r=$?
rm -rf "$d/run"
h1=$(hash600 "$OUT/title/dcrun.log"); h2=$(hash600 "$d/dcrun.log")
st=ok; [ $r -eq 0 ] && [ -n "$h1" ] && [ "$h1" = "$h2" ] && [ "$h1" != 0x00000000 ] || st=FAIL
res repeat "$st" "(dcrun $r) title hash600 $h1 and $h2"
exit $bad
