#!/usr/bin/env bash
# Installs per-user desktop entries and icons for the Linux game install.
# Opt-in: nothing in the build or the install runs it, because it writes into
# the user's profile (~/.local/share).
#
# The in-process icon (SDL_SetWindowIcon) covers X11 and Wayland compositors
# that speak xdg-toplevel-icon-v1 (KDE). GNOME's compositor does not, so there
# the dock, alt-tab and the titlebar can only take the icon from a desktop
# entry whose file name matches the program's Wayland app-id, which SDL sets
# from SDL_SetAppMetadata. The ids below are the ones in the code (the Game:
# Sources/src/Platform/SDLApplication.cpp; the editors:
# Sources/editor/kit/app_identity.zig); a kit test keeps them equal.
#
# Usage: install_desktop_entry.sh [game-install-dir]
#   Run from the install (<install>/Tools/install_desktop_entry.sh) it uses
#   that install; from the repository it uses
#   zig-out/game/linux/x86_64/release. The editors get an entry too when their
#   executables (MapEditor, ResourceEditor, ELK) sit beside the Game.
# Undo:  install_desktop_entry.sh --remove
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
apps_dir="$data_home/applications"
icon_dir="$data_home/icons/hicolor/48x48/apps"

# executable|app id|name|comment
programs=(
    "Game|org.blitzkrieg.game|Blitzkrieg|Blitzkrieg community source port"
    "MapEditor|org.blitzkrieg.mapeditor|Blitzkrieg Map Editor|Blitzkrieg map editor"
    "ResourceEditor|org.blitzkrieg.resourceeditor|Blitzkrieg Resource Editor|Blitzkrieg resource editor"
    "ELK|org.blitzkrieg.elk|Blitzkrieg ELK|Blitzkrieg localisation kit"
)

if [ "${1:-}" = "--remove" ]; then
    for program in "${programs[@]}"; do
        IFS='|' read -r _ app_id _ _ <<< "$program"
        rm -f "$apps_dir/$app_id.desktop" "$icon_dir/$app_id.png"
    done
    echo "removed the Blitzkrieg desktop entries from $apps_dir"
    exit 0
fi

if [ -n "${1:-}" ]; then
    install_dir="$1"
elif [ -x "$script_dir/../Game" ]; then
    install_dir="$(cd "$script_dir/.." && pwd)"
else
    install_dir="$(cd "$script_dir/.." && pwd)/zig-out/game/linux/x86_64/release"
fi

if [ ! -x "$install_dir/Game" ]; then
    echo "error: no Game executable in $install_dir (build with: zig build install-game --release=fast)" >&2
    exit 1
fi
if [ ! -f "$install_dir/Data/icon.png" ]; then
    echo "error: $install_dir/Data/icon.png is missing" >&2
    exit 1
fi

mkdir -p "$icon_dir" "$apps_dir"

for program in "${programs[@]}"; do
    IFS='|' read -r exe app_id name comment <<< "$program"
    [ -x "$install_dir/$exe" ] || continue
    cp "$install_dir/Data/icon.png" "$icon_dir/$app_id.png"

    # Icon= carries the absolute file path, not the themed name: a long-running
    # gnome-shell resolves themed names through an in-process icon-theme cache
    # that can miss icons installed after the session started (it kept showing
    # the generic gears here), while a file path bypasses that cache entirely.
    # The hicolor copy above stays for other consumers of the themed name.
    cat > "$apps_dir/$app_id.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$name
Comment=$comment
Exec=$install_dir/$exe
Path=$install_dir
Icon=$icon_dir/$app_id.png
Terminal=false
Categories=Game;StrategyGame;
StartupWMClass=$app_id
EOF
    echo "installed $apps_dir/$app_id.desktop -> $install_dir/$exe"
done

# Cache refreshers are best-effort: GNOME picks the files up without them on
# the next app launch, they just make it immediate.
command -v update-desktop-database >/dev/null && update-desktop-database "$apps_dir" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q "$data_home/icons/hicolor" 2>/dev/null || true
