#!/usr/bin/env bash
# dc/tools/setup_kos.sh: install the Dreamcast toolchain at pinned versions.
#
# Builds, into one prefix (default /opt/toolchains/dc, or $HOME/dc when that
# is not writable):
#
#   <prefix>/sh-elf/   sh-elf binutils + gcc + newlib from KallistiOS'
#                      kos-chain (profile and versions from kos_pins.env),
#                      --enable-threads=kos, m4-single + m4-single-only
#   <prefix>/kos/      KallistiOS at the pinned commit, built (KOS_BASE),
#                      with a generated environ.sh
#   <prefix>/bin/      host tools: makeip, scramble, bin2c, bin2o, genromfs,
#                      cdi4dc + mds4dc (img4dc)
#
# The script is idempotent: every stage writes a stamp holding the pins it
# was built from, and a second run with the same pins is a no-op (well under
# 5 s). Changing kos_pins.env rebuilds only what depends on the changed pins.
# An interrupted run resumes at the toolchain stage it stopped in.
#
# Disk: about 3.4 GB peak while gcc builds (measured; 278 MB installed). Sources, archives and build trees
# are deleted as soon as each part is installed, so the finished prefix is
# much smaller (see docs/DREAMCAST.md for the measured numbers).
#
# Usage: dc/tools/setup_kos.sh [options]
#   --prefix DIR       install prefix (default: $DC_PREFIX, else
#                      /opt/toolchains/dc if writable, else $HOME/dc)
#   -j N, --jobs N     parallel make jobs (default 2)
#   --install-deps     apt-get install the host packages first (uses sudo
#                      when not root)
#   --check            only verify an existing install: exit 0 when it is
#                      complete and matches the pins, 1 otherwise
#   --verify           after installing (or with --check), also build the
#                      KallistiOS hello example in a temporary copy
#   --force            rebuild everything, even when the stamps match
#   --keep-sources     keep kos-chain's downloads and logs (debugging only)
#   --min-free-gb N    refuse to start the toolchain build with less free
#                      space than this on the prefix's file system (default 4)
#   --print-profile    print the versions in kos-chain's profile for the
#                      pinned KallistiOS commit, then exit
#   -h, --help         this text
#
# Exit status: 0 success, 1 failure, 2 usage error.
# Owner: dreamcast.

set -euo pipefail

SETUP_VERSION=1                      # bump to invalidate every stamp
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
pins_file="$here/kos_pins.env"

die()  { printf 'setup_kos: error: %s\n' "$*" >&2; exit 1; }
say()  { printf 'setup_kos: %s\n' "$*"; }
usage() { sed -n '/^# Usage:/,/^# Exit status/p' "$0" | sed 's/^# \{0,1\}//'; }

# ---- options -------------------------------------------------------------------------
prefix=${DC_PREFIX:-}
jobs=2
install_deps=0
check_only=0
verify=0
force=0
keep_sources=0
min_free_gb=4
print_profile=0
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)        [ $# -ge 2 ] || { usage >&2; exit 2; }; prefix=$2; shift ;;
        --prefix=*)      prefix=${1#*=} ;;
        -j|--jobs)       [ $# -ge 2 ] || { usage >&2; exit 2; }; jobs=$2; shift ;;
        -j*)             jobs=${1#-j} ;;
        --jobs=*)        jobs=${1#*=} ;;
        --install-deps)  install_deps=1 ;;
        --check)         check_only=1 ;;
        --verify)        verify=1 ;;
        --force)         force=1 ;;
        --keep-sources)  keep_sources=1 ;;
        --min-free-gb)   [ $# -ge 2 ] || { usage >&2; exit 2; }; min_free_gb=$2; shift ;;
        --min-free-gb=*) min_free_gb=${1#*=} ;;
        --print-profile) print_profile=1 ;;
        -h|--help)       usage; exit 0 ;;
        *)               printf 'setup_kos: unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done
case "$jobs" in ''|*[!0-9]*|0) die "--jobs needs a positive number (got '$jobs')" ;; esac
case "$min_free_gb" in ''|*[!0-9]*) die "--min-free-gb needs a number (got '$min_free_gb')" ;; esac

