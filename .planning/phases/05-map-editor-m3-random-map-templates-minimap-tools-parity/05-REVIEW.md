---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
reviewed: 2026-10-03T00:00:00Z
depth: standard
diff_base: 78b96aa9e78ab615b3e967a8aa7898bc39e39661
files_reviewed: 96
areas: [A editor app, B editor core, C engine C++, D build/CI/tools]
findings:
  critical: 6
  warning: 29
  info: 19
  total: 54
status: issues_found
---

# Phase 5 code review

The phase changed 96 source files (about 110,000 added lines), so the review ran as four parallel standard-depth reviews, one per area. Finding IDs carry the area letter (CR-A01, WR-C03, ...). Each area's file list is in its section.


# Area A


## Phase 5 (area A): Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 23

## Summary

Reviewed the portable editor's app layer (Zig) and the ImGui backend against `78b96aa9e`. There is no
`CLAUDE.md` in the repository, so no project conventions were applied beyond what the code itself states.
Read in full: `single_instance.zig`, `c_bridge.zig`, `minimap.zig`, `tool_registry.zig`, `game_reads_common.zig`,
`imgui_backend.cpp`, most of `panels_m3.zig`, `panels_m2.zig` and the new parts of `panels.zig`,
`panels_logic.zig`, `view.zig`, `view_math.zig`, `testlaunch.zig`, `main.zig` and `commands.zig`.
`smoke.zig`, `c_bridge_test.zig`, `game_reads_m2/m3.zig` and `markers.zig` were only scanned (test and render
code): no unsafe patterns turned up by grep, but they were not read line by line.

Three defects are real memory-safety or correctness problems that ordinary use reaches: File > New leaves the
view and brush unwired (CR-A01), the Fields Composer's "Remove tile/object" indexes freed list entries in the same
frame (CR-A02), and a hand-edited settings file can overflow a fixed buffer (CR-A03). `single_instance.zig`
is well structured (queue, watchdog, abandon-able hand-off job are sound), but its stale-detection treats any
unanswered connection as "dead owner", which can delete a live owner's socket, and `deinit` can block forever
(WR-A01, WR-A02). The scripted scenarios (`BK_EDITOR_AUTO`) exercise commands, not the ImGui buttons that call
the same core functions directly, which is why CR-A01 and CR-A02 were not caught.

## Critical Issues

### CR-A01: File > New never wires the view, the brush tiles or the placer (mapOpened returns early for a path-less document)

