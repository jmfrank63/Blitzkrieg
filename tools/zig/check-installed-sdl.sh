#!/usr/bin/env bash
# The installed game must load exactly one libSDL3: the one beside it, by @rpath. A Mach-O that
# links a second build of SDL by its cache path (the ObjC runtime then prints "Class SDL3Window is
# implemented in both ...") or carries an -rpath into a directory that holds a libSDL3 makes dyld
# load that one instead. Usage: check-installed-sdl.sh <install dir>   (macOS: needs otool)
set -u
dir=${1:?usage: check-installed-sdl.sh <install dir>}
bad=0
for f in "$dir"/*.dylib "$dir"/Game; do
  [ -f "$f" ] || continue
  # Dependencies: libSDL3 only as @rpath/libSDL3.dylib.
  while read -r dep; do
    case "$dep" in
      @rpath/libSDL3.dylib) ;;
      *) echo "FAIL $f: SDL3 dependency $dep"; bad=1 ;;
    esac
  done < <(otool -L "$f" | awk 'NR > 1 { print $1 }' | grep -i 'libSDL3')
  # Run paths: none may point at a directory that contains a libSDL3.
  while read -r rp; do
    case "$rp" in @*) continue ;; esac
    if ls "$rp"/libSDL3* >/dev/null 2>&1; then echo "FAIL $f: -rpath $rp holds a libSDL3"; bad=1; fi
  done < <(otool -l "$f" | awk '$1 == "cmd" && $2 == "LC_RPATH" { r = 1 } r && $1 == "path" { print $2; r = 0 }')
done
[ "$bad" = 0 ] && echo "installed SDL3: one copy, via @rpath"
exit "$bad"