# ---- pins ----------------------------------------------------------------------------
# Parse KEY=value lines without sourcing the file as shell code.
[ -f "$pins_file" ] || die "missing $pins_file"
pin_keys="KOS_REPO KOS_COMMIT KOS_CHAIN_PROFILE KOS_BINUTILS_VER KOS_GCC_VER KOS_NEWLIB_VER
          KOS_PRECISION_MODES KOS_DEFAULT_PRECISION KOS_LANGUAGES IMG4DC_REPO IMG4DC_COMMIT"
for k in $pin_keys; do
    v=$(sed -n "s/^$k=\(.*\)\$/\1/p" "$pins_file" | tail -n 1)
    [ -n "$v" ] || die "$pins_file: $k is not set"
    case "$v" in *[[:space:]\"\'\$\`\;\&\|\<\>\(\)]*) die "$pins_file: $k has a character that is not allowed: $v" ;; esac
    printf -v "$k" '%s' "$v"
done
case "$KOS_COMMIT"    in *[!0-9a-f]*) die "KOS_COMMIT must be a full hex SHA" ;; esac
case "$IMG4DC_COMMIT" in *[!0-9a-f]*) die "IMG4DC_COMMIT must be a full hex SHA" ;; esac
[ ${#KOS_COMMIT} -eq 40 ] && [ ${#IMG4DC_COMMIT} -eq 40 ] || die "commit pins must be 40-character SHAs"
case "$KOS_LANGUAGES" in
    c)     enable_cpp=0 ;;
    c,c++) enable_cpp=1 ;;
    *)     die "KOS_LANGUAGES must be 'c' or 'c,c++' (got '$KOS_LANGUAGES')" ;;
esac

# ---- prefix --------------------------------------------------------------------------
if [ -z "$prefix" ]; then
    if { [ -d /opt/toolchains/dc ] && [ -w /opt/toolchains/dc ]; } ||
       { [ ! -e /opt/toolchains/dc ] && mkdir -p /opt/toolchains/dc 2>/dev/null; }; then
        prefix=/opt/toolchains/dc
    else
        prefix="$HOME/dc"
    fi
fi
case "$prefix" in /*) ;; *) prefix="$PWD/$prefix" ;; esac
prefix=${prefix%/}
kos="$prefix/kos"
tc="$prefix/sh-elf"
bindir="$prefix/bin"
state="$prefix/.setup_kos"
chain="$kos/utils/kos-chain"

# Stage keys: a stage is rebuilt when its key differs from its stamp.
# The toolchain key includes the git tree hash of utils/kos-chain at the pinned
# commit, so a KallistiOS bump that changes kos-chain's patches rebuilds gcc,
# and one that does not only rebuilds KallistiOS itself.
pins_hash() { sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$pins_file" | sha256sum | cut -c1-16; }
all_key="v$SETUP_VERSION pins=$(pins_hash) prefix=$prefix"

stamp_is()  { [ -f "$state/$1" ] && [ "$(cat "$state/$1")" = "$2" ]; }
stamp_ok()  { [ "$force" -eq 0 ] && stamp_is "$1" "$2"; }
stamp_set() { mkdir -p "$state"; printf '%s\n' "$2" > "$state/$1.tmp" && mv "$state/$1.tmp" "$state/$1"; }

installed_ok() {
    [ -x "$tc/bin/sh-elf-gcc" ] &&
    [ -f "$kos/lib/dreamcast/libkallisti.a" ] &&
    [ -f "$kos/environ.sh" ] &&
    [ -x "$bindir/makeip" ] && [ -x "$bindir/scramble" ] && [ -x "$bindir/cdi4dc" ] &&
    [ "$(git -C "$kos" rev-parse -q --verify HEAD 2>/dev/null)" = "$KOS_COMMIT" ]
}

# ---- verification (also used by --check) -------------------------------------------
verify_install() {
    # $1 = "nostamp": skip the final stamp check (used right before writing it).
    local ok=1 v out
    say "checking $prefix"
    if ! installed_ok; then say "  install is incomplete or not at the pinned commit"; return 1; fi
    v=$("$tc/bin/sh-elf-gcc" -dumpversion)
    say "  sh-elf-gcc $v ($tc/bin/sh-elf-gcc)"
    [ "$v" = "$KOS_GCC_VER" ] || { say "  gcc version $v, pinned $KOS_GCC_VER"; ok=0; }
    # (Captured first: with pipefail, `gcc -v | grep -q` can fail on SIGPIPE.)
    out=$("$tc/bin/sh-elf-gcc" -v 2>&1)
    case "$out" in
        *--enable-threads=kos*) say "  thread model: kos" ;;
        *) say "  sh-elf-gcc was not configured with --enable-threads=kos"; ok=0 ;;
    esac
    local ml
    ml=$("$tc/bin/sh-elf-gcc" -print-multi-lib | tr '\n' ' ')
    say "  multilibs: $ml"
    case "$ml" in *m4-single-only*) ;; *) say "  the m4-single-only multilib is missing"; ok=0 ;; esac
    out=$("$tc/bin/sh-elf-as" --version)
    say "  ${out%%$'\n'*}"
    say "  KallistiOS $(git -C "$kos" rev-parse --short=12 HEAD) ($kos)"
    for t in makeip scramble bin2c bin2o genromfs cdi4dc; do
        [ -x "$bindir/$t" ] || { say "  missing $bindir/$t"; ok=0; }
    done
    [ "$ok" -eq 1 ] || return 1
    [ "${1:-}" != nostamp ] || return 0
    stamp_is complete "$all_key" || { say "  stamps do not match the pins (run the script to update)"; return 1; }
    return 0
}

verify_hello() {
    # Build examples/dreamcast/hello in a throwaway copy so the KOS tree stays clean.
    local tmp rc=0
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/kos-hello.XXXXXX")
    cp -r "$kos/examples/dreamcast/hello" "$tmp/hello"
    say "building the KallistiOS hello example in $tmp/hello"
    if ( set +u; . "$kos/environ.sh" >/dev/null && make -C "$tmp/hello" ) > "$tmp/build.log" 2>&1 &&
       [ -f "$tmp/hello/hello.elf" ]; then
        say "  hello.elf: $(stat -c %s "$tmp/hello/hello.elf") bytes, $("$tc/bin/sh-elf-readelf" -h "$tmp/hello/hello.elf" | sed -n 's/^ *Machine: *//p')"
    else
        tail -n 30 "$tmp/build.log" >&2
        say "  hello example FAILED"; rc=1
    fi
    rm -rf "$tmp"
    return "$rc"
}

