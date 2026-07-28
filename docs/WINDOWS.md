# OpenA8DJ For Windows

OpenA8DJ for Windows is an experimental Audio 8 DJ driver package. It works on
Windows 10/11 x64, but the driver is test-signed rather than Microsoft-signed.
Windows therefore needs a one-boot signature-enforcement override during
installation.

## Download

Use the current Windows package:

- [Windows installer EXE](../windows/releases/OpenA8DJUsb-Release-x64-installer.exe)
- [Windows installer ZIP](../windows/releases/OpenA8DJUsb-Release-x64-installer.zip)
- [SHA-256 checksums](../windows/releases/SHA256SUMS.txt)

The EXE is the easiest option. The ZIP contains the same installer payload plus
the scripts and diagnostics tool. There is no MSI at this time.

## Install

1. Close Traktor and other audio applications, then unplug the Audio 8 DJ.
2. Open **Windows Terminal (Admin)** and run:

   ```text
   shutdown /r /o /t 0
   ```

3. Choose **Troubleshoot** → **Advanced options** → **Startup Settings** →
   **Restart**, then press **7** for **Disable driver signature enforcement**.
4. After Windows starts, double-click
   `OpenA8DJUsb-Release-x64-installer.exe` and approve the UAC prompt.
5. If Windows asks whether to install the test driver, accept it.
6. Reconnect the Audio 8 DJ.

For the ZIP, extract it first and run `install.cmd` as administrator. It uses
the same installation flow.

## Use

After reconnecting the device, select **Audio 8 DJ** in your Windows audio or
DJ application. The current package provides:

- 8 inputs and 8 outputs, grouped as stereo pairs A/B/C/D.
- 44.1 kHz and 48 kHz operation.
- Windows audio playback and Traktor-facing output routing.
- `opena8djctl.exe` for status, topology, and diagnostics.

MIDI, ASIO, complete DVS/timecode input validation, and Microsoft signing are
still being completed. This is why the package remains experimental.

## Verify Or Remove

From the extracted package, run these files as administrator:

```text
verify.cmd
uninstall.cmd
```

The verification report includes the installed package, device detection,
signature state, hashes, and driver diagnostics.

## Technical Details

[Maintainer documentation index](WINDOWS_DETAILS.md)
