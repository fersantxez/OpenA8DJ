PROJECT := opena8dj
VERSION := 0.3.135
TOOL := build/opena8dj-probe
SRC := src/opena8dj-probe.m
HAL_BUNDLE := build/OpenA8DJ.driver
HAL_BIN := $(HAL_BUNDLE)/Contents/MacOS/OpenA8DJHAL
HAL_CONFIG := build/hal-build-config.txt
HAL_SRC := src/hal/OpenA8DJHAL.c src/hal/OpenA8DJUSB.m
HAL_IPC_AUTH := src/hal/OpenA8DJIPCAuth.h
DRIVER_MODE_HEADER := src/hal/OpenA8DJDriverMode.h
HAL_PLIST := resources/OpenA8DJ.driver/Contents/Info.plist
HAL_SMOKE := build/hal-smoke
HAL_SMOKE_SRC := src/tools/hal-smoke.c
HAL_PARITY_SMOKE := build/hal-parity-smoke
HAL_PARITY_SMOKE_SRC := src/tools/hal-parity-smoke.c
AUDIO_LIST := build/audio-list
AUDIO_LIST_SRC := src/tools/audio-list.c
AUDIO_INSPECT := build/audio-inspect
AUDIO_INSPECT_SRC := src/tools/audio-inspect.c
AUDIO_IO_TEST := build/audio-io-test
AUDIO_IO_TEST_SRC := src/tools/audio-io-test.c
AUDIO_WAV_PLAY := build/audio-wav-play
AUDIO_WAV_PLAY_SRC := src/tools/audio-wav-play.c
AUDIO_RECORD := build/audio-record
AUDIO_RECORD_SRC := src/tools/audio-record.c
AUDIO_CONFIG := build/audio-config
AUDIO_CONFIG_SRC := src/tools/audio-config.c
AUDIO_DEFAULT := build/audio-default
AUDIO_DEFAULT_SRC := src/tools/audio-default.c
AUDIO_PAIR_TONE := build/audio-pair-tone
AUDIO_PAIR_TONE_SRC := src/tools/audio-pair-tone.c
AUDIO_ROUTE := build/audio-route
AUDIO_ROUTE_SRC := src/tools/audio-route.c
AUDIO_DEVICE_CONTROLS := build/audio-device-controls
AUDIO_DEVICE_CONTROLS_SRC := src/tools/audio-device-controls.c
INPUT_METER := build/audio-input-meter
INPUT_METER_SRC := src/tools/audio-input-meter.c
MACBOOK_MIC_RECORD := build/macbook-mic-record
MACBOOK_MIC_RECORD_SRC := src/tools/macbook-mic-record.c
USB_PLAY := build/opena8dj-usb-play
USB_PLAY_SRC := src/tools/opena8dj-usb-play.m
USB_INPUT_METER := build/opena8dj-usb-input-meter
USB_INPUT_METER_SRC := src/tools/opena8dj-usb-input-meter.m
USB_RESET_DEVICE := build/usb-reset-device
USB_RESET_DEVICE_SRC := src/tools/usb-reset-device.m
MIDI_BRIDGE := build/opena8dj-midid
MIDI_BRIDGE_SRC := src/tools/opena8dj-midid.m
CONTROL_TOOL := build/opena8dj-control
CONTROL_TOOL_SRC := src/tools/opena8dj-control.c
PUBLIC_API_TEST := tests/public_api_contract_test.py
PUBLIC_API_PEER_POLICY_TEST := tests/public_api_peer_policy_test.c
DRIVER_MODE_TEST := tests/driver_mode_offline_test.py
DRIVER_MODE_STATE_TEST := tests/driver_mode_state_test.c
USB_QUALITY_CLI_TEST := tests/usb_quality_cli_test.py
HARDWARE_PROFILER := build/opena8dj-hardware-profiler
HARDWARE_PROFILER_TEST := build/opena8dj-hardware-profiler-test
HARDWARE_PROFILER_SRC := src/tools/opena8dj-hardware-profiler.swift
HARDWARE_PROFILER_GENERATED_SRC := build/opena8dj-hardware-profiler-main.swift
HARDWARE_PROFILER_CATALOG := resources/hardware-profiler-known-issues-v1.json
HARDWARE_PROFILER_TEST_SRC := tests/hardware_profiler_offline_test.py
CONTROL_CENTER_APP := build/OpenA8DJControlCenter.app
CONTROL_CENTER_SRC := macos/OpenA8DJControlCenter/OpenA8DJControlCenter.swift
CONTROL_CENTER_PLIST := macos/OpenA8DJControlCenter/Info.plist
MIDI_LIST := build/midi-list
MIDI_LIST_SRC := src/tools/midi-list.c
LAUNCH_AGENT_PLIST := resources/org.opena8dj.midid.plist
PKG_ROOT := build/pkgroot
PKG_SCRIPTS := resources/pkg/scripts
PKG_SANITIZER := scripts/sanitize-macos-pkg.sh
PKG := build/OpenA8DJ-$(VERSION).pkg
CONTROL_PKG_ROOT := build/control-pkgroot
CONTROL_PKG_SCRIPTS := resources/control-surfaces-pkg/scripts
CONTROL_PKG := build/opena8dj-tools-$(VERSION).pkg
DMG_ROOT := build/dmgroot
DMG := build/OpenA8DJ-$(VERSION).dmg
DMG_README := resources/dmg/README.txt
CONTROL_DMG_ROOT := build/control-dmgroot
CONTROL_DMG := build/opena8dj-tools-$(VERSION).dmg
CONTROL_DMG_README := resources/control-surfaces-dmg/README.txt
CHECKSUMS := build/OpenA8DJ-$(VERSION)-checksums.txt
RELEASE_NOTES := docs/RELEASE_NOTES_$(VERSION).md
SIGN_IDENTITY ?= -
PKG_SIGN_IDENTITY ?=
DMG_SIGN_IDENTITY ?=
HAL_DIAGNOSTIC ?= 0
HAL_OUTPUT_GAIN ?= 0.50f
HAL_OUTPUT_PREFETCH_FRAMES ?= 64
HAL_INPUT_DECODE ?= 1
HAL_INPUT_STREAMS ?= 1
HAL_INPUT_CHECKS ?= 0
HAL_OUTPUT_STREAMS ?= 4
HAL_ISO_FRAMES ?= 64
HAL_CAPTURE_QUEUE ?= 8
HAL_PLAYBACK_QUEUE ?= 8
HAL_PLAYBACK_CAPTURE_PACED ?= 1
HAL_CAPTURE_PACED_OUT_LEAD ?= 1
HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1
HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE ?= 0
HAL_USB_CLOCK_ANCHOR ?= 0
HAL_USB_STABLE_FRAME ?= 0
HAL_USB_ZERO_TIMESTAMP ?= 0
HAL_USB_ANCHOR_FILTER ?= 8
HAL_EXPLICIT_SCHED ?= 0
HAL_OUTPUT_NATIVE ?= 0
HAL_FAST_OUTPUT_QUANTIZER ?= 0
HAL_FAST_OUTPUT_PREFETCH_CLEAR ?= 1
HAL_STREAM_USAGE ?= 0
HAL_TRANSFER_POOL ?= 1
HAL_STRICT_TRANSFER_POOL ?= 0
HAL_TRANSFER_POOL_CURSOR ?= 0
HAL_REUSE_ISOC_COMPLETIONS ?= 0
HAL_LEGACY_OUT_SLOTS ?= 0
HAL_USB_QUEUE_QOS ?= 0
HAL_OUTPUT_SAMPLE_TIME_FOLLOWER ?= 0
HAL_CADENCE_DIAGNOSTIC ?= 0
HAL_STREAM_KEEPALIVE ?= 0
HAL_OUTPUT_AMPLITUDE_STATS ?= 0
HAL_HOT_STREAM_STATS ?= 1
HAL_HOT_STREAM_STATS_INTERVAL ?= 1
HAL_OUTPUT_WRITE_STATS ?= 1
HAL_PROPERTY_BACKOFF_USEC ?= 0
HAL_OUTPUT_START_BYTE ?= 4
HAL_VALID_CAPTURE_OUT_LAYOUT ?= 0
HAL_BACKGROUND_WARM_OPEN ?= 0
HAL_BACKGROUND_PREOPEN_ON_INIT ?= 1
HAL_STOP_GRACE_USEC ?= 10000000
HAL_OPTFLAGS ?= -O2
HAL_STOP_ISOC_ON_STOP ?= 1
HAL_FAST_ISO_TRANSFER_CONFIG ?= 0
HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM ?= 1
HAL_FLUSH_TOUCHED_OUTPUT ?= 0
SOUNDCHECK_MUSIC ?=
SOUNDCHECK_MUSIC_DIR ?= $(HOME)/Music
SOUNDCHECK_CAPTURE ?=
SOUNDCHECK_CAPTURE_CHANNELS ?= 1,2
SOUNDCHECK_PAIR ?= A
SOUNDCHECK_RATE ?= 48000
SOUNDCHECK_BUFFER ?= 512
SOUNDCHECK_SECONDS ?= 20
SOUNDCHECK_PREFLIGHT_SECONDS ?= 5
SOUNDCHECK_MODE ?= dense
SOUNDCHECK_PREFLIGHT_MODE ?= start
SOUNDCHECK_CPU_STRESS ?= 0
SOUNDCHECK_CPU_STRESS_AFTER ?= 5
SOUNDCHECK_CPU_STRESS_SECONDS ?= 8
SOUNDCHECK_CPU_STRESS_WORKERS ?= auto
SOUNDCHECK_MAX_MID_BAND_RESIDUAL_RATIO ?= 0.04
SOUNDCHECK_MAX_MID_BAND_CPU_CORR ?= 0.60
SIM_OUTPUT_SECONDS ?= 3
SIM_OUTPUT_MODE ?= dense
SIM_OUTPUT_PAIR ?= A
SIM_OUTPUT_GAIN ?= 0.5
PLAYBACK_GATE_MUSIC ?= local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav
PLAYBACK_GATE_PAIR ?= A
PLAYBACK_GATE_CPU_STRESS ?= 1
PLAYBACK_GATE_CPU_STRESS_AFTER ?= 5
PLAYBACK_GATE_CPU_STRESS_SECONDS ?= 8
PLAYBACK_GATE_CPU_STRESS_WORKERS ?= auto
PLAYBACK_GATE_UI_STRESS ?= 1
PLAYBACK_GATE_UI_STRESS_AFTER ?= 12
PLAYBACK_GATE_UI_STRESS_SECONDS ?= 8
PLAYBACK_GATE_UI_STRESS_INTERVAL ?= 0.5
NO_IRIG_CLICK_RISK_RUNS ?= 3
NO_IRIG_CLICK_RISK_LABEL ?= $(VERSION)
DIGITAL_AUDIO_GATE_LABEL ?= $(VERSION)
DIGITAL_AUDIO_GATE_PAIRS ?= A,B,C,D
DIGITAL_AUDIO_GATE_SIM_SECONDS ?= 6
DIGITAL_AUDIO_GATE_SIM_MODE ?= dense
DIGITAL_AUDIO_GATE_CLICK_RISK_RUNS ?= 1
IRIG_RECOVERY_WAIT ?= 120
IRIG_RECOVERY_INTERVAL ?= 2
IRIG_RECOVERY_RECORD_SECONDS ?= 2
IRIG_RECOVERY_CANDIDATE ?= current-loaded
IRIG_RECOVERY_MUSIC ?= local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav
IRIG_RECOVERY_BASELINE_JSON ?= local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/physical-music-gate.json
IRIG_RECOVERY_RUN_CANDIDATE_GATE ?= 0
CANDIDATE_PREFLIGHT_LABEL ?= current-loaded
CANDIDATE_PREFLIGHT_MUSIC ?= local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav
CANDIDATE_PREFLIGHT_BASELINE_JSON ?= local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/physical-music-gate.json
CANDIDATE_PREFLIGHT_IRIG_WAIT ?= 120
CANDIDATE_PREFLIGHT_IRIG_INTERVAL ?= 2
CANDIDATE_PREFLIGHT_IRIG_RECORD_SECONDS ?= 2
CANDIDATE_PREFLIGHT_RUN_PHYSICAL_GATE ?= 1
CANDIDATE_WATCH_LABEL ?= current-loaded
CANDIDATE_WATCH_MUSIC ?= local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav
CANDIDATE_WATCH_BASELINE_JSON ?= local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/physical-music-gate.json
CANDIDATE_WATCH_WAIT ?= 3600
CANDIDATE_WATCH_INTERVAL ?= 5
CANDIDATE_WATCH_STABLE_POLLS ?= 3
CANDIDATE_READY_EMAIL_TO ?= fernandosanchezmunoz@gmail.com
CANDIDATE_WATCH_READY_EMAIL_WAIT ?= 3600
CANDIDATE_WATCH_READY_EMAIL_INTERVAL ?= 5
CANDIDATE_WATCH_READY_EMAIL_STABLE_POLLS ?= $(CANDIDATE_WATCH_STABLE_POLLS)
SAFE_REPLUG_WATCH_WAIT ?= 7200
SAFE_REPLUG_WATCH_INTERVAL ?= 5
SAFE_REPLUG_WATCH_STABLE_POLLS ?= $(CANDIDATE_WATCH_STABLE_POLLS)
AUTONOMOUS_AUDIO_QA_WAIT ?= 0
AUTONOMOUS_AUDIO_QA_INTERVAL ?= 30
AUTONOMOUS_AUDIO_QA_STABLE_POLLS ?= $(CANDIDATE_WATCH_STABLE_POLLS)
AUTONOMOUS_AUDIO_QA_RECOVERY_WAIT ?= 20
AUTONOMOUS_AUDIO_QA_RECOVERY_INTERVAL_CYCLES ?= 10
AUTONOMOUS_AUDIO_QA_CANDIDATE_GATE_WAIT ?= 240
OUTPUT_PAIR_GATE_LABEL ?= current-loaded
OUTPUT_PAIR_GATE_PAIRS ?= A,B,C,D
OUTPUT_PAIR_GATE_SECONDS ?= 2
CAPTURE_DIAGNOSE_DEVICE ?= iRig Stream
PHYSICAL_BENCH_CAPTURE_DEVICE ?= iRig Stream
PHYSICAL_BENCH_CAPTURE_CHANNELS ?= 1,2
PHYSICAL_BENCH_RECORD_SECONDS ?= 2
IRIG_ISOLATION_WAIT ?= 8
IRIG_USB_RECOVERY_WAIT ?= 60
IRIG_USB_RECOVERY_INTERVAL ?= 3
QUALITY_WINDOW_LABEL ?= current-loaded
QUALITY_WINDOW_WAIT_LOCK ?= 300

