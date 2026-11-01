#!/bin/sh
# check_elf.sh ELF NM - checks on the linked Dreamcast ELF that need no
# emulator (owner: dreamcast). Called by `make -C dc check-elf` / `test`.
#   - an SH little-endian ELF whose entry point is 0x8c010000
#   - the harness mailbox `dc_mailbox` is there, 32-byte aligned, with the
#     size of DcMailbox; prints its RAM offset (addr & 0xFFFFFF), which is
#     what the Flycast harness reads through RETRO_MEMORY_SYSTEM_RAM
#   - main, doomo_init and doomo_frame are linked
#   - lists the core's weak platform defaults still in the link (the
#     placeholders the later DC packages replace); informational
set -eu
elf=$1; nm=${2:-sh-elf-nm}
readelf=$(dirname "$(command -v "$nm" 2>/dev/null || echo "$nm")")/sh-elf-readelf
[ -x "$readelf" ] || readelf=sh-elf-readelf
fail() { echo "check-elf: FAIL: $*" >&2; exit 1; }

hdr=$("$readelf" -h "$elf")
echo "$hdr" | grep -q "little endian" || fail "not little endian"
echo "$hdr" | grep -q "SuperH" || fail "not an SH ELF"
echo "$hdr" | grep -q "Entry point address: *0x8c010000" || fail "entry point is not 0x8c010000"

syms=$("$nm" -S "$elf")
mb=$(echo "$syms" | awk '$4 == "_dc_mailbox" { print $1, $2 }')
[ -n "$mb" ] || fail "no dc_mailbox symbol"
addr=$(echo "$mb" | cut -d' ' -f1); msize=$(echo "$mb" | cut -d' ' -f2)
[ $((0x$addr % 32)) -eq 0 ] || fail "dc_mailbox at 0x$addr is not 32-byte aligned"
# DcMailbox (dc/harness/dc_mailbox.h, the harness contract): 16 words +
# name[20] + scene[24] + log_wr + log[4096] = 4196 B
[ $((0x$msize)) -eq 4196 ] || fail "dc_mailbox is $((0x$msize)) B, dc/harness/dc_mailbox.h says 4196 (update this check and the harness)"
# the unprefixed alias (misc.c) for tools that look the C name up
alias=$(echo "$syms" | awk '$NF == "dc_mailbox" { print $1 }')
[ "$alias" = "$addr" ] || fail "no dc_mailbox alias at 0x$addr (got '${alias}')"
for s in _main _doomo_init _doomo_frame _plat_init_hw; do
    echo "$syms" | awk -v s=$s '$NF == s && ($3 == "T" || $(NF-1) == "T") { f = 1 } END { exit !f }' || fail "$s is not linked"
done
printf 'check-elf: ok: dc_mailbox at 0x%s (RAM offset 0x%06x, %d B)\n' "$addr" $((0x$addr & 0xFFFFFF)) $((0x$msize))
weak=$("$nm" "$elf" | awk '$2 ~ /^[Ww]$/ && $3 ~ /^_plat_/ { sub(/^_/, "", $3); print $3 }' | tr '\n' ' ')
echo "check-elf: core weak platform defaults still linked: ${weak:-none}"
