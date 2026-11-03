#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if ! command -v python3 >/dev/null 2>&1; then
  echo 'Python 3.8+ is required (normally preinstalled on Linux Mint 20+). Nothing is auto-installed.' >&2
  exit 1
fi
if (( $# )); then exec python3 ./run.py "$@"; fi
printf '%s\n' 'Munk2D native Linux x86-64 AVX2 comparison' '' \
  'Plug into AC power; close heavy apps. Keep power/governor/security settings unchanged.' \
  'F = full: 26 scenes, smoke + reference, 8 balanced pairs per scene.' \
  'Q = quick: 8 selected reference scenes, 4 balanced pairs per scene.' \
  'Both include exact checks, a 60-second warmup and A/A + original controls.' \
  'Full may take 20-50 minutes on an older laptop; Quick is a screen only.' ''
read -r -t 20 -p 'Choose F or Q [F after 20 seconds]: ' mode || mode=F
case "${mode^^}" in
  Q|QUICK) exec python3 ./run.py --mode quick ;;
  F|FULL|'') exec python3 ./run.py --mode full ;;
  *) echo 'Please run ./run.sh again and choose F or Q.' >&2; exit 2 ;;
esac
