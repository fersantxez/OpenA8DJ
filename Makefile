PROJECT := opena8dj
VERSION := 0.3.25
TOOL := build/opena8dj-probe
SRC := src/opena8dj-probe.m
HAL_BUNDLE := build/OpenA8DJ.driver
HAL_BIN := $(HAL_BUNDLE)/Contents/MacOS/OpenA8DJHAL
HAL_FLAGS_STAMP := build/.hal-cflags.stamp
HAL_SRC := src/hal/OpenA8DJHAL.c src/hal/OpenA8DJUSB.m
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
INPUT_METER := build/audio-input-meter
INPUT_METER_SRC := src/tools/audio-input-meter.c
MACBOOK_MIC_RECORD := build/macbook-mic-record
MACBOOK_MIC_RECORD_SRC := src/tools/macbook-mic-record.c
USB_PLAY := build/opena8dj-usb-play
USB_PLAY_SRC := src/tools/opena8dj-usb-play.m
USB_PLAY_PLAIN := build/opena8dj-usb-play-plain
USB_PLAY_PLAIN_GAIN05 := build/opena8dj-usb-play-plain-gain05
USB_INPUT_METER := build/opena8dj-usb-input-meter
USB_INPUT_METER_SRC := src/tools/opena8dj-usb-input-meter.m
MIDI_BRIDGE := build/opena8dj-midid
MIDI_BRIDGE_SRC := src/tools/opena8dj-midid.m
CONTROL_TOOL := build/opena8dj-control
CONTROL_TOOL_SRC := src/tools/opena8dj-control.c
MIDI_LIST := build/midi-list
MIDI_LIST_SRC := src/tools/midi-list.c
LAUNCH_AGENT_PLIST := resources/org.opena8dj.midid.plist
PKG_ROOT := build/pkgroot
PKG_SCRIPTS := resources/pkg/scripts
PKG_SANITIZER := scripts/sanitize-macos-pkg.sh
PKG := build/OpenA8DJ-$(VERSION).pkg
DMG_ROOT := build/dmgroot
DMG := build/OpenA8DJ-$(VERSION).dmg
DMG_README := resources/dmg/README.txt
CHECKSUMS := build/OpenA8DJ-$(VERSION)-checksums.txt
RELEASE_NOTES := docs/RELEASE_NOTES_$(VERSION).md
SIGN_IDENTITY ?= -
PKG_SIGN_IDENTITY ?=
DMG_SIGN_IDENTITY ?=
HAL_DIAGNOSTIC ?= 0
HAL_OUTPUT_GAIN ?= 0.50f
HAL_OUTPUT_PREFETCH_FRAMES ?= 256
HAL_INPUT_DECODE ?= 1
HAL_INPUT_DECODE_ACTIVE_GATING ?= 0
HAL_INPUT_STREAMS ?= 1
HAL_INPUT_CHECKS ?= 0
HAL_OUTPUT_STREAMS ?= 4
HAL_ISO_FRAMES ?= 5
HAL_CAPTURE_QUEUE ?= 64
HAL_PLAYBACK_QUEUE ?= 64
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
HAL_FAST_OUTPUT_PREFETCH_CLEAR ?= 0
HAL_STREAM_USAGE ?= 0
HAL_TRANSFER_POOL ?= 1
HAL_TRANSFER_POOL_CURSOR ?= 0
HAL_OUTPUT_SAMPLE_TIME_FOLLOWER ?= 0
HAL_CADENCE_DIAGNOSTIC ?= 0
HAL_STREAM_KEEPALIVE ?= 0
HAL_OUTPUT_AMPLITUDE_STATS ?= 0
HAL_OUTPUT_WRITE_STATS ?= 1
HAL_HOT_STREAM_STATS ?= 1
HAL_HOT_STREAM_STATS_INTERVAL ?= 1
HAL_PROPERTY_BACKOFF_USEC ?= 0
HAL_OUTPUT_START_BYTE ?= 4
HAL_OUTPUT_CHECK_OFFSET ?= 8
HAL_VALID_CAPTURE_OUT_LAYOUT ?= 0
HAL_BACKGROUND_WARM_OPEN ?= 0
HAL_BACKGROUND_PREOPEN_ON_INIT ?= 0
HAL_STOP_GRACE_USEC ?= 10000000
HAL_STOP_ISOC_ON_STOP ?= 0
HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM ?= 1
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

