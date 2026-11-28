#!/bin/sh
# mkcdi.sh ELF IP_TXT WORKDIR CDI [PACK [INTRO [TALKDIR]]] - build the self-booting Dreamcast disc
# image from the linked ELF (owner: dreamcast). Called by `make -C dc disc`.
#
#   ELF --objcopy--> doomogachi.bin --scramble--> WORKDIR/cd/1ST_READ.BIN
#   ip.txt --makeip--> WORKDIR/IP.BIN (the bootstrap: original title and
#                      maker strings; makeip adds the hardware id the boot
#                      ROM checks and its own default logo-free template)
#   WORKDIR/cd + IP.BIN --mkisofs -C 0,11702 -G IP.BIN--> doomogachi.iso
#   doomogachi.iso --cdi4dc--> CDI (audio/data image, MSINFO 11702)
# An optional PACK (a DMPK pack for port dc, E10) is copied to the disc root as PACK.DMP.
# An optional INTRO (the boot FMV, dc/tools/mkfmv.py, DC9C) is copied there as INTRO.DMV.
# An optional TALKDIR (core/build/talk, TD1) gives the disc Talk's databases: every TALK_<code>.BIN in
# it is copied to the disc root in capitals (TALK_EN.BIN), with a lossless TLZ1 packed copy
# (TALK_EN.LZB) preferred by dc/src/talkdisc.c; the test
# bank TALK_gentest.BIN stays off the disc.
# 1ST_READ.BIN and IP.BIN are also copied next to the CDI.
#
# Tools: the KOS sh-elf objcopy, and scramble, makeip, cdi4dc from
# dc/tools/setup_kos.sh (DC_TOOLS_BASE or <prefix>/bin), genisoimage or
# mkisofs (or xorriso -as mkisofs) from the host.
set -eu
elf=$1; iptxt=$2; work=$3; cdi=$4; pack=${5:-}; intro=${6:-}; talkdir=${7:-}
tools=${DC_TOOLS_BASE:-}
need() {
    for c in "$@"; do
        if [ -n "$tools" ] && [ -x "$tools/$c" ]; then echo "$tools/$c"; return 0; fi
        if command -v "$c" >/dev/null 2>&1; then command -v "$c"; return 0; fi
    done
    return 1
}
die() { printf '\n*** mkcdi: %s\n    (docs/DREAMCAST.md, "Disc image")\n\n' "$*" >&2; exit 1; }
objcopy=$(need sh-elf-objcopy) || objcopy=${KOS_CC_BASE:-/opt/toolchains/dc/sh-elf}/bin/sh-elf-objcopy
[ -x "$objcopy" ] || die "sh-elf-objcopy not found (source <prefix>/kos/environ.sh)"
scramble=$(need scramble) || die "scramble not found (dc/tools/setup_kos.sh installs it)"
makeip=$(need makeip) || die "makeip not found (dc/tools/setup_kos.sh installs it)"
cdi4dc=$(need cdi4dc) || die "cdi4dc not found (dc/tools/setup_kos.sh installs it)"
if iso=$(need genisoimage mkisofs); then isoargs=""
elif iso=$(need xorriso); then isoargs="-as mkisofs"
else die "no genisoimage, mkisofs or xorriso (Ubuntu/Debian: sudo apt install genisoimage)"
fi

rm -rf "$work"
mkdir -p "$work/cd"
"$objcopy" -R .stack -O binary "$elf" "$work/doomogachi.bin"
"$scramble" "$work/doomogachi.bin" "$work/cd/1ST_READ.BIN"
if [ -n "$pack" ]; then
    [ -f "$pack" ] || die "pack $pack does not exist"
    cp "$pack" "$work/cd/PACK.DMP"
fi
if [ -n "$intro" ]; then
    [ -f "$intro" ] || die "intro $intro does not exist"
    cp "$intro" "$work/cd/INTRO.DMV"
fi
if [ -n "$talkdir" ]; then
    n=0
    for f in "$talkdir"/TALK_??.BIN "$talkdir"/TALK_??-??.BIN; do
        [ -f "$f" ] || continue
        b=$(basename "$f" | tr '[:lower:]' '[:upper:]')
        cp "$f" "$work/cd/$b"
        python3 "$(dirname "$0")/talkpack.py" "$f" "$work/cd/${b%.BIN}.LZB"
        n=$((n + 1))
    done
    [ "$n" -gt 0 ] || die "no TALK_<code>.BIN in $talkdir (make -C core talk)"
fi
"$makeip" -f "$iptxt" "$work/IP.BIN" >/dev/null
# -C 0,11702: the data session starts after cdi4dc's audio track (MSINFO)
# shellcheck disable=SC2086
"$iso" $isoargs -quiet -C 0,11702 -V BOREAGOTCHI -G "$work/IP.BIN" -r -J -l \
    -o "$work/doomogachi.iso" "$work/cd" 2>"$work/mkisofs.log" || { cat "$work/mkisofs.log" >&2; die "mkisofs failed"; }
"$cdi4dc" "$work/doomogachi.iso" "$cdi" >"$work/cdi4dc.log" 2>&1 || { cat "$work/cdi4dc.log" >&2; die "cdi4dc failed"; }
grep -q "All done OK" "$work/cdi4dc.log" || { cat "$work/cdi4dc.log" >&2; die "cdi4dc did not finish"; }
# the copies the disc does not need; 1ST_READ.BIN and IP.BIN also go next
# to the CDI (for burning a GDI/CD by other means, or dc-load)
rm -f "$work/doomogachi.iso" "$work/doomogachi.bin"
out=$(dirname "$cdi")
cp "$work/cd/1ST_READ.BIN" "$work/IP.BIN" "$out/"
printf 'mkcdi: %s (%d bytes); 1ST_READ.BIN %d bytes; IP.BIN %d bytes\n' "$cdi" \
    "$(wc -c < "$cdi")" "$(wc -c < "$work/cd/1ST_READ.BIN")" "$(wc -c < "$work/IP.BIN")"
