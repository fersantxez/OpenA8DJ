# OpenA8DJ 0.5.1 Stable Reference

Date: 2026-07-25

OpenA8DJ 0.5.1 is the current public macOS C++ release baseline. It freezes the
responsive `output3072` profile accepted by the operator in Traktor.

## Public Reference

```text
release=OpenA8DJ 0.5.1 Responsive Freeze
release_url=https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1
branch=main
platform=macOS
distribution=one self-contained driver DMG plus checksum file
audio_source_commit=927d8af
packaging_fix_commit=567c191
driver=Core Audio HAL user-space driver
runtime_state=DVS Vinyl input active by default
```

## Frozen Profile

```text
HAL_TIMECODE_INPUT_GAIN=1.0f
HAL_TIMECODE_INPUT_GATE_THRESHOLD=0.0f
HAL_TIMECODE_INPUT_GATE_HOLD_FRAMES=0
HAL_INPUT_MAX_LATENCY_FRAMES=0
HAL_OUTPUT_GAIN=0.75f
HAL_OUTPUT_START_LATENCY_FRAMES=3072
HAL_OUTPUT_RESTART_LATENCY_FRAMES=1536
HAL_OUTPUT_TARGET_LATENCY_FRAMES=3072
HAL_OUTPUT_ELASTIC_HIGH_WATER_FRAMES=9216
HAL_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
HAL_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
HAL_OUTPUT_ZERO_FLOOR=0.0f
HAL_TRANSFER_POOL_CURSOR=1
HAL_FAST_ISO_TRANSFER_CONFIG=1
HAL_REUSE_ISOC_COMPLETIONS=0
HAL_RAW_ISOC_COMPLETIONS=0
```

## Public Artifacts

```text
OpenA8DJ-0.5.1.dmg
  sha256=3ca1a7e0e4478c1583919a51490977a56b540e8cd1f10354686c45d8671fdbd0
OpenA8DJ-0.5.1-checksums.txt
```

The GitHub release has no standalone PKG or tools DMG. The public DMG contains
the stapled PKG and installs:

```text
OpenA8DJ.driver
OpenA8DJ Control Center.app
opena8dj-control
opena8dj-midid
opena8dj-uninstall
Control Center and Traktor documentation
```

## Signing And Notarization

```text
make verify-signed-release=PASS
DMG Gatekeeper=Notarized Developer ID
DMG stapled ticket=PASS
embedded PKG Gatekeeper=Notarized Developer ID
embedded PKG stapled ticket=PASS
installed code signatures=PASS
trusted timestamps=PASS
```

Final Apple submission IDs:

```text
763346e5-3bf5-47a5-9db0-1f775385a6a3  OpenA8DJ-0.5.1.pkg
42274ddd-3e40-4404-8228-84298f72b42e  OpenA8DJ-0.5.1.dmg
6b60378a-0667-442e-b268-7b206679ffcc  opena8dj-tools-0.5.1.pkg
bbfca1d0-0dac-4f10-98c3-d1503771bf93  opena8dj-tools-0.5.1.dmg
```

## GitHub-Downloaded Installation

```text
evidence=local-analysis/github-install-e2e-20260725T2208Z
download_location=~/Downloads/OpenA8DJ-0.5.1-GitHub-Final
download_checksum=PASS
DMG integrity=PASS
quarantined Finder open=PASS
normal Installer welcome=PASS, no Gatekeeper warning
sudo fallback used=yes, for unattended completion after GUI-path verification
driver install=PASS
installed receipt=org.opena8dj.driver 0.5.1
Core Audio visibility=Open Audio 8 DJ, 8 input / 8 output at 48 kHz
MIDI visibility=Open Audio 8 DJ MIDI In/Out present
Control Center presence and signature=PASS
control CLI and MIDI bridge presence and signatures=PASS
audio stack health after install=PASS
final idle driver CPU=0.0%
final idle CoreAudio CPU=0.0%
CoreAudio restart=installer postinstall only
physical USB handling=none
playback or recording=none
hardware lock after validation=FREE
```

Installed binary hashes matched the build exactly:

```text
4af4b1207f81846208fd2fcc0b8f5a600c2e11346d523ee5fd8e4f55700b9f66  OpenA8DJHAL
55e7473ee8147179a195ca960041aea74ad3ac7b8f295cf7aca4c29dd6644de2  OpenA8DJControlCenter
73be7d6a5c780cc21e3ca1baa77300ef83b3bd01374ae2fed2db80a34f7b42c2  opena8dj-control
3d1e3697c7814580c31b47b88614e33b682d8129ec7407ea5f8ad30bc1e1ca75  opena8dj-midid
```

The final GitHub installation gate did not repeat physical playback or capture.
The audio engine is the frozen 0.5.1 candidate previously accepted by the
operator in Traktor and protected by the existing physical and offline quality
evidence. Future latency candidates must pass
`docs/project/timecode-latency-checkpoints.md` before replacing this baseline.
