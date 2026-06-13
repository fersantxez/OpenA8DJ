#!/usr/bin/env bash
set -euo pipefail

pair="A"
rate="48000"
buffer="512"
seconds="8"
install_driver="0"
leave_installed_on_fail="0"
out_dir=""
hal_output_native="${OPENA8DJ_HAL_OUTPUT_NATIVE:-0}"
change_note="${OPENA8DJ_CHANGE_NOTE:-not_specified}"
gate_status="INCOMPLETE"
gate_reason=""
if [ "${OPENA8DJ_SUDO_WITH_STDIN:-0}" = "1" ]; then
  sudo_cmd=(sudo -S)
else
  sudo_cmd=(sudo -n)
fi

usage() {
  cat <<USAGE
usage: scripts/verify-loopback-quality.sh [options]

Options:
  --pair A|B|C|D          Output/input pair to verify (default: A)
  --rate HZ              Sample rate (default: 48000)
  --buffer FRAMES        Core Audio buffer size (default: 512)
  --seconds N            Test duration (default: 8)
  --out-dir PATH         Output report directory
  --install              Build and install the diagnostic HAL before testing
  --native-output         Build with native/little 24-bit physical output
  --big-output            Build with big-endian 24-bit physical output (default)
  --change-note TEXT      Human-readable change being tested
  --leave-installed-on-fail
                          Do not move OpenA8DJ.driver out of HAL on failure
USAGE
}

run_timeout() {
  timeout_seconds="$1"
  shift
  "$@" &
  command_pid="$!"
  (
    sleep "$timeout_seconds"
    kill "$command_pid" 2>/dev/null || true
  ) &
  watchdog_pid="$!"
  wait "$command_pid"
  status="$?"
  kill "$watchdog_pid" 2>/dev/null || true
  wait "$watchdog_pid" 2>/dev/null || true
  return "$status"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --pair)
      pair="$2"; shift 2 ;;
    --rate)
      rate="$2"; shift 2 ;;
    --buffer)
      buffer="$2"; shift 2 ;;
    --seconds)
      seconds="$2"; shift 2 ;;
    --out-dir)
      out_dir="$2"; shift 2 ;;
    --install)
      install_driver="1"; shift ;;
    --native-output)
      hal_output_native="1"; shift ;;
    --big-output)
      hal_output_native="0"; shift ;;
    --change-note)
      change_note="$2"; shift 2 ;;
    --leave-installed-on-fail)
      leave_installed_on_fail="1"; shift ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2 ;;
  esac
done

case "$pair" in
  A|a|0) pair="A"; pair_index="0" ;;
  B|b|1) pair="B"; pair_index="1" ;;
  C|c|2) pair="C"; pair_index="2" ;;
  D|d|3) pair="D"; pair_index="3" ;;
  *)
    echo "pair must be A-D or 0-3" >&2
    exit 2 ;;
esac

repo="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo"

if [ -z "$out_dir" ]; then
  stamp="$(date +%Y%m%d-%H%M%S)"
  out_dir="local-analysis/loopback-quality-${stamp}"
fi
mkdir -p "$out_dir"

write_build_variant() {
  bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' build/OpenA8DJ.driver/Contents/Info.plist 2>/dev/null || echo unknown)"
  bundle_build="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' build/OpenA8DJ.driver/Contents/Info.plist 2>/dev/null || echo unknown)"
  hal_sha256="missing"
  if [ -f build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL ]; then
    hal_sha256="$(shasum -a 256 build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL | awk '{print $1}')"
  fi
  git_head="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
  if ! git diff --quiet --ignore-submodules -- 2>/dev/null || [ -n "$(git status --short 2>/dev/null)" ]; then
    git_dirty="1"
  else
    git_dirty="0"
  fi
  {
    echo "change_note=$change_note"
    echo "bundle_version=$bundle_version"
    echo "bundle_build=$bundle_build"
    echo "hal_sha256=$hal_sha256"
    echo "git_head=$git_head"
    echo "git_dirty=$git_dirty"
    echo "hal_output_native=$hal_output_native"
    if [ "$hal_output_native" = "1" ]; then
      echo "output_byte_order=native"
    else
      echo "output_byte_order=big"
    fi
    echo "rate=$rate"
    echo "buffer=$buffer"
    echo "pair=$pair"
  } > "$out_dir/build-variant.txt"
}