# ---- fast paths ----------------------------------------------------------------------
if [ "$print_profile" -eq 1 ]; then
    [ -d "$kos/.git" ] || die "no KallistiOS checkout in $kos yet (run the script once)"
    git -C "$kos" cat-file -e "$KOS_COMMIT^{commit}" 2>/dev/null ||
        git -C "$kos" fetch -q --depth 1 origin "$KOS_COMMIT"
    git -C "$kos" show "$KOS_COMMIT:utils/kos-chain/profiles/dreamcast/$KOS_CHAIN_PROFILE.mk" |
        grep -E '^(binutils|gcc|newlib)_ver='
    exit 0
fi
if [ "$check_only" -eq 1 ]; then
    verify_install || exit 1
    [ "$verify" -eq 0 ] || verify_hello || exit 1
    say "OK"
    exit 0
fi
if [ "$force" -eq 0 ] && stamp_ok complete "$all_key" && installed_ok; then
    say "up to date: $prefix (KallistiOS ${KOS_COMMIT:0:12}, gcc $KOS_GCC_VER, profile $KOS_CHAIN_PROFILE)"
    [ "$verify" -eq 0 ] || verify_hello || exit 1
    exit 0
fi

# ---- host dependencies -----------------------------------------------------------------
apt_pkgs="build-essential git curl make gawk patch bzip2 xz-utils tar texinfo flex bison
          libelf-dev libgmp-dev libmpfr-dev libmpc-dev libpng-dev libjpeg-dev cmake genisoimage"
if [ "$install_deps" -eq 1 ]; then
    command -v apt-get >/dev/null || die "--install-deps needs apt-get (install these by hand: $apt_pkgs)"
    sudo=""; [ "$(id -u)" -eq 0 ] || sudo=sudo
    say "installing host packages with apt-get"
    # shellcheck disable=SC2086
    $sudo apt-get update -q && $sudo apt-get install -y -q $apt_pkgs
fi
missing=""
for t in git curl make gcc g++ gawk patch bzip2 xz tar makeinfo flex bison cmake; do
    command -v "$t" >/dev/null || missing="$missing $t"