**File:** `Sources/editor/app/panels.zig:1523` (also `:1805-1818`, `:3568-3570`)
**Issue:** `State.mapOpened` ends with `if (!mapIsOpen(self.editor)) return;` (line 1523) before it calls
`self.view.showMap(...)`, loads `tile_buffer`/`describeTiles` and sets the placer's default object. `mapIsOpen` is
"document path non-empty". A map created by File > New is, by design (05-01-SUMMARY: "`mapIsOpen` is for file-bound
questions only"), a document with an empty path, and the `.new_map` arm of `act` (lines 1805-1818) calls only
`state.mapOpened()`. So after File > New:
- `view.showMap` is never called: `view.map` stays `{}` (first map of the session) or the *previous* map's size,
  `view.current_path`/`remembered` and the tool state resets (`roads_rivers.reset()` ... `ai_tool.reset()` in `showMap`)
  are skipped. `Camera.clamp(self.map)` (view_math.zig:145) then clamps every pan, `centreOn`, minimap click and
  Go-to to (0,0)..(0,0) or to the old map's bounds.
- `tile_count` was zeroed at line 1462 and is never refilled, so the Brush panel says "no map open: no tiles to paint"
  (line 3568-3570) and, after Save As (which does not call `mapOpened` either), "no tiles to paint: " with an empty reason.
- `placer.name` is whatever the previous map left, so Place refuses until a map is reopened.
The scenarios in `build.zig` (lines 6682-6720, 7114) only use `heights`/`fill`/`update` and the title after New,
and `HOME`, so they pass.
**Fix:** Gate the per-map reload on a loaded document, not on a file path, and call it again once a never-saved document
gets its path:
```zig
// panels.zig, State.mapOpened
if (!documentLoaded(self.editor)) return;
self.view.showMap(self.real, self.editor.document.path.items, self.editor.document.info, self.defaultPlacerObject());
```
`View.showMap` must then accept an empty path (do not key `remembered` on `""`; skip `saveCurrentView`/`remembered.get`
for it). Also call `state.view.rebindPath(...)` (or `showMap` again) after the first successful Save As of a never-saved map
so `view.current_path` follows the document. Add an app-level test that New followed by `tiles().len > 0` and
`view.map.width_tiles == 8`.

### CR-A02: "Remove tile" / "Remove object" in the Fields Composer indexes a list it just shrank, in the same frame

**File:** `Sources/editor/app/panels_m3.zig:2706-2747`
**Issue:** `drawEntryList` reads `count` at line 2706 and sizes `fc_entry_selected` to it. The Remove button (2711-2718)
then calls `composers.removeShellObjects`/`removeShellTiles` (the lists shrink immediately) and
`state.fc_entry_selected[kind].clearRetainingCapacity()` (length 0). The loop at 2739 still runs `for (0..count)` over the
*old* count and reads `f.object_shells.items[shell].objects.items[i]` / `f.tile_shells.items[shell].tiles.items[i]` and
`state.fc_entry_selected[kind].items[i]` (line 2746). In Debug/ReleaseSafe this is an index-out-of-bounds panic on the very
first iteration of the frame in which Remove is clicked (`items.len == 0`); in ReleaseFast/Small it reads stale memory
beyond `len`. The command route (`rmgf_tile_del`/`rmgf_object_del`) is what the scenarios drive, so the button path is untested.
**Fix:** Recompute after the mutation (or skip the rest of the frame):
```zig
if (ig.igButton(if (objects) "Remove object" else "Remove tile")) {
    ...removeShell*...
    state.fc_entry_selected[kind].clearRetainingCapacity();
    ig.igEndDisabled();
    return; // the lists changed; draw them next frame
}
```
or re-read `count` and call `ensureSelection` again after the button block and before the loop.

### CR-A03: `filter_active` from mapeditor.cfg overflows `filter_delete_popup` (64 bytes) when "Delete Filter" is clicked

**File:** `Sources/editor/app/panels.zig:3937-3943` (field at `:419`; source at `core/settings.zig:258-259`)
**Issue:** `settings.filter_active` is a `FixedPath` (1024 bytes) loaded verbatim from the user-editable
`mapeditor.cfg` with no validation against the existing filters. `drawPaletteFilters` copies it with
`@memcpy(state.filter_delete_popup[0..combo_name.len], combo_name[0..combo_name.len])` into
`filter_delete_popup: [64:0]u8`. With a `filter_active=` value longer than 64 bytes, clicking "Delete Filter" panics
(safe builds) or writes up to 960 bytes past the field into the adjacent `State` members in ReleaseFast/Small (the `State`
lives by value in `interactive`'s frame; neighbours include slices and ArrayLists), i.e. memory corruption from file input.
**Fix:**
```zig
if (combo_name.len != 0 and combo_name.len < state.filter_delete_popup.len) { ... } else { setStatus("filter: ", "that filter name is too long"); }
```
and validate on load (`filterSelect`-style: drop a `filter_active`/`filter_slot_*` that names no filter, or longer than the filter-name limit).

## Warnings

### WR-A01: Any unanswered hand-off is treated as "stale", so a live owner's socket can be deleted and a second editor started

**File:** `Sources/editor/app/single_instance.zig:275-278`, `:301`, `:410-416`, `:369-371`, `:520-526`
**Issue:** The client folds every read failure into `Handoff.no_reply`, and `endpointIsStale(.no_reply)` is true, so
`acquireAt` deletes the socket file and binds its own. But the owner closes a connection without any answer in several
live-owner cases: the `max_connections` cap (line 275-278: `stream.close` unanswered), `takeDelimiterExclusive` errors
(line 301), a failed write, and the watchdog shutting a slow client. Four silent local connections (or a loaded machine
that makes a launch slower than 1.5 s) therefore make the next launch evict a perfectly healthy owner: two primaries,
the first one's listener unreachable by path, and (WR-A02) the first one's exit then removes the second's file.
**Fix:** Distinguish "connected, then EOF/reset" (a live listener: return `.refused`/new `.dropped`, never stale) from
"connected and nothing within the timeout" (`.no_reply`). Make the owner answer `busy\n` instead of closing when over the
connection cap, and test the cap case.

### WR-A02: `Instance.deinit` can block forever on `thread.join()` and deletes whatever file is at the socket path

**File:** `Sources/editor/app/single_instance.zig:227-244`
**Issue:** The accept thread is woken only by connecting to `self.path()` (230-232) and the errors are swallowed. If the
socket file is gone or replaced (a second launch judged this one stale, the user or a tmp cleaner removed it, the
fallback path lives in `$TMPDIR`), `connect` fails or reaches another listener, the accept never returns, and
`thread.join()` never returns. `interactive` defers `owner.deinit()` first, so it runs last: the window is gone but the
process never exits. `deleteFile(self.path())` (line 236) also unlinks a *different* instance's live socket.
**Fix:** Do not depend on the path for shutdown: wait for the accept thread with a deadline like the connection wait
below it (flag set by `serve` on exit; after ~1 s detach and leak, the process is exiting), or poll the listening fd with a
timeout in `serve`. Record the socket file's inode/`st_dev` at bind and delete only if it still matches.

### WR-A03: The hand-off drops `-mod=`, so the map opens under whichever mod the running editor has

**File:** `Sources/editor/app/main.zig:381-414` (`acquireInstance`)
**Issue:** The line sent is the absolute map path only; `mod_folder`/`mod_requested` are not part of the protocol or
the call. `editor.open` in the running editor therefore reads a mod map under another object database, which the code base
itself documents as the cause of "1359 unknown objects" (panels_logic.zig:1473-1483). The second launch then exits 0, so the
user's explicit `-mod=` is lost without any message.
**Fix:** Either carry the mod in the line (`<mod>\t<path>` is rejected by `parseLine`; use a second line or a prefix) and
have the owner route it through the mod-switch guard, or skip the hand-off when `mod_requested` differs from the owner's mod
and start normally.

### WR-A04: The shared-`/tmp` fallback endpoint is predictable and its owner is never verified

**File:** `Sources/editor/app/single_instance.zig:134-139`, `:500-531`
**Issue:** When the user root is too long for `sun_path`, the endpoint becomes `$TMPDIR/bk-mapeditor-<fnv64(root)>.sock`
(`/tmp` when `TMPDIR` is unset: world-writable on Linux). Another local user can pre-bind that name (sticky bit: the victim
cannot remove it). The victim's launch then connects to the squatter, gets `ok` and exits 0 (`handed_off`) - the editor
never starts and the squatter receives the map path - or gets `no_reply`, fails to delete the file and silently runs without
single-instance. Nothing checks that the socket is owned by the current user.
**Fix:** Put the fallback in a per-user 0700 directory (`$XDG_RUNTIME_DIR`, or `/tmp/bk-<uid>/` created with mode 0700 and
checked to be owned by the user), and `fstat` the socket file's `uid` before trusting a listener.

### WR-A05: A frozen main loop is not "a hung peer": the helper thread keeps acknowledging

**File:** `Sources/editor/app/single_instance.zig:29-33` (claim), `:262-312` (behaviour)
**Issue:** The header says a hung owner "answers nothing within `timeout_ms`" and so never blocks a start. But `serveOne`
answers `ok` from its own thread as soon as the line is queued; only a deadlocked *accept thread* is ever detected. If the
main loop is stuck (a synchronous Create Random Map / Update Map, or a real deadlock), every second launch is acked,
queues up to 8 lines and exits 0; the user cannot start another editor and nothing tells them why.
**Fix:** Have the main loop publish a heartbeat (atomic timestamp updated each frame); the accept thread answers `ok` only
if it is recent, else `busy`, and the client treats `busy` after a grace period as stale. Otherwise correct the comment and
the threat-model text so it does not claim this.

### WR-A06: A never-saved (File > New) map is never autosaved or given a recovery copy

**File:** `Sources/editor/app/panels.zig:2072-2080`
**Issue:** `tickAutosave` returns early when `!mapIsOpen(state.editor)` (path empty), then calls `note(now_ms, dirty and
mapIsOpen)`, i.e. never dirty. 05-CONTEXT D-22 (line 138) says a created map "opens as a never-saved document
(recovery-copy autosave)", and `core.autosave.recoveryName` already maps an empty path to `untitled.bzm`. Work on a new
map is lost on a crash. (Test in game is gated the same way, panels.zig:2513 and menu line 2766.)
**Fix:** Use `documentLoaded` in `tickAutosave` and `requestTestLaunch`; make `writeRecoverySidecar` write a sidecar for a
path-less document (original path empty) and teach `scanRecoveryOffers` to list it.

### WR-A07: The Select tool swallows the second click of any quick pair, and opens Properties for the previous selection

**File:** `Sources/editor/app/view_math.zig:441-450`, `Sources/editor/app/view.zig:506-512`, `Sources/editor/core/tools.zig:348`
**Issue:** `kindOf` turns every left press with `clicks == 2` into `.double_click` when the tool `needs_double_click` (Select
has it since M3), and the matching release into nothing. SDL's `clicks` counts within 500 ms and 32 px, regardless of what is
under the pointer. The Selector ignores `.double_click` (tools.zig:348), so a quick click on a *neighbouring* object, or a
quick second Ctrl+click while multi-selecting adjacent units, never reaches the tool: nothing is selected, and
`dispatch` (view.zig ~636-645) then sets `props_open_request` because `editor.selection != null`, popping the Properties
window for the old object.
**Fix:** Deliver `.double_click` only if the second click lands on the same object (`pointer.object == selection`) and has no
Ctrl/Shift; otherwise dispatch an ordinary press/release pair (in `kindOf` take a `same_target` flag, or let the Selector
treat an unmatched `.double_click` as press + release).

### WR-A08: Check Map "Fix all" acts on findings that may be older than the map

**File:** `Sources/editor/app/commands.zig:3073-3081` (with `Sources/editor/app/panels_m3.zig:954-967`, `Sources/editor/core/editor.zig:1104-1111`)
**Issue:** `checkMapFixAll` re-runs the checks only when `check_findings.len == 0`. Otherwise it hands the findings from the
last run to `editor.fixAll`, which deletes roads and rivers by the stored `vso_index` (and objects by `link_id`). Any edit
or undo between the check and the click (the window stays open and enabled) shifts those indices, so "Fix all and remove
them" can delete a different, healthy road or river. One undo step recovers it, but nothing tells the user.
**Fix:** Always `runChecks(state)` at the top of `checkMapFixAll` (and compare the counts if you want to warn when they
differ), or invalidate `check_findings` from the editor's generation counters (like the other cached lists do).

### WR-A09: Template Diplomacy popup underflows `tc_dipl_count - 1` on a template with no diplomacy entries

**File:** `Sources/editor/app/panels_m3.zig:3595-3605`
**Issue:** "Add a player on side 0/1" does `state.tc_dipl_sides[state.tc_dipl_count - 1] = ...`. `openDiplomacy` copies
`t.diplomacies.items.len` (0 for an opened template file with no diplomacy), the buttons are enabled while
`count < max_diplomacies`, so the `usize` subtraction wraps (panic in safe builds, wild write in ReleaseFast).
**Fix:** `ig.igBeginDisabled(count == 0 or count >= core.rmg.max_diplomacies)`, or seed an empty list with the neutral entry
in `openDiplomacy`.

### WR-A10: The previous document's recovery copy is left behind when New (or a drop/second-instance open) replaces it

**File:** `Sources/editor/app/panels.zig:1763-1787`, `:1805-1818` (compare `:1749`, `:1756`, `:1794`, `:3152`)
**Issue:** Quit, Save, Save As and Close/Mod-switch call `deleteRecoveryIfActive`. The open arm and the new `.new_map` arm
do not, so after "Don't save" the discarded document's recovery file and sidecar stay on disk (offered as "unsaved work" at
the next start) and `recovery_active` still names them until the next write overwrites the field and orphans them for good.
File > Open had this before; File > New, drop and second-instance opens (D-34) make it routine.
**Fix:** Call `deleteRecoveryIfActive(state)` and `state.autosave.note(0, false)` before replacing the document in both arms
(only when the prompt answered Don't save / Save landed).

## Info

### IN-A01: Registry digit shortcuts also fire with Ctrl/Cmd/Alt held

**File:** `Sources/editor/app/view.zig:684-687`
**Issue:** The `else` arm of `handleKey` calls `tool_registry.byShortcut(key.key)` without looking at `key.mod`, although
the registry documents the keys as "bare digit ... never with a modifier". Cmd+4 or Ctrl+1 switch tools (and `Q`/`E` rotate
on Cmd+Q).
**Fix:** `if (!key.repeat and key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT) == 0)`.

### IN-A02: A failed minimap rebuild is never retried and its flag is never read

**File:** `Sources/editor/app/minimap.zig:205-206`, `:248-250`
**Issue:** `defer mm.revision_seen = state.editor.mapRevision()` runs on every exit of `rebuild`, including the early
`return`s after `read_failed = true`; `read_failed` is not read anywhere. A transient failure (allocation, tile read, texture
create) leaves a grey rectangle until the next edit, with no message. The same happens to the Heights shading when
`break :heights` is taken.
**Fix:** Set `revision_seen` only on success; show a one-line "minimap could not be read" when `read_failed`.

### IN-A03: Second-instance bookkeeping reports before the fact and can mislabel a later open

**File:** `Sources/editor/app/panels.zig:2927-2938`, `:1830-1841`, `:270-271`
**Issue:** `openFromSecondInstance` sets the status "opened from second instance" and prints the console line the
double-launch check reads when the path is only *queued*; behind the unsaved prompt the open can still be cancelled, in
which case `open_origin` stays set and the next, unrelated open is announced as coming from a second instance/drop.
**Fix:** Print in `announceOpen` only; clear `open_origin` when the prompt is cancelled (`answer == .dropped`).

### IN-A04: `minimap.create_pending` can stay set and make a later Save create the pictures

**File:** `Sources/editor/app/commands.zig:3224-3229`, `Sources/editor/app/panels.zig:1731-1734`
**Issue:** `minimapCreate` sets `create_pending` and `save_requested`; if the Save As dialog is already up, `next()` answers
`dialog_busy` and `act` only sets a status, leaving the flag. The next unrelated Save runs `finishPendingMinimapCreate` and
writes eight files nobody asked for.
**Fix:** Clear `create_pending` in the `.dialog_busy` arm of `act`.

### IN-A05: `storageNameFromBrowse`/`patchNameFromBrowse` accept `..` segments under `Data/`

**File:** `Sources/editor/app/panels_logic.zig:3824-3842`, `:4249-4263`
**Issue:** Only the textual prefix `<base>Data/` is checked; `<base>Data/../../x/y.xml` yields the storage name
`..\..\x\y`, which is then handed to the bridge. The bridge is the real guard, but the app layer should not forward it.
**Fix:** Return null when any segment of `rest` is `..` (or `.`) or empty.

### IN-A06: Options text can override the editor's own `-profile=`/`-mod=`; empty quoted groups use up argument slots

**File:** `Sources/editor/app/testlaunch.zig:69-110`, `:157-161`
**Issue:** The extra arguments come after `-profile=MapEditorTest -mod=...`, so a `-profile=X` typed into Options makes the
test game write the user's real profile, as the MFC allowed. Argument splitting is shell-free (good), but `""` pairs each
take one of the 16 slots before the empty ones are filtered, so real arguments after 16 empties are dropped.
**Fix:** Document the override (or reject `-profile=`/`-mod=` in `splitParameters`); filter empties before the cap check.

### IN-A07: `std_options.unexpected_error_tracing = false` is global

**File:** `Sources/editor/app/main.zig:122-125`
**Issue:** Set to hide the stack dump for the expected "connection refused" of a second launch; it also hides the trace of
every other `error.Unexpected` in Debug builds for all modes (`--check`, `--smoke`, `--game-reads-it*`).
**Fix:** Keep tracing on and map the connect error to a known error in `HandoffJob.run`, or enable the option only around
`single_instance.acquire`.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

# Area B


## Phase 5, Area B: Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 21 (no CLAUDE.md exists in the repository root; none applied)

## Summary

The core layer is generally careful. The reserve-before-bridge pattern, the
two-pass bridge reads (`fetchContainer`, `fetchGraph`, `fetchTemplate`) and the
errdefer chains in the record readers all held up when traced. The history
`clean_depth` bookkeeping is consistent in every sequence I traced
(`recordAssumeCapacity` catches `clean_depth > len` before the append, so a
stale value can never alias a later depth).

Script-path handling in `script_file.zig` is sound for the Windows/macOS case.
Both separators are split, every destination is a fixed directory plus one
validated component, and the stored/loaded forms agree with the C++ rule from
commit f54732c46. The findings below are therefore about multi-step edits that
can leave the bridge, the document and the history disagreeing, one numeric
overflow on untrusted map data, the settings file format, and a few
Windows-only edges.

I did not review `shipped.zig`, `autosave.zig` or `document.zig` (not in the
file list).

## Critical Issues

### CR-B01: `deleteMany` is not all-or-nothing; a refusal part-way deletes members with no history entry

**File:** `Sources/editor/core/editor.zig:653-678` (caller ordering at `889-906`)

**Issue:** The doc comment says "A member's refused delete refuses the whole
delete and changes nothing", and that members are checked "before any of them
goes". The pre-check (`657-660`) only tests for duplicates and presence in the
document. It does not test the refusals the bridge actually applies
(`bridge.h:188-190`): a bridge span, a trench piece, an object carrying a
passenger, an object whose link ID is shared.

The loop at `665-674` then deletes members one at a time. When member k > 0 is
refused, `try self.noteOutcome(...)` returns. Members 0..k-1 are already gone
from the bridge and from `document.objects`, and they were already removed from
the selection. `deleted` is dropped by its `errdefer`, so nothing is recorded:

- the deletions cannot be undone,
- `history.dirty()` stays false, so the editor does not offer to save and
  autosave does not see a change,
- `bumpCascadeGenerations` and `noteCascade` are skipped, so the start-command
  and reserve-position panels show stale data.

This is reachable without anything exotic. `deleteSelection` (`889-906`)
orders `all` as "passengers of members who are not themselves selected", then
the members in ascending link ID. If a host H and its own passenger P are both
selected (rubber band, Ctrl+click) and H has the lower link ID, the order is
`[..., H, P]`. The host is deleted first and the bridge refuses ("object
carrying a passenger"). Any unrelated lower-ID member earlier in the list has
already been deleted by then, and that deletion is silently kept and
unrecoverable. A map with a shared link ID (the kind Check Map reports as
`duplicate_link`) produces the same partial result.

`deleteMany`, `deleteSelection` and `deleteHost` have no test in `editor.zig`
or `tools.zig` that makes a later member refuse.

**Fix:** Make the loop transactional and fix the ordering. Reserve restore room
first, roll back on failure, and put every passenger before its host.

```zig
pub fn deleteMany(self: *Editor, link_ids: []const i32) EditError!void {
    if (link_ids.len == 0) return;
    for (link_ids, 0..) |link_id, i| {
        if (std.mem.indexOfScalar(i32, link_ids[0..i], link_id) != null) return error.Failed;
        if (self.document.indexOf(link_id) == null) return error.Failed;
    }
    try self.history.reserve(self.allocator);
    // Room for the rollback's restores, so it cannot fail half way.
    try self.document.objects.ensureUnusedCapacity(self.allocator, link_ids.len);
    var deleted: std.ArrayListUnmanaged(history_mod.DeletedRecord) = .empty;
    errdefer deleted.deinit(self.allocator);
    try deleted.ensureTotalCapacity(self.allocator, link_ids.len);
    for (link_ids) |link_id| {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        self.noteOutcome(self.bridge.deleteObject(link_id)) catch |err| {
            const message_len = self.status_len; // keep the refusal's reason
            var back = deleted.items.len;
            while (back != 0) : (back -= 1) {
                const member = deleted.items[back - 1];
                if (self.bridge.restoreObject(member.object.link_id) != .ok) self.replay_broken = true;
                self.document.objects.insertAssumeCapacity(@min(member.index, self.document.objects.items.len), member.object);
            }
            self.status_len = message_len;
            self.bumpCascadeGenerations();
            return err;
        };
        const object = self.document.objects.orderedRemove(index);
        deleted.appendAssumeCapacity(.{ .object = object, .index = index });
        if (self.selection == link_id) self.selection = null;
        _ = self.selection_set.remove(link_id);
    }
    self.noteCascade();
    self.bumpCascadeGenerations();
    self.history.recordAssumeCapacity(self.allocator, .{ .multi_delete = .{ .deleted = deleted } }, 0);
}
```

In `deleteSelection`, build `all` so that for every member the passengers come
first (selected or not), for example by pushing each non-member passenger as
now and then appending the members sorted so that a record whose `link_with`
names another member comes before that member. Add tests for "host and its
selected passenger", "ordinary member then a span", and "ordinary member then a
shared-link-ID member"; each must leave the document, the bridge object list
and the history unchanged.

## Warnings

### WR-B01: a failed undo/redo of a multi-step command is not unwound; the entry becomes permanently stuck

**File:** `Sources/editor/core/editor.zig:2633-2659` (`.paint`, `.multi_delete`), contrast `2712-2737` (`.edit`)

**Issue:** `.edit` entries unwind the tokens already replayed when one fails
(WR-B01 in the source comments) and set `replay_broken` if even that fails.
`.paint` (one token per stroke frame) and `.multi_delete` have no such handling.
If `undoPaint(token[i])` or `restoreObject` fails after earlier members went
through, `undo` returns with the entry still on the undo stack. The bridge has
already undone the newer tokens (or restored the earlier members). The next
`undo` retries from the newest token again:

- `undoPaint` is refused ("paints are undone newest first"), or
- `restoreInto` finds the object already live,

so the same entry fails forever, and `replay_broken` is never set, so no clear
message explains why. A `multi_delete` redo fails the same way:
`removeFrom` -> `document.indexOf` is null -> `error.Failed`.

`replayComposite` rolls back the completed steps but not the partial effects of
the step that failed, so a composite containing a paint or multi-delete step
inherits the problem.

**Fix:** Give both cases the `.edit` treatment. On a mid-loop failure, replay
the already-done members in the opposite direction, and set `replay_broken`
when that fails too:

```zig
.paint => |p| {
    var done: usize = 0;
    if (forwards) {
        for (p.tokens.items) |token| {
            self.noteOutcome(self.bridge.redoPaint(token)) catch |err| {
                var back = done;
                while (back != 0) : (back -= 1) {
                    if (self.bridge.undoPaint(p.tokens.items[back - 1]) != .ok) self.replay_broken = true;
                }
                return err;
            };
            done += 1;
        }
    } else { /* mirror image: on failure redoPaint the tokens already undone */ }
},
```

and the same shape for `.multi_delete` (`removeFrom`/`restoreInto` pairs).

### WR-B02: integer overflow when rotating an object whose stored direction is near `maxInt(i32)`

**File:** `Sources/editor/core/editor.zig:752-757`, `Sources/editor/core/tools.zig:361-366`

**Issue:** `ObjectRecord.dir` is the raw `nDir` int from the map file
(`session.cpp:730`, no clamp). Both rotation paths add to it in `i32`:

- `rotateSelection`: `@mod(start + turn_units, 65536)` with `start` the object's `dir`
- Selector Q/E: `@mod(object.dir + step, full_turn)`

A map (the editor opens downloaded maps) with `nDir = 2147483647` makes the sum
overflow. That is a safety panic in Debug/ReleaseSafe and silent wrap in
ReleaseFast/ReleaseSmall, which then writes a wrong direction.

**Fix:** Widen before the mod:

```zig
const target: i32 = @intCast(@mod(@as(i64, start) + turn_units, 65536));
// tools.zig
.dir = @intCast(@mod(@as(i64, object.dir) + step, full_turn)),
```

### WR-B03: a newline in a recent path, maps folder or filter name injects keys into `mapeditor.cfg`

**File:** `Sources/editor/core/settings.zig:59-62` (`FixedPath.set`), `162-180` (`pushRecent`), `321` (`format`)

**Issue:** `setGameParameters` strips control characters because "a newline
would split the settings file's own line", but every other free-text field is
stored and written raw: `recent`, `maps_folder`, `filter_active` and
`filter_slot_N` (`format` prints them with `{s}`). On macOS and Linux a file
name may contain `\n`. Opening a map called `a\ngame_parameters=--opt x.bzm`
from an untrusted archive writes `recent=.../a` followed by
`game_parameters=--opt x.bzm`. On the next start `parse` reads the second line
as a real key. `game_parameters` is then passed as argv to the game on Test in
game, so a hostile file name controls extra game arguments. `hidden_panels` and
`scroll_speed` can be set the same way. There is no shell (T-05-11-03 holds),
but the file format's own integrity does not.

**Fix:** Drop control characters in the one shared setter, and refuse paths that
contain them in `pushRecent`:

```zig
pub fn set(self: *FixedPath, text: []const u8) void {
    var len: usize = 0;
    for (text) |byte| {
        if (byte < 0x20 or byte == 0x7f) continue;
        if (len == self.buffer.len) break;
        self.buffer[len] = byte;
        len += 1;
    }
    self.len = len;
}
```

For `recent` it is better to skip the entry entirely, since a path with
characters removed names a different file. Also skip an empty `recent=` value
in `applyKey`, which currently adds an empty entry.

### WR-B04: the once-per-session `.bak` is keyed by exact path text, so Windows spellings of one file defeat it

**File:** `Sources/editor/core/editor.zig:443-468`

**Issue:** `backed_up` is an exact-match `StringHashMap` of `path_os`. On Windows
`C:\Maps\A.bzm` and `c:\maps\a.bzm` are one file. `settings.sameOsPath` already
knows this for the recent list. Save As to one spelling and a later save through
the other in the same session makes `backed_up.contains` false.
`files.copy(path_os, backup_os)` then overwrites the `.bak` of the original
with the already-edited file, which loses the pre-session state D-19 promises to
keep.

**Fix:** Normalise the key on Windows (lower-case ASCII, or the `realPath`
result when the file exists) before `contains` and `put`:

```zig
var key_buffer: [files_mod.max_path]u8 = undefined;
const key = if (builtin.os.tag == .windows) std.ascii.lowerString(&key_buffer, path_os) else path_os;
```

### WR-B05: `Document.cancel` discards the redo stack (and the oldest undo snapshot) that `begin` already destroyed

**File:** `Sources/editor/core/rmg.zig:1277-1299`

**Issue:** `begin` clears `redo_stack` and, at `undo_depth`, drops the oldest
snapshot before it appends. `cancel` pops the new snapshot and restores `dirty`,
but it cannot bring back either of those. Many composer paths call
`begin()` first and `cancel()` when nothing changed: `setShellTileWeights`,
`setFieldSeason` to the current season, `removeFieldShells` with nothing
removed, `editField` with `changed == false`, and so on. A user who undoes an
edit and then triggers one of these no-op edits loses the redo branch. At depth
64 the oldest undo point is lost permanently as well.

**Fix:** Make `cancel` transactional. Have `begin` remember what it removed and
`cancel` restore it, or do the cheap "would this change anything" test before
calling `begin`:

```zig
// in begin(): keep the removed state for cancel
self.begun_redo = self.redo_stack; // move, do not free, until commit
self.redo_stack = .empty;
```

and in `cancel`, put `begun_redo` back and drop it only when the next edit
commits.

### WR-B06: a map in a filesystem root makes the script folder resolve to the wrong directory

**File:** `Sources/editor/core/script_file.zig:187-192` (`listBeside`), `312` (`folderUrl`)

**Issue:**

- `listBeside` trims every trailing separator while `dir_len > 1`. For
  `D:\a.bzm`, `directoryOf` gives `D:\`, which is trimmed to `D:`. That is the
  drive-relative form (the current directory on drive D), not the root. The
  Script dialog then offers the wrong `.lua` files, or none.
- `folderUrl` uses `script_path[0 .. lastIndexOfAny(...) orelse 0]`. For
  `/x.lua`, and for `D:\x.lua` (index 2 gives `D:`), the folder is "" or `D:`.
  `realPath(".")` or `realPath("D:")` then resolves the working directory and
  "Open script folder" opens the wrong place.

A map directly in a drive root is plausible on Windows.

**Fix:** Keep the separator when what remains is a root or a bare drive:

```zig
// listBeside
while (dir_len > 1 and isSep(beside[dir_len - 1]) and !(dir_len == 3 and beside[1] == ':')) dir_len -= 1;
if (dir_len == 2 and beside[1] == ':') dir_len = 3; // "D:" -> "D:\"
// folderUrl
const cut = std.mem.lastIndexOfAny(u8, script_path, "/\\") orelse 0;
const script_dir = if (cut == 0 or (cut == 2 and script_path[1] == ':')) script_path[0 .. cut + 1] else script_path[0..cut];
```

## Info

### IN-B01: `Canvas.release` leaks the undo snapshot when `addNode` fails

**File:** `Sources/editor/core/rmg.zig:2701-2706`

**Issue:** `var before = try doc.current.clone(allocator); if (try doc.current.addNode(...))`.
If `addNode` returns `error.OutOfMemory`, the `try` leaves `before` undeleted.
Only the OOM path leaks, but the sibling `.link` arm (`2736-2737`) does have an
`errdefer before.deinit`.

**Fix:** Add `errdefer before.deinit(allocator);` after the clone, and drop it
once `pushUndo` has taken ownership, as the link arm does.

### IN-B02: settings values do not always round-trip

**File:** `Sources/editor/core/settings.zig:287-291`

**Issue:** `parse` trims spaces and tabs from every value, and `nameValid`
allows a leading or trailing space in a filter name. A filter or path with edge
whitespace is written as is, read back trimmed, and no longer matches
(`fire_range_filter`, `filter_active`, `filter_slot_N`, `recent`). Related:
`FixedPath.set` truncates at 1024 bytes in the middle of a UTF-8 sequence.

**Fix:** Reject edge whitespace in `nameValid`, or escape it when writing.
Truncate on a code point boundary.

### IN-B03: the Heights tool has no `reset`/cancel, so an open stroke survives a map close

**File:** `Sources/editor/core/tools_heights.zig:33-101`, `Sources/editor/app/view.zig:381-392`

**Issue:** Every other stateful tool has `reset()` and `closeMap` calls it. The
Heights tool's `gesture`, `stroke_start` and click reference are never reset. A
stroke that is open when the map closes (or whose release arrives with a stale
button mask) leaves `gesture != 0`. The next `press` then skips its "fresh
stroke" branch, so `stroke_start` stays false and the click reference is stale.

**Fix:** Add `pub fn reset(self: *Heights) void { self.gesture = 0; self.stroke_start = false; }`
and call it from `closeMap`, `showMap` and on tool change.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

# Area C


## Phase 5, Area C: Code Review Report (C++ engine side)

**Reviewed:** 2026-10-03
**Depth:** standard (changed hunks read in full, callers and callees traced)
**Files Reviewed:** 38
**Status:** issues_found

No `CLAUDE.md` exists in the worktree root, so no project-specific rules were applied.

## Summary

The C ABI in `bridge.cpp` is carefully hardened: capacity/null/negative checks, two-pass reads, `Guarded` catch-all, and `memchr`/`strnlen` termination checks are nearly uniform. I found no buffer overflow on a normal call path.

The real defects sit one layer down, in the session logic behind the ABI. Two are serious:

- A mutating entry point that reports REFUSED after it has already applied and logged its edit. This permanently wedges the undo stack.
- An unbounded recursion in the object delete path, reachable through an editor-creatable link cycle.

The game-side changes were traced separately.

- **Script path / checksum.** `fmtMapScriptPath.h` is consistent across `GameCreation`, `iMissionInternal`, and `CommandsHistory`. `ExpandOnLoad` followed by `BesideMap` is idempotent. The checksum hashes the script file's bytes, not the path string. For legacy values (any backslash, or absolute) the behaviour is unchanged. For new relative values both the checksum and the load read `<map folder>/<name>.lua`. Server and client agree, because the host sends the same `BesideMap(...)+".lua"` name and `WriteReceivedFile` validates it. I found no new desync. The pre-existing asymmetry remains for legacy values: the checksum reads `maps\Name.lua` from the storage root while the game loads `<folder>\Name.lua`.
- **Other game-side changes.** `BK_MAP_TRACE` is env-gated. The railroad `>= 2` guard, the `SpriteVisObj::IsHit` rectangle unscale, and the `RemoveRoad` return value are behaviourally sound. The `GFXGPU` wireframe state is a pipeline-key bit (48) that does not collide with the existing bits (up to 47).
- **Padding.** The named-padding fixes for `SLinkInfo` and `SVectorStripeObjectPoint` are correct. Their `static_assert` sizes match the layouts (12 and 40). The sibling struct `SVertexAltitude` was not fixed (see WR-C03).

## Critical Issues

### CR-C01: `BkEditorApplyField` answers REFUSED after the edit was applied and logged, and drops the undo token

**File:** `Sources/src/EditorBridge/bridge.cpp:4867-4883`
**Issue:** `ApplyFieldInSession` has already modified both map copies and the engine, and has already called `LogEdit`, which pushed an `SFieldEdit` on `edits` and `appliedEdits`. The bridge then does:

```cpp
if ( !ApplyFieldInSession( ..., &nToken ) ) return ...;
*pnReportCount = int( report.size() );
...
if ( int( report.size() ) > nReportCapacity )
    return BK_EDITOR_REFUSED;      // <- edit stays applied
*pnToken = nToken;                 // <- never reached
```

When the caller's `out_report` capacity is below the number of objects the shells produced:

- The caller is told REFUSED, which `bridge.h:2082` documents as "changes nothing".
- The map has in fact been filled.
- The caller never receives the token.
- The orphan token stays on top of `appliedEdits`. `UndoEditInSession` requires `appliedEdits.back() == nToken` ("edits are undone newest first"), so every earlier edit becomes un-undoable.

The two-pass "ask the size first" convention cannot work for this call: only `check_passability_only` is side-effect free, and the object count is unknown until the fill has run. Any real field with `place_objects` over a dense area can exceed a fixed caller capacity.

**Fix:** Always return the token once the edit is logged, and treat a short report buffer as a truncation, not a refusal.

```cpp
*pnReportCount = int( report.size() );
const int nWrite = Min( int( report.size() ), nReportCapacity );
// ... copy nWrite rows ...
*pnToken = nToken;                       // before any non-OK return
if ( int( report.size() ) > nReportCapacity )
{
    pSession->szMessage = NStr::Format( "the report holds %d objects, room was given for %d (the field WAS applied; token %d)", ... );
    return BK_EDITOR_OK;                 // or a distinct status that still carries the token
}
return BK_EDITOR_OK;
```

If a REFUSED answer is wanted, run the report-only pass (`bCheckPassabilityOnly`) first inside the bridge to size the buffer, and refuse before applying.

### CR-C02: `DeleteObjectFromSession` recurses forever on an `nLinkWith` cycle, and `SetLink` can create the cycle

**File:** `Sources/src/EditorBridge/session.cpp:1163-1193` (recursion); `session.cpp:1787-1798` and `Sources/src/MapFile/MapRecords.cpp:395-402` (cycle creation)
**Issue:** The M3 passenger cascade collects every record whose `nLinkWith == nLinkID` and deletes each through `DeleteObjectFromSession`. Nothing tracks visited IDs, and the object being deleted stays in the map while its passengers are processed. Take A with `A.nLinkWith = B` and `B.nLinkWith = A`:

1. Delete A: the passengers of A are `{B}`. Delete B.
2. Delete B: the passengers of B are `{A}`, which is still present. Delete A.
3. Step 1 repeats until the stack overflows.

A stack overflow is not caught by `Guarded`'s `catch (...)`, so the editor process dies and unsaved work is lost.

The cycle is creatable in-editor. `CanLinkInSession`'s "train fallback" (`session.cpp:1787-1798`) accepts train car to train car for any pair, with no direction or cycle check. `NMapRecords::SetObjectLink` only rejects the self-link (`nLinkWith == nLinkID`, `MapRecords.cpp:397`) and says it does so because a cycle would make the game's loaders chase it. A map file can also contain such a cycle.

**Fix:** Track a visited set in the cascade and refuse the delete if the walk meets the object being deleted again.

```cpp
// in DeleteObjectFromSession, before collecting passengers:
std::set<int> chain; chain.insert( nLinkID );
if ( !CollectPassengers( pSession, nLinkID, &chain ) )   // false when a passenger names a host already in the chain
{
    pSession->szMessage = "objects link to each other in a cycle; unlink one first";
    if ( pbRefused ) *pbRefused = true;
    return false;
}
```

Also reject a cycle at link time. In `SetLinkInSession` and `NMapRecords::SetObjectLink`, walk `nLinkWith` from the target and refuse if it reaches the source.

## Warnings

### WR-C01: Deleting the single object that carries link ID 0 deletes every unlinked object

**File:** `Sources/src/EditorBridge/session.cpp:1163-1173`
**Issue:** `RefuseSharedLinkID` only refuses when two or more records share the ID, so a map with exactly one link-ID-0 object passes it. The passenger scan then runs with `nLinkID == 0`:

```cpp
if ( rObject.link.nLinkID != nLinkID && rObject.link.nLinkWith == nLinkID )   // nLinkWith == 0 == "linked with nothing"
```

0 is exactly the value every unlinked object carries (`SLinkInfo` default, and what `AddObjectToSession` writes). So every other unlinked object becomes a "passenger" and is deleted with it. The overlay layer guards 0 explicitly (`WhyRefused`, `FindReferences`, `RemoveFromStartCommands`). This layer does not. It is undoable via the tombstones, but it is a map-wide wipe from a single click.

**Fix:** Skip the passenger scan for IDs that are not references.

```cpp
if ( nLinkID > 0 )
{ /* collect passengers */ }
```

### WR-C02: `BkEditorMoveObject` / `BkEditorPlaceObject` / `BkEditorAddObject` accept NaN, infinity and out-of-range coordinates, and the engine round-trip check cannot see it

**File:** `Sources/src/EditorBridge/bridge.cpp:599-638` (no finite check); `Sources/src/EditorBridge/session.cpp:659-669, 1076-1099`
**Issue:** Every newer entry point validates `isfinite` (ghost, batch move, areas, VSO). These three do not. `PlaceObjectInSession` writes the requested position into both map copies first (`NMapOverlay::MoveObject`). It then asks the engine through `ToEngineCoord`, which converts to `short`, undefined for NaN or anything outside about +/-32768.5. It then verifies with `EngineIsAt`, which applies the same conversion to the request. Both sides collapse to the same garbage value (0 on x86, saturated on ARM64), so the verification can pass.

If the collapsed position is a legal spot, the engine accepts it and the snapshot keeps the NaN or 1e9 position. The next save writes it. `nDir` has the same shape: the snapshot keeps the raw `int` while the engine compares `WORD( nDir )`.

**Fix:** Validate at the ABI and in the session.

```cpp
if ( !std::isfinite( x ) || !std::isfinite( y ) || std::fabs( x ) > 1.0e6f || std::fabs( y ) > 1.0e6f ) return BK_EDITOR_BAD_ARGUMENT;
// and in PlaceObjectInSession: reject vPos outside [0, mapWidthAI) x [0, mapHeightAI), and nDir outside 0..65535
```

### WR-C03: `SVertexAltitude` still has unnamed padding; generated and new maps write heap bytes into the file

**File:** `Sources/src/Formats/fmtMap.h:239-250` (also `session.cpp:307-312` for the editor-side patch)
**Issue:** This diff fixes the same bug for `SLinkInfo` (`fmtMap.h`) and `SVectorStripeObjectPoint` (`fmtVSO.h`), and cites "broken window 6/8". The sibling raw-saved struct `SVertexAltitude` is `{ float fHeight; BYTE shade; }`, with three unnamed padding bytes at offsets 5-7. It is saved raw through `CArray2D` (`saver.Add( 9, &altitudes )`). `CArray2D::Create` is `new T[n]`, and the constructor leaves the padding indeterminate. `CArray2D::Copy` is a member-wise assignment, so a copy also does not carry the padding.

The editor works around this only for the open and new paths: `InstallMapInSession` memsets the padding at install. Maps built by `CMapInfo::CreateRandomMap` (editor "Create Random Map" and the shipped game's random missions) go straight to the file with whatever the allocator left. The same seed therefore does not give a byte-identical `.bzm`, which is the property the VSO-point fix was made for. The magic number `sizeof( SVertexAltitude ) - 5` at `session.cpp:311` also silently depends on that exact layout.

**Fix:** Apply the same pattern as the other two structs, which removes the install-time workaround.

```cpp
BYTE shade;
BYTE cReserved[3];                                   // always zero, the map format's own padding
SVertexAltitude() : fHeight( 0 ), shade( 255 ), cReserved() {}
};
static_assert( sizeof( SVertexAltitude ) == 8, "saved as raw bytes" );
```

### WR-C04: `UpdateMapInSession` returns false mid-pipeline after mutating the map, with nothing logged and nothing restored

**File:** `Sources/src/EditorBridge/session_terrain.cpp:703-708, 717-722`
**Issue:** The composite captures its "before" state up front into `edit`. Several failure exits then `return false` without using it:

- If `UpdateTerrainCrosses` succeeds on the snapshot and fails on the working copy, the snapshot's crosses are changed.
- If `UpdateTerrainShades` fails on the second copy, the first copy's shades are changed.
- The engine-side `UpdateAllHeights` and `UpdateTerrain` have already run.

`edit` is destroyed unlogged, so the snapshot is changed with no undo entry, and the status is FAILED rather than a clean refusal. The sibling composites (`ApplyAltitudesInSession`, `PaintIntoSession`, `ApplyFieldInSession`) all roll back on these paths.

**Fix:** On each failure exit after step 3, put the captured state back before returning, as the altitudes path does:

```cpp
PutRegionBack( pSession, pEdit->tilesBefore );
PutAltitudeEditBack( pSession, pEdit->altitudesBefore );
PutVsoZBack( pSession, pEdit->vsoSnapshotBefore, pEdit->vsoWorkingBefore, pEdit->vsoEngineBefore );
```

### WR-C05: `BkEditorMoveObjects` dereferences `pnLinkIDs` before it checks for null

**File:** `Sources/src/EditorBridge/bridge.cpp:796-799`
**Issue:** The duplicate-ID loop reads `pnLinkIDs[i]` and `pnLinkIDs[j]` for any `nCount >= 2`. The null/`nCount <= 0` check lives later, in `MoveObjectsInSession` (`session.cpp:2078`), after the loop. `BkEditorMoveObjects(s, NULL, 2, ...)` crashes in the ABI layer instead of answering BAD_ARGUMENT. Every sibling checks this first.

**Fix:**

```cpp
if ( nCount < 0 || ( nCount > 0 && pnLinkIDs == 0 ) ) return BK_EDITOR_BAD_ARGUMENT;
```

### WR-C06: The user filter file is written non-atomically and with incomplete escaping

**File:** `Sources/src/EditorBridge/filters.cpp:100-156`
**Issue:** `WriteUserFilterMap` truncates and rewrites `filter.xml` in place. A crash or full disk mid-write leaves a truncated file. `ReadUserFilterMap` treats a malformed file as empty ("never an error"), so all of the user's filters silently vanish. `WriteXmlText` escapes only `& < >`; names and words from the ABI are accepted with any bytes (only length is checked), so a control character (below 0x20) produces XML the engine's reader rejects. That has the same effect: the whole user file reads empty. The other editor writers (map save, RMG records) use temp-write plus read-back verification.

**Fix:** Write to `filter.xml.tmp`, then rename over the original. Reject or strip control characters in `BkEditorSaveObjectFilters` as `session_rmg.cpp:IsPlainText` already does for RMG records.

### WR-C07: Several ABI structs have their char arrays read as C strings with no termination check

**File:** `Sources/src/EditorBridge/bridge.cpp:1080, 1093` (`szModFolder`, `szName` of `BkEditorNewMapParams`), `bridge.cpp:4845, 4862` (`field_set`, `object_filter` of `BkEditorFieldApplyParams`)
**Issue:** `std::string x = pParams->field_set;` and the like run to a NUL that a caller may never have written. The neighbouring entry points (`BkEditorCreateRandomMap`, `BkEditorSetScriptFile`, units, RMG records) all use `strnlen`/`memchr` first. These four do not. `ApplyField` is the only one in its group that skips it, and `szName` is passed on as a raw `const char*` to `NewMapInSession`. The struct comment says "always terminated", which is a contract, not a check.

**Fix:** Add `memchr( field, 0, sizeof field ) == 0 -> BK_EDITOR_BAD_ARGUMENT` for each array before it is read.

### WR-C08: `SetSessionUnitCreation` does not roll the snapshot back when the working copy refuses

**File:** `Sources/src/EditorBridge/session_records.cpp:2121-2128`
**Issue:** `PutUnitCreation( &snapshot, ... )` is applied. If `PutUnitCreation( &working, ... )` returns false, the function returns false and leaves the snapshot already changed. Every other collection setter in this file restores the snapshot on that path (`SetSessionGroup`, `SetScriptAreaInSession`, `SetStartCommandInSession`, and so on).

**Fix:**

```cpp
if ( !NMapRecords::PutUnitCreation( &pSession->working, nPlayer, wanted, rRecord.slot_count ) )
{
    NMapRecords::PutUnitCreation( &pSession->snapshot, nPlayer, current, nSlots );
    return false;
}
```

## Info

### IN-C01: `IsMapTraceOn()` is copy-pasted into four translation units, and a comment is now orphaned from its function

**File:** `AILogicInternal.cpp:463`, `GeneralInternal.cpp:449`, `Scripts.cpp:106`, `iMissionInternal.cpp:~857`
**Issue:** The same four-line `static bool IsMapTraceOn()` exists in four files. In `iMissionInternal.cpp` it was inserted between the long comment about the camera-anchor rule and the `GetPlayerUnitsCenter` function that comment documents, so the comment no longer sits above its function.
**Fix:** Move one inline definition into a shared header (for example `Misc/` or `Main/`), and put the `IsMapTraceOn` block above the camera-anchor comment.

### IN-C02: `BkEditorSetMapType` stores any `int` into the map file

**File:** `Sources/src/EditorBridge/bridge.cpp:2830-2846`
**Issue:** `CMapInfo::GAME_TYPE` defines 0..2, `TYPE_COUNT`, and `TYPE_NONE`. `BkEditorSetAttackingSide` next to it validates; this does not. The shipped game degrades gracefully (`CUIConsts::GetMapTypeString` falls to the "unknown" string), so this is a hygiene gap, not a crash.
**Fix:** Reject values outside the enum.

### IN-C03: Random-map output is not reproducible across libc implementations

**File:** `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp:913-926` with `Formats/fmtTerrain.h:87` (`STileTypeDesc::GetMapsIndex`)
**Issue:** The seed block deliberately seeds `srand( nLegacySeed )` so a stored seed regenerates the same map. But tile variants come from the C library's `rand() % 10000`. That sequence differs between MSVC UCRT (`RAND_MAX` 32767) and macOS/glibc. A `.seed` from a Windows run therefore does not recreate the same `.bzm` on macOS. `MapInfo_StaticMethods.cpp:1944-2000` also does `rand() * n / ( RAND_MAX + 1 )`, which is signed overflow (UB) where `RAND_MAX == INT_MAX`; that file is outside this diff.
**Fix:** Route `GetMapsIndex` and those callers through the engine's own `Random()` or `NWin32Random`, which the seed block already controls.

### IN-C04: Field-placed objects get `bIntention = true`, contradicting the comment above it

**File:** `Sources/src/EditorBridge/session_fields.cpp:137-142`
**Issue:** The comment says "The palette add's own rules (AddObjectToSession)", but `AddObjectToSession` leaves `bIntention` false (`NMapOverlay::AddObject` sets it false), while this sets it true. The game reads it only when `nLinkWith > 0`, so it is harmless today, but it makes the two add paths write different bytes for the same kind of object.
**Fix:** Match `AddObjectToSession`, or document why the Fields tool differs.

### IN-C05: `IsUnderInstalledData` fails open

**File:** `Sources/src/EditorBridge/bridge.cpp:1844-1861`
**Issue:** If `weakly_canonical` reports an error for either path, the function returns false ("not under Data"). `BkEditorCreateMiniMapImage` then deletes and rewrites eight picture files beside the given map path. The guard exists to keep the shipped Data folder from being written, so on error it should refuse.
**Fix:** `if ( error ) return true;`.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_

# Area D


## Phase 5, Area D: Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard (diff against 78b96aa9e, plus the surrounding code each change calls)
**Files Reviewed:** 14

## Summary

There is no `CLAUDE.md` in the repo root, so no project rules were applied.

**Clean checks:**
- No stale reference to the deleted `Sources/src/MapEditor` or the MFC `MapEditor.exe` remains in `build.zig`, the CI workflow, `.vscode`, `tools/zig/stage.zig`, `game_install.ps1`, `check_no_gamespy_runtime.ps1` or `A7.sln`. The remaining `MapEditor.exe` mentions (CI packaging checks, `stage_test.zig`) are the new Zig editor, which sits at the package root.
- Every EditorBridge and MapFile `.cpp` on disk is listed in `build.zig`. The 12 bridge sources are all present.
- `addEditorBridgeTest` runs before `addMapEditor`, so the `editor-craft-fixtures` lookup cannot panic.
- The scenario schedules in `auto_m2_entries` and `auto_m3_entries` have strictly ascending frames. Every `compare=` has a matching `shot=`. No `do=`/`expect=` argument exceeds the 64-character limit.
- The new `Run` steps in `addEngineHostedTool` do not need `has_side_effects`. I read `std/Build/Step/Run.zig`, and a `Run` with no output args already counts as having side effects, so it never caches.

Everything found is a test-quality or hermeticity weakness. Nothing is a correctness or security defect in shipped code.

## Warnings

### WR-D01: Most M3 scenario `file_save_bzm` paths climb two levels, not four, and land outside the scratch folder

**File:** `build.zig:6688, 6752, 6945, 6957, 7121` (compare `build.zig:6981`)
**Issue:** The scenario passes `do=file_save_bzm:../../local-test/map-editor-m3-auto/<name>` and says the path is relative to the staged game root. That root is `stage_root = zig-out/game/<os>/<arch>/<variant>`, five levels under the repo root. Two `..` reach `zig-out/game/<os>/`, not `zig-out/`.
- The minimap entry at line 6981 uses four `..`, which does reach `zig-out/local-test`.
- The five other entries therefore write to `zig-out/game/<os>/local-test/map-editor-m3-auto/`.
- This is already on disk. `zig-out/game/macos/local-test/map-editor-m3-auto/` holds `m3.bzm`, `m3-checked.bzm`, `m3-layers.bzm` and several `.bak` files, while the documented scratch directory `zig-out/local-test/map-editor-m3-auto` is where `BK_EDITOR_AUTO_DIR` points.
- Maps from one scenario end up in two directories.
- No cleanup step covers the stray directory, so `.bak` files and stale maps accumulate between runs.
- It also breaks the rule at `build.zig:7894` that nothing but `zig-out/local-test` is written outside the installation.
- The `m2` scenario derives its prefix from `stage_root` (`auto_m2_fixture`, `build.zig`, near the M2 scenario). The `m3` entries hand-wrote the prefix.

**Fix:** Build the prefix once from `stage_root`, as `auto_m2_fixture` does, and format it into all six entries. The prefix has to climb only to `zig-out/`, which is `stage_root`'s slash count (four `..`), so the argument stays under the 64-character limit (the 4-up minimap entry is 55 characters).

```zig
const auto_m3_up = up: {
    var up: std.ArrayListUnmanaged(u8) = .empty;
    for (0..std.mem.count(u8, stage_root, "/")) |_| up.appendSlice(b.allocator, "../") catch @panic("OOM");
    break :up up.items;
};
// ...
b.fmt("32:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3.bzm", .{auto_m3_up}),
```

### WR-D02: On Windows the M3 scenario and the M3 game-reads-it step write into the real user profile

**File:** `build.zig:7504-7507` (`auto_m3_run`), `build.zig:7566-7570` (`game_reads_it_m3_run`)
**Issue:** Both steps isolate the user root with `XDG_DATA_HOME`. `Sources/src/Platform/Paths.cpp` reads `XDG_DATA_HOME` only in the `#if !defined(_WIN32)` branch (`preferenceRoot`). On Windows, `Initialize()` uses `SDL_GetPrefPath("Nival", "Blitzkrieg")` and ignores the variable.
- The `game_reads_it_m3` comment admits this ("the same files are rewritten identically each run").
- The `auto_m3` comment says only that the generated random map and exported lists "land here and not in the person's own maps and logs folders". On Windows they land in `%APPDATA%\Nival\Blitzkrieg`, the developer's real profile.
- A developer's own user maps and RMG records can be overwritten or polluted, and a failed run leaves files behind.

**Fix:** Give `Paths.cpp` an override honoured on every OS, applied before `SDL_GetPrefPath`. Then set it in both steps.

```cpp
// Initialize(), _WIN32 branch
if (const char *root = std::getenv("BK_USER_ROOT"); root && *root) gUser = ensureSeparator(root);
else if (char *preference = SDL_GetPrefPath("Nival", "Blitzkrieg")) { gUser = ensureSeparator(preference); SDL_free(preference); }
```

```zig
auto_m3_run.setEnvironmentVariable("BK_USER_ROOT", b.pathFromRoot("zig-out/local-test/map-editor-m3-auto-user"));
```

If a runtime change is out of scope, make both steps refuse to run on Windows, or at least state the Windows leak in the `auto_m3_run` comment.

### WR-D03: Every engine-hosted C++ tool exits 0 when it skips, and the CI steps cannot tell a skip from a pass

**File:** `tools/zig/composer_roundtrip_test.cpp:418-423, 436-441, 453-459`, `tools/zig/rmg_determinism_test.cpp:210-216, 230-236, 247-253`, `tools/zig/editor_bridge_test.cpp:11509, 11564, 11597`, `.github/workflows/cross-platform.yml:324-326, 366-374, 666-675`
**Issue:** "no video driver", "no staged game" and `BK_EDITOR_NO_DEVICE` each print `skipped` and `return 0`. The workflow comment (line 324) says the tier "skips honestly where there is none".
- A runner whose GPU probe changes, a Windows runner image update that drops the software device, or a wrong root argument keeps all of these steps green while checking nothing.
- The new "RMG composer round trip" and "RMG determinism" steps (Windows and macOS) are the only coverage for the composer and seed contracts in this phase.
- The `GPU device probe` step is `continue-on-error: true`, so nothing correlates its result with the later skips.

**Fix:** Add an environment switch, for example `BK_REQUIRE_ENGINE=1`, that turns each skip branch into `printf("FAIL: ..."); return 1;`. Set it in the Windows and macOS test steps:

```yaml
- name: RMG composer round trip
  env: { BK_REQUIRE_ENGINE: "1" }
```

Alternatively, make the step assert that the output contains `PASS`.

### WR-D04: The composer round trip advertises 43/102/404/27 records but asserts only "not empty"

**File:** `tools/zig/composer_roundtrip_test.cpp:477` (header comment lines 1-5)
**Issue:** The test claims every shipped template, graph, container and field set is round-tripped. It asserts only `!containers.empty() && !graphs.empty() && !fieldSets.empty() && !templates.empty()`. The loops and the final `bAll` compare against the number the scan found, so a scan that returns a subset passes: a `BkEditorListRmg` regression, a changed `ListRmg` filter, or a sparse checkout that drops a subfolder under `Data/Scenarios`. I confirmed the shipped counts on disk are 43 templates, 102 graphs, 404 containers and 27 field sets.
**Fix:** Pin lower bounds:

```cpp
Check( templates.size() >= 43 && graphs.size() >= 102 && containers.size() >= 404 && fieldSets.size() >= 27,
       NStr::Format( "the scan finds the shipped records (%d/%d/%d/%d)", ... ) );
```

### WR-D05: The short-railroad game crash guard, the M3 editor scenario and the M3 bridge slices run in no CI job

**File:** `build.zig:7550-7579` (`map-editor-game-reads-it-m3`), `build.zig:7512-7518` (`map-editor-m3-auto`), `.github/workflows/cross-platform.yml:316-336, 640-660`
**Issue:** The real-Game load of a railroad with fewer than two control points (D-33, the `CRailroadGraphConstructor` crash) is only a local step. The workflow does not call `map-editor-game-reads-it-m3`, `map-editor-game-reads-it-m2` or `map-editor-m3-auto`. The only `test-map-editor-auto` in CI is the parser unit tier. The crash fix therefore has no regression gate: reintroducing the crash leaves every CI job green. The M3 scenario's 315 `expect=` predicates also never run in CI. The `--m3-*-only` and `--m2-sweep` bridge slices are explicitly local, but the full `test-editor-bridge` run covers the same tests.
**Fix:** Add `map-editor-game-reads-it-m3` (and ideally `map-editor-m3-auto`) as steps in the Windows and macOS jobs, after `Engine tier`. It needs `editor-craft-fixtures`, which the step already depends on. If local-only is a deliberate decision, record it in the workflow next to the "Engine tier" comment so the gap is visible.

## Info

### IN-D01: Byte-identical checks accept two empty files

**File:** `tools/zig/rmg_determinism_test.cpp:82-92, 273-275, 296-300, 326, 349`; `tools/zig/map_file_test.cpp:568-606`
**Issue:** `FirstDifference` returns `-1` (identical) when both files are zero bytes, and the determinism test never checks that a generated map is non-empty. `FilesAreIdentical` in `map_file_test.cpp` also says "identical" for two empty files (`nSize == 0 ||`). A generator that wrote zero bytes twice would pass. This is unlikely because `Generate` also checks `BkEditorCreateRandomMap` and `copy_file`, but the guard is cheap.
**Fix:** In `FirstDifference` return `-2` if `left.empty() || right.empty()`, and have `FilesAreIdentical` fail for `nSize == 0`.

### IN-D02: `TestM3FieldRegion` writes scratch maps into the working directory

**File:** `tools/zig/map_file_test.cpp:3734-3736`
**Issue:** `fields-region-edited.bzm`, `-undone.bzm` and `-original.bzm` are written relative to the cwd, which is the repo root for this run (`setCwd(b.path("."))`). Every other test here writes under `zig-out/local-test/`. They are removed at the end of the block, but a crash between write and `remove` leaves untracked `.bzm` files in the repo root.
**Fix:** Use `"zig-out\\local-test\\fields-region-*.bzm"`, as the neighbouring tests do.

### IN-D03: The new Windows `os_version_min` override raises the floor for every Windows output

**File:** `build.zig:808-812`
**Issue:** The comment justifies the override by the editor's AF_UNIX socket. The code applies it to every Windows target, including `x86_64-windows-gnu` and the shipped `Game.exe`, whenever `-Dtarget` does not pin a version. In practice the game's minimum becomes Windows 10 1803. That is probably acceptable, but it is a project-wide decision made in a build-script hunk, and the only record is the comment. On a Windows host with no `-Dtarget`, setting `os_version_min` also turns the native query into a non-native one (`Query.isNativeOs` requires `os_version_min == null`), so the build is no longer treated as the native OS. I did not run a Windows build to see if that matters.
**Fix:** Say in the README or release notes that Windows 10 1803 is the floor. If only the editor needs it, scope the override to the `MapEditor` and test modules instead of the global target.

### IN-D04: The M3 scenario takes 40 screenshots and compares none of them

**File:** `build.zig:6668-7500` (`auto_m3_entries`)
**Issue:** The M2 schedule pairs each `shot=` with `compare=` (16 of 16). The M3 schedule has 40 `shot=` entries and no `compare=`. Its 315 `expect=` predicates are real assertions, but the rendering regressions that M2's `compare=` entries catch (layers, minimap, placement ghost) are not asserted in M3. If the screenshots are meant for manual review only, that is fine, but it should be stated.
**Fix:** Add `compare=` for the stable shots (grid, passability, minimap, ghost), or note in the schedule header that M3's pictures are review artefacts and not assertions.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
