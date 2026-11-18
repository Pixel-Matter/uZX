#!/bin/sh
# Installs the app next to this script for the current user: on the PATH, and in the
# applications menu with its icon. Run it again after unpacking a newer release.
set -e
cd "$(dirname "$0")"
for desktop in *.desktop; do id="${desktop%.desktop}"; done

bin="$HOME/.local/bin/$id"
icons="$HOME/.local/share/icons/hicolor/1024x1024/apps"
menu="$HOME/.local/share/applications"
mkdir -p "$(dirname "$bin")" "$icons" "$menu"
cp "$id" "$bin"
chmod 755 "$bin"
cp "$id.png" "$icons/"
# The menu starts it by its full path, whether or not ~/.local/bin is on the PATH
sed "s|^Exec=.*|Exec=\"$bin\" %F|" "$id.desktop" > "$menu/$id.desktop"
if command -v update-desktop-database > /dev/null; then update-desktop-database -q "$menu" || true; fi

echo "Installed $id: it is in the applications menu, and starts as $bin"
