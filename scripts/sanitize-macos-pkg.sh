#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
  echo "usage: sanitize-macos-pkg.sh <pkg> <payload-root> <scripts-dir>" >&2
  exit 2
fi

pkg="$1"
payload_root="$2"
scripts_dir="$3"

pkg_dir="$(cd "$(dirname "$pkg")" && pwd -P)"
pkg_name="$(basename "$pkg")"
pkg_path="$pkg_dir/$pkg_name"
payload_root_path="$(cd "$payload_root" && pwd -P)"
scripts_dir_path="$(cd "$scripts_dir" && pwd -P)"

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/opena8dj-pkg.XXXXXX")"
expanded="$tmp_dir/expanded"
payload_archive="$tmp_dir/Payload"
scripts_archive="$tmp_dir/Scripts"
out_pkg="$tmp_dir/$pkg_name"

cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT INT TERM

pkgutil --expand "$pkg_path" "$expanded"
rm -rf "$expanded/Payload" "$expanded/Scripts" "$expanded/Bom"

mkbom "$payload_root_path" "$expanded/Bom"

payload_files="$(cd "$payload_root_path" && find . ! -name '._*' ! -name '.DS_Store' -print | wc -l | tr -d ' ')"
payload_kb="$(du -sk "$payload_root_path" | awk '{print $1}')"
perl -0pi -e "s/<payload numberOfFiles=\"\\d+\" installKBytes=\"\\d+\"\\/>/<payload numberOfFiles=\"$payload_files\" installKBytes=\"$payload_kb\"\\/>/" "$expanded/PackageInfo"

(
  cd "$payload_root_path"
  COPYFILE_DISABLE=1 find . ! -name '._*' ! -name '.DS_Store' -print |
    cpio -o --format odc 2>/dev/null |
    gzip -c > "$payload_archive"
)

(
  cd "$scripts_dir_path"
  COPYFILE_DISABLE=1 find . ! -name '._*' ! -name '.DS_Store' -print |
    cpio -o --format odc 2>/dev/null |
    gzip -c > "$scripts_archive"
)

mv "$payload_archive" "$expanded/Payload"
mv "$scripts_archive" "$expanded/Scripts"

(
  cd "$expanded"
  # The Payload and Scripts files are already gzip-compressed cpio streams.
  # Store them raw in the xar, matching pkgbuild's component-package layout.
  xar --distribution --compression none -cf "$out_pkg" Bom Payload Scripts PackageInfo
)

mv "$out_pkg" "$pkg_path"