CC := xcrun clang
CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic -O2
HAL_CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic $(HAL_OPTFLAGS) -DOPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE=$(HAL_DIAGNOSTIC) -DOPENA8DJ_OUTPUT_GAIN=$(HAL_OUTPUT_GAIN) -DOPENA8DJ_OUTPUT_PREFETCH_FRAMES=$(HAL_OUTPUT_PREFETCH_FRAMES) -DOPENA8DJ_ENABLE_INPUT_DECODE=$(HAL_INPUT_DECODE) -DOPENA8DJ_INPUT_STREAM_COUNT=$(HAL_INPUT_STREAMS) -DOPENA8DJ_ENABLE_INPUT_CHECKS=$(HAL_INPUT_CHECKS) -DOPENA8DJ_OUTPUT_STREAM_COUNT=$(HAL_OUTPUT_STREAMS) -DOPENA8DJ_ISO_FRAMES_PER_TRANSFER=$(HAL_ISO_FRAMES) -DOPENA8DJ_CAPTURE_QUEUE_DEPTH=$(HAL_CAPTURE_QUEUE) -DOPENA8DJ_PLAYBACK_QUEUE_TARGET=$(HAL_PLAYBACK_QUEUE) -DOPENA8DJ_PLAYBACK_CAPTURE_PACED=$(HAL_PLAYBACK_CAPTURE_PACED) -DOPENA8DJ_CAPTURE_PACED_OUT_LEAD=$(HAL_CAPTURE_PACED_OUT_LEAD) -DOPENA8DJ_PLAYBACK_COALESCE_TRANSFERS=$(HAL_PLAYBACK_COALESCE_TRANSFERS) -DOPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=$(HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE) -DOPENA8DJ_ENABLE_USB_CLOCK_ANCHOR=$(HAL_USB_CLOCK_ANCHOR) -DOPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL=$(HAL_USB_STABLE_FRAME) -DOPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP=$(HAL_USB_ZERO_TIMESTAMP) -DOPENA8DJ_USB_ANCHOR_FILTER=$(HAL_USB_ANCHOR_FILTER) -DOPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING=$(HAL_EXPLICIT_SCHED) -DOPENA8DJ_OUTPUT_NATIVE_I24=$(HAL_OUTPUT_NATIVE) -DOPENA8DJ_FAST_OUTPUT_QUANTIZER=$(HAL_FAST_OUTPUT_QUANTIZER) -DOPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR=$(HAL_FAST_OUTPUT_PREFETCH_CLEAR) -DOPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY=$(HAL_STREAM_USAGE) -DOPENA8DJ_ENABLE_TRANSFER_POOL=$(HAL_TRANSFER_POOL) -DOPENA8DJ_STRICT_TRANSFER_POOL=$(HAL_STRICT_TRANSFER_POOL) -DOPENA8DJ_TRANSFER_POOL_CURSOR=$(HAL_TRANSFER_POOL_CURSOR) -DOPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS=$(HAL_REUSE_ISOC_COMPLETIONS) -DOPENA8DJ_ENABLE_LEGACY_OUT_SLOTS=$(HAL_LEGACY_OUT_SLOTS) -DOPENA8DJ_USB_QUEUE_QOS=$(HAL_USB_QUEUE_QOS) -DOPENA8DJ_PROPERTY_BACKOFF_USEC=$(HAL_PROPERTY_BACKOFF_USEC) -DOPENA8DJ_OUTPUT_START_BYTE=$(HAL_OUTPUT_START_BYTE) -DOPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER=$(HAL_OUTPUT_SAMPLE_TIME_FOLLOWER) -DOPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC=$(HAL_CADENCE_DIAGNOSTIC) -DOPENA8DJ_ENABLE_STREAM_KEEPALIVE=$(HAL_STREAM_KEEPALIVE) -DOPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS=$(HAL_OUTPUT_AMPLITUDE_STATS) -DOPENA8DJ_ENABLE_HOT_STREAM_STATS=$(HAL_HOT_STREAM_STATS) -DOPENA8DJ_HOT_STREAM_STATS_INTERVAL=$(HAL_HOT_STREAM_STATS_INTERVAL) -DOPENA8DJ_ENABLE_OUTPUT_WRITE_STATS=$(HAL_OUTPUT_WRITE_STATS) -DOPENA8DJ_VALID_CAPTURE_OUT_LAYOUT=$(HAL_VALID_CAPTURE_OUT_LAYOUT) -DOPENA8DJ_BACKGROUND_WARM_OPEN=$(HAL_BACKGROUND_WARM_OPEN) -DOPENA8DJ_BACKGROUND_PREOPEN_ON_INIT=$(HAL_BACKGROUND_PREOPEN_ON_INIT) -DOPENA8DJ_STOP_GRACE_USEC=$(HAL_STOP_GRACE_USEC) -DOPENA8DJ_STOP_ISOC_ON_STOP=$(HAL_STOP_ISOC_ON_STOP) -DOPENA8DJ_FAST_ISO_TRANSFER_CONFIG=$(HAL_FAST_ISO_TRANSFER_CONFIG) -DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=$(HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM) -DOPENA8DJ_FLUSH_TOUCHED_OUTPUT=$(HAL_FLUSH_TOUCHED_OUTPUT)
FRAMEWORKS := -framework Foundation -framework IOKit -framework IOUSBHost
HAL_FRAMEWORKS := -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -framework CoreMIDI -framework Foundation -framework IOKit -framework IOUSBHost
MIDI_FRAMEWORKS := -framework Foundation -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

