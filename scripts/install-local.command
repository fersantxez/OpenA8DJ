#!/bin/bash
set -u

cd "$(dirname "$0")/.."

rm -f /tmp/opena8dj-install-done /tmp/opena8dj-install-failed

echo "Installing OpenA8DJ locally..."
echo "macOS may ask for your administrator password."
echo

if make SIGN_IDENTITY="OpenA8DJ Local Code Signing" install-hal install-tools install-midid; then
    touch /tmp/opena8dj-install-done
    echo
    echo "OpenA8DJ local install finished."
else
    touch /tmp/opena8dj-install-failed
    echo
    echo "OpenA8DJ local install failed."
    exit 1
fi