done
# Headers the KOS utils need (makeip: libpng; dcbumpgen/vqenc/kmgenc: libpng + libjpeg).
# libelf-dev is in the apt list for dc-tool (serial/IP loader), which is not built here.
for h in png.h jpeglib.h; do
    printf '#include <%s>\n' "$h" | gcc -E -x c - -o /dev/null 2>/dev/null || missing="$missing <$h>"
done
[ -z "$missing" ] || die "missing host tools/headers:$missing
  Ubuntu/Debian: sudo apt install $(echo $apt_pkgs)
  (or rerun with --install-deps)"

mkdir -p "$prefix" "$state" "$bindir" 2>/dev/null || die "cannot create $prefix (use --prefix \$HOME/dc)"
[ -w "$prefix" ] || die "$prefix is not writable (use --prefix \$HOME/dc)"

# Peak disk use of the prefix, sampled in the background (reported at the end).
peak_file="$state/peak_kb"
echo 0 > "$peak_file"
( while sleep 15; do
      k=$(du -sxk "$prefix" 2>/dev/null | cut -f1) || continue
      [ "${k:-0}" -le "$(cat "$peak_file" 2>/dev/null || echo 0)" ] || echo "$k" > "$peak_file"
  done ) &
sampler=$!
trap 'kill "$sampler" 2>/dev/null || true' EXIT
t_start=$(date +%s)

stage_time() { say "$1 done in $(( $(date +%s) - $2 )) s"; }

# ---- 1. KallistiOS sources -------------------------------------------------------------
kos_fetch() {
    if [ ! -d "$kos/.git" ]; then
        say "fetching KallistiOS $KOS_COMMIT"
        rm -rf "$kos"
        git init -q "$kos"
        git -C "$kos" remote add origin "$KOS_REPO"
    fi
    git -C "$kos" remote set-url origin "$KOS_REPO"
    local head
    head=$(git -C "$kos" rev-parse -q --verify HEAD 2>/dev/null || true)
    if [ "$head" != "$KOS_COMMIT" ]; then
        if [ -n "$head" ] && [ -n "$(git -C "$kos" status --porcelain --untracked-files=no)" ] && [ "$force" -eq 0 ]; then
            die "$kos has local changes to tracked files; commit or discard them, or rerun with --force"
        fi
        say "checking out KallistiOS $KOS_COMMIT (was ${head:-nothing})"
        git -C "$kos" fetch -q --depth 1 origin "$KOS_COMMIT"
        git -C "$kos" -c advice.detachedHead=false checkout -q -f "$KOS_COMMIT"
        git -C "$kos" clean -q -fdx      # build outputs of the previous commit
        rm -f "$state/kos" "$state/hosttools"
    fi
}
kos_fetch

chain_tree=$(git -C "$kos" rev-parse "$KOS_COMMIT:utils/kos-chain")
tc_key="v$SETUP_VERSION chain=$chain_tree profile=$KOS_CHAIN_PROFILE binutils=$KOS_BINUTILS_VER gcc=$KOS_GCC_VER newlib=$KOS_NEWLIB_VER prec=$KOS_PRECISION_MODES default=$KOS_DEFAULT_PRECISION langs=$KOS_LANGUAGES prefix=$tc"
kos_key="v$SETUP_VERSION kos=$KOS_COMMIT tc=$(printf '%s' "$tc_key" | sha256sum | cut -c1-16) prefix=$prefix"
i4_key="v$SETUP_VERSION img4dc=$IMG4DC_COMMIT"

