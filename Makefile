PROJECT := opena8dj
VERSION := 0.2.5
TOOL := build/opena8dj-probe
SRC := src/opena8dj-probe.m
HAL_BUNDLE := build/OpenA8DJ.driver
HAL_BIN := $(HAL_BUNDLE)/Contents/MacOS/OpenA8DJHAL
HAL_SRC := src/hal/OpenA8DJHAL.c src/hal/OpenA8DJUSB.m
HAL_PLIST := resources/OpenA8DJ.driver/Contents/Info.plist
HAL_SMOKE := build/hal-smoke
HAL_SMOKE_SRC := src/tools/hal-smoke.c
AUDIO_LIST := build/audio-list
AUDIO_LIST_SRC := src/tools/audio-list.c
AUDIO_INSPECT := build/audio-inspect
AUDIO_INSPECT_SRC := src/tools/audio-inspect.c
AUDIO_IO_TEST := build/audio-io-test
AUDIO_IO_TEST_SRC := src/tools/audio-io-test.c
AUDIO_DEFAULT := build/audio-default
AUDIO_DEFAULT_SRC := src/tools/audio-default.c
AUDIO_PAIR_TONE := build/audio-pair-tone
AUDIO_PAIR_TONE_SRC := src/tools/audio-pair-tone.c
AUDIO_ROUTE := build/audio-route
AUDIO_ROUTE_SRC := src/tools/audio-route.c
MACBOOK_MIC_RECORD := build/macbook-mic-record
MACBOOK_MIC_RECORD_SRC := src/tools/macbook-mic-record.c
USB_PLAY := build/opena8dj-usb-play
USB_PLAY_SRC := src/tools/opena8dj-usb-play.m
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
SIGN_IDENTITY ?= -
PKG_SIGN_IDENTITY ?=
DMG_SIGN_IDENTITY ?=

CC := xcrun clang
CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic -O2
HAL_CFLAGS := -fobjc-arc -Wall -Wextra -Wpedantic -O2
FRAMEWORKS := -framework Foundation -framework IOKit -framework IOUSBHost
HAL_FRAMEWORKS := -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -framework CoreMIDI -framework Foundation -framework IOKit -framework IOUSBHost
MIDI_FRAMEWORKS := -framework Foundation -framework CoreMIDI -framework CoreAudio -framework CoreFoundation

.PHONY: all clean probe claim hal sign-hal install-hal install-midid install-tools smoke-hal audio-list audio-inspect audio-io-test audio-default audio-pair-tone audio-route macbook-mic-record usb-play midi-list package dmg checksums dist

all: $(TOOL) hal $(AUDIO_LIST) $(AUDIO_INSPECT) $(AUDIO_IO_TEST) $(AUDIO_DEFAULT) $(AUDIO_PAIR_TONE) $(AUDIO_ROUTE) $(MACBOOK_MIC_RECORD) $(USB_PLAY) $(MIDI_BRIDGE) $(CONTROL_TOOL) $(MIDI_LIST)

$(TOOL): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $<

hal: $(HAL_BIN)

$(HAL_BIN): $(HAL_SRC) $(HAL_PLIST)
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

$(MACBOOK_MIC_RECORD): $(MACBOOK_MIC_RECORD_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -framework CoreAudio -framework CoreFoundation -framework AudioToolbox -o $@ $<

macbook-mic-record: $(MACBOOK_MIC_RECORD)

$(USB_PLAY): $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m src/hal/OpenA8DJUSB.h
	@mkdir -p build
	$(CC) $(CFLAGS) -framework Foundation -framework IOKit -framework IOUSBHost -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o $@ $(USB_PLAY_SRC) src/hal/OpenA8DJUSB.m

usb-play: $(USB_PLAY)

$(MIDI_BRIDGE): $(MIDI_BRIDGE_SRC)
	@mkdir -p build
	xcrun clang $(CFLAGS) $(MIDI_FRAMEWORKS) -o $@ $<

$(CONTROL_TOOL): $(CONTROL_TOOL_SRC)
	@mkdir -p build
	xcrun clang -Wall -Wextra -Wpedantic -O2 -o $@ $<

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
