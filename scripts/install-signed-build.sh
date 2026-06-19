#!/usr/bin/env bash
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
run_dir="$ROOT/local-analysis/install-signed-build/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run_dir"

# shellcheck source=scripts/audio-gate-lock.sh
source "$ROOT/scripts/audio-gate-lock.sh"
trap audio_gate_release_lock EXIT
if ! audio_gate_acquire_lock "install-signed-build" "$run_dir"; then
  {
    echo "install_signed_build=FAIL"
    echo "reason=${audio_gate_lock_error:-audio_gate_lock_busy}"
    echo "run_dir=$run_dir"
  } | tee "$run_dir/result.txt"
  exit 75
fi
audio_gate_export_inherited_lock "install-signed-build" "$run_dir"

/usr/bin/codesign --verify --strict --verbose=2 "$ROOT/build/OpenA8DJ.driver"

/usr/bin/install -d /Library/Audio/Plug-Ins/HAL
/bin/rm -rf /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/bin/cp -R "$ROOT/build/OpenA8DJ.driver" /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/usr/bin/xattr -cr /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/usr/bin/codesign --verify --strict --verbose=2 /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver

/usr/bin/install -d /usr/local/bin
/usr/bin/install -m 755 "$ROOT/build/opena8dj-control" /usr/local/bin/opena8dj-control
/usr/bin/install -m 755 "$ROOT/build/opena8dj-midid" /usr/local/bin/opena8dj-midid

/usr/bin/install -d /Library/LaunchAgents
/usr/bin/install -m 644 "$ROOT/resources/org.opena8dj.midid.plist" /Library/LaunchAgents/org.opena8dj.midid.plist

/usr/bin/killall coreaudiod 2>/dev/null || true

{
  echo "install_signed_build=PASS"
  echo "run_dir=$run_dir"
} | tee "$run_dir/result.txt"