.PHONY: all clean probe claim hal sign-hal install-hal install-midid install-tools install-control-surfaces control-center driver-mode-offline-test public-api-offline-test usb-quality-offline-test hardware-profiler-offline-test smoke-hal parity-smoke-hal audio-list audio-inspect audio-io-test audio-wav-play audio-record audio-config audio-default audio-pair-tone audio-route audio-device-controls audio-input-meter macbook-mic-record audio-stack-health audio-stack-guard audio-stack-recover audio-stack-reset soundcheck-preflight soundcheck simulated-output-soundcheck playback-cpu-gate no-irig-click-risk-gate digital-audio-quality-gate output-pair-smoke-gate capture-device-diagnose capture-device-diagnose-selftest physical-bench-sanity-gate physical-music-quality-gate-selftest timecode-smoke-gate irig-recovery-gate irig-isolation-diagnose irig-usb-recovery-diagnose candidate-preflight candidate-watch candidate-status candidate-ready-email-gate candidate-watch-ready-email-gate safe-replug-watch-start safe-replug-watch-status safe-replug-watch-stop autonomous-audio-qa-start autonomous-audio-qa-status autonomous-audio-qa-stop shared-hardware-lock-status quality-window-internal quality-window-candidate usb-play usb-input-meter usb-reset-device midi-list package control-surfaces-package tools-package dmg control-surfaces-dmg tools-dmg checksums dist FORCE