# ---- 2. toolchain (kos-chain) ----------------------------------------------------------
chain_make() {
    # kos-chain's build targets are phony and depend on each other, so each
    # stage marks the stages before it as done with -o. This lets the script
    # delete binutils' and newlib's sources as soon as they are installed,
    # which keeps the peak disk use down, and lets an interrupted run resume.
    local log="$state/kos-chain.log"
    say "  make $*  (log: $log)"
    if ! make -C "$chain" "$@" >> "$log" 2>&1; then
        tail -n 40 "$log" >&2
        local f
        f=$(ls -t "$chain"/logs/*.log 2>/dev/null | head -n 1) || true
        if [ -n "$f" ]; then printf '\n--- %s (last lines) ---\n' "$f" >&2; tail -n 25 "$f" >&2; fi
        die "kos-chain failed: make $* (full logs in $log and $chain/logs/)"
    fi
}

# Patterns are passed quoted and expanded here, inside the kos-chain directory.
chain_sources_rm() {
    ( cd "$chain" || exit 1
      shopt -s nullglob
      for p in "$@"; do
          # shellcheck disable=SC2086
          set -- $p
          [ $# -eq 0 ] || rm -rf -- "$@"
      done )
}

write_chain_cfg() {
    # Makefile.cfg for kos-chain: Makefile.dreamcast.cfg with the pinned options.
    local k
    sed -e "s|^toolchain_profile=.*|toolchain_profile=$KOS_CHAIN_PROFILE|" \
        -e "s|^toolchain_path=.*|toolchain_path=$tc|" \
        -e "s|^makejobs=.*|makejobs=$jobs|" \
        -e "s|^verbose=.*|verbose=0|" \
        -e "s|^erase=.*|erase=1|" \
        -e "s|^enable_cpp=.*|enable_cpp=$enable_cpp|" \
        -e "s|^enable_objc=.*|enable_objc=0|" \
        -e "s|^enable_objcpp=.*|enable_objcpp=0|" \
        -e "s|^precision_modes=.*|precision_modes=$KOS_PRECISION_MODES|" \
        -e "s|^default_precision=.*|default_precision=$KOS_DEFAULT_PRECISION|" \
        -e "s|^thread_model=.*|thread_model=kos|" \
        "$chain/Makefile.dreamcast.cfg" > "$chain/Makefile.cfg"
    # GMP/MPFR/MPC/ISL at the profile's versions, fetched by kos-chain over
    # https. (gcc's own contrib/download_prerequisites uses plain http, which
    # proxies that only allow https refuse.)
    printf '\n# Added by setup_kos.sh\nuse_custom_dependencies=1\n' >> "$chain/Makefile.cfg"
    for k in "toolchain_profile=$KOS_CHAIN_PROFILE" "toolchain_path=$tc" "makejobs=$jobs" "erase=1" \
             "enable_cpp=$enable_cpp" "enable_objc=0" "enable_objcpp=0" "thread_model=kos" \
             "precision_modes=$KOS_PRECISION_MODES" "default_precision=$KOS_DEFAULT_PRECISION" \
             "use_custom_dependencies=1"; do
        grep -qx "$k" "$chain/Makefile.cfg" || die "could not set '$k' in kos-chain's Makefile.cfg (its format changed?)"
    done
}

toolchain_build() {
    local prof="$chain/profiles/dreamcast/$KOS_CHAIN_PROFILE.mk" k want got
    [ -f "$prof" ] || die "kos-chain has no profile '$KOS_CHAIN_PROFILE' at KallistiOS ${KOS_COMMIT:0:12}"
    for k in binutils gcc newlib; do
        case $k in binutils) want=$KOS_BINUTILS_VER ;; gcc) want=$KOS_GCC_VER ;; newlib) want=$KOS_NEWLIB_VER ;; esac
        got=$(sed -n "s/^${k}_ver=//p" "$prof" | tr -d '[:space:]')
        [ "$got" = "$want" ] || die "kos-chain profile '$KOS_CHAIN_PROFILE' builds $k $got, but kos_pins.env pins $want (update the pins: --print-profile)"
    done
    write_chain_cfg

    local free_kb
    free_kb=$(df -Pk "$prefix" | awk 'NR==2 {print $4}')
    if [ "$free_kb" -lt $(( min_free_gb * 1024 * 1024 )) ]; then
        die "only $(( free_kb / 1024 )) MB free on $prefix; the toolchain build needs about 3.5 GB at its peak (--min-free-gb $min_free_gb)"
    fi

    local t0
    if ! stamp_ok tc.binutils "$tc_key"; then
        say "toolchain: starting from scratch in $tc"
        rm -rf "$tc" "$state"/tc.* "$state/kos-chain.log"
        t0=$(date +%s)
        say "toolchain 1/4: binutils $KOS_BINUTILS_VER"
        chain_make build-binutils
        chain_sources_rm "binutils-$KOS_BINUTILS_VER" "binutils-$KOS_BINUTILS_VER.tar.*"
        stamp_set tc.binutils "$tc_key"; stage_time "binutils" "$t0"
    fi
    if ! stamp_ok tc.gcc1 "$tc_key"; then
        t0=$(date +%s)
        say "toolchain 2/4: gcc $KOS_GCC_VER pass 1"
        # Unpack gcc (+ GMP/MPFR/MPC/ISL) first and drop gcc/testsuite except
        # selftests/ (which the build reads): about 110,000 small files,
        # roughly 0.9 GB on disk, that are only used by `make check`. This is
        # the biggest single cut to the peak disk use.
        chain_make fetch-gcc
        [ -d "$chain/gcc-$KOS_GCC_VER/gcc/testsuite/selftests" ] ||
            die "gcc-$KOS_GCC_VER/gcc/testsuite/selftests is missing; delete $chain/gcc-$KOS_GCC_VER and rerun"
        find "$chain/gcc-$KOS_GCC_VER/gcc/testsuite" -mindepth 1 -maxdepth 1 ! -name selftests -exec rm -rf {} +
        chain_make -o build-binutils build-gcc-pass1
        stamp_set tc.gcc1 "$tc_key"; stage_time "gcc pass 1" "$t0"
    fi
    if ! stamp_ok tc.newlib "$tc_key"; then
        t0=$(date +%s)
        say "toolchain 3/4: newlib $KOS_NEWLIB_VER"
        chain_make -o build-binutils -o build-gcc-pass1 build-newlib
        chain_sources_rm "newlib-$KOS_NEWLIB_VER" "newlib-$KOS_NEWLIB_VER.tar.*"
        stamp_set tc.newlib "$tc_key"; stage_time "newlib" "$t0"
    fi
    if ! stamp_ok tc.gcc2 "$tc_key"; then
        t0=$(date +%s)
        say "toolchain 4/4: gcc $KOS_GCC_VER pass 2"
        chain_make -o build-binutils -o build-gcc-pass1 -o build-newlib-only -o fixup-newlib build-gcc-pass2
        stamp_set tc.gcc2 "$tc_key"; stage_time "gcc pass 2" "$t0"
    fi
    if [ "$keep_sources" -eq 0 ]; then
        say "toolchain: deleting kos-chain sources, archives and build trees"
        chain_sources_rm 'binutils-*' 'gcc-*' 'newlib-*' 'gdb-*' 'gmp-*' 'mpfr-*' 'mpc-*' 'isl-*' 'build-*' '*.tar.*' '*.stamp' logs
        rm -f "$state/kos-chain.log"
    fi
    stamp_set toolchain "$tc_key"
}

if stamp_ok toolchain "$tc_key" && [ -x "$tc/bin/sh-elf-gcc" ]; then
    say "toolchain up to date: sh-elf-gcc $KOS_GCC_VER in $tc"
else
    toolchain_build
fi

# ---- 3. KallistiOS ---------------------------------------------------------------------
write_environ() {
    # doc/environ.sh.sample with this prefix. The toolchain's bin directory and
    # the host tools go to the FRONT of PATH, so that `sh-elf-gcc` means the
    # KallistiOS compiler (not a distro sh-elf-gcc) once environ.sh is sourced.
    local out="$kos/environ.sh"
    sed -e "s|^export KOS_BASE=.*|export KOS_BASE=\"$kos\"|" \
        -e "s|^export KOS_CC_BASE=.*|export KOS_CC_BASE=\"$tc\"|" \
        -e "s|^export DC_TOOLS_BASE=.*|export DC_TOOLS_BASE=\"$bindir\"|" \
        -e "s|^export KOS_SH4_PRECISION=.*|export KOS_SH4_PRECISION=\"-$KOS_DEFAULT_PRECISION\"|" \
        -e "s|^\. \${KOS_BASE}/env/environ_base.sh|case \":\$PATH:\" in *\":$tc/bin:\"*) ;; *) export PATH=\"$tc/bin:$bindir:\$PATH\" ;; esac\n&|" \
        "$kos/doc/environ.sh.sample" > "$out.tmp"
    for k in "export KOS_BASE=\"$kos\"" "export KOS_CC_BASE=\"$tc\"" "export DC_TOOLS_BASE=\"$bindir\"" \
             "export KOS_SH4_PRECISION=\"-$KOS_DEFAULT_PRECISION\""; do
        grep -qxF "$k" "$out.tmp" || die "could not set '$k' in environ.sh (doc/environ.sh.sample changed?)"
    done
    grep -q '^\. \${KOS_BASE}/env/environ_base.sh' "$out.tmp" || die "environ.sh.sample no longer sources environ_base.sh"
    printf '\n# Generated by DOOMOGACHI dc/tools/setup_kos.sh (KallistiOS %s).\n' "$KOS_COMMIT" >> "$out.tmp"
    mv "$out.tmp" "$out"
}

kos_build() {
    local t0 log="$state/kos-build.log"
    t0=$(date +%s)
    say "KallistiOS: build ${KOS_COMMIT:0:12} with -j$jobs (log: $log)"
    # Start from a clean tree: objects built by an older toolchain or an
    # older commit must not be linked in. (kos-chain's own files are kept.)
    git -C "$kos" clean -q -fdx -e utils/kos-chain/
    write_environ
    # Refresh the KOS headers kos-chain copied into newlib (cheap; they come
    # from the KallistiOS tree, which may be newer than the toolchain).
    write_chain_cfg
    chain_make -o build-newlib-only fixup-newlib
    if ! ( set +u; cd "$kos" && . ./environ.sh && make -j"$jobs" ) > "$log" 2>&1; then
        tail -n 40 "$log" >&2
        die "KallistiOS build failed (full log: $log)"
    fi
    [ -f "$kos/lib/dreamcast/libkallisti.a" ] || die "KallistiOS build finished without lib/dreamcast/libkallisti.a"
    rm -f "$log" "$state/kos-chain.log"
    stamp_set kos "$kos_key"; stage_time "KallistiOS" "$t0"
}

if stamp_ok kos "$kos_key" && [ -f "$kos/lib/dreamcast/libkallisti.a" ] && [ -f "$kos/environ.sh" ]; then
    say "KallistiOS up to date: ${KOS_COMMIT:0:12} in $kos"
else
    kos_build
fi

# ---- 4. host tools ---------------------------------------------------------------------
if ! stamp_ok hosttools "$kos_key"; then
    # Built by the KallistiOS make above (utils/); copied next to cdi4dc.
    for t in makeip/makeip scramble/scramble bin2c/bin2c bin2o/bin2o genromfs/genromfs; do
        [ -x "$kos/utils/$t" ] || die "KallistiOS did not build utils/$t"
        install -m 0755 "$kos/utils/$t" "$bindir/"
    done
    stamp_set hosttools "$kos_key"
    say "host tools: makeip scramble bin2c bin2o genromfs -> $bindir"
fi

if stamp_ok img4dc "$i4_key" && [ -x "$bindir/cdi4dc" ]; then
    say "img4dc up to date: cdi4dc in $bindir"
else
    t0=$(date +%s)
    src="$state/src/img4dc"
    rm -rf "$state/src"
    mkdir -p "$src"
    say "img4dc: fetching ${IMG4DC_COMMIT:0:12}"
    git -C "$src" init -q
    git -C "$src" remote add origin "$IMG4DC_REPO"
    git -C "$src" fetch -q --depth 1 origin "$IMG4DC_COMMIT"
    git -C "$src" -c advice.detachedHead=false checkout -q "$IMG4DC_COMMIT"
    log="$state/img4dc.log"
    # img4dc asks for CMake 3.0; newer CMake needs the policy floor raised.
    if ! { cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 &&
           cmake --build "$src/build" -j "$jobs"; } > "$log" 2>&1; then
        tail -n 30 "$log" >&2
        die "img4dc build failed (log: $log)"
    fi
    for t in cdi4dc mds4dc; do
        f=$(find "$src/build" -type f -name "$t" -perm -u+x | head -n 1) || true
        [ -n "$f" ] || die "img4dc did not build $t"
        install -m 0755 "$f" "$bindir/$t"
    done
    rm -rf "$state/src" "$log"
    stamp_set img4dc "$i4_key"; stage_time "img4dc" "$t0"
fi

# ---- done ----------------------------------------------------------------------------
kill "$sampler" 2>/dev/null || true
wait "$sampler" 2>/dev/null || true
trap - EXIT
k=$(du -sxk "$prefix" | cut -f1)
[ "$k" -le "$(cat "$peak_file")" ] || echo "$k" > "$peak_file"
say "installed in $(( $(date +%s) - t_start )) s: $(( k / 1024 )) MB in $prefix (peak about $(( $(cat "$peak_file") / 1024 )) MB, sampled every 15 s)"
verify_install nostamp || die "the finished install did not pass its checks"
stamp_set complete "$all_key"
[ "$verify" -eq 0 ] || verify_hello || exit 1
say "done. Use it with:  source $kos/environ.sh"