write_quality_summary() {
  status="$1"
  summary_status="$gate_status"
  if [ "$status" -eq 0 ] && [ "$summary_status" = "INCOMPLETE" ]; then
    summary_status="PASS"
  fi
  if [ -d "$out_dir" ] && [ -x scripts/summarize-quality-run.py ]; then
    scripts/summarize-quality-run.py "$out_dir" \
      --status "$summary_status" \
      --reason "$gate_reason" \
      --write > "$out_dir/quality-summary.stdout" 2>&1 || true
  fi
}

fail_gate() {
  code="$1"
  reason="$2"
  gate_status="FAIL"
  gate_reason="$reason"
  echo "loopback_gate=FAIL"
  echo "reason=$reason"
  echo "out_dir=$out_dir"
  exit "$code"
}

disable_on_fail() {
  status="$1"
  if [ "$status" -ne 0 ] && [ "$install_driver" = "1" ] && [ "$leave_installed_on_fail" != "1" ]; then
    stamp="$(date +%Y%m%d-%H%M%S)"
    "${sudo_cmd[@]}" mkdir -p /Library/Audio/Plug-Ins/HAL.disabled || true
    if [ -d /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver ]; then
      "${sudo_cmd[@]}" mv /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver \
        "/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.failed-loopback-${stamp}" || true
      "${sudo_cmd[@]}" killall coreaudiod || true
    fi
  fi
}
trap 'status=$?; write_quality_summary "$status"; disable_on_fail "$status"; exit "$status"' EXIT

make \
  HAL_DIAGNOSTIC=1 \
  HAL_INPUT_DECODE=1 \
  HAL_OUTPUT_NATIVE="$hal_output_native" \
  HAL_STREAM_USAGE=1 \
  HAL_ISO_FRAMES=64 \
  HAL_CAPTURE_QUEUE=8 \
  HAL_PLAYBACK_QUEUE=8 \
  hal \
  build/audio-wav-play \
  build/audio-config \
  build/audio-list \
  build/audio-inspect \
  build/audio-default \
  build/opena8dj-control

write_build_variant

if [ "$install_driver" = "1" ]; then
  if ! "${sudo_cmd[@]}" true 2>/dev/null; then
    fail_gate 20 "sudo_auth_required_run_sudo_v_first"
  fi
  codesign --force --sign "-" --timestamp=none build/OpenA8DJ.driver
  "${sudo_cmd[@]}" install -d /Library/Audio/Plug-Ins/HAL
  "${sudo_cmd[@]}" rm -rf /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
  "${sudo_cmd[@]}" cp -R build/OpenA8DJ.driver /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
  "${sudo_cmd[@]}" xattr -cr /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
  "${sudo_cmd[@]}" codesign --force --sign "-" --timestamp=none /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
  "${sudo_cmd[@]}" killall coreaudiod || true
  sleep 3
fi

if [ ! -d /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver ]; then
  fail_gate 10 "OpenA8DJ.driver is not installed in active HAL"
fi

if ! run_timeout 8 ./build/audio-list > "$out_dir/audio-list-before.txt" 2>&1; then
  fail_gate 14 "audio-list timed out or failed"
fi
if ! run_timeout 8 ./build/audio-inspect org.opena8dj.Audio8DJ > "$out_dir/audio-inspect-before.txt" 2>&1; then
  fail_gate 15 "audio-inspect timed out or failed"
fi

if ! rg -q "Open Audio 8 DJ" "$out_dir/audio-list-before.txt"; then
  fail_gate 10 "Open Audio 8 DJ is not visible to Core Audio"
fi

./build/audio-config org.opena8dj.Audio8DJ "$rate" "$buffer" > "$out_dir/audio-config.txt" 2>&1

reference="$out_dir/reference.wav"
scripts/generate-loopback-reference.py "$reference" --rate "$rate" --seconds "$seconds" \
  > "$out_dir/reference.txt"

diagnostic_tmp_files=(
  /tmp/opena8dj-output-capture.txt
  /tmp/opena8dj-output-written-f32.raw
  /tmp/opena8dj-output-consumed-f32.raw
  /tmp/opena8dj-input-loopback-f32.raw
  /tmp/opena8dj-input-packed-usb.raw
  /tmp/opena8dj-output-packed-usb.raw
  /tmp/opena8dj-output-events.tsv
)

