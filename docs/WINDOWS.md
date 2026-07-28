# OpenA8DJ On Windows

OpenA8DJ works on Windows 10 and Windows 11 with the Audio 8 DJ. The current
Windows package is an experimental driver package and is not Microsoft-signed
for production distribution.

The only special Windows requirement is that test-signing mode must be enabled
so Windows can load the driver. This is a normal requirement for a development
driver package; it does not change the Audio 8 DJ hardware.

## Install

You need:

- Windows 10 or Windows 11, x64.
- Administrator access.
- The Audio 8 DJ and its USB cable.
- Permission to restart Windows once during setup.

1. Download the latest Windows ZIP from
   [GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases).
2. Extract the ZIP to a folder you can find again, such as
   `C:\OpenA8DJ`.
3. Right-click `install.cmd` and choose **Run as administrator**.
4. If Windows reports that test-signing was enabled, restart the computer.
5. After the restart, run `install.cmd` again as administrator.
6. Connect or reconnect the Audio 8 DJ.

The installer imports the package's test certificate, enables Windows
test-signing when needed, and installs the driver in the Windows Driver Store.

## Verify

From the extracted package, run `verify.cmd` as administrator. You can also
check the device directly:

```cmd
driver\opena8djctl.exe status
driver\opena8djctl.exe surface
driver\opena8djctl.exe topology
driver\opena8djctl.exe diagnostics
```

The verification report records the installed package, device detection,
driver signature state, hashes, and driver diagnostics.

## Uninstall

Run `uninstall.cmd` as administrator from the extracted package. If you no
longer need other test drivers on the computer, turn test-signing off and
restart:

```cmd
bcdedit /set testsigning off
```

## Need More Detail?

- [Windows technical details](WINDOWS_DETAILS.md)
- [Windows installer design](WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md)
- [Windows implementation plan](WINDOWS_IMPLEMENTATION_PLAN_2026-06-19.md)
- [Windows/Linux merge policy](EXPERIMENTAL_WINDOWS_LINUX_MERGE_POLICY_2026-06-19.md)

The package is experimental because it is not Microsoft-signed yet. The
architecture and validation notes live on the technical-details page so the
installation page stays focused on getting the device running.
