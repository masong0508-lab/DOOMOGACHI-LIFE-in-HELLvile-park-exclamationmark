#!/bin/sh
# elfsize.sh ELF SIZE NM - the Dreamcast ELF's RAM use against the 16 MB of
# main RAM (owner: dreamcast). Called by `make -C dc sizes`.
#
# The whole program (code, constant data, data and .bss) is loaded into main
# RAM at 0x8c010000 (KOS' default link address); what is left above _end is
# the heap and the stacks. Prints one line per part and the total, and fails
# if the image leaves less than DC_RAM_MIN_FREE (default 4 MB) free.
set -eu
elf=$1; size=${2:-sh-elf-size}; nm=${3:-sh-elf-nm}
min_free=${DC_RAM_MIN_FREE:-4194304}
ram_base=$((0x8c000000)); ram_end=$((0x8d000000))
"$size" -A "$elf" | awk -v base=$ram_base -v rend=$ram_end -v minf=$min_free '
    $1 ~ /^\.(text|init|fini)$/            { code += $2 }
    $1 ~ /^\.(rodata|eh_frame|ctors|dtors)$/ { ro += $2 }
    $1 ~ /^\.(data|got)$/                  { data += $2 }
    $1 == ".bss"                           { bss = $2; bss_at = $3 }
    END {
        img_end = bss_at + bss
        total = code + ro + data + bss
        free = rend - img_end
        printf "DC RAM (16 MB main RAM, image at 0x8c010000):\n"
        printf "  code   %9d B  %6.2f MB\n", code, code / 1048576
        printf "  rodata %9d B  %6.2f MB\n", ro, ro / 1048576
        printf "  data   %9d B  %6.2f MB\n", data, data / 1048576
        printf "  bss    %9d B  %6.2f MB\n", bss, bss / 1048576
        printf "  total  %9d B  %6.2f MB; heap + stacks above _end: %d B (%.2f MB)\n", total, total / 1048576, free, free / 1048576
        if (free < minf) { printf "  FAIL: less than %d B free (DC_RAM_MIN_FREE)\n", minf; exit 1 }
    }'
