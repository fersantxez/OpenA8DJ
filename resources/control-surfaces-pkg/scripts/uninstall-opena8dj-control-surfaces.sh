#!/bin/sh
set -eu

/bin/rm -rf "/Applications/OpenA8DJ Control Center.app"
/bin/rm -f /usr/local/bin/opena8dj-control
/bin/rm -rf /Library/Documentation/OpenA8DJ/ControlSurfaces
/bin/rmdir /Library/Documentation/OpenA8DJ 2>/dev/null || true

exit 0