all: $(TOOL) hal $(AUDIO_LIST) $(AUDIO_INSPECT) $(AUDIO_IO_TEST) $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(AUDIO_CONFIG) $(AUDIO_DEFAULT) $(AUDIO_PAIR_TONE) $(AUDIO_ROUTE) $(AUDIO_DEVICE_CONTROLS) $(INPUT_METER) $(MACBOOK_MIC_RECORD) $(USB_PLAY) $(USB_INPUT_METER) $(USB_RESET_DEVICE) $(MIDI_BRIDGE) $(CONTROL_TOOL) $(HARDWARE_PROFILER) $(MIDI_LIST)

$(TOOL): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $<

hal: $(HAL_BIN)

$(HAL_CONFIG): FORCE
	@mkdir -p build
	@tmp="$@.tmp"; \
	{ \
		printf '%s\n' '$(HAL_CFLAGS)'; \
		printf 'VERSION=%s\n' '$(VERSION)'; \
	} > "$$tmp"; \
	if [ ! -f "$@" ] || ! cmp -s "$$tmp" "$@"; then \
		mv "$$tmp" "$@"; \
	else \
		rm -f "$$tmp"; \
	fi

$(HAL_BIN): $(HAL_SRC) $(HAL_IPC_AUTH) $(DRIVER_MODE_HEADER) $(HAL_PLIST) $(HAL_CONFIG)
	@mkdir -p $(HAL_BUNDLE)/Contents/MacOS
	@cp $(HAL_PLIST) $(HAL_BUNDLE)/Contents/Info.plist
	@/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(VERSION)" $(HAL_BUNDLE)/Contents/Info.plist
	@/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(VERSION)" $(HAL_BUNDLE)/Contents/Info.plist
	xcrun clang $(HAL_CFLAGS) -bundle $(HAL_FRAMEWORKS) -o $@ $(HAL_SRC)

sign-hal: $(HAL_BIN)
	codesign --force --sign "$(SIGN_IDENTITY)" --timestamp=none $(HAL_BUNDLE)

