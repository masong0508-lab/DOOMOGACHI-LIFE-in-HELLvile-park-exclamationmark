#!/bin/sh
# selftest.sh BUILD - dcrun's own tests against the mock core (owner:
# dreamcast). Checks the script commands, the mailbox, PNG writing,
# determinism (two runs, byte-identical PNGs) and the exit status on
# failures and timeouts.
set -u
B=${1:-build}
here=$(cd "$(dirname "$0")" && pwd)
out=$B/selftest
rm -rf "$out"
mkdir -p "$out/a" "$out/b"
run() { "$B/dcrun" "$B/mockcore.so" mock.elf --mailbox 8c010000 --run-dir "$out/run" --fresh "$@"; }
ok=0; bad=0
check() { if [ "$1" = "$2" ]; then ok=$((ok+1)); else echo "selftest: $3 (exit $1, wanted $2)"; bad=$((bad+1)); fi; }

run --script "$here/selftest.txt" --shots "$out/a" >"$out/a.log" 2>&1; check $? 0 "selftest.txt run 1"
run --script "$here/selftest.txt" --shots "$out/b" --quiet >"$out/b.log" 2>&1; check $? 0 "selftest.txt run 2"
for f in title held_up vmu; do
    [ -s "$out/a/$f.png" ] || { echo "selftest: $f.png missing"; bad=$((bad+1)); continue; }
    if cmp -s "$out/a/$f.png" "$out/b/$f.png"; then ok=$((ok+1)); else echo "selftest: $f.png differs between runs"; bad=$((bad+1)); fi
done
cmp -s "$out/a/title.png" "$out/a/held_up.png" && { echo "selftest: title and held_up are the same picture"; bad=$((bad+1)); }
run --script "$here/selftest.txt" --shots "$out/h" --half --quiet >/dev/null 2>&1; check $? 0 "--half run"
run --script "$here/sync.txt" --sync --shots "$out/s" >"$out/sync.log" 2>&1; check $? 0 "sync.txt (--sync)"
run --script "$here/fail_log.txt" --quiet >/dev/null 2>&1; check $? 1 "expect-log failure"
run --script "$here/fail_scene.txt" --quiet >/dev/null 2>&1; check $? 1 "expect-scene failure"
run --script "$here/fail_ticks.txt" --quiet >/dev/null 2>&1; check $? 1 "expect-ticks failure"
run --script "$here/fail_load.txt" --quiet >/dev/null 2>&1; check $? 1 "failed load"
run --frames 100000 --timeout 1 --quiet >/dev/null 2>&1; check $? 1 "timeout"
"$B/dcrun" "$B/mockcore.so" mock.elf --mailbox 8c020000 --run-dir "$out/run" --frames 30 --quiet >/dev/null 2>&1
check $? 1 "no mailbox magic"
"$B/dcrun" /nonexistent.so mock.elf >/dev/null 2>&1; check $? 2 "missing core"
echo "selftest: $ok ok, $bad failed"
[ "$bad" = 0 ]
