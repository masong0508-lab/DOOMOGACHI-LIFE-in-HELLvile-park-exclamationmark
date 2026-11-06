#!/bin/sh
# dpatch.sh - apply the newest (or a named) patch zip from the Downloads folder: pull, unpack, apply, commit, push.
#
#   sh tools/dpatch.sh                 newest ~/storage/downloads/doomo-*.zip
#   sh tools/dpatch.sh NAME            ~/storage/downloads/NAME.zip (".zip" optional) or any path
#
# Every patch zip holds NAME/apply.sh (+ patch.diff, commit.txt, RESTORE.txt, made by tools/mkpatch.sh).
# The commit message comes from commit.txt, so nobody has to retype it. Safe to run twice (apply.sh says
# "already applied"; nothing is committed when nothing changed). Stops before touching anything when the
# tree has uncommitted changes (DOOMO_FORCE=1 goes on anyway), the pull is not a fast-forward, or the patch
# does not fit. Needs: git, unzip, patch, sh (all in Termux: pkg install git unzip patch).
#
#   DOOMO_DL=dir    where the zips are (default ~/storage/downloads)
#   DOOMO_WORK=dir  scratch unpack folder (default ~/vs, cleared every run)
#   DOOMO_NOPULL=1  skip git pull      DOOMO_NOPUSH=1  commit but do not push
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DL=${DOOMO_DL:-$HOME/storage/downloads}
WORK=${DOOMO_WORK:-$HOME/vs}
LOG=${DOOMO_LOG:-$HOME/dpatch.log}
say() { printf '%s\n' "$*"; }
die() { say "dpatch: $*"; exit 1; }

cd "$ROOT"
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "$ROOT is not a git checkout"
arg=${1:-}
if [ -n "$arg" ]; then
  case $arg in */*) zip=$arg ;; *) zip=$DL/${arg%.zip}.zip ;; esac
else
  zip=$(ls -t "$DL"/doomo-*.zip 2>/dev/null | head -1 || true)
fi
[ -n "${zip:-}" ] && [ -f "$zip" ] || die "no patch zip found (put doomo-*.zip in $DL, or pass its name)"
name=$(basename "$zip" .zip)
say "dpatch: $name"

if [ "${DOOMO_FORCE:-0}" != 1 ] && [ -n "$(git status --porcelain)" ]; then
  git status --short | head -10
  die "uncommitted changes above: commit or stash them first (DOOMO_FORCE=1 to go on)"
fi
[ "${DOOMO_NOPULL:-0}" = 1 ] || git pull --ff-only || die "git pull --ff-only failed: fix that first (nothing was changed)"

rm -rf "$WORK"; mkdir -p "$WORK"
unzip -qo "$zip" -d "$WORK" || die "cannot unzip $zip"
app=$(find "$WORK" -name apply.sh | head -1)
[ -n "$app" ] || die "no apply.sh inside $zip"
dir=$(dirname "$app")

sh "$app" || die "apply.sh failed: the tree is untouched or can be reset with: git checkout -- . && git clean -fd"

if [ -f "$dir/commit.txt" ]; then msg=$(cat "$dir/commit.txt"); else msg="patch $name"; fi
git add -A
if git diff --cached --quiet; then
  say "dpatch: nothing to commit (already applied?)"
else
  git commit -q -m "$msg"
  say "dpatch: committed: $(git log --oneline -1)"
fi
if [ "${DOOMO_NOPUSH:-0}" != 1 ]; then
  git push || die "push refused: run  git pull --rebase && git push  (the commit is safe locally)"
fi
printf '%s %s %s\n' "$(date +%F' '%T)" "$name" "$(git rev-parse --short HEAD)" >> "$LOG" 2>/dev/null || true
say "dpatch: done. undo (if not pushed yet): git reset --hard HEAD~1"