CC := xcrun clang
CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic -O2
HAL_CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic -O2 -DOPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE=$(HAL_DIAGNOSTIC) -DOPENA8DJ_OUTPUT_GAIN=$(HAL_OUTPUT_GAIN) -DOPENA8DJ_OUTPUT_PREFETCH_FRAMES=$(HAL_OUTPUT_PREFETCH_FRAMES) -DOPENA8DJ_ENABLE_INPUT_DECODE=$(HAL_INPUT_DECODE) -DOPENA8DJ_INPUT_DECODE_ACTIVE_GATING=$(HAL_INPUT_DECODE_ACTIVE_GATING) -DOPENA8DJ_INPUT_STREAM_COUNT=$(HAL_INPUT_STREAMS) -DOPENA8DJ_ENABLE_INPUT_CHECKS=$(HAL_INPUT_CHECKS) -DOPENA8DJ_OUTPUT_STREAM_COUNT=$(HAL_OUTPUT_STREAMS) -DOPENA8DJ_ISO_FRAMES_PER_TRANSFER=$(HAL_ISO_FRAMES) -DOPENA8DJ_CAPTURE_QUEUE_DEPTH=$(HAL_CAPTURE_QUEUE) -DOPENA8DJ_PLAYBACK_QUEUE_TARGET=$(HAL_PLAYBACK_QUEUE) -DOPENA8DJ_PLAYBACK_CAPTURE_PACED=$(HAL_PLAYBACK_CAPTURE_PACED) -DOPENA8DJ_CAPTURE_PACED_OUT_LEAD=$(HAL_CAPTURE_PACED_OUT_LEAD) -DOPENA8DJ_PLAYBACK_COALESCE_TRANSFERS=$(HAL_PLAYBACK_COALESCE_TRANSFERS) -DOPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=$(HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE) -DOPENA8DJ_ENABLE_USB_CLOCK_ANCHOR=$(HAL_USB_CLOCK_ANCHOR) -DOPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL=$(HAL_USB_STABLE_FRAME) -DOPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP=$(HAL_USB_ZERO_TIMESTAMP) -DOPENA8DJ_USB_ANCHOR_FILTER=$(HAL_USB_ANCHOR_FILTER) -DOPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING=$(HAL_EXPLICIT_SCHED) -DOPENA8DJ_OUTPUT_NATIVE_I24=$(HAL_OUTPUT_NATIVE) -DOPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR=$(HAL_FAST_OUTPUT_PREFETCH_CLEAR) -DOPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY=$(HAL_STREAM_USAGE) -DOPENA8DJ_ENABLE_TRANSFER_POOL=$(HAL_TRANSFER_POOL) -DOPENA8DJ_TRANSFER_POOL_CURSOR=$(HAL_TRANSFER_POOL_CURSOR) -DOPENA8DJ_PROPERTY_BACKOFF_USEC=$(HAL_PROPERTY_BACKOFF_USEC) -DOPENA8DJ_OUTPUT_START_BYTE=$(HAL_OUTPUT_START_BYTE) -DOPENA8DJ_OUTPUT_CHECK_OFFSET=$(HAL_OUTPUT_CHECK_OFFSET) -DOPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER=$(HAL_OUTPUT_SAMPLE_TIME_FOLLOWER) -DOPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC=$(HAL_CADENCE_DIAGNOSTIC) -DOPENA8DJ_ENABLE_STREAM_KEEPALIVE=$(HAL_STREAM_KEEPALIVE) -DOPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS=$(HAL_OUTPUT_AMPLITUDE_STATS) -DOPENA8DJ_ENABLE_OUTPUT_WRITE_STATS=$(HAL_OUTPUT_WRITE_STATS) -DOPENA8DJ_ENABLE_HOT_STREAM_STATS=$(HAL_HOT_STREAM_STATS) -DOPENA8DJ_HOT_STREAM_STATS_INTERVAL=$(HAL_HOT_STREAM_STATS_INTERVAL) -DOPENA8DJ_VALID_CAPTURE_OUT_LAYOUT=$(HAL_VALID_CAPTURE_OUT_LAYOUT) -DOPENA8DJ_BACKGROUND_WARM_OPEN=$(HAL_BACKGROUND_WARM_OPEN) -DOPENA8DJ_BACKGROUND_PREOPEN_ON_INIT=$(HAL_BACKGROUND_PREOPEN_ON_INIT) -DOPENA8DJ_STOP_GRACE_USEC=$(HAL_STOP_GRACE_USEC) -DOPENA8DJ_STOP_ISOC_ON_STOP=$(HAL_STOP_ISOC_ON_STOP) -DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=$(HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM)
FRAMEWORKS := -framework Foundation -framework IOKit -framework IOUSBHost
HAL_FRAMEWORKS := -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -framework CoreMIDI -framework Foundation -framework IOKit -framework IOUSBHost
MIDI_FRAMEWORKS := -framework Foundation -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