$(HAL_SMOKE): $(HAL_SMOKE_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

smoke-hal: $(HAL_BIN) $(HAL_SMOKE)
	./$(HAL_SMOKE) $(HAL_BUNDLE)

$(HAL_PARITY_SMOKE): $(HAL_PARITY_SMOKE_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

parity-smoke-hal: $(HAL_BIN) $(HAL_PARITY_SMOKE)
	./$(HAL_PARITY_SMOKE) $(HAL_BUNDLE)

$(AUDIO_LIST): $(AUDIO_LIST_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-list: $(AUDIO_LIST)
	./$(AUDIO_LIST)

$(AUDIO_INSPECT): $(AUDIO_INSPECT_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-inspect: $(AUDIO_INSPECT)
	./$(AUDIO_INSPECT)

$(AUDIO_IO_TEST): $(AUDIO_IO_TEST_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-io-test: $(AUDIO_IO_TEST)
	./$(AUDIO_IO_TEST)

$(AUDIO_WAV_PLAY): $(AUDIO_WAV_PLAY_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-wav-play: $(AUDIO_WAV_PLAY)
	./$(AUDIO_WAV_PLAY)

$(AUDIO_RECORD): $(AUDIO_RECORD_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-record: $(AUDIO_RECORD)
	./$(AUDIO_RECORD)

$(AUDIO_CONFIG): $(AUDIO_CONFIG_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-config: $(AUDIO_CONFIG)
	./$(AUDIO_CONFIG) org.opena8dj.Audio8DJ 48000 512

$(AUDIO_DEFAULT): $(AUDIO_DEFAULT_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-default: $(AUDIO_DEFAULT)
	./$(AUDIO_DEFAULT)

$(AUDIO_PAIR_TONE): $(AUDIO_PAIR_TONE_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -o $@ $<

audio-pair-tone: $(AUDIO_PAIR_TONE)
	./$(AUDIO_PAIR_TONE) A 3 440 0.06

$(AUDIO_ROUTE): $(AUDIO_ROUTE_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-route: $(AUDIO_ROUTE)
	./$(AUDIO_ROUTE) org.opena8dj.Audio8DJ both

$(AUDIO_DEVICE_CONTROLS): $(AUDIO_DEVICE_CONTROLS_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-device-controls: $(AUDIO_DEVICE_CONTROLS)
	./$(AUDIO_DEVICE_CONTROLS) "iRig Stream"

$(INPUT_METER): $(INPUT_METER_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

audio-input-meter: $(INPUT_METER)
	./$(INPUT_METER) 5

$(MACBOOK_MIC_RECORD): $(MACBOOK_MIC_RECORD_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -o $@ $<

macbook-mic-record: $(MACBOOK_MIC_RECORD)

audio-stack-health:
	./scripts/audio-stack-health

audio-stack-guard:
	./scripts/audio-stack-guard

audio-stack-recover:
	./scripts/audio-stack-guard --recover --unload-opena8dj

audio-stack-reset:
	./scripts/audio-stack-guard --recover --unload-opena8dj

soundcheck-preflight: $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(AUDIO_CONFIG) $(CONTROL_TOOL)
	./scripts/run-soundcheck --skip-build --prepare-only \
		$(if $(SOUNDCHECK_MUSIC),--music-file "$(SOUNDCHECK_MUSIC)",--music-dir "$(SOUNDCHECK_MUSIC_DIR)") \
		--pair "$(SOUNDCHECK_PAIR)" --rate "$(SOUNDCHECK_RATE)" --buffer "$(SOUNDCHECK_BUFFER)" \
		--seconds "$(SOUNDCHECK_PREFLIGHT_SECONDS)" --mode "$(SOUNDCHECK_PREFLIGHT_MODE)"

soundcheck: $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(AUDIO_CONFIG) $(CONTROL_TOOL)
	./scripts/run-soundcheck --skip-build \
		$(if $(SOUNDCHECK_MUSIC),--music-file "$(SOUNDCHECK_MUSIC)",--music-dir "$(SOUNDCHECK_MUSIC_DIR)") \
		--pair "$(SOUNDCHECK_PAIR)" --rate "$(SOUNDCHECK_RATE)" --buffer "$(SOUNDCHECK_BUFFER)" \
		--seconds "$(SOUNDCHECK_SECONDS)" --mode "$(SOUNDCHECK_MODE)" \
		--capture-device "$(SOUNDCHECK_CAPTURE)" --capture-channels "$(SOUNDCHECK_CAPTURE_CHANNELS)" \
		--max-mid-band-residual-ratio "$(SOUNDCHECK_MAX_MID_BAND_RESIDUAL_RATIO)" \
		--max-mid-band-cpu-corr "$(SOUNDCHECK_MAX_MID_BAND_CPU_CORR)" $(if $(filter 1 true yes on,$(SOUNDCHECK_CPU_STRESS)),--cpu-stress --cpu-stress-after "$(SOUNDCHECK_CPU_STRESS_AFTER)" --cpu-stress-seconds "$(SOUNDCHECK_CPU_STRESS_SECONDS)" --cpu-stress-workers "$(SOUNDCHECK_CPU_STRESS_WORKERS)",)

simulated-output-soundcheck:
	./scripts/run-simulated-output-soundcheck \
		$(if $(SOUNDCHECK_MUSIC),--music-file "$(SOUNDCHECK_MUSIC)",--music-dir "$(SOUNDCHECK_MUSIC_DIR)") \
		--pair "$(SIM_OUTPUT_PAIR)" --rate "$(SOUNDCHECK_RATE)" \
		--seconds "$(SIM_OUTPUT_SECONDS)" --mode "$(SIM_OUTPUT_MODE)" \
		--gain "$(SIM_OUTPUT_GAIN)" \
		--max-mid-band-residual-ratio "$(SOUNDCHECK_MAX_MID_BAND_RESIDUAL_RATIO)" \
		--max-mid-band-cpu-corr "$(SOUNDCHECK_MAX_MID_BAND_CPU_CORR)"

playback-cpu-gate: $(AUDIO_WAV_PLAY) $(CONTROL_TOOL)
	./scripts/playback-cpu-gate \
		--music-file "$(PLAYBACK_GATE_MUSIC)" \
		--pair "$(PLAYBACK_GATE_PAIR)" \
		--max-elastic-drops 0 \
		--max-elastic-replays 0 \
		--max-late-write-frames 0 \
		--max-late-write-batches 0 \
		--max-playback-completion-delta-outliers 0 \
		--max-capture-to-playback-queue-delta-outliers 0 \
		--max-playback-zero-complete-transactions 0 \
		$(if $(filter 1 true yes on,$(PLAYBACK_GATE_CPU_STRESS)),--cpu-stress --cpu-stress-after "$(PLAYBACK_GATE_CPU_STRESS_AFTER)" --cpu-stress-seconds "$(PLAYBACK_GATE_CPU_STRESS_SECONDS)" --cpu-stress-workers "$(PLAYBACK_GATE_CPU_STRESS_WORKERS)",) \
		$(if $(filter 1 true yes on,$(PLAYBACK_GATE_UI_STRESS)),--ui-stress --ui-stress-after "$(PLAYBACK_GATE_UI_STRESS_AFTER)" --ui-stress-seconds "$(PLAYBACK_GATE_UI_STRESS_SECONDS)" --ui-stress-interval "$(PLAYBACK_GATE_UI_STRESS_INTERVAL)",)

no-irig-click-risk-gate: $(AUDIO_WAV_PLAY) $(CONTROL_TOOL)
	./scripts/no-irig-click-risk-gate \
		--candidate "$(NO_IRIG_CLICK_RISK_LABEL)" \
		--runs "$(NO_IRIG_CLICK_RISK_RUNS)" \
		--music-file "$(PLAYBACK_GATE_MUSIC)"

digital-audio-quality-gate: $(AUDIO_WAV_PLAY) $(CONTROL_TOOL)
	./scripts/digital-audio-quality-gate \
		--candidate "$(DIGITAL_AUDIO_GATE_LABEL)" \
		--music-file "$(PLAYBACK_GATE_MUSIC)" \
		--pairs "$(DIGITAL_AUDIO_GATE_PAIRS)" \
		--sim-seconds "$(DIGITAL_AUDIO_GATE_SIM_SECONDS)" \
		--sim-mode "$(DIGITAL_AUDIO_GATE_SIM_MODE)" \
		--click-risk-runs "$(DIGITAL_AUDIO_GATE_CLICK_RISK_RUNS)"

output-pair-smoke-gate: $(AUDIO_WAV_PLAY) $(CONTROL_TOOL)
	./scripts/output-pair-smoke-gate \
		--candidate "$(OUTPUT_PAIR_GATE_LABEL)" \
		--pairs "$(OUTPUT_PAIR_GATE_PAIRS)" \
		--seconds "$(OUTPUT_PAIR_GATE_SECONDS)"

capture-device-diagnose: $(AUDIO_LIST)
	./scripts/capture-device-diagnose \
		--preferred-capture-device "$(CAPTURE_DIAGNOSE_DEVICE)"

capture-device-diagnose-selftest: $(AUDIO_LIST)
	./scripts/capture-device-diagnose-selftest

physical-bench-sanity-gate: $(AUDIO_LIST) $(AUDIO_RECORD)
	./scripts/physical-bench-sanity-gate \
		--capture-device "$(PHYSICAL_BENCH_CAPTURE_DEVICE)" \
		--capture-channels "$(PHYSICAL_BENCH_CAPTURE_CHANNELS)" \
		--record-seconds "$(PHYSICAL_BENCH_RECORD_SECONDS)"

physical-music-quality-gate-selftest:
	./scripts/physical-music-quality-gate-selftest

timecode-smoke-gate: $(AUDIO_LIST) $(AUDIO_IO_TEST) $(CONTROL_TOOL)
	./scripts/timecode-smoke-gate

irig-recovery-gate: $(AUDIO_LIST) $(AUDIO_RECORD)
	./scripts/irig-recovery-gate \
		--wait "$(IRIG_RECOVERY_WAIT)" \
		--interval "$(IRIG_RECOVERY_INTERVAL)" \
		--record-seconds "$(IRIG_RECOVERY_RECORD_SECONDS)" \
		--candidate "$(IRIG_RECOVERY_CANDIDATE)" \
		--music-file "$(IRIG_RECOVERY_MUSIC)" \
		--physical-baseline-json "$(IRIG_RECOVERY_BASELINE_JSON)" \
		$(if $(filter 1 true yes on,$(IRIG_RECOVERY_RUN_CANDIDATE_GATE)),--run-candidate-gate,)

irig-isolation-diagnose: $(AUDIO_LIST)
	./scripts/irig-isolation-diagnose \
		--wait "$(IRIG_ISOLATION_WAIT)"

irig-usb-recovery-diagnose: $(AUDIO_LIST) $(USB_RESET_DEVICE)
	./scripts/irig-usb-recovery-diagnose \
		--wait "$(IRIG_USB_RECOVERY_WAIT)" \
		--interval "$(IRIG_USB_RECOVERY_INTERVAL)"

candidate-preflight: $(AUDIO_LIST) $(AUDIO_RECORD) $(AUDIO_WAV_PLAY) $(CONTROL_TOOL)
	./scripts/candidate-preflight \
		--candidate "$(CANDIDATE_PREFLIGHT_LABEL)" \
		--music-file "$(CANDIDATE_PREFLIGHT_MUSIC)" \
		--physical-baseline-json "$(CANDIDATE_PREFLIGHT_BASELINE_JSON)" \
		--irig-wait "$(CANDIDATE_PREFLIGHT_IRIG_WAIT)" \
		--irig-interval "$(CANDIDATE_PREFLIGHT_IRIG_INTERVAL)" \
		--irig-record-seconds "$(CANDIDATE_PREFLIGHT_IRIG_RECORD_SECONDS)" \
		$(if $(filter 0 false no off,$(CANDIDATE_PREFLIGHT_RUN_PHYSICAL_GATE)),--no-physical-gate,)

candidate-watch: $(AUDIO_LIST)
	./scripts/candidate-watch \
		--candidate "$(CANDIDATE_WATCH_LABEL)" \
		--music-file "$(CANDIDATE_WATCH_MUSIC)" \
		--physical-baseline-json "$(CANDIDATE_WATCH_BASELINE_JSON)" \
		--wait "$(CANDIDATE_WATCH_WAIT)" \
		--interval "$(CANDIDATE_WATCH_INTERVAL)" \
		--stable-polls "$(CANDIDATE_WATCH_STABLE_POLLS)"

candidate-status: $(AUDIO_LIST)
	./scripts/candidate-status

candidate-ready-email-gate: $(AUDIO_LIST)
	./scripts/candidate-ready-email-gate \
		--to "$(CANDIDATE_READY_EMAIL_TO)"

candidate-watch-ready-email-gate: $(AUDIO_LIST)
	./scripts/candidate-watch-ready-email-gate \
		--candidate "$(CANDIDATE_WATCH_LABEL)" \
		--music-file "$(CANDIDATE_WATCH_MUSIC)" \
		--physical-baseline-json "$(CANDIDATE_WATCH_BASELINE_JSON)" \
		--wait "$(CANDIDATE_WATCH_READY_EMAIL_WAIT)" \
		--interval "$(CANDIDATE_WATCH_READY_EMAIL_INTERVAL)" \
		--stable-polls "$(CANDIDATE_WATCH_READY_EMAIL_STABLE_POLLS)" \
		--to "$(CANDIDATE_READY_EMAIL_TO)"

safe-replug-watch-start: $(AUDIO_LIST)
	./scripts/start-safe-replug-watch \
		--candidate "$(CANDIDATE_WATCH_LABEL)" \
		--wait "$(SAFE_REPLUG_WATCH_WAIT)" \
		--interval "$(SAFE_REPLUG_WATCH_INTERVAL)" \
		--stable-polls "$(SAFE_REPLUG_WATCH_STABLE_POLLS)" \
		--to "$(CANDIDATE_READY_EMAIL_TO)"

safe-replug-watch-status:
	./scripts/safe-replug-watch-status

safe-replug-watch-stop:
	./scripts/stop-safe-replug-watch

autonomous-audio-qa-start: $(AUDIO_LIST)
	./scripts/start-autonomous-audio-qa \
		--candidate "$(CANDIDATE_WATCH_LABEL)" \
		--wait "$(AUTONOMOUS_AUDIO_QA_WAIT)" \
		--interval "$(AUTONOMOUS_AUDIO_QA_INTERVAL)" \
		--stable-polls "$(AUTONOMOUS_AUDIO_QA_STABLE_POLLS)" \
		--recovery-wait "$(AUTONOMOUS_AUDIO_QA_RECOVERY_WAIT)" \
		--recovery-interval-cycles "$(AUTONOMOUS_AUDIO_QA_RECOVERY_INTERVAL_CYCLES)" \
		--candidate-gate-wait "$(AUTONOMOUS_AUDIO_QA_CANDIDATE_GATE_WAIT)" \
		--to "$(CANDIDATE_READY_EMAIL_TO)"

autonomous-audio-qa-status:
	./scripts/autonomous-audio-qa-status

autonomous-audio-qa-stop:
	./scripts/stop-autonomous-audio-qa

shared-hardware-lock-status:
	./scripts/shared-hardware-lock-status

quality-window-internal: $(AUDIO_LIST) $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(CONTROL_TOOL)
	./scripts/run-quality-window \
		--candidate "$(QUALITY_WINDOW_LABEL)" \
		--mode internal \
		--wait-lock "$(QUALITY_WINDOW_WAIT_LOCK)" \
		--music-file "$(PLAYBACK_GATE_MUSIC)"

quality-window-candidate: $(AUDIO_LIST) $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(CONTROL_TOOL)
	./scripts/run-quality-window \
		--candidate "$(QUALITY_WINDOW_LABEL)" \
		--mode candidate \
		--wait-lock "$(QUALITY_WINDOW_WAIT_LOCK)" \
		--music-file "$(CANDIDATE_PREFLIGHT_MUSIC)" \
		--physical-baseline-json "$(CANDIDATE_PREFLIGHT_BASELINE_JSON)"

$(USB_PLAY): $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m

usb-play: $(USB_PLAY)

$(USB_INPUT_METER): $(USB_INPUT_METER_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_INPUT_METER_SRC) src/hal/OpenA8DJUSB.m

usb-input-meter: $(USB_INPUT_METER)
	./$(USB_INPUT_METER) 6 48000

$(USB_RESET_DEVICE): $(USB_RESET_DEVICE_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $<

usb-reset-device: $(USB_RESET_DEVICE)
	./scripts/shared-hardware-lock-run \
		--gate make-usb-reset-device \
		--run-dir local-analysis/shared-hardware-lock-run/make-usb-reset-device-$$(date +%Y%m%d-%H%M%S) \
		--wait-lock 0 \
		-- ./$(USB_RESET_DEVICE) || test $$? -eq 2

$(MIDI_BRIDGE): $(MIDI_BRIDGE_SRC)
	@mkdir -p build
	xcrun clang $(CFLAGS) $(MIDI_FRAMEWORKS) -o $@ $<

$(CONTROL_TOOL): $(CONTROL_TOOL_SRC) $(DRIVER_MODE_HEADER)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

public-api-offline-test: $(CONTROL_TOOL) $(PUBLIC_API_TEST) $(PUBLIC_API_PEER_POLICY_TEST) $(HAL_IPC_AUTH)
	python3 $(PUBLIC_API_TEST) --repo . --shipping-binary $(CONTROL_TOOL)

driver-mode-offline-test: $(CONTROL_TOOL) $(DRIVER_MODE_TEST) $(DRIVER_MODE_STATE_TEST) $(DRIVER_MODE_HEADER)
	python3 $(DRIVER_MODE_TEST) --repo . --shipping-binary $(CONTROL_TOOL)

usb-quality-offline-test: $(CONTROL_TOOL) $(USB_QUALITY_CLI_TEST)
	python3 $(USB_QUALITY_CLI_TEST) --repo .

$(HARDWARE_PROFILER_GENERATED_SRC): Makefile $(HARDWARE_PROFILER_SRC)
	@mkdir -p build
	@printf 'let openA8DJHardwareProfilerVersion = "%s"\n' "$(VERSION)" > "$@"
	@sed -n '1,$$p' "$(HARDWARE_PROFILER_SRC)" >> "$@"

$(HARDWARE_PROFILER): $(HARDWARE_PROFILER_GENERATED_SRC) $(HARDWARE_PROFILER_CATALOG)
	@mkdir -p build
	xcrun swiftc -O -framework CoreAudio -framework IOKit \
		-o "$@" "$(HARDWARE_PROFILER_GENERATED_SRC)"
	cp "$(HARDWARE_PROFILER_CATALOG)" build/hardware-profiler-known-issues-v1.json

$(HARDWARE_PROFILER_TEST): $(HARDWARE_PROFILER_GENERATED_SRC)
	@mkdir -p build
	xcrun swiftc -O -D OPENA8DJ_HARDWARE_PROFILER_TESTING \
		-framework CoreAudio -framework IOKit -o "$@" "$(HARDWARE_PROFILER_GENERATED_SRC)"

hardware-profiler-offline-test: $(HARDWARE_PROFILER) $(HARDWARE_PROFILER_TEST) $(HARDWARE_PROFILER_TEST_SRC)
	python3 "$(HARDWARE_PROFILER_TEST_SRC)" --repo .

$(CONTROL_CENTER_APP): $(CONTROL_CENTER_SRC) $(CONTROL_CENTER_PLIST) $(CONTROL_TOOL)
	rm -rf "$(CONTROL_CENTER_APP)"
	mkdir -p "$(CONTROL_CENTER_APP)/Contents/MacOS" "$(CONTROL_CENTER_APP)/Contents/Resources"
	cp "$(CONTROL_CENTER_PLIST)" "$(CONTROL_CENTER_APP)/Contents/Info.plist"
	cp "$(CONTROL_TOOL)" "$(CONTROL_CENTER_APP)/Contents/Resources/opena8dj-control"
	xcrun swiftc -parse-as-library -O -framework SwiftUI -framework AppKit -o "$(CONTROL_CENTER_APP)/Contents/MacOS/OpenA8DJControlCenter" "$(CONTROL_CENTER_SRC)"
	codesign --force --deep --sign - "$(CONTROL_CENTER_APP)"

control-center: $(CONTROL_CENTER_APP)

$(MIDI_LIST): $(MIDI_LIST_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreMIDI -framework CoreFoundation -o $@ $<

midi-list: $(MIDI_LIST)
	./$(MIDI_LIST)

install-hal: sign-hal
	./scripts/test-hal-candidate-safety \
		--candidate "$(HAL_BUNDLE)" \
		--cycles 1 \
		--leave-loaded \
		--run-dir local-analysis/hal-candidate-safety/make-install-hal-$$(date +%Y%m%d-%H%M%S)

install-tools: $(CONTROL_TOOL) $(HARDWARE_PROFILER) $(HARDWARE_PROFILER_CATALOG)
	sudo install -d /usr/local/bin "/Library/Application Support/OpenA8DJ"
	sudo install -m 755 $(CONTROL_TOOL) /usr/local/bin/opena8dj-control
	sudo install -m 755 $(HARDWARE_PROFILER) /usr/local/bin/opena8dj-hardware-profiler
	sudo install -m 644 $(HARDWARE_PROFILER_CATALOG) "/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"

install-control-surfaces: $(CONTROL_CENTER_APP) $(HARDWARE_PROFILER) $(HARDWARE_PROFILER_CATALOG)
	sudo install -d /usr/local/bin /Applications /Library/Documentation/OpenA8DJ/ControlSurfaces "/Library/Application Support/OpenA8DJ"
	sudo install -m 755 $(CONTROL_TOOL) /usr/local/bin/opena8dj-control
	sudo install -m 755 $(HARDWARE_PROFILER) /usr/local/bin/opena8dj-hardware-profiler
	sudo install -m 644 $(HARDWARE_PROFILER_CATALOG) "/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"
	sudo rm -rf "/Applications/OpenA8DJ Control Center.app"
	COPYFILE_DISABLE=1 sudo cp -R "$(CONTROL_CENTER_APP)" "/Applications/OpenA8DJ Control Center.app"
	sudo install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_USER_GUIDE.md /Library/Documentation/OpenA8DJ/ControlSurfaces/USER_GUIDE.md
	sudo install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md /Library/Documentation/OpenA8DJ/ControlSurfaces/DEMO_RUNBOOK_2026-06-19.md
	sudo install -m 644 docs/AUDIO8DJ_CONTROL_SURFACE_VERIFICATION_2026-06-19.md /Library/Documentation/OpenA8DJ/ControlSurfaces/VERIFICATION_2026-06-19.md
	sudo install -m 755 "$(CONTROL_PKG_SCRIPTS)/uninstall-opena8dj-control-surfaces.sh" /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh
	sudo xattr -cr "/Applications/OpenA8DJ Control Center.app" /Library/Documentation/OpenA8DJ/ControlSurfaces 2>/dev/null || true

install-midid: $(MIDI_BRIDGE) $(LAUNCH_AGENT_PLIST)
	sudo install -d /usr/local/bin
	sudo install -m 755 $(MIDI_BRIDGE) /usr/local/bin/opena8dj-midid
	sudo install -d /Library/LaunchAgents
	sudo install -m 644 $(LAUNCH_AGENT_PLIST) /Library/LaunchAgents/org.opena8dj.midid.plist
	launchctl bootout gui/$$(id -u) "$$HOME/Library/LaunchAgents/org.opena8dj.midid.plist" 2>/dev/null || true
	rm -f "$$HOME/Library/LaunchAgents/org.opena8dj.midid.plist"
	launchctl bootout gui/$$(id -u) /Library/LaunchAgents/org.opena8dj.midid.plist 2>/dev/null || true
	launchctl bootstrap gui/$$(id -u) /Library/LaunchAgents/org.opena8dj.midid.plist
	launchctl kickstart -k gui/$$(id -u)/org.opena8dj.midid

package: all sign-hal
	rm -rf $(PKG_ROOT)
	install -d "$(PKG_ROOT)/Library/Audio/Plug-Ins/HAL"
	COPYFILE_DISABLE=1 cp -R "$(HAL_BUNDLE)" "$(PKG_ROOT)/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver"
	install -d "$(PKG_ROOT)/usr/local/bin"
	install -m 755 "$(CONTROL_TOOL)" "$(PKG_ROOT)/usr/local/bin/opena8dj-control"
	install -m 755 "$(HARDWARE_PROFILER)" "$(PKG_ROOT)/usr/local/bin/opena8dj-hardware-profiler"
	install -m 755 "$(MIDI_BRIDGE)" "$(PKG_ROOT)/usr/local/bin/opena8dj-midid"
	install -d "$(PKG_ROOT)/Library/Application Support/OpenA8DJ"
	install -m 644 "$(HARDWARE_PROFILER_CATALOG)" "$(PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"
	install -m 755 "$(PKG_SCRIPTS)/uninstall-opena8dj.sh" "$(PKG_ROOT)/usr/local/bin/opena8dj-uninstall"
	install -d "$(PKG_ROOT)/Library/LaunchAgents"
	install -m 644 "$(LAUNCH_AGENT_PLIST)" "$(PKG_ROOT)/Library/LaunchAgents/org.opena8dj.midid.plist"
	install -d "$(PKG_ROOT)/Library/Documentation/OpenA8DJ"
	install -m 644 LICENSE "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/LICENSE"
	install -m 644 NOTICE.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/NOTICE.md"
	install -m 644 docs/LEGAL.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/LEGAL.md"
	install -m 644 docs/HARDWARE_PROFILER.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/HARDWARE_PROFILER.md"
	install -m 644 BRAND_POLICY.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/BRAND_POLICY.md"
	if [ -f "$(RELEASE_NOTES)" ]; then install -m 644 "$(RELEASE_NOTES)" "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/RELEASE_NOTES.md"; fi
	test -x "$(PKG_ROOT)/usr/local/bin/opena8dj-hardware-profiler"
	test -f "$(PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"
	test "$$(shasum -a 256 "$(HARDWARE_PROFILER_CATALOG)" | awk '{print $$1}')" = "$$(shasum -a 256 "$(PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json" | awk '{print $$1}')"
	chmod +x "$(PKG_SCRIPTS)/preinstall" "$(PKG_SCRIPTS)/postinstall" "$(PKG_SCRIPTS)/uninstall-opena8dj.sh"
	xattr -cr "$(PKG_ROOT)" 2>/dev/null || true
	find "$(PKG_ROOT)" -name '._*' -delete
	COPYFILE_DISABLE=1 pkgbuild --root "$(PKG_ROOT)" --scripts "$(PKG_SCRIPTS)" --identifier org.opena8dj.driver --version "$(VERSION)" --install-location / --filter '(^|/)\._.*' --filter '(^|/)\.DS_Store$$' $(if $(PKG_SIGN_IDENTITY),--sign "$(PKG_SIGN_IDENTITY)") "$(PKG)"
	if [ -z "$(PKG_SIGN_IDENTITY)" ]; then "$(PKG_SANITIZER)" "$(PKG)" "$(PKG_ROOT)" "$(PKG_SCRIPTS)"; fi

control-surfaces-package: $(CONTROL_CENTER_APP) $(HARDWARE_PROFILER) $(HARDWARE_PROFILER_CATALOG)
	rm -rf "$(CONTROL_PKG_ROOT)"
	install -d "$(CONTROL_PKG_ROOT)/Applications"
	COPYFILE_DISABLE=1 cp -R "$(CONTROL_CENTER_APP)" "$(CONTROL_PKG_ROOT)/Applications/OpenA8DJ Control Center.app"
	install -d "$(CONTROL_PKG_ROOT)/usr/local/bin"
	install -m 755 "$(CONTROL_TOOL)" "$(CONTROL_PKG_ROOT)/usr/local/bin/opena8dj-control"
	install -m 755 "$(HARDWARE_PROFILER)" "$(CONTROL_PKG_ROOT)/usr/local/bin/opena8dj-hardware-profiler"
	install -d "$(CONTROL_PKG_ROOT)/Library/Application Support/OpenA8DJ"
	install -m 644 "$(HARDWARE_PROFILER_CATALOG)" "$(CONTROL_PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"
	install -d "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces"
	install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_USER_GUIDE.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/USER_GUIDE.md"
	install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/DEMO_RUNBOOK_2026-06-19.md"
	install -m 644 docs/AUDIO8DJ_CONTROL_SURFACE_VERIFICATION_2026-06-19.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/VERIFICATION_2026-06-19.md"
	install -m 644 LICENSE "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/LICENSE"
	install -m 644 NOTICE.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/NOTICE.md"
	install -m 644 docs/LEGAL.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/LEGAL.md"
	install -m 644 docs/HARDWARE_PROFILER.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/HARDWARE_PROFILER.md"
	install -m 644 BRAND_POLICY.md "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/BRAND_POLICY.md"
	install -m 755 "$(CONTROL_PKG_SCRIPTS)/uninstall-opena8dj-control-surfaces.sh" "$(CONTROL_PKG_ROOT)/Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh"
	test -x "$(CONTROL_PKG_ROOT)/usr/local/bin/opena8dj-hardware-profiler"
	test -f "$(CONTROL_PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json"
	test "$$(shasum -a 256 "$(HARDWARE_PROFILER_CATALOG)" | awk '{print $$1}')" = "$$(shasum -a 256 "$(CONTROL_PKG_ROOT)/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json" | awk '{print $$1}')"
	chmod +x "$(CONTROL_PKG_SCRIPTS)/preinstall" "$(CONTROL_PKG_SCRIPTS)/postinstall" "$(CONTROL_PKG_SCRIPTS)/uninstall-opena8dj-control-surfaces.sh"
	xattr -cr "$(CONTROL_PKG_ROOT)" 2>/dev/null || true
	find "$(CONTROL_PKG_ROOT)" -name '._*' -delete
	COPYFILE_DISABLE=1 pkgbuild --root "$(CONTROL_PKG_ROOT)" --scripts "$(CONTROL_PKG_SCRIPTS)" --identifier org.opena8dj.tools --version "$(VERSION)" --install-location / --filter '(^|/)\._.*' --filter '(^|/)\.DS_Store$$' $(if $(PKG_SIGN_IDENTITY),--sign "$(PKG_SIGN_IDENTITY)") "$(CONTROL_PKG)"
	if [ -z "$(PKG_SIGN_IDENTITY)" ]; then "$(PKG_SANITIZER)" "$(CONTROL_PKG)" "$(CONTROL_PKG_ROOT)" "$(CONTROL_PKG_SCRIPTS)"; fi

tools-package: control-surfaces-package

dmg: package $(DMG_README)
	rm -rf "$(DMG_ROOT)" "$(DMG)"
	install -d "$(DMG_ROOT)"
	install -m 644 "$(PKG)" "$(DMG_ROOT)/OpenA8DJ-$(VERSION).pkg"
	install -m 644 "$(DMG_README)" "$(DMG_ROOT)/README.txt"
	install -m 644 LICENSE "$(DMG_ROOT)/LICENSE"
	install -m 644 NOTICE.md "$(DMG_ROOT)/NOTICE.md"
	install -m 644 docs/LEGAL.md "$(DMG_ROOT)/LEGAL.md"
	install -m 644 BRAND_POLICY.md "$(DMG_ROOT)/BRAND_POLICY.md"
	if [ -f "$(RELEASE_NOTES)" ]; then install -m 644 "$(RELEASE_NOTES)" "$(DMG_ROOT)/RELEASE_NOTES.md"; fi
	hdiutil create -volname "OpenA8DJ $(VERSION)" -srcfolder "$(DMG_ROOT)" -ov -format UDZO "$(DMG)"
	if [ -n "$(DMG_SIGN_IDENTITY)" ]; then codesign --force --sign "$(DMG_SIGN_IDENTITY)" --timestamp "$(DMG)"; fi

control-surfaces-dmg: control-surfaces-package $(CONTROL_DMG_README)
	rm -rf "$(CONTROL_DMG_ROOT)" "$(CONTROL_DMG)"
	install -d "$(CONTROL_DMG_ROOT)"
	install -m 644 "$(CONTROL_PKG)" "$(CONTROL_DMG_ROOT)/opena8dj-tools-$(VERSION).pkg"
	install -m 644 "$(CONTROL_DMG_README)" "$(CONTROL_DMG_ROOT)/README.txt"
	install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_USER_GUIDE.md "$(CONTROL_DMG_ROOT)/CONTROL_SURFACES_USER_GUIDE.md"
	install -m 644 docs/AUDIO8DJ_CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md "$(CONTROL_DMG_ROOT)/CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md"
	install -m 644 LICENSE "$(CONTROL_DMG_ROOT)/LICENSE"
	install -m 644 NOTICE.md "$(CONTROL_DMG_ROOT)/NOTICE.md"
	install -m 644 docs/LEGAL.md "$(CONTROL_DMG_ROOT)/LEGAL.md"
	install -m 644 BRAND_POLICY.md "$(CONTROL_DMG_ROOT)/BRAND_POLICY.md"
	hdiutil create -volname "opena8dj-tools $(VERSION)" -srcfolder "$(CONTROL_DMG_ROOT)" -ov -format UDZO "$(CONTROL_DMG)"
	if [ -n "$(DMG_SIGN_IDENTITY)" ]; then codesign --force --sign "$(DMG_SIGN_IDENTITY)" --timestamp "$(CONTROL_DMG)"; fi

tools-dmg: control-surfaces-dmg

checksums: dmg
	(cd build && shasum -a 256 "OpenA8DJ-$(VERSION).dmg" "OpenA8DJ-$(VERSION).pkg" > "OpenA8DJ-$(VERSION)-checksums.txt")

dist: checksums

probe: $(TOOL)
	./$(TOOL)

claim: $(TOOL)
	./$(TOOL) --claim

clean:
	rm -rf build
