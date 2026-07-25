#!/bin/sh
set -eu

console_uid="$(/usr/bin/stat -f %u /dev/console 2>/dev/null || echo 0)"
if [ "$console_uid" != "0" ]; then
  /bin/launchctl bootout "gui/$console_uid" /Library/LaunchAgents/org.opena8dj.midid.plist 2>/dev/null || true
fi

/bin/rm -f /Library/LaunchAgents/org.opena8dj.midid.plist
/usr/bin/find /usr/local/bin /Library/LaunchAgents /Library/Audio/Plug-Ins/HAL -name '._*' -delete 2>/dev/null || true
/bin/rm -f /usr/local/bin/opena8dj-control /usr/local/bin/opena8dj-midid
/bin/rm -rf /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/bin/rm -rf "/Applications/OpenA8DJ Control Center.app"
/bin/rm -rf /Library/Documentation/OpenA8DJ
/bin/rm -f /usr/local/bin/opena8dj-uninstall
/usr/bin/killall coreaudiod 2>/dev/null || true

exit 0
