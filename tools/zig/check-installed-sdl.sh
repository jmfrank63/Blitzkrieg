#!/usr/bin/env bash
# The installed game must load exactly one libSDL3: the one beside it, by @rpath. A Mach-O that
# links a second build of SDL by its cache path (the ObjC runtime then prints "Class SDL3Window is
# implemented in both ...") or carries an -rpath that is not @-relative makes dyld load that one
# instead: Zig's build-cache rpaths are relative to the current directory (stage.zig drops them).
# Usage: check-installed-sdl.sh <install dir>   (macOS: needs otool)
set -u
dir=${1:?usage: check-installed-sdl.sh <install dir>}
bad=0
for f in "$dir"/* "$dir"/Tools/*; do
  [ -f "$f" ] && otool -h "$f" >/dev/null 2>&1 || continue
  # Dependencies: libSDL3 only as @rpath/libSDL3.dylib.
  while read -r dep; do
    case "$dep" in
      @rpath/libSDL3.dylib) ;;
      *) echo "FAIL $f: SDL3 dependency $dep"; bad=1 ;;
    esac
  done < <(otool -L "$f" | awk 'NR > 1 { print $1 }' | grep -i 'libSDL3')
  # Run paths: only @loader_path/@executable_path ones.
  while read -r rp; do
    case "$rp" in @*) continue ;; esac
    echo "FAIL $f: build -rpath $rp"; bad=1
  done < <(otool -l "$f" | awk '$1 == "cmd" && $2 == "LC_RPATH" { r = 1 } r && $1 == "path" { print $2; r = 0 }')
done
[ "$bad" = 0 ] && echo "installed SDL3: one copy, via @rpath"
exit "$bad"
