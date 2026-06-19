#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

rate="${1:-48000}"
buffer="${2:-512}"
amplitude="${3:-0.25}"
seconds="${4:-2}"

make build/audio-io-test build/audio-config build/opena8dj-control >/dev/null

./build/audio-config org.opena8dj.Audio8DJ "$rate" "$buffer" >/tmp/opena8dj-verify-audio-config.log
./build/opena8dj-control stream-stats >/dev/null 2>&1 || true

io_log="$(mktemp /tmp/opena8dj-verify-audio-io.XXXXXX)"
stats_log="$(mktemp /tmp/opena8dj-verify-stream-stats.XXXXXX)"
stats_try="$(mktemp /tmp/opena8dj-verify-stream-stats-try.XXXXXX)"
: >"$stats_log"

./build/audio-io-test "$seconds" "$rate" "$buffer" "$amplitude" >"$io_log" 2>&1 &
io_pid=$!
for _ in $(seq 1 120); do
    [ -S /tmp/opena8dj-control.sock ] && break
    sleep 0.1
done

stats_seen=0
while kill -0 "$io_pid" 2>/dev/null; do
    if ./build/opena8dj-control stream-stats >"$stats_try" 2>&1; then
        {
            printf '%s\n' '--- stream-stats sample ---'
            cat "$stats_try"
        } >>"$stats_log"
        stats_seen=1
    fi
    sleep 0.25
done
wait "$io_pid"
if [ "$stats_seen" -eq 0 ]; then
    echo "VERIFY OUTPUT PATH: FAIL"
    echo "  stream-stats never became available while audio was active"
    cat "$stats_try"
    cat "$io_log"
    exit 1
fi
io_output="$(cat "$io_log")"
stats_output="$(cat "$stats_log")"

generator_peak="$(printf '%s\n' "$io_output" | awk -F'outputPeak=' '/outputPeak=/ {split($2, a, " "); print a[1]; exit}')"
generator_samples="$(printf '%s\n' "$io_output" | awk -F'outputSamples=' '/outputSamples=/ {split($2, a, " "); print a[1]; exit}')"
read -r driver_peak active_underruns playback_failed <<EOF
$(python3 - "$stats_log" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
peaks = [float(value) for value in re.findall(r"output-level:\s+peak=([0-9.]+)", text)]
underruns = [int(value) for value in re.findall(r"active-underruns=([0-9]+)", text)]
failed = [int(value) for value in re.findall(r"playback:\s+.*?failed=([0-9]+)", text)]
print(f"{max(peaks) if peaks else 0.0} {max(underruns) if underruns else 0} {max(failed) if failed else 0}")
PY
)
EOF

python3 - "$generator_peak" "$generator_samples" "$driver_peak" "$active_underruns" "$playback_failed" <<'PY'
import sys

generator_peak = float(sys.argv[1] or 0)
generator_samples = int(sys.argv[2] or 0)
driver_peak = float(sys.argv[3] or 0)
active_underruns = int(sys.argv[4] or 0)
playback_failed = int(sys.argv[5] or 0)

errors = []
if generator_samples <= 0:
    errors.append("generator wrote zero samples")
if generator_peak < 0.20:
    errors.append(f"generator peak too low: {generator_peak:.6f}")
if driver_peak < 0.10:
    errors.append(f"driver peak too low: {driver_peak:.6f}")
if active_underruns != 0:
    errors.append(f"active underruns: {active_underruns}")
if playback_failed != 0:
    errors.append(f"playback transfer failures: {playback_failed}")

if errors:
    print("VERIFY OUTPUT PATH: FAIL")
    for error in errors:
        print(f"  {error}")
    sys.exit(1)

print("VERIFY OUTPUT PATH: PASS")
print(f"  generator_peak={generator_peak:.6f}")
print(f"  driver_peak={driver_peak:.6f}")
print(f"  active_underruns={active_underruns}")
print(f"  playback_failed={playback_failed}")
PY

printf '%s\n' "$io_output"
printf '%s\n' "$stats_output"
