#!/usr/bin/env python3
"""The GOG Blitzkrieg 1.2 scenarios, as the reference for Data/Scenarios.

Reads the GOG archives - copied from the GOG installation into
zig-out/local-test/gog-original, never committed - in the order the game
layers them, the later archive winning, and keeps what is under scenarios/.

  manifest  write tools/data/gog-1.2-scenarios.sha256
  diff      list the paths whose Data copy differs from GOG
  restore   copy the given GOG paths into Data

XML and Lua are compared after normalise() below. tools/zig/mission_data_test.zig
implements the same rules; change both together.
"""
import argparse
import hashlib
import os
import re
import sys
import zipfile

LAYERS = [
    "data.pak",
    "patch-1.pak",
    "patch-2.pak",
    "update-1.pak",
    "patch_galaxy.pak",
    "BK1_loca_englisch.pak",
    "patch_galaxy_texts_en.pak",
]
MANIFEST = "tools/data/gog-1.2-scenarios.sha256"


def resolved(gog_dir):
    """lower-case path -> (archive, member name), the highest layer winning."""
    files = {}
    for archive in LAYERS:
        with zipfile.ZipFile(os.path.join(gog_dir, archive)) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                key = info.filename.replace("\\", "/").lower()
                if key.startswith("scenarios/"):
                    files[key] = (archive, info.filename)
    return files


def read_member(gog_dir, entry):
    archive, member = entry
    with zipfile.ZipFile(os.path.join(gog_dir, archive)) as z:
        return z.read(member)


def normalise(path, data):
    """Line endings everywhere; for XML also the editor's <History> block,
    comments, whitespace between tags and <tag/> spelled <tag></tag>.
    ASCII rules only (re.A), as the Zig side has them."""
    lower = path.lower()
    if lower.endswith(".lua"):
        return data.replace(b"\r", b"")
    if not lower.endswith(".xml"):
        return data
    text = data.replace(b"\r", b"").decode("latin-1")
    text = re.sub(r"<History>.*?</History>", "", text, flags=re.S | re.A)
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S | re.A)
    text = re.sub(r">\s+<", "><", text, flags=re.A)
    text = text.strip(" \t\n\r\x0b\x0c")
    text = re.sub(r"<([A-Za-z_][A-Za-z0-9_.]*)/>", r"<\1></\1>", text, flags=re.A)
    return text.encode("latin-1")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def find_in_data(data_dir, key):
    """The Data path for a lower-case key, matching each component without
    case the way the game's data storage does; None when absent."""
    current = data_dir
    for part in key.split("/"):
        if not os.path.isdir(current):
            return None
        names = {name.lower(): name for name in os.listdir(current)}
        if part not in names:
            return None
        current = os.path.join(current, names[part])
    return current


def target_in_data(data_dir, key, member):
    """Where a GOG file goes: existing directories keep their case, new ones
    take GOG's spelling."""
    current = data_dir
    parts = member.replace("\\", "/").split("/")
    for part in parts:
        names = {name.lower(): name for name in os.listdir(current)} if os.path.isdir(current) else {}
        current = os.path.join(current, names.get(part.lower(), part))
    return current


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["manifest", "diff", "restore"])
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--gog", default="zig-out/local-test/gog-original")
    parser.add_argument("--data", default="Data")
    args = parser.parse_args()
    files = resolved(args.gog)

    if args.command == "manifest":
        with open(MANIFEST, "w", newline="\n") as out:
            for key in sorted(files):
                out.write(f"{sha256(normalise(key, read_member(args.gog, files[key])))}  {key}\n")
        print(f"{len(files)} files -> {MANIFEST}")
        return 0

    if args.command == "diff":
        differing = 0
        for key in sorted(files):
            path = find_in_data(args.data, key)
            if path is None:
                print(f"missing  {key}")
                differing += 1
                continue
            with open(path, "rb") as f:
                ours = normalise(key, f.read())
            if sha256(ours) != sha256(normalise(key, read_member(args.gog, files[key]))):
                print(f"differs  {key}")
                differing += 1
        print(f"{differing} of {len(files)} differ")
        return 0

    for key in args.paths:
        key = key.lower()
        if key not in files:
            print(f"not in GOG: {key}", file=sys.stderr)
            return 1
        path = find_in_data(args.data, key) or target_in_data(args.data, key, files[key][1])
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(read_member(args.gog, files[key]))
        print(f"restored {key} -> {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
