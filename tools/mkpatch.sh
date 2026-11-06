#!/bin/sh
# mkpatch.sh - build a patch zip in the project's format (used by whoever edits the tree: Claude or you).
#
#   sh tools/mkpatch.sh NAME "commit message" PRISTINE_TREE EDITED_TREE OUT_DIR
#
# NAME is new for every patch (doomo-<what>-<monthday><letter>). Writes OUT_DIR/NAME.zip containing
#   NAME/patch.diff    unified diff (patch -p1), new files included, build/.git/__pycache__ left out
#   NAME/apply.sh      dry run first, "already applied" is fine, refuses a tree it does not fit
#   NAME/commit.txt    the commit message (tools/dpatch.sh reads it)
#   NAME/RESTORE.txt   the copyable commands, so an interrupted session can be picked up again
set -eu
[ $# -eq 5 ] || { sed -n '2,10p' "$0"; exit 2; }
NAME=$1; MSG=$2; A=$3; B=$4; OUT=$5
[ -d "$A" ] && [ -d "$B" ] || { echo "mkpatch: both trees must be folders"; exit 2; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
P=$T/$NAME; mkdir -p "$P" "$OUT"
OUT=$(cd "$OUT" && pwd)
ln -s "$(cd "$A" && pwd)" "$T/a"; ln -s "$(cd "$B" && pwd)" "$T/b"
( cd "$T" && diff -urN -x .git -x build -x __pycache__ -x 'gmon*' -x '*.o' -x '*.d' a b ) > "$P/patch.diff" || true
[ -s "$P/patch.diff" ] || { echo "mkpatch: the trees are identical"; exit 1; }
printf '%s\n' "$MSG" > "$P/commit.txt"
cat > "$P/apply.sh" <<'APPLYEND'
#!/bin/sh
# Run from the repo root (tools/dpatch.sh does that). Dry run first; safe to run twice.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
[ -f core/Makefile ] || { echo "run this from the repo root (core/Makefile not found)"; exit 1; }
if patch -p1 -R --dry-run -s < "$here/patch.diff" >/dev/null 2>&1; then
  echo "$(basename "$here"): already applied, nothing to do"; exit 0
fi
patch -p1 --dry-run < "$here/patch.diff" >/dev/null || { echo "$(basename "$here"): does not fit this tree (git pull first, or an earlier patch is missing)"; exit 1; }
patch -p1 --no-backup-if-mismatch < "$here/patch.diff"
echo "$(basename "$here"): applied"
if [ "${DOOMO_TEST:-0}" = 1 ]; then make -C core -j4 test 2>&1 | tail -4; fi
APPLYEND
R=DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark
cat > "$P/RESTORE.txt" <<RESTEND
$NAME  -  $MSG

Apply (Termux, from anywhere; the repo clone is ~/$R):
  sh ~/$R/tools/dpatch.sh $NAME

If the repo does not have tools/dpatch.sh yet (first time), the long form:
  cd ~/$R && git pull --ff-only && rm -rf ~/vs && mkdir ~/vs && unzip -qo ~/storage/downloads/$NAME.zip -d ~/vs && sh ~/vs/$NAME/apply.sh && git add -A && { git diff --cached --quiet || git commit -F ~/vs/$NAME/commit.txt ; } && git push

Undo before pushing:   cd ~/$R && git reset --hard HEAD~1
Test after applying:   DOOMO_TEST=1 sh ~/vs/$NAME/apply.sh     (make -C core test, needs gcc + python3)
Session interrupted?   upload the repo zip (and this zip) to a new chat and say "continue $NAME";
                       docs/NEXT_SESSION_PROMPT.md in the repo says what to read first.
RESTEND
( cd "$T" && zip -qr "$OUT/$NAME.zip" "$NAME" )
echo "mkpatch: $OUT/$NAME.zip ($(grep -c '^+++ ' "$P/patch.diff") files)"
