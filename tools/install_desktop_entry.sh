#!/usr/bin/env bash
# Installs a per-user desktop entry and icon for the Linux game install.
#
# The in-process icon (SDL_SetWindowIcon) covers X11 and Wayland compositors
# that speak xdg-toplevel-icon-v1 (KDE). GNOME's compositor does not, so there
# the dock and the titlebar can only take the icon from a desktop entry whose
# file name matches the game's Wayland app-id (org.blitzkrieg.game, set via
# SDL_SetAppMetadata). This script provides that entry.
#
# Usage: tools/install_desktop_entry.sh [game-install-dir]
# Undo:  rm ~/.local/share/applications/org.blitzkrieg.game.desktop \
#           ~/.local/share/icons/hicolor/48x48/apps/org.blitzkrieg.game.png
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
install_dir="${1:-$repo_root/zig-out/game/linux/x86_64/release}"

if [ ! -x "$install_dir/Game" ]; then
    echo "error: no Game executable in $install_dir (build with: zig build install-game --release=fast)" >&2
    exit 1
fi
if [ ! -f "$install_dir/Data/icon.png" ]; then
    echo "error: $install_dir/Data/icon.png is missing" >&2
    exit 1
fi

app_id="org.blitzkrieg.game"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"

mkdir -p "$data_home/icons/hicolor/48x48/apps" "$data_home/applications"
cp "$install_dir/Data/icon.png" "$data_home/icons/hicolor/48x48/apps/$app_id.png"

# Icon= carries the absolute file path, not the themed name: a long-running
# gnome-shell resolves themed names through an in-process icon-theme cache
# that can miss icons installed after the session started (it kept showing
# the generic gears here), while a file path bypasses that cache entirely.
# The hicolor copy above stays for other consumers of the themed name.
icon_path="$data_home/icons/hicolor/48x48/apps/$app_id.png"

cat > "$data_home/applications/$app_id.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Blitzkrieg
Comment=Blitzkrieg community source port
Exec=$install_dir/Game
Path=$install_dir
Icon=$icon_path
Terminal=false
Categories=Game;StrategyGame;
StartupWMClass=$app_id
EOF

# Cache refreshers are best-effort: GNOME picks the files up without them on
# the next app launch, they just make it immediate.
command -v update-desktop-database >/dev/null && update-desktop-database "$data_home/applications" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q "$data_home/icons/hicolor" || true

echo "installed $data_home/applications/$app_id.desktop -> $install_dir/Game"
