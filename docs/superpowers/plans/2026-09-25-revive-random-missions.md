# Revive the Original Random Missions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every random (template) mission of the Blitzkrieg 1.2 campaigns generates and plays again, each chapter unlocks its historical mission only after a random win as the original scripts intend, and the chapter screen's fallback that listed every historical mission is gone.

**Architecture:** GOG Blitzkrieg 1.2 is the read-only reference. A small Python tool reads its archives, writes a hash manifest into the repository and restores the mission files we lost. A Zig data check holds `Data/Scenarios` to that manifest and to the rules the chapter screen relies on. A C++ engine-tier test, built like `editor-bridge-test`, runs the random map generator for every chapter, template and difficulty. New `BK_AUTO_UI` verbs let a local run enter a chapter, start a chosen random mission and win it through the mission script. The fallback in `Chapter.cpp` is deleted last, once the data guarantees it is never needed.

**Tech Stack:** C++ (engine, MSVC and clang through `zig build`), Zig 0.16 (build and data test, `std.Io`), Python 3 (the GOG tool, dev-only), Lua (the game's mission scripts), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-25-revive-random-missions-design.md`

## Global Constraints

- Work on branch `fix/revive-random-missions` in `.worktrees/random-missions`; never check out or modify the shared main checkout `/Users/johannes/Projects/src/Blitzkrieg`.
- The GOG machine (`ssh win-home`, `D:\GOG\Blitzkrieg`) is read only: copy from it, never write to it.
- GOG files (`*.pak`, anything unpacked from them) are never committed; only hashes and the files restored into `Data` are.
- Test artifacts and scratch generated data go under `zig-out/local-test`, never `/tmp`.
- Never touch the live profile, the release game in the main checkout, or cloud state; game runs use the worktree's own staged debug game and the throw-away profile `MissionRun`.
- The AchtungPanzer2 mod is unlicensed: it may be copied into the worktree's staged game for a local check, never committed or pushed.
- Never `zig fmt` `build.zig`; after editing it run `zig test tools/zig/build_hermeticity_test.zig`.
- NEVER use git stash in any form.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- Push and merge only when Johannes says so.

## Review Focus

1. **Loading a save made in a random mission** regenerates the map from its `.seed` file (`Main/RandomMapHelper.cpp`); the player expects the same map. Pinned by the regeneration check in Task 3.
2. **An objective anchored on a scenario object** (defend and escort templates) is positioned from `mapInfo.objects[...]` indexed by the scenario-object index, a read past the end when there are more scenario objects than objects. The player expects the objective flag on the briefing map where the objective is. Pinned by the anchor-position check in Task 3 and fixed in Task 4.
3. **A save made on a chapter screen before this change** stores mission indices that listed historical missions the script had not enabled. The player expects it to load, with no crash, showing only what the script allows. Pinned by the save-and-reload step in Task 7.
4. **A mod campaign** (AchtungPanzer2) whose chapters enable missions directly. The player expects every chapter still to list its missions once the fallback is gone. Pinned by the mod step in Task 8.
5. **Windows debug builds with asserts on** (`_DEBUG`, `_DO_ASSERT_SLOW`) hit `NI_ASSERT`s that the portable builds never evaluate; a firing assert ends in a debug-CRT dialog that hangs CI. The player on Windows expects no assert on any template. Pinned by running the Task 3 test in the Windows CI job with CRT asserts routed to stderr (Task 9).

---

## File Structure

| File | Responsibility |
|---|---|
| `tools/data/gog_scenarios.py` (create) | Reads the GOG archives in layer order; `manifest` writes the hash list, `diff` lists files that differ from `Data`, `restore` copies GOG files into `Data`. The normalisation rules live here and in the Zig test, identically. |
| `tools/data/gog-1.2-scenarios.sha256` (create, generated) | `<sha256>  <lower-case path>` for every GOG file under `scenarios/`, normalised. |
| `tools/data/gog-1.2-deviations.txt` (create) | Files under `Data/Scenarios` that deliberately differ from GOG, one per line with its reason. |
| `tools/zig/mission_data_test.zig` (create) | `test-mission-data`: manifest comparison, no generated leftovers in `Data`, every campaign template resolves, gated chapters have templates and placeholders. |
| `tools/zig/random_missions_test.cpp` (create) | `test-random-missions`: the engine-tier generator sweep. |
| `build.zig` (modify) | Registers both tests. |
| `Data/Scenarios/...` (modify) | Restored mission data; `Data/Maps/templatemaps` deleted. |
| `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp` (modify) | Task 4's anchor fix; other generator fixes from Task 5. |
| `Sources/src/Game/GameMain.cpp` (modify) | `chapter=`, `mission=`, `lua=` harness verbs. |
| `Sources/src/Common/InterfaceScreenBase.cpp` (modify) | `BK_NO_HELP` skips one-time help screens in harness runs. |
| `Sources/src/GameTT/Chapter.cpp` (modify) | Trace of the offered missions; the fallback removed. |
| `tools/missions/run_random_mission.sh` (create) | Local game run: enter a chapter, win a chosen random mission, check the chapter screen. |
| `.github/workflows/cross-platform.yml` (modify) | CI steps for both tests. |
| `docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md` (create, Task 5) | The diagnosis: every failing case and its root cause. |

---

### Task 1: The GOG reference tool and the mission data check

**Files:**
- Create: `tools/data/gog_scenarios.py`
- Create: `tools/data/gog-1.2-scenarios.sha256` (generated by the tool)
- Create: `tools/data/gog-1.2-deviations.txt`
- Create: `tools/zig/mission_data_test.zig`
- Modify: `build.zig` (register `test-mission-data` next to `test-runtime-platform-audit`, around line 841)

**Interfaces:**
- Produces: `python3 tools/data/gog_scenarios.py {manifest|diff|restore} [--gog DIR] [--data DIR] [paths…]`; `zig build test-mission-data -Dtest-mode=run`, whose output lines `differs from GOG 1.2: <path>` / `missing: <path>` name each bad file; the normalisation rule (below) that both sides share.

The data check fails on this task's commit, by design: it lists exactly the files Task 2 restores. It is not added to CI until Task 2.

- [ ] **Step 1: Copy the GOG archives (read-only on the Windows side)**

```bash
mkdir -p zig-out/local-test/gog-original
for f in data.pak patch-1.pak patch-2.pak update-1.pak patch_galaxy.pak BK1_loca_englisch.pak patch_galaxy_texts_en.pak; do
  scp -q "win-home:D:/GOG/Blitzkrieg/data/$f" zig-out/local-test/gog-original/
done
ls -la zig-out/local-test/gog-original
```

Expected: seven files, `data.pak` 187945071 bytes, `update-1.pak` 58189 bytes.

- [ ] **Step 2: Write the tool**

`tools/data/gog_scenarios.py`:

```python
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
```

- [ ] **Step 3: Generate the manifest and look at the differences**

```bash
python3 tools/data/gog_scenarios.py manifest
python3 tools/data/gog_scenarios.py diff
```

Expected: the manifest has more than 1000 lines. `diff` prints exactly these 23 paths, then `23 of N differ`:

```
differs  scenarios/campaigns/allies/allies.xml
differs  scenarios/campaigns/german/german.xml
differs  scenarios/campaigns/ussr/ussr.xml
differs  scenarios/chapters/german/barbarossa/context.xml
differs  scenarios/chapters/german/france/1.xml
differs  scenarios/chapters/german/france/context.xml
differs  scenarios/chapters/ussr/finland/1.xml
differs  scenarios/chapters/ussr/leningrad/1.xml
differs  scenarios/patches/spring_ukraine/p_hunted_e_1.bzm   (and _n_, _s_, _w_)
differs  scenarios/patches/summer_russia/p_hunted_e_1.bzm    (and _n_, _s_, _w_)
differs  scenarios/patches/winter_russia/p_hunted_e_1.bzm    (and _n_, _s_, _w_)
differs  scenarios/templatemissions/all/summer_france/securearea01/1.xml
differs  scenarios/templatemissions/all/summer_france/securearea05/1.xml
```

If the list differs, stop and report it: the spec's table was built from this comparison.

- [ ] **Step 4: Write the deviations file**

`tools/data/gog-1.2-deviations.txt` (a tab between path and reason):

```
# Files under Data/Scenarios that deliberately differ from GOG Blitzkrieg 1.2.
# <lower-case path><TAB><reason>. Read by tools/zig/mission_data_test.zig.
scenarios/chapters/german/barbarossa/context.xml	GOG offers the American T34_Calliope_USA as a German random-mission reward; ours has BM_13, as the repository's first data import (5e48dedd8) had it.
```

- [ ] **Step 5: Write the data check**

`tools/zig/mission_data_test.zig`:

```zig
//! The mission data tier: Data/Scenarios against GOG Blitzkrieg 1.2, and the
//! rules the chapter screen relies on now that it has no fallback.
//!
//! Runs from the repository root (build.zig sets the cwd). The normalisation
//! below must stay identical to normalise() in tools/data/gog_scenarios.py,
//! which wrote the manifest.
const std = @import("std");

const io = std.testing.io;
const max_file = 64 * 1024 * 1024;

const campaigns = [_]struct { file: []const u8, template_count: usize }{
    .{ .file = "scenarios/campaigns/german/german.xml", .template_count = 76 },
    .{ .file = "scenarios/campaigns/allies/allies.xml", .template_count = 63 },
    .{ .file = "scenarios/campaigns/ussr/ussr.xml", .template_count = 63 },
};

fn endsWithIgnoreCase(text: []const u8, suffix: []const u8) bool {
    return text.len >= suffix.len and std.ascii.eqlIgnoreCase(text[text.len - suffix.len ..], suffix);
}

fn isSpace(c: u8) bool {
    return c == ' ' or c == '\t' or c == '\n' or c == '\r' or c == 0x0b or c == 0x0c;
}

fn removeBlocks(a: std.mem.Allocator, text: []const u8, open: []const u8, close: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) {
        const start = std.mem.indexOfPos(u8, text, i, open) orelse break;
        const end = std.mem.indexOfPos(u8, text, start + open.len, close) orelse break;
        try out.appendSlice(a, text[i..start]);
        i = end + close.len;
    }
    try out.appendSlice(a, text[i..]);
    return out.items;
}

fn collapseBetweenTags(a: std.mem.Allocator, text: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) : (i += 1) {
        try out.append(a, text[i]);
        if (text[i] != '>') continue;
        var j = i + 1;
        while (j < text.len and isSpace(text[j])) j += 1;
        if (j > i + 1 and j < text.len and text[j] == '<') i = j - 1;
    }
    return out.items;
}

fn isNameStart(c: u8) bool {
    return std.ascii.isAlphabetic(c) or c == '_';
}

fn isNameChar(c: u8) bool {
    return std.ascii.isAlphanumeric(c) or c == '_' or c == '.';
}

fn expandEmptyTags(a: std.mem.Allocator, text: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) {
        if (text[i] == '<' and i + 1 < text.len and isNameStart(text[i + 1])) {
            var j = i + 2;
            while (j < text.len and isNameChar(text[j])) j += 1;
            if (j + 1 < text.len and text[j] == '/' and text[j + 1] == '>') {
                const name = text[i + 1 .. j];
                try out.append(a, '<');
                try out.appendSlice(a, name);
                try out.appendSlice(a, "></");
                try out.appendSlice(a, name);
                try out.append(a, '>');
                i = j + 2;
                continue;
            }
        }
        try out.append(a, text[i]);
        i += 1;
    }
    return out.items;
}

/// normalise() of tools/data/gog_scenarios.py.
fn normalise(a: std.mem.Allocator, path: []const u8, bytes: []const u8) ![]const u8 {
    const xml = endsWithIgnoreCase(path, ".xml");
    if (!xml and !endsWithIgnoreCase(path, ".lua")) return bytes;
    var text = std.ArrayList(u8).empty;
    for (bytes) |c| if (c != '\r') try text.append(a, c);
    if (!xml) return text.items;
    var s: []const u8 = text.items;
    s = try removeBlocks(a, s, "<History>", "</History>");
    s = try removeBlocks(a, s, "<!--", "-->");
    s = try collapseBetweenTags(a, s);
    s = std.mem.trim(u8, s, " \t\n\r\x0b\x0c");
    return expandEmptyTags(a, s);
}

/// `relative` (any case, / or \) under `root`, each component matched without
/// case as the game's data storage matches it. Null when absent.
fn resolve(a: std.mem.Allocator, root: []const u8, relative: []const u8) !?[]const u8 {
    const cwd = std.Io.Dir.cwd();
    var current: []const u8 = root;
    var parts = std.mem.tokenizeAny(u8, relative, "/\\");
    while (parts.next()) |part| {
        var dir = cwd.openDir(io, current, .{ .iterate = true }) catch return null;
        defer dir.close(io);
        var it = dir.iterate();
        var found: ?[]const u8 = null;
        while (try it.next(io)) |entry| {
            if (std.ascii.eqlIgnoreCase(entry.name, part)) {
                found = try a.dupe(u8, entry.name);
                break;
            }
        }
        current = try std.fmt.allocPrint(a, "{s}/{s}", .{ current, found orelse return null });
    }
    return current;
}

fn readData(a: std.mem.Allocator, relative: []const u8) !?[]const u8 {
    const path = (try resolve(a, "Data", relative)) orelse return null;
    return try std.Io.Dir.cwd().readFileAlloc(io, path, a, .limited(max_file));
}

/// A data name ("scenarios\\chapters\\ussr\\kursk\\1") plus an extension.
fn dataFile(a: std.mem.Allocator, name: []const u8, extension: []const u8) ![]const u8 {
    return std.fmt.allocPrint(a, "{s}{s}", .{ name, extension });
}

/// The text of the first <tag>…</tag> in normalised XML, or "".
fn tagValue(xml: []const u8, comptime tag: []const u8) []const u8 {
    const open = "<" ++ tag ++ ">";
    const start = (std.mem.indexOf(u8, xml, open) orelse return "") + open.len;
    const end = std.mem.indexOfScalarPos(u8, xml, start, '<') orelse return "";
    return xml[start..end];
}

/// The section between <tag> and </tag>, or "".
fn section(xml: []const u8, comptime tag: []const u8) []const u8 {
    const open = "<" ++ tag ++ ">";
    const start = (std.mem.indexOf(u8, xml, open) orelse return "") + open.len;
    const end = std.mem.indexOfPos(u8, xml, start, "</" ++ tag ++ ">") orelse return "";
    return xml[start..end];
}

/// Every <tag>value</tag> value in xml.
fn allValues(a: std.mem.Allocator, xml: []const u8, comptime tag: []const u8) ![]const []const u8 {
    var values = std.ArrayList([]const u8).empty;
    const open = "<" ++ tag ++ ">";
    var i: usize = 0;
    while (std.mem.indexOfPos(u8, xml, i, open)) |at| {
        const start = at + open.len;
        const end = std.mem.indexOfScalarPos(u8, xml, start, '<') orelse break;
        try values.append(a, xml[start..end]);
        i = end;
    }
    return values.items;
}

fn readXml(a: std.mem.Allocator, relative: []const u8) !?[]const u8 {
    const bytes = (try readData(a, relative)) orelse return null;
    return try normalise(a, relative, bytes);
}

test "normalise matches the Python rules" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const input = "<?xml version=\"1.0\"?>\r\n<base><History><A>x</A></History>\r\n\t<RPG>\r\n<KeyName/><!-- gone -->\r\n<X a=\"1\"/>\r\n<MODName>\r\n</MODName></RPG></base>\r\n";
    try std.testing.expectEqualStrings(
        "<?xml version=\"1.0\"?><base><RPG><KeyName></KeyName><X a=\"1\"/><MODName></MODName></RPG></base>",
        try normalise(a, "x.xml", input),
    );
    try std.testing.expectEqualStrings("a\nb\n", try normalise(a, "x.lua", "a\r\nb\r\n"));
    try std.testing.expectEqualStrings("a\r\n", try normalise(a, "x.txt", "a\r\n"));
}

test "Data/Scenarios matches GOG 1.2 apart from the recorded deviations" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const cwd = std.Io.Dir.cwd();

    var deviations = std.StringHashMapUnmanaged(void){};
    const deviation_text = try cwd.readFileAlloc(io, "tools/data/gog-1.2-deviations.txt", a, .limited(1024 * 1024));
    var deviation_lines = std.mem.tokenizeScalar(u8, deviation_text, '\n');
    while (deviation_lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, "\r ");
        if (line.len == 0 or line[0] == '#') continue;
        const tab = std.mem.indexOfScalar(u8, line, '\t') orelse return error.DeviationWithoutReason;
        try deviations.put(a, line[0..tab], {});
    }

    const manifest = try cwd.readFileAlloc(io, "tools/data/gog-1.2-scenarios.sha256", a, .limited(16 * 1024 * 1024));
    var checked: usize = 0;
    var failures: usize = 0;
    var lines = std.mem.tokenizeScalar(u8, manifest, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, "\r ");
        if (line.len < 67) continue;
        const expected = line[0..64];
        const path = line[66..];
        if (deviations.contains(path)) continue;
        checked += 1;
        const bytes = (try readData(a, path)) orelse {
            std.debug.print("missing: {s}\n", .{path});
            failures += 1;
            continue;
        };
        var digest: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(try normalise(a, path, bytes), &digest, .{});
        const actual = std.fmt.bytesToHex(digest, .lower);
        if (!std.mem.eql(u8, &actual, expected)) {
            std.debug.print("differs from GOG 1.2: {s}\n", .{path});
            failures += 1;
        }
    }
    try std.testing.expect(checked > 1000);
    try std.testing.expectEqual(@as(usize, 0), failures);
}

test "Data holds no generated random maps" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    // The game once generated into Data; generated maps now belong to the
    // player's cache (StreamIO/GeneratedData.h) and would shadow nothing but
    // confuse every comparison.
    try std.testing.expect((try resolve(arena.allocator(), "Data", "maps/templatemaps")) == null);
}

test "every campaign template resolves and every gated chapter can offer random missions" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    var failures: usize = 0;

    for (campaigns) |campaign| {
        const campaign_xml = (try readXml(a, campaign.file)) orelse return error.CampaignMissing;
        const templates = try allValues(a, section(campaign_xml, "Templates"), "item");
        if (templates.len != campaign.template_count) {
            std.debug.print("{s}: {d} templates, GOG 1.2 has {d}\n", .{ campaign.file, templates.len, campaign.template_count });
            failures += 1;
        }

        var per_setting = std.StringHashMapUnmanaged(usize){};
        for (templates) |template| {
            const mission = (try readXml(a, try dataFile(a, template, ".xml"))) orelse {
                std.debug.print("{s}: template mission {s} missing\n", .{ campaign.file, template });
                failures += 1;
                continue;
            };
            const setting = tagValue(mission, "SettingName");
            const entry = try per_setting.getOrPut(a, setting);
            if (!entry.found_existing) entry.value_ptr.* = 0;
            entry.value_ptr.* += 1;

            const template_map = tagValue(mission, "TemplateMap");
            const map_xml = try readXml(a, try dataFile(a, template_map, ".xml"));
            if (map_xml == null) {
                std.debug.print("{s}: template map {s} missing\n", .{ template, template_map });
                failures += 1;
            } else {
                const script = tagValue(map_xml.?, "Scripts");
                if (script.len != 0 and (try readData(a, try dataFile(a, script, ".lua"))) == null) {
                    std.debug.print("{s}: script {s}.lua missing\n", .{ template_map, script });
                    failures += 1;
                }
            }
            inline for (.{ "HeaderText", "DescriptionText" }) |tag| {
                const text = tagValue(mission, tag);
                if ((try readData(a, try dataFile(a, text, ".txt"))) == null) {
                    std.debug.print("{s}: {s} {s}.txt missing\n", .{ template, tag, text });
                    failures += 1;
                }
            }
        }

        for (try allValues(a, section(campaign_xml, "AllChapters"), "Chapter")) |chapter| {
            const chapter_xml = (try readXml(a, try dataFile(a, chapter, ".xml"))) orelse {
                std.debug.print("{s}: chapter {s} missing\n", .{ campaign.file, chapter });
                failures += 1;
                continue;
            };
            const script = (try readData(a, try dataFile(a, tagValue(chapter_xml, "Script"), ".lua"))) orelse "";
            // The chapters whose script enables the historical mission only
            // after a random win - all but the first chapter of a campaign.
            if (std.mem.indexOf(u8, script, "Mission.Current.Random") == null) continue;
            const setting = tagValue(chapter_xml, "SettingName");
            const template_count = per_setting.get(setting) orelse 0;
            const placeholders = std.mem.count(u8, section(chapter_xml, "PlaceHolders"), "<Position");
            std.debug.print("{s}: {d} templates for {s}, {d} placeholders\n", .{ chapter, template_count, setting, placeholders });
            if (template_count < 3 or placeholders < 3) {
                std.debug.print("  cannot offer one random mission per difficulty\n", .{});
                failures += 1;
            }
        }
    }
    try std.testing.expectEqual(@as(usize, 0), failures);
}
```

If a `std` name used here does not exist in Zig 0.16 (`std.fmt.bytesToHex`, `std.StringHashMapUnmanaged`), use the 0.16 equivalent and keep the behaviour; `tools/zig/compare_trees.zig` and `tools/zig/runtime_platform_audit.zig` show the 0.16 file idioms.

- [ ] **Step 6: Register the test in build.zig**

After the `runtime_platform_audit_step` block (around line 851), add:

```zig
    // Data/Scenarios against GOG Blitzkrieg 1.2 and the chapter screen's rules
    // (docs/superpowers/specs/2026-09-25-revive-random-missions-design.md).
    // Reads Data, so it runs from the repository root.
    const mission_data_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/mission_data_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const mission_data_tests = b.addTest(.{ .root_module = mission_data_module });
    const mission_data_run = b.addRunArtifact(mission_data_tests);
    mission_data_run.setCwd(b.path("."));
    const mission_data_step = b.step("test-mission-data", "Check Data/Scenarios against GOG 1.2 and the chapter rules");
    mission_data_step.dependOn(&mission_data_tests.step);
    if (test_mode == .run) mission_data_step.dependOn(&mission_data_run.step);
```

Run: `zig test tools/zig/build_hermeticity_test.zig`
Expected: all tests pass.

- [ ] **Step 7: Run the check and confirm it fails for the right reasons**

Run: `zig build test-mission-data -Dtest-mode=run 2>&1 | tee zig-out/local-test/mission-data-before.txt`

Expected: `normalise matches the Python rules` passes. The manifest test prints `differs from GOG 1.2:` for the 22 non-deviation paths of Step 3 and fails. The templatemaps test fails. The chapter test prints `43 templates, GOG 1.2 has 63` (and 56/76 for German), and `cannot offer one random mission per difficulty` for `scenarios\chapters\ussr\kursk\1`, `…\ussr\rumania\1` and `…\german\kharkov42\1`, and fails.

If the manifest test reports any other path, the Zig and Python normalisations disagree: fix whichever side departs from the rule in the Python docstring, until the list is exactly the 22.

- [ ] **Step 8: Commit**

```bash
git add tools/data/gog_scenarios.py tools/data/gog-1.2-scenarios.sha256 tools/data/gog-1.2-deviations.txt tools/zig/mission_data_test.zig build.zig
git commit -m "test(missions): Data/Scenarios is checked against GOG Blitzkrieg 1.2

A hash manifest of GOG's scenarios, made by tools/data/gog_scenarios.py from
the local copy of the archives, and a data tier that compares Data/Scenarios
with it and checks what the chapter screen needs: every campaign template
resolves, and every chapter that asks for a random win can offer one. It
fails for now, listing exactly the files the next commit restores.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Restore the mission data

**Files:**
- Modify: the 22 non-deviation files of Task 1 Step 3 under `Data/Scenarios`
- Delete: `Data/Maps/templatemaps/` (12 files)
- Modify: `.github/workflows/cross-platform.yml` (a `Mission data tier` step in the Linux x64 job)

**Interfaces:**
- Consumes: `tools/data/gog_scenarios.py restore`, `zig build test-mission-data` (Task 1).
- Produces: `Data/Scenarios` equal to GOG 1.2 except the Barbarossa deviation; campaign files listing 76/63/63 templates.

- [ ] **Step 1: Restore**

```bash
python3 tools/data/gog_scenarios.py restore \
  scenarios/campaigns/allies/allies.xml \
  scenarios/campaigns/german/german.xml \
  scenarios/campaigns/ussr/ussr.xml \
  scenarios/chapters/german/france/1.xml \
  scenarios/chapters/german/france/context.xml \
  scenarios/chapters/ussr/finland/1.xml \
  scenarios/chapters/ussr/leningrad/1.xml \
  scenarios/patches/spring_ukraine/p_hunted_e_1.bzm scenarios/patches/spring_ukraine/p_hunted_n_1.bzm \
  scenarios/patches/spring_ukraine/p_hunted_s_1.bzm scenarios/patches/spring_ukraine/p_hunted_w_1.bzm \
  scenarios/patches/summer_russia/p_hunted_e_1.bzm scenarios/patches/summer_russia/p_hunted_n_1.bzm \
  scenarios/patches/summer_russia/p_hunted_s_1.bzm scenarios/patches/summer_russia/p_hunted_w_1.bzm \
  scenarios/patches/winter_russia/p_hunted_e_1.bzm scenarios/patches/winter_russia/p_hunted_n_1.bzm \
  scenarios/patches/winter_russia/p_hunted_s_1.bzm scenarios/patches/winter_russia/p_hunted_w_1.bzm \
  scenarios/templatemissions/all/summer_france/securearea01/1.xml \
  scenarios/templatemissions/all/summer_france/securearea05/1.xml
git rm -r -q Data/Maps/templatemaps
git status --short | head -40
```

Expected: 22 `restored` lines, each into an existing file (no new paths in `git status`), and 12 deletions.

- [ ] **Step 2: Run the data check**

Run: `zig build test-mission-data -Dtest-mode=run`
Expected: all four tests pass; the chapter test lists every gated chapter with 4 to 24 templates and 7 to 10 placeholders.

- [ ] **Step 3: Check the restored XML survives Git's line-ending rules**

`.gitattributes` stores `*.xml` and `*.lua` as LF and checks them out CRLF.

```bash
git add -A Data
git diff --cached --stat | tail -3
rm Data/Scenarios/Campaigns/USSR/ussr.xml && git checkout -- Data/Scenarios/Campaigns/USSR/ussr.xml
zig build test-mission-data -Dtest-mode=run
```

Expected: still passes; the normalisation ignores the line endings Git rewrites.

- [ ] **Step 4: Add the CI step**

In `.github/workflows/cross-platform.yml`, in the Linux x64 job next to `test-map-files` (line ~102), add:

```yaml
      - name: Mission data tier
        run: zig build test-mission-data -Dtarget=x86_64-linux-gnu -Dtest-mode=run
```

- [ ] **Step 5: Commit**

```bash
git add -A Data .github/workflows/cross-platform.yml
git commit -m "fix(missions): the original random missions and GOG's hunt patches are back

The three campaigns list GOG 1.2's templates again - 76, 63 and 63 - after
c1532955b had cut the defend, escort and hunt ones, which left Kursk,
Rumania and Kharkov 1942 with no random mission at all. GOG's update-1
replaces the twelve hunt map pieces and France's rewards; ours were the
unpatched ones. France's and Leningrad's chapters, and two secure-area
templates, lose what an old build generated into Data, and the generated
maps it left in Data/Maps/templatemaps are gone.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: The random missions tier

**Files:**
- Create: `tools/zig/random_missions_test.cpp`
- Modify: `build.zig` (an `addRandomMissionsTest` function beside `addEditorBridgeTest`, line ~5467; its call beside line 2122; a `random-missions-sweep` option)

**Interfaces:**
- Consumes: `BkEditorStart`, `BkEditorOpenMap`, `BkEditorStop`, `BkEditorLastMessage` (`Sources/src/EditorBridge/bridge.h`); `CMapInfo::CreateRandomMap` (`RandomMapGen/MapInfo_Types.h:324`); `NMapFile::Read`, `NMapFile::AreEquivalent` (`MapFile/MapFile.h`, `MapFile/MapEquivalence.h`); `NGDB::GetGameStats<T>`; `LoadDataResource` with `RMGC_TEMPLATE_XML_NAME`.
- Produces: `zig build test-random-missions -Dtest-mode=run [-Drandom-missions-sweep=all|cover|only=<text>]`. Each case prints `random-missions: <campaign> <chapter> <template> d<difficulty> <ms> ms` and, on failure, `FAIL: <what>` plus `kept: <dir>`. The last line is `random-missions: <n> cases, <failures> failed, <seconds> s`.

This task builds the instrument; it is expected to fail on some templates. That is Task 5's input. It is not added to CI until Task 9.

- [ ] **Step 1: Write the test**

`tools/zig/random_missions_test.cpp`:

```cpp
// The random missions tier: every template a chapter can offer, generated at
// every difficulty the way the briefing generates it (GameTT/Mission.cpp), then
// read back, checked and opened in the engine. Needs a hidden SDL window and a
// GPU device, as the engine tier does, and skips honestly where there is none.
//
// argv: <installation> <scratch> [all | cover | only=<text>]
//   all     every gated chapter x every template of its setting x 3 difficulties
//   cover   every gated chapter x template pair once, the difficulty rotating
//   only=   the cases whose chapter or template name contains <text>
#include "StdAfx.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <set>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/Resource_Types.h"
#include "../../Sources/src/Main/GameStats.h"
#include "../../Sources/src/Main/GameDB.h"
#include "../../Sources/src/StreamIO/RandomGen.h"
#include "../../Sources/src/StreamIO/StreamIOTypes.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#endif

static int g_nFailures = 0;

static bool Check( bool bCondition, const std::string &szWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", szWhat.c_str() );
		++g_nFailures;
	}
	return bCondition;
}

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

static bool SamePath( const char *pszLeft, const char *pszRight )
{
#if defined(_WIN32) || defined(_WIN64)
	char left[_MAX_PATH], right[_MAX_PATH];
	if ( _fullpath( left, pszLeft, _MAX_PATH ) == 0 || _fullpath( right, pszRight, _MAX_PATH ) == 0 )
		return false;
	return _stricmp( left, right ) == 0;
#else
	char left[PATH_MAX], right[PATH_MAX];
	if ( realpath( pszLeft, left ) == 0 || realpath( pszRight, right ) == 0 )
		return false;
	return strcmp( left, right ) == 0;
#endif
}

static std::string Lower( std::string sz )
{
	NStr::ToLower( sz );
	return sz;
}

// A generated-data root the way NGeneratedData::Root spells one: backslashes
// and a trailing one, which is what CreateRandomMap appends names to.
static std::string GeneratedRoot( const std::filesystem::path &directory )
{
	std::string sz = std::filesystem::absolute( directory ).string();
	for ( char &c : sz )
		if ( c == '/' )
			c = '\\';
	return sz + "\\";
}

struct SCase
{
	std::string szCampaign;
	std::string szChapter;
	std::string szContext;
	std::string szTemplate;
	int nDifficulty;
	bool bRegenerate;					// the first case of each template
};

static const char *const CAMPAIGNS[] = {
	"scenarios\\campaigns\\german\\german",
	"scenarios\\campaigns\\allies\\allies",
	"scenarios\\campaigns\\ussr\\ussr",
};

static std::vector<SCase> CollectCases( const std::string &szSweep )
{
	std::vector<SCase> cases;
	std::set<std::string> regenerated;
	for ( const char *pszCampaign : CAMPAIGNS )
	{
		const SCampaignStats *pCampaign = NGDB::GetGameStats<SCampaignStats>( pszCampaign, IObjectsDB::CAMPAIGN );
		if ( !Check( pCampaign != 0, std::string( "campaign stats " ) + pszCampaign ) )
			continue;
		for ( const SCampaignStats::SChapter &chapter : pCampaign->chapters )
		{
			const std::string szChapter = Lower( chapter.szChapter );
			const SChapterStats *pChapter = NGDB::GetGameStats<SChapterStats>( szChapter.c_str(), IObjectsDB::CHAPTER );
			if ( !Check( pChapter != 0, "chapter stats " + szChapter ) )
				continue;
			// The first chapters have no placeholders and no random missions,
			// in the original as here: their scripts enable missions directly.
			if ( pChapter->placeHolders.empty() )
				continue;
			int nPair = 0;
			for ( const std::string &szTemplateName : pCampaign->templateMissions )
			{
				const std::string szTemplate = Lower( szTemplateName );
				const SMissionStats *pMission = NGDB::GetGameStats<SMissionStats>( szTemplate.c_str(), IObjectsDB::MISSION );
				if ( !Check( pMission != 0, "template mission stats " + szTemplate ) )
					continue;
				// The chapter screen's own rule (GameTT/Chapter.cpp).
				if ( pMission->szSettingName != pChapter->szSettingName )
					continue;
				for ( int nDifficulty = 0; nDifficulty < 3; ++nDifficulty )
				{
					if ( szSweep == "cover" && nDifficulty != nPair % 3 )
						continue;
					if ( szSweep.compare( 0, 5, "only=" ) == 0 && szChapter.find( szSweep.substr( 5 ) ) == std::string::npos && szTemplate.find( szSweep.substr( 5 ) ) == std::string::npos )
						continue;
					SCase c;
					c.szCampaign = pszCampaign;
					c.szChapter = szChapter;
					c.szContext = pChapter->szContextName;
					c.szTemplate = szTemplate;
					c.nDifficulty = nDifficulty;
					c.bRegenerate = regenerated.insert( szTemplate ).second;
					cases.push_back( c );
				}
				++nPair;
			}
		}
	}
	return cases;
}

static int GraphIndex( const SMissionStats *pMission, const std::string &szGraphName )
{
	SRMTemplate randomMapTemplate;
	if ( !LoadDataResource( pMission->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
		return -1;
	for ( int i = 0; i < randomMapTemplate.graphs.size(); ++i )
		if ( randomMapTemplate.graphs[i] == szGraphName )
			return i;
	return -1;
}

static bool RunCase( BkEditorSession *pSession, const SCase &c, const std::filesystem::path &scratch )
{
	const std::string szName = c.szChapter + " " + c.szTemplate + " d" + std::to_string( c.nDifficulty );
	// The briefing hands the shared stats to the generator, which writes the
	// objectives' map positions into them; so does this.
	SMissionStats *pMission = const_cast<SMissionStats*>( NGDB::GetGameStats<SMissionStats>( c.szTemplate.c_str(), IObjectsDB::MISSION ) );
	std::string szDir = c.szChapter + "_" + c.szTemplate + "_d" + std::to_string( c.nDifficulty );
	for ( char &ch : szDir )
		if ( ch == '\\' || ch == '/' )
			ch = '_';
	const std::filesystem::path dir = scratch / "random-missions" / szDir;
	std::filesystem::remove_all( dir );
	const std::string szRoot = GeneratedRoot( dir / "a" );
	const int nFailuresBefore = g_nFailures;

	const auto start = std::chrono::steady_clock::now();
	SRMUsedTemplateInfo used;
	const bool bGenerated = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, -1, -1, true, true, &used, 0, szRoot );
	const long long nMs = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - start ).count();
	printf( "random-missions: %s %s %lld ms\n", c.szCampaign.c_str(), szName.c_str(), nMs );
	fflush( stdout );

	if ( Check( bGenerated, szName + ": generates" ) )
	{
		const std::string szMap = szRoot + "maps\\" + pMission->szFinalMap + ".bzm";
		CMapInfo map;
		std::string szError;
		if ( Check( NMapFile::Read( szMap.c_str(), &map, &szError ), szName + ": the generated map reads (" + szError + ")" ) )
		{
			std::set<int> scriptIDs;
			for ( const SMapObjectInfo &object : map.objects )
				scriptIDs.insert( object.nScriptID );
			for ( const SMapObjectInfo &object : map.scenarioObjects )
				scriptIDs.insert( object.nScriptID );
			for ( int i = 0; i < pMission->objectives.size(); ++i )
			{
				const SMissionStats::SObjective &objective = pMission->objectives[i];
				if ( objective.nAnchorScriptID == RMGC_INVALID_SCRIPT_ID_VALUE || objective.nAnchorScriptID == RMGC_DEFAULT_SCRIPT_ID_VALUE )
					continue;
				const std::string szObjective = szName + ": objective " + std::to_string( i ) + " (anchor " + std::to_string( objective.nAnchorScriptID ) + ")";
				Check( scriptIDs.count( objective.nAnchorScriptID ) != 0, szObjective + " has its anchor on the map" );
				// The briefing map is 512x512 (CreateRandomMap, "Place objectives").
				Check( objective.vPosOnMap.x >= 0.0f && objective.vPosOnMap.x < 512.0f && objective.vPosOnMap.y >= 0.0f && objective.vPosOnMap.y < 512.0f,
				       szObjective + " lands on the briefing map at " + std::to_string( objective.vPosOnMap.x ) + "," + std::to_string( objective.vPosOnMap.y ) );
			}
			Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, szName + ": the engine opens it (" + BkEditorLastMessage( pSession ) + ")" );

			if ( c.bRegenerate )
			{
				// What loading a save does (Main/RandomMapHelper.cpp): the seed
				// the first run stored, the graph and angle it chose.
				CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
				CPtr<IDataStream> pSeedStream = CreateFileStream( ( szRoot + "maps\\" + pMission->szFinalMap + ".seed" ).c_str(), STREAM_ACCESS_READ );
				if ( Check( pSeedStream != 0, szName + ": the seed was stored" ) )
				{
					pSeed->Restore( pSeedStream );
					GetSingleton<IRandomGen>()->SetSeed( pSeed );
					const std::string szRootB = GeneratedRoot( dir / "b" );
					const bool bAgain = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, GraphIndex( pMission, used.szGraphName ), used.nGraphAngle, true, true, 0, 0, szRootB );
					CMapInfo again;
					std::string szWhere;
					if ( Check( bAgain, szName + ": regenerates from its seed" )
					     && Check( NMapFile::Read( ( szRootB + "maps\\" + pMission->szFinalMap + ".bzm" ).c_str(), &again, &szError ), szName + ": the regenerated map reads" ) )
						Check( NMapFile::AreEquivalent( map, again, &szWhere ), szName + ": the seed gives the same map (differs at " + szWhere + ")" );
				}
			}
		}
	}

	if ( g_nFailures == nFailuresBefore )
		std::filesystem::remove_all( dir );
	else
		printf( "kept: %s\n", dir.string().c_str() );
	return g_nFailures == nFailuresBefore;
}

int main( int argc, char **argv )
{
	// See the engine tier: a Windows debug assert must not wait behind a dialog.
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( strstr( pszError, "video driver" ) != 0 || strstr( pszError, "No available" ) != 0 )
		{
			printf( "random-missions: skipped: no video driver (%s)\n", pszError );
			return 0;
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "random-missions-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::filesystem::path scratch = argc > 2 ? argv[2] : szSelfDir;
	const std::string szSweep = argc > 3 ? argv[3] : "all";
	std::filesystem::create_directories( scratch );
	if ( !std::filesystem::exists( std::string( pszRoot ) + "/Data/consts.xml" ) )
	{
		printf( "random-missions: skipped: no staged game at %s (run: zig build install-game)\n", pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	// Every engine module derives its roots from the executable's location; see
	// the engine tier (editor_bridge_test.cpp) for why this must hold.
	if ( !Check( SamePath( szSelfDir.c_str(), pszRoot ), "the executable lives in the installation it tests" ) )
	{
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		printf( "random-missions: skipped: no GPU device (%s)\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const auto start = std::chrono::steady_clock::now();
		const std::vector<SCase> cases = CollectCases( szSweep );
		Check( szSweep.compare( 0, 5, "only=" ) == 0 || cases.size() > 150, "the sweep found the chapters' templates (" + std::to_string( cases.size() ) + ")" );
		int nFailedCases = 0;
		for ( const SCase &c : cases )
			if ( !RunCase( pSession, c, scratch ) )
				++nFailedCases;
		const long long nSeconds = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::steady_clock::now() - start ).count();
		printf( "random-missions: %d cases, %d failed, %lld s\n", int( cases.size() ), nFailedCases, nSeconds );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	return g_nFailures == 0 ? 0 : 1;
}
```

Anything you have to change to make it compile or run goes in the task report with the reason.

- [ ] **Step 2: Register it in build.zig**

In `build()`, near the other options, add:

```zig
    const random_missions_sweep = b.option([]const u8, "random-missions-sweep", "test-random-missions: all, cover or only=<text> (default all)") orelse "all";
```

Beside the `addEditorBridgeTest(...)` call (line 2122) add:

```zig
    addRandomMissionsTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode, random_missions_sweep);
```

After `addEditorBridgeTest`, add `addRandomMissionsTest`: a copy of `addEditorBridgeTest` (lines 5467-5575) with these changes and no others: the extra parameter `sweep: []const u8` after `test_mode`; the source file `tools/zig/random_missions_test.cpp`; the executable name `random-missions-test`; after `run.addArg(b.pathFromRoot("zig-out/local-test"));` add `run.addArg(sweep);`; and the step:

```zig
    const step = b.step("test-random-missions", "Generate every random mission a chapter can offer and open it in the engine");
```

Run: `zig test tools/zig/build_hermeticity_test.zig`
Expected: pass.

- [ ] **Step 3: Build and run one template that works today**

```bash
zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=summer_france\\securearea01 2>&1 | tee zig-out/local-test/random-missions-one.txt
```

Expected: a case line per chapter using `summer_france` (German France, Allies France, Italy, Normandy) at each difficulty, `0 failed`, exit 0. If it fails, the instrument is wrong, not the data: fix the test until a secure-area template, which the game generates today, passes.

- [ ] **Step 4: Commit**

```bash
git add tools/zig/random_missions_test.cpp build.zig
git commit -m "test(missions): a tier that generates every random mission a chapter offers

For each chapter that has placeholders, each campaign template of its
setting and each difficulty, it runs the generator as the briefing does,
reads the map back, checks the objectives' anchors are on it and land on
the briefing map, opens it in the engine, and for each template regenerates
it from the stored seed the way loading a save does. A failing case keeps
its files under zig-out/local-test/random-missions.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Objectives anchored on scenario objects

**Files:**
- Modify: `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp:1798-1810`

**Interfaces:**
- Consumes: `zig build test-random-missions` (Task 3).

The generator averages the positions of the objects that carry an objective's anchor script ID. In the scenario-object loop it reads `mapInfo.objects[nObjectIndex].vPos` with the scenario-object index: the wrong object, and past the end of `objects` whenever there are more scenario objects than objects.

- [ ] **Step 1: See it fail**

Run: `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=defend 2>&1 | tee zig-out/local-test/random-missions-defend-before.txt; grep -c "lands on the briefing map" zig-out/local-test/random-missions-defend-before.txt`

Expected: failures naming `lands on the briefing map`, or a crash inside `CreateRandomMap`. Record which in the task report. If neither shows (no defend template anchors on scenario objects), still make the fix: it is an out-of-bounds read on any data that does.

- [ ] **Step 2: Fix it**

In the scenario-object loop, replace both lines

```cpp
						objectiveIterator->vPosOnMap.x += mapInfo.objects[nObjectIndex].vPos.x;
						objectiveIterator->vPosOnMap.y += mapInfo.objects[nObjectIndex].vPos.y;
```

(the pair inside `for ( int nObjectIndex = 0; nObjectIndex < mapInfo.scenarioObjects.size(); ++nObjectIndex )`) with

```cpp
						// The scenario object's own position: this read objects[] with
						// the scenario-object index - another object's position, and
						// past the end of objects when there are more scenario objects.
						objectiveIterator->vPosOnMap.x += mapInfo.scenarioObjects[nObjectIndex].vPos.x;
						objectiveIterator->vPosOnMap.y += mapInfo.scenarioObjects[nObjectIndex].vPos.y;
```

Leave the `objects` loop above it unchanged.

- [ ] **Step 3: See it pass**

Run: `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=defend`
Expected: no `lands on the briefing map` failure. Other failures may remain; they belong to Task 5.

- [ ] **Step 4: Commit**

```bash
git add Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp
git commit -m "fix(rmg): an objective on a scenario object is placed at that object

The generator averaged the anchor objects' positions, and for scenario
objects read objects[] with the scenario-object index: another object's
position, or past the end of objects when a map has more scenario objects.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Diagnose every failing random mission

**Files:**
- Create: `docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md`

**Interfaces:**
- Consumes: `zig build test-random-missions` (Tasks 3-4).
- Produces: the findings document, with one row per failing case and one section per root cause. The controller appends a fix task per root cause to this plan (as Task 5.1, 5.2, … in the form below) before Task 6 starts.

This task changes no code. Follow superpowers:systematic-debugging for each failure: reproduce with `only=`, read the kept files and the generator's `DebugTrace` output, find the first point where the data or the state goes wrong, and state the root cause with the evidence. Do not guess from code reading alone.

- [ ] **Step 1: Run the full sweep**

```bash
zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=all 2>&1 | tee zig-out/local-test/random-missions-all.txt
tail -1 zig-out/local-test/random-missions-all.txt
grep "^FAIL" zig-out/local-test/random-missions-all.txt | sort | uniq -c | sort -rn | head -50
```

Record the total time from the last line: Task 9 decides the CI sweep by it.

- [ ] **Step 2: Group the failures**

Group `FAIL` lines by template type (securearea, defend, escort, hunt) and by check (generates, reads, anchor, briefing map, engine opens, regenerates, same map). A crash ends the run: note the case (the last `random-missions:` line printed), reproduce it with `only=<template>`, get the backtrace (`lldb -b -o run -o bt -- <exe> . <scratch> only=<text>` from the staged game directory), and continue the sweep with the crashing template excluded by running the remaining types with `only=`.

- [ ] **Step 3: Find each root cause**

For each group, reproduce one case and trace it to its cause. Check these candidates first, as the spec names them:

1. Case-sensitive paths: a template, patch, field or script name whose case differs from the file on disk (`resolve` in the data tier matches without case; the generator may not).
2. 64-bit: a `long`/`DWORD`/pointer-size assumption in `RandomMapGen` (sizes read from `.bzm` patches, `int` casts of pointers).
3. The mission scripts (`Data/Scenarios/Scripts/Defend_Area00`, `Escort00`, `Intercept00`): a Lua error the game would print when the mission starts. The generator test does not run scripts; note script questions for Task 8.
4. Windows debug asserts: run the sweep in the Windows debug build too, on `win-home` or in CI (Task 9's steps with `only=`), since `NI_ASSERT` is only live there.

- [ ] **Step 4: Write the findings**

`docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md`:

```markdown
# Random missions: diagnosis

Sweep: `all`, <n> cases, <failures> failed, <seconds> s on <machine>.

## Failing cases

| Template | Chapters | Difficulties | Check | Root cause |
|---|---|---|---|---|

## Root causes

### RC1: <one-line statement>

Evidence: <the trace, the file, the values>.
Fix: <file:line and the change>.
Test: <the case that fails before and passes after>.
```

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md
git commit -m "docs(missions): why the random missions fail, case by case

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

**The fix tasks the controller appends, one per root cause:**

````markdown
### Task 5.N: <root cause, as a fix>

**Files:** Modify: `<file:lines>`

- [ ] **Step 1: See it fail**
Run: `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=<text>`
Expected: `FAIL: <the finding's check>`.

- [ ] **Step 2: Fix the cause** <the exact change from the findings>

- [ ] **Step 3: See it pass** (same command) Expected: `0 failed`.

- [ ] **Step 4: Run the full sweep** Expected: no new failure elsewhere.

- [ ] **Step 5: Commit** `fix(rmg): <what the player gets>`
````

A root cause in data (a wrong setting, a missing file) is fixed in `Data` only if GOG has the same defect and the fix is recorded in `tools/data/gog-1.2-deviations.txt` with its reason, so `test-mission-data` keeps passing.

---

### Task 6: Harness verbs to enter a chapter, start a random mission and win it

**Files:**
- Modify: `Sources/src/Game/GameMain.cpp` (the `BK_AUTO_UI` action chain, after the `campaign=` branch at line ~1344)
- Modify: `Sources/src/Common/InterfaceScreenBase.cpp:778` (`ShowTutorialIfNotShown`)
- Modify: `Sources/src/GameTT/Chapter.cpp` (a trace after the three mission loops, line ~480)

**Interfaces:**
- Produces: `chapter=<chapter stats name>`, `mission=<template mission name>`, `lua=<Lua call>`; `BK_NO_HELP=1`; the trace line `BK_UI_TRACE: chapter "<chapter>" offers <random|historical> mission "<mission>"` (with `BK_UI_TRACE=1`).

- [ ] **Step 1: Add the trace of what a chapter offers**

In `CInterfaceChapter::InitWindow`, after the existing `BK_UI_TRACE: chapter map final` block, add:

```cpp
	// What the chapter screen offers, for harness runs: the random missions it
	// generated and the historical ones the chapter script has enabled.
	if ( getenv( "BK_UI_TRACE" ) )
	{
		for ( int i = 0; i < missionIndeces.size(); ++i )
		{
			const SChapterStats::SMission &offered = pStats->missions[ missionIndeces[i] ];
			fprintf( stderr, "BK_UI_TRACE: chapter \"%s\" offers %s mission \"%s\"\n", GetGlobalVar( "Chapter.Current.Name", "" ),
			         ( offered.pMission != 0 && offered.pMission->IsTemplate() ) ? "random" : "historical", offered.szMission.c_str() );
		}
	}
```

- [ ] **Step 2: Add the verbs**

After the `campaign=` branch in `GameMain.cpp`, add:

```cpp
					else if ( szAction.compare( 0, 8, "chapter=" ) == 0 )
					{
						// chapter=<chapter stats name>: enables the chapter and opens its
						// screen, as picking it on the campaign map does (Campaign.cpp).
						// Needs campaign= first.
						std::string szChapter = szAction.substr( 8 );
						NStr::ToLower( szChapter );
						SetGlobalVar( NStr::Format( "Chapter.%s.Status", szChapter.c_str() ), 1 );
						SetGlobalVar( "Chapter.Current.Name", szChapter.c_str() );
						pMainLoop->Command( MISSION_COMMAND_CHAPTER, "" );
					}
					else if ( szAction.compare( 0, 8, "mission=" ) == 0 )
					{
						// mission=<template mission name>: opens that random mission's
						// briefing from the chapter screen, which generates its map - what
						// the chapter's OK does for a selected mission, except that the
						// harness names the template instead of taking one of the three
						// the chapter happened to generate. The difficulty is that of the
						// chapter's entry for this template, or of its first random entry.
						std::string szMission = szAction.substr( 8 );
						NStr::ToLower( szMission );
						std::string szChapter = GetGlobalVar( "Chapter.Current.Name", "" );
						NStr::ToLower( szChapter );
						int nIndex = -1;
						if ( const SChapterStats *pChapter = NGDB::GetGameStats<SChapterStats>( szChapter.c_str(), IObjectsDB::CHAPTER ) )
						{
							for ( int i = 0; i < pChapter->missions.size(); ++i )
							{
								std::string szEntry = pChapter->missions[i].szMission;
								NStr::ToLower( szEntry );
								const bool bTemplate = pChapter->missions[i].pMission != 0 && pChapter->missions[i].pMission->IsTemplate();
								if ( szEntry == szMission || ( nIndex == -1 && bTemplate ) )
									nIndex = i;
								if ( szEntry == szMission )
									break;
							}
						}
						if ( nIndex == -1 )
							fprintf( stderr, "BK_AUTO_UI: mission=%s: chapter \"%s\" has no random mission entry\n", szMission.c_str(), szChapter.c_str() );
						else
						{
							SetGlobalVar( "Mission.Current.Index", nIndex );
							SetGlobalVar( "Mission.Current.IsTemplate", 1 );
							SetGlobalVar( "Mission.Current.Name", szMission.c_str() );
							pMainLoop->Command( MISSION_COMMAND_ABOUT_MISSION, "" );
						}
					}
					else if ( szAction.compare( 0, 4, "lua=" ) == 0 )
					{
						// lua=<call>: runs a call in the mission's script, e.g. lua=Win(0)
						// to win through the mission's own Win - the path its objectives
						// take. Only meaningful while a mission runs.
						if ( IAILogic *pAI = GetSingleton<IAILogic>() )
							pAI->CallScriptFunction( szAction.c_str() + 4 );
					}
```

- [ ] **Step 3: Let a harness run skip the one-time help screens**

At the top of `CInterfaceScreenBase::ShowTutorialIfNotShown()`:

```cpp
	// A harness run with BK_NO_HELP skips the help screens a fresh profile gets
	// the first time a screen opens: its scripted actions would land on them.
	static const bool bNoHelp = getenv( "BK_AUTO_UI" ) != 0 && getenv( "BK_NO_HELP" ) != 0;
	if ( bNoHelp )
		return;
```

- [ ] **Step 4: Build and stage the debug game**

Run: `zig build install-game`
Expected: success; the game is at `zig-out/game/macos/arm64/debug`.

- [ ] **Step 5: Drive a chapter and a mission by hand**

```bash
mkdir -p zig-out/local-test/mission-run
cd zig-out/game/macos/arm64/debug
rm -rf profiles/MissionRun "$HOME/.local/share/Nival/Blitzkrieg/cache/generated/MissionRun"
BK_UI_TRACE=1 BK_NO_HELP=1 \
BK_AUTO_UI='60:campaign=1=scenarios\campaigns\ussr\ussr,120:chapter=scenarios\chapters\ussr\finland\1,300:chapter=scenarios\chapters\ussr\kursk\1,600:mission=scenarios\templatemissions\all\summer_ukraine\securearea00\1,900:shot,1000:exit' \
  ./Game -profile=MissionRun -windowed 2> ../../../../local-test/mission-run/verbs.log
cd -
grep "BK_UI_TRACE: chapter\|BK_AUTO_UI: frame .* action\|random mission" zig-out/local-test/mission-run/verbs.log
```

Expected: the actions logged in order; `offers` lines for Finland (whose `EnterChapter` gives the player units) and then Kursk; `random mission "…securearea00\1" generated=1`. Adjust the frame numbers if a screen was not up yet when its action fired: the log's `frame N at …` lines show the pacing. The Finland step is there because a random mission uses the player's army, which the campaign's first chapter creates.

- [ ] **Step 6: Commit**

```bash
git add Sources/src/Game/GameMain.cpp Sources/src/Common/InterfaceScreenBase.cpp Sources/src/GameTT/Chapter.cpp
git commit -m "feat(harness): enter a chapter, start a named random mission and win it

BK_AUTO_UI gains chapter=, which opens a chapter as the campaign map does;
mission=, which opens a named random mission's briefing and so generates
its map; and lua=, which runs a call such as Win(0) in the mission script.
BK_NO_HELP skips a fresh profile's help screens, and BK_UI_TRACE lists the
missions a chapter screen offers.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Remove the chapter screen's fallback

**Files:**
- Modify: `Sources/src/GameTT/Chapter.cpp:426-452` (the `if ( missionIndeces.empty() )` block that lists every historical mission)
- Create: `tools/missions/run_random_mission.sh`

**Interfaces:**
- Consumes: the Task 6 verbs and trace.
- Produces: `tools/missions/run_random_mission.sh <campaign index> <campaign> <first chapter> <chapter> <template> <historical>`; exit 0 when the chapter offers no historical mission before the random win and offers `<historical>` after it.

- [ ] **Step 1: Write the run script**

`tools/missions/run_random_mission.sh`:

```bash
#!/usr/bin/env bash
# Plays one random mission in the worktree's staged debug game, wins it through
# the mission script, and checks the chapter screen: before the win it offers no
# historical mission, after it the chapter's historical mission.
#
# usage: tools/missions/run_random_mission.sh <campaign index> <campaign> \
#          <first chapter> <chapter> <template mission> <historical mission>
#   campaign index: 0 German, 1 USSR, 2 Allies (GameTT/UIState.cpp)
#
# Uses the throw-away profile MissionRun; never the player's profiles.
# Output: zig-out/local-test/mission-run/<template>.log and .png
set -euo pipefail
[ $# -eq 6 ] || { sed -n 2,12p "$0"; exit 2; }
index=$1 campaign=$2 first=$3 chapter=$4 template=$5 historical=$6
root=$(cd "$(dirname "$0")/../.." && pwd)
game="$root/zig-out/game/macos/arm64/debug"
out="$root/zig-out/local-test/mission-run"
name=$(echo "$template" | tr '\\/' '__')
log="$out/$name.log"
mkdir -p "$out"
rm -rf "$game/profiles/MissionRun" "$HOME/.local/share/Nival/Blitzkrieg/cache/generated/MissionRun"

# Frames: tuned with the frame lines of a BK_UI_TRACE log (Task 6 Step 5). The
# briefing generates the map inside one frame, so everything after it is late.
schedule="60:campaign=$index=$campaign,120:chapter=$first,300:chapter=$chapter"
schedule="$schedule,600:mission=$template,900:ok,1500:key=SPACE,1800:lua=Win(0)"
schedule="$schedule,2100:ok,2300:ok,2500:ok,2700:ok,2900:ok,3100:shot,3200:exit"

cd "$game"
BK_UI_TRACE=1 BK_NO_HELP=1 BK_AUTO_UI="$schedule" ./Game -profile=MissionRun -windowed 2> "$log" || true
cd "$root"

win_line=$(grep -n "action lua=Win(0)" "$log" | head -1 | cut -d: -f1)
[ -n "$win_line" ] || { echo "FAIL: the run never reached lua=Win(0); see $log"; exit 1; }
before=$(head -n "$win_line" "$log" | grep -F "chapter \"$chapter\" offers" || true)
after=$(tail -n "+$win_line" "$log" | grep -F "chapter \"$chapter\" offers" || true)
status=0
if echo "$before" | grep -q "offers historical"; then
  echo "FAIL: before the random win the chapter offered a historical mission:"; echo "$before"; status=1
fi
echo "$before" | grep -q "offers random" || { echo "FAIL: before the win the chapter offered no random mission"; status=1; }
echo "$after" | grep -qF "offers historical mission \"$historical\"" || {
  echo "FAIL: after the random win the chapter did not offer $historical:"; echo "$after"; status=1; }
[ $status -eq 0 ] && echo "PASS: $template in $chapter"
exit $status
```

`chmod +x tools/missions/run_random_mission.sh`

- [ ] **Step 2: See the fallback break the rule**

```bash
tools/missions/run_random_mission.sh 1 'scenarios\campaigns\ussr\ussr' 'scenarios\chapters\ussr\finland\1' \
  'scenarios\chapters\ussr\stalingrad\1' 'scenarios\templatemissions\all\winter_russia\securearea00\1' \
  'scenarios\scenariomissions\ussr\stalingrad\1'
```

Expected: `FAIL: before the random win the chapter offered a historical mission` with `offers historical mission "scenarios\scenariomissions\ussr\stalingrad\1"`. If the run never reaches `lua=Win(0)`, tune the frames first (the script's comment says how) until it fails only for this reason.

- [ ] **Step 3: Save on the chapter screen, for Review Focus 3**

Run the game once more with `…,300:chapter=scenarios\chapters\ussr\stalingrad\1,500:cmdc=0x10010014=FallbackEra.sav;0,600:exit` (`MAIN_COMMAND_SAVE`, `Sources/src/Main/iMainClassIDs.h:21`), and copy `profiles/MissionRun/…/FallbackEra.sav` (find it with `find zig-out/game/macos/arm64/debug -name FallbackEra.sav`) to `zig-out/local-test/mission-run/`.

- [ ] **Step 4: Remove the fallback**

Delete this block from `CInterfaceChapter::InitWindow`, and nothing else:

```cpp
	if ( missionIndeces.empty() )
	{
		NStr::DebugTrace( "CInterfaceChapter::InitWindow(), no enabled scenario missions for chapter \"%s\", using fallback list\n", GetGlobalVar( "Chapter.Current.Name", "" ) );
		for ( int i = 0; i < pStats->missions.size(); ++i )
		{
			…
			missionIndeces.push_back( i );
		}
	}
```

Keep the stale-index guard, the `missionIndeces.empty()` return to the campaign screen further down, and `IncrementChapterVisited`'s restoring of the previous mission set. Above `nNumberOfScenarioMissions = missionIndeces.size();` add:

```cpp
	// Only what the chapter script has enabled. Before the chapter's first random
	// win that is nothing, and the screen offers only random missions: the
	// original's design (Data/Scenarios/Chapters/*/*/script.lua, MissionFinished).
```

- [ ] **Step 5: See the rule hold**

Run: `zig build install-game`, then the Step 2 command again.
Expected: `PASS: scenarios\templatemissions\all\winter_russia\securearea00\1 in scenarios\chapters\ussr\stalingrad\1`.

- [ ] **Step 6: Load the fallback-era save**

Copy `FallbackEra.sav` back into the `MissionRun` profile's saves, then run with `60:cmdc=0x10010015=FallbackEra.sav,400:shot,500:exit` (`MAIN_COMMAND_LOAD`, `iMainClassIDs.h:22`; if it wants a different configuration string, `MainLoopCommands.cpp` shows it). Expected: no crash, and the log's `offers` lines for Stalingrad list random missions only.

- [ ] **Step 7: Commit**

```bash
git add Sources/src/GameTT/Chapter.cpp tools/missions/run_random_mission.sh
git commit -m "fix(chapter): a chapter offers only what its script has enabled

Until a chapter's first random win the chapter screen now offers only its
random missions, as the original does; the port listed every historical
mission of the chapter instead, finished ones included, whenever the script
had enabled none - which was always, so the player went from historical
mission to historical mission. The data tier guarantees every chapter that
asks for a random win can offer one, which the fallback was covering for.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Win each kind of random mission in the game

**Files:**
- Modify: `docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md` (a "Game runs" section)

**Interfaces:**
- Consumes: `tools/missions/run_random_mission.sh` (Task 7).

- [ ] **Step 1: The four kinds**

```bash
s=tools/missions/run_random_mission.sh
$s 1 'scenarios\campaigns\ussr\ussr' 'scenarios\chapters\ussr\finland\1' 'scenarios\chapters\ussr\kursk\1' \
   'scenarios\templatemissions\all\summer_ukraine\defend00\1' 'scenarios\scenariomissions\ussr\kursk\1'
$s 0 'scenarios\campaigns\german\german' 'scenarios\chapters\german\poland\1' 'scenarios\chapters\german\kharkov42\1' \
   'scenarios\templatemissions\all\summer_ukraine\escort00\1' 'scenarios\scenariomissions\german\kharkov42\1'
$s 1 'scenarios\campaigns\ussr\ussr' 'scenarios\chapters\ussr\finland\1' 'scenarios\chapters\ussr\rumania\1' \
   'scenarios\templatemissions\all\summer_ukraine\hunt00\1' 'scenarios\scenariomissions\ussr\rumania\1'
$s 0 'scenarios\campaigns\german\german' 'scenarios\chapters\german\poland\1' 'scenarios\chapters\german\france\1' \
   'scenarios\templatemissions\all\summer_france\securearea01\1' 'scenarios\scenariomissions\german\france\1'
```

Expected: four `PASS` lines. Look at each `.png` in `zig-out/local-test/mission-run`: the chapter screen after the win. Grep each log for `Lua`/`script` errors; a script error is a finding even when the run passes.

A failure here that the generator tier did not catch (a script error, a crash when the mission starts) gets a root cause and a fix task exactly as in Task 5.

- [ ] **Step 2: The mod keeps working (local only, never committed)**

```bash
cp -Rc /Users/johannes/Projects/src/Blitzkrieg/zig-out/game/macos/arm64/release/mods/AchtungPanzer2 zig-out/game/macos/arm64/debug/mods/
```

For each chapter of the mod's campaign (their names are the `<Chapter>` values of the mod's campaign XML under `mods/AchtungPanzer2`), run the game with `-mod=AchtungPanzer2 -profile=MissionRun` and `campaign=<index>=<mod campaign>`, then `chapter=<chapter>`, `shot`, `exit`, and check the log offers at least one mission for the chapter. Expected: every chapter offers at least one. Then `rm -rf zig-out/game/macos/arm64/debug/mods/AchtungPanzer2`, and confirm `git status` shows nothing of the mod.

- [ ] **Step 3: Record and commit**

Add a "Game runs" section to the findings: the four runs, their results, anything seen in the pictures or logs, and the mod check.

```bash
git add docs/superpowers/plans/2026-09-25-revive-random-missions-findings.md
git commit -m "docs(missions): each kind of random mission won in the game

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: The random missions tier in CI

**Files:**
- Modify: `.github/workflows/cross-platform.yml` (after each `Engine tier` step: Windows x64 line ~310, macOS arm64 line ~481)

**Interfaces:**
- Consumes: the sweep time recorded in Task 5.

- [ ] **Step 1: Choose the CI sweep**

If the full sweep took 900 s or less locally (Task 5 Step 1, after all fixes: re-run it now and use the new time), CI runs `all`. Otherwise CI runs `cover`, and the full sweep stays a local step before merging (Step 4).

- [ ] **Step 2: Add the steps**

Windows x64, after `Engine tier`:

```yaml
      - name: Random missions tier
        run: zig build test-random-missions -Dtarget=x86_64-windows-msvc -Dtest-mode=run "-Drandom-missions-sweep=<all|cover>" "-Dmsvc-include=$env:MSVC_INCLUDE" "-Dwindows-sdk-include=$env:WINDOWS_SDK_INCLUDE" "-Dmsvc-lib=$env:MSVC_LIB" "-Dwindows-sdk-lib=$env:WINDOWS_SDK_LIB"
```

macOS arm64, after `Engine tier`:

```yaml
      - name: Random missions tier
        run: zig build --sysroot "$MACOS_SYSROOT" test-random-missions -Dtarget=aarch64-macos -Dtest-mode=run -Drandom-missions-sweep=<all|cover>
```

- [ ] **Step 3: Push the branch and watch CI**

Ask Johannes before pushing. After he agrees: `git push -u origin fix/revive-random-missions`, then watch the run (`gh run watch`). Expected: all jobs green; the two `Random missions tier` steps print `0 failed`. A Windows failure that macOS does not show is a Windows-only assert (Review Focus 5): diagnose and fix it as in Task 5.

- [ ] **Step 4: The full sweep, locally, when CI runs cover**

Run: `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=all`
Expected: `0 failed`.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/cross-platform.yml
git commit -m "ci: the random missions tier runs on the macOS and Windows GPU runners

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: Johannes plays them

**Files:** none.

- [ ] **Step 1: Stage a release build from the branch for him**

Run: `zig build install-game --release=fast` in the worktree. Tell Johannes the path, `zig-out/game/macos/arm64/release` inside `.worktrees/random-missions`, and that it has its own `profiles/`; his main game is untouched.

- [ ] **Step 2: He plays**

One defend, one escort and one hunt mission, in any gated chapter, from the chapter screen as a player would; then checks the chapter offers its historical mission after the first win. Record his findings in the findings document; any problem becomes a Task 5-style fix.

- [ ] **Step 3: Finish**

When he is satisfied, use superpowers:finishing-a-development-branch.