.PHONY: all clean probe claim hal sign-hal install-hal install-midid install-tools smoke-hal parity-smoke-hal audio-list audio-inspect audio-io-test audio-wav-play audio-record audio-config audio-default audio-pair-tone audio-route audio-input-meter macbook-mic-record audio-stack-health audio-stack-guard audio-stack-recover audio-stack-reset soundcheck-preflight soundcheck simulated-output-soundcheck usb-play usb-play-plain usb-play-plain-gain05 usb-input-meter midi-list package dmg checksums dist FORCE

all: $(TOOL) hal $(AUDIO_LIST) $(AUDIO_INSPECT) $(AUDIO_IO_TEST) $(AUDIO_WAV_PLAY) $(AUDIO_RECORD) $(AUDIO_CONFIG) $(AUDIO_DEFAULT) $(AUDIO_PAIR_TONE) $(AUDIO_ROUTE) $(INPUT_METER) $(MACBOOK_MIC_RECORD) $(USB_PLAY) $(USB_INPUT_METER) $(MIDI_BRIDGE) $(CONTROL_TOOL) $(MIDI_LIST)

$(TOOL): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $<

hal: $(HAL_BIN)

$(HAL_FLAGS_STAMP): FORCE
	@mkdir -p build
	@tmp="$@.tmp"; printf '%s\n' '$(HAL_CFLAGS)' > "$$tmp"; \
	if ! cmp -s "$$tmp" "$@"; then \
		mv "$$tmp" "$@"; \
		rm -f $(HAL_BIN) $(USB_PLAY); \
	else \
		rm "$$tmp"; \
	fi

$(HAL_BIN): $(HAL_SRC) $(HAL_PLIST) $(HAL_FLAGS_STAMP)
	@mkdir -p $(HAL_BUNDLE)/Contents/MacOS
	@cp $(HAL_PLIST) $(HAL_BUNDLE)/Contents/Info.plist
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