rm -f "${diagnostic_tmp_files[@]}" 2>/dev/null || "${sudo_cmd[@]}" rm -f "${diagnostic_tmp_files[@]}"

./build/audio-wav-play "$reference" "$pair" > "$out_dir/audio-wav-play.txt" 2>&1 &
play_pid="$!"
stats_seen="0"
stats_tmp="$out_dir/stream-stats-live.tmp"
for _ in $(seq 1 160); do
  if ! kill -0 "$play_pid" 2>/dev/null; then
    break
  fi
  if run_timeout 2 ./build/opena8dj-control stream-stats > "$stats_tmp" 2>&1; then
    cat "$stats_tmp" >> "$out_dir/stream-stats-live.txt"
    printf "\n" >> "$out_dir/stream-stats-live.txt"
    cp "$stats_tmp" "$out_dir/stream-stats-after.txt"
    stats_seen="1"
  fi
  sleep 0.25
done
if ! wait "$play_pid"; then
  fail_gate 16 "audio playback failed"
fi
if [ "$stats_seen" != "1" ]; then
  fail_gate 17 "stream stats unavailable during playback"
fi
sleep 1

for path in "${diagnostic_tmp_files[@]}"; do
  if [ -e "$path" ]; then
    cp "$path" "$out_dir/"
  fi
done

run_timeout 2 ./build/opena8dj-control stream-stats > "$out_dir/stream-stats-final.txt" 2>&1 || true
./build/audio-default BuiltInSpeakerDevice > "$out_dir/audio-default-macbook.txt" 2>&1 || true

if [ ! -s "$out_dir/opena8dj-input-loopback-f32.raw" ]; then
  fail_gate 11 "no decoded input loopback capture"
fi

if [ -s "$out_dir/opena8dj-output-consumed-f32.raw" ]; then
  scripts/analyze-loopback-quality.py \
    --reference-wav "$reference" \
    --captured-f32 "$out_dir/opena8dj-output-consumed-f32.raw" \
    --sample-rate "$rate" \
    --channels 8 \
    --pair "$pair_index" \
    --min-snr-db 50 \
    --min-correlation 0.995 \
    --max-clicks 0 \
    --max-ripple-db 6 \
    > "$out_dir/internal-consumed-analysis.txt" || {
      fail_gate 13 "internal_consumed_quality_failed"
    }
else
  echo "internal_consumed_analysis=missing" > "$out_dir/internal-consumed-analysis.txt"
fi

if [ -s "$out_dir/opena8dj-output-packed-usb.raw" ]; then
  scripts/analyze-driver-capture.py "$reference" \
    --usb-raw "$out_dir/opena8dj-output-packed-usb.raw" \
    --usb-check-offset 8 \
    --usb-start-byte 4 \
    --pair "$pair" \
    --max-seconds 1 \
    --max-lag 256 \
    > "$out_dir/usb-raw-analysis.txt" || {
      fail_gate 18 "usb_raw_quality_failed"
    }
else
  echo "usb_raw_analysis=missing" > "$out_dir/usb-raw-analysis.txt"
fi

if [ -s "$out_dir/opena8dj-input-packed-usb.raw" ]; then
  scripts/analyze-driver-capture.py "$reference" \
    --usb-raw "$out_dir/opena8dj-input-packed-usb.raw" \
    --usb-check-offset 0 \
    --usb-start-byte auto \
    --usb-byte-order auto \
    --usb-auto-scan-bytes 524288 \
    --pair "$pair" \
    --max-seconds 1 \
    --max-lag 256 \
    > "$out_dir/input-raw-auto-analysis.txt" || true
else
  echo "input_raw_analysis=missing" > "$out_dir/input-raw-auto-analysis.txt"
fi

scripts/analyze-loopback-quality.py \
  --reference-wav "$reference" \
  --captured-f32 "$out_dir/opena8dj-input-loopback-f32.raw" \
  --sample-rate "$rate" \
  --channels 8 \
  --pair "$pair_index" \
  --min-snr-db 35 \
  --min-correlation 0.98 \
  --max-clicks 8 \
  --max-ripple-db 12 \
  > "$out_dir/loopback-analysis.txt" || {
    fail_gate 12 "analog_loopback_quality_failed"
  }

gate_status="PASS"
gate_reason="all_quality_gates_passed"
echo "loopback_gate=PASS"
echo "out_dir=$out_dir"
