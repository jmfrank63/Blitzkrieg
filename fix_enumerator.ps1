$f = Get-Content -Path 'Sources\src\StreamIOZig\streamio.zig' -Raw
$old = @"
pub export fn bk_storage_enumerator_create(handle: ?*anyopaque) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    const enumerator = allocator.create(Enumerator) catch return null;
    enumerator.* = .{};
    collectFiles(storage, enumerator, "");
    return enumerator;
}
"@
$new = @"
fn collectArchiveFiles(storage: *const Storage, enumerator: *Enumerator) void {
    for (storage.archives.items) |*loaded| {
        for (loaded.archive.entries) |*entry| {
            const name_copy = allocator.dupeZ(u8, entry.name) catch continue;
            enumerator.names.append(allocator, name_copy) catch allocator.free(name_copy);
        }
    }
    var index = storage.overlays.items.len;
    while (index > 0) {
        index -= 1;
        collectArchiveFiles(storage.overlays.items[index].storage, enumerator);
    }
}

pub export fn bk_storage_enumerator_create(handle: ?*anyopaque) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    const enumerator = allocator.create(Enumerator) catch return null;
    enumerator.* = .{};
    collectFiles(storage, enumerator, "");
    collectArchiveFiles(storage, enumerator);
    return enumerator;
}
"@
$f = $f.Replace($old, $new)
Set-Content -Path 'Sources\src\StreamIOZig\streamio.zig' -Value $f -NoNewline