$(USB_PLAY): $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h $(HAL_FLAGS_STAMP)
	@mkdir -p build
	$(CC) $(HAL_CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m

usb-play: $(USB_PLAY)

$(USB_PLAY_PLAIN): $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m

usb-play-plain: $(USB_PLAY_PLAIN)

$(USB_PLAY_PLAIN_GAIN05): $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DOPENA8DJ_OUTPUT_GAIN=0.50f -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m

usb-play-plain-gain05: $(USB_PLAY_PLAIN_GAIN05)

$(USB_INPUT_METER): $(USB_INPUT_METER_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_INPUT_METER_SRC) src/hal/OpenA8DJUSB.m

usb-input-meter: $(USB_INPUT_METER)
	./$(USB_INPUT_METER) 6 48000

$(MIDI_BRIDGE): $(MIDI_BRIDGE_SRC)
	@mkdir -p build
	xcrun clang $(CFLAGS) $(MIDI_FRAMEWORKS) -o $@ $<

$(CONTROL_TOOL): $(CONTROL_TOOL_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -o $@ $<

$(MIDI_LIST): $(MIDI_LIST_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreMIDI -framework CoreFoundation -o $@ $<

midi-list: $(MIDI_LIST)
	./$(MIDI_LIST)

install-hal: sign-hal
	sudo install -d /Library/Audio/Plug-Ins/HAL
	sudo rm -rf /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
	sudo cp -R $(HAL_BUNDLE) /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
	sudo xattr -cr /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
	sudo codesign --force --sign "$(SIGN_IDENTITY)" --timestamp=none /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
	sudo killall coreaudiod || true

install-tools: $(CONTROL_TOOL)
	sudo install -d /usr/local/bin
	sudo install -m 755 $(CONTROL_TOOL) /usr/local/bin/opena8dj-control

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
	install -m 755 "$(MIDI_BRIDGE)" "$(PKG_ROOT)/usr/local/bin/opena8dj-midid"
	install -m 755 "$(PKG_SCRIPTS)/uninstall-opena8dj.sh" "$(PKG_ROOT)/usr/local/bin/opena8dj-uninstall"
	install -d "$(PKG_ROOT)/Library/LaunchAgents"
	install -m 644 "$(LAUNCH_AGENT_PLIST)" "$(PKG_ROOT)/Library/LaunchAgents/org.opena8dj.midid.plist"
	install -d "$(PKG_ROOT)/Library/Documentation/OpenA8DJ"
	install -m 644 LICENSE "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/LICENSE"
	install -m 644 NOTICE.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/NOTICE.md"
	install -m 644 docs/LEGAL.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/LEGAL.md"
	install -m 644 BRAND_POLICY.md "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/BRAND_POLICY.md"
	if [ -f "$(RELEASE_NOTES)" ]; then install -m 644 "$(RELEASE_NOTES)" "$(PKG_ROOT)/Library/Documentation/OpenA8DJ/RELEASE_NOTES.md"; fi
	chmod +x "$(PKG_SCRIPTS)/preinstall" "$(PKG_SCRIPTS)/postinstall" "$(PKG_SCRIPTS)/uninstall-opena8dj.sh"
	xattr -cr "$(PKG_ROOT)" 2>/dev/null || true
	find "$(PKG_ROOT)" -name '._*' -delete
	COPYFILE_DISABLE=1 pkgbuild --root "$(PKG_ROOT)" --scripts "$(PKG_SCRIPTS)" --identifier org.opena8dj.driver --version "$(VERSION)" --install-location / --filter '(^|/)\._.*' --filter '(^|/)\.DS_Store$$' $(if $(PKG_SIGN_IDENTITY),--sign "$(PKG_SIGN_IDENTITY)") "$(PKG)"
	if [ -z "$(PKG_SIGN_IDENTITY)" ]; then "$(PKG_SANITIZER)" "$(PKG)" "$(PKG_ROOT)" "$(PKG_SCRIPTS)"; fi

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

checksums: dmg
	(cd build && shasum -a 256 "OpenA8DJ-$(VERSION).dmg" "OpenA8DJ-$(VERSION).pkg" > "OpenA8DJ-$(VERSION)-checksums.txt")

dist: checksums

probe: $(TOOL)
	./$(TOOL)

claim: $(TOOL)
	./$(TOOL) --claim

clean:
	rm -rf build
