#!/bin/sh
# ci_apt.sh PKG... - apt-get install for CI with a stall guard.
#   A runner's package download sometimes crawls (CI 2026-11-06: 62.8 MB took 9 min 46 s on one runner, 4 s
#   for 175 MB on another in the same minute). So: download first (cached debs in $APT_DEBS when set, which the
#   workflow keeps with actions/cache, so a good run makes every later run fast), each attempt capped at 150 s
#   (apt resumes partial files), the 2nd attempt on the main Ubuntu mirror; the install itself then runs from
#   the downloaded files with no time cap (killing dpkg half way is worse than waiting).
set -u
OPTS=""
if [ -n "${APT_DEBS:-}" ]; then
  mkdir -p "$APT_DEBS/partial"
  sudo chmod -R a+rwX "$APT_DEBS"
  OPTS="-o Dir::Cache::archives=$APT_DEBS"
fi
printf 'Acquire::Retries "5";\nAcquire::http::Timeout "20";\nAcquire::https::Timeout "20";\n' | sudo tee /etc/apt/apt.conf.d/99ci >/dev/null
sudo apt-get update -q || true
n=0
while [ "$n" -lt 3 ]; do
  n=$((n + 1))
  # shellcheck disable=SC2086
  if sudo timeout -k 10 150 apt-get install -y -q --no-install-recommends --download-only $OPTS "$@"; then
    # shellcheck disable=SC2086
    sudo apt-get install -y -q --no-install-recommends $OPTS "$@" && exit 0
    echo "ci_apt: install failed after a good download"; exit 1
  fi
  echo "ci_apt: download attempt $n stalled or failed"
  if [ "$n" -eq 1 ] && [ -f /etc/apt/apt-mirrors.txt ]; then
    echo "ci_apt: trying the main Ubuntu mirror first"
    { echo 'http://archive.ubuntu.com/ubuntu/ priority:1'; sed 's/priority:[0-9]*/priority:9/' /etc/apt/apt-mirrors.txt; } > "${TMPDIR:-/tmp}/apt-mirrors.new"
    sudo cp "${TMPDIR:-/tmp}/apt-mirrors.new" /etc/apt/apt-mirrors.txt
    sudo apt-get update -q || true
  fi
done
echo "ci_apt: gave up after 3 download attempts"
exit 1
