//! Spell check (D-11) without the speller: `CSpellChecker`'s word and
//! delimiter rules (Sources/src/ELK/SpellChecker.cpp), the pass over a
//! translation, the user dictionary `custom.dic` and the dictionary folder's
//! languages. The speller is an `Engine`, the seam the app fills with the
//! vendored Hunspell (app/hunspell.zig), so this stays std-only and its
//! tests need no dictionary.
//!
//! Texts are the editor's UTF-8. The delimiters are the MFC tables' ASCII
//! bytes, as the MBCS build compared them, so a byte of a multi-byte
//! character is never one and a Cyrillic word stays whole.
const std = @import("std");
const database = @import("database.zig");

/// `CSpellChecker::SPELLING_WORD_DELIMITERS`: what ends a word to check.
pub const spelling_delimiters = " \t\n\r`~!@#$%^&*()-_=+\\|[{]};:'\",<.>/?";
/// `CSpellChecker::WORD_DELIMITERS`: the blanks `GetTextCounts` ends a word at.
pub const word_delimiters = " \t\n\r";
/// `CSpellChecker::IGNORE_SYMBOLS`: not counted as symbols.
pub const ignore_symbols = "\n\r";

/// The user dictionary's name beside elk.cfg; a legacy `CUSTOM.DIC` copied
/// there is found without regard to case.
pub const user_dictionary_name = "custom.dic";

pub fn isSpellingDelimiter(byte: u8) bool {
    return std.mem.indexOfScalar(u8, spelling_delimiters, byte) != null;
}

/// A word of a text: its byte offset and length.
pub const Word = struct {
    offset: usize,
    len: usize,

    pub fn of(self: Word, text: []const u8) []const u8 {
        return text[self.offset..][0..self.len];
    }
};

/// `GetWord` from `from` on: the run of spelling delimiters skipped
/// (`RemoveFirstDelimiter`), then the word up to the next delimiter or the
/// end. Null when only delimiters are left.
pub fn nextWord(text: []const u8, from: usize) ?Word {
    var start = @min(from, text.len);
    while (start < text.len and isSpellingDelimiter(text[start])) start += 1;
    if (start == text.len) return null;
    var end = start;
    while (end < text.len and !isSpellingDelimiter(text[end])) end += 1;
    return .{ .offset = start, .len = end - start };
}

/// The speller behind the seam: whether it knows a word, its suggestions,
/// and a word the user added, taken for the rest of the session.
pub const Engine = struct {
    context: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        spell: *const fn (context: *anyopaque, word: []const u8) bool,
        /// The suggestions, best first, each allocated in `allocator`.
        suggest: *const fn (context: *anyopaque, allocator: std.mem.Allocator, word: []const u8) error{OutOfMemory}![]const []const u8,
        add: *const fn (context: *anyopaque, word: []const u8) void,
    };

    pub fn spell(self: Engine, word: []const u8) bool {
        return self.vtable.spell(self.context, word);
    }

    pub fn suggest(self: Engine, allocator: std.mem.Allocator, word: []const u8) error{OutOfMemory}![]const []const u8 {
        return self.vtable.suggest(self.context, allocator, word);
    }

    pub fn add(self: Engine, word: []const u8) void {
        self.vtable.add(self.context, word);
    }
};

/// The words of `text` neither the user dictionary nor the engine accepts,
/// in the text's order, each place it occurs: `OnToolsSpelling`'s walk
/// taken to the end of the text at once.
pub fn misspellings(allocator: std.mem.Allocator, engine: Engine, user: *const UserDictionary, text: []const u8) ![]Word {
    var found: std.ArrayListUnmanaged(Word) = .empty;
    errdefer found.deinit(allocator);
    var at: usize = 0;
    while (nextWord(text, at)) |word| : (at = word.offset + word.len) {
        const spelt = word.of(text);
        if (user.contains(spelt) or engine.spell(spelt)) continue;
        try found.append(allocator, word);
    }
    return found.toOwnedSlice(allocator);
}

/// `custom.dic`: UTF-8, one word per line, `#` starting a comment line, so
/// the legacy `CUSTOM.DIC` and its `#LID 1049` header read as no words.
/// Lines that are not UTF-8 (a legacy code page's words) are skipped.
pub const UserDictionary = struct {
    words: std.StringArrayHashMapUnmanaged(void) = .empty,

    pub fn deinit(self: *UserDictionary, gpa: std.mem.Allocator) void {
        for (self.words.keys()) |word| gpa.free(word);
        self.words.deinit(gpa);
    }

    pub fn parse(gpa: std.mem.Allocator, bytes: []const u8) !UserDictionary {
        var dictionary: UserDictionary = .{};
        errdefer dictionary.deinit(gpa);
        const text = if (std.mem.startsWith(u8, bytes, "\xEF\xBB\xBF")) bytes[3..] else bytes;
        var lines = std.mem.splitScalar(u8, text, '\n');
        while (lines.next()) |line| {
            const word = std.mem.trim(u8, line, " \t\r");
            if (word.len == 0 or word[0] == '#' or !std.unicode.utf8ValidateSlice(word)) continue;
            _ = try dictionary.add(gpa, word);
        }
        return dictionary;
    }

    /// The file at `path`; none there is an empty dictionary.
    pub fn load(gpa: std.mem.Allocator, io: std.Io, path: []const u8) !UserDictionary {
        const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(16 * 1024 * 1024)) catch |err| switch (err) {
            error.FileNotFound => return .{},
            else => return err,
        };
        defer gpa.free(bytes);
        return parse(gpa, bytes);
    }

    pub fn contains(self: *const UserDictionary, word: []const u8) bool {
        return self.words.contains(word);
    }

    /// False when the word was there already.
    pub fn add(self: *UserDictionary, gpa: std.mem.Allocator, word: []const u8) !bool {
        if (self.words.contains(word)) return false;
        const owned = try gpa.dupe(u8, word);
        errdefer gpa.free(owned);
        try self.words.put(gpa, owned, {});
        return true;
    }
};

/// Adds `word` to the file at `path` as a line of its own, after whatever
/// the file holds (comments and the legacy header kept) and with its line
/// ends, written through a temporary file like every ELK write.
pub fn appendUserWord(gpa: std.mem.Allocator, io: std.Io, path: []const u8, word: []const u8) !void {
    const old = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(16 * 1024 * 1024)) catch |err| switch (err) {
        error.FileNotFound => try gpa.dupe(u8, ""),
        else => return err,
    };
    defer gpa.free(old);
    const line_end = if (std.mem.indexOf(u8, old, "\r\n") != null) "\r\n" else "\n";
    const separator = if (old.len == 0 or old[old.len - 1] == '\n') "" else line_end;
    const bytes = try std.mem.concat(gpa, u8, &.{ old, separator, word, line_end });
    defer gpa.free(bytes);
    try database.writeFileAtomic(io, gpa, path, bytes);
}

/// The user dictionary in `folder`: a `custom.dic` there in any case, else
/// the name it will be written under.
pub fn userDictionaryPath(gpa: std.mem.Allocator, io: std.Io, folder: []const u8) ![]u8 {
    if (try database.findEntry(gpa, io, folder, user_dictionary_name, .file)) |found| return found;
    return std.fs.path.join(gpa, &.{ folder, user_dictionary_name });
}

/// The languages of a dictionary folder: each `<name>` with both a
/// `<name>.aff` and a `<name>.dic` (extensions in any case), sorted. Null
/// when the folder does not open.
pub fn languages(gpa: std.mem.Allocator, io: std.Io, folder: []const u8) !?[][]u8 {
    var dir = std.Io.Dir.cwd().openDir(io, folder, .{ .iterate = true }) catch return null;
    defer dir.close(io);
    var affixes: std.ArrayListUnmanaged([]u8) = .empty;
    defer {
        for (affixes.items) |name| gpa.free(name);
        affixes.deinit(gpa);
    }
    var dictionaries: std.StringHashMapUnmanaged(void) = .empty;
    defer {
        var keys = dictionaries.keyIterator();
        while (keys.next()) |name| gpa.free(name.*);
        dictionaries.deinit(gpa);
    }
    var it = dir.iterate();
    while (try it.next(io)) |entry| {
        if (entry.kind != .file or entry.name.len <= 4) continue;
        const stem = entry.name[0 .. entry.name.len - 4];
        const extension = entry.name[entry.name.len - 4 ..];
        if (std.ascii.eqlIgnoreCase(extension, ".aff")) {
            try affixes.append(gpa, try gpa.dupe(u8, stem));
        } else if (std.ascii.eqlIgnoreCase(extension, ".dic")) {
            const owned = try gpa.dupe(u8, stem);
            const slot = try dictionaries.getOrPut(gpa, owned);
            if (slot.found_existing) gpa.free(owned);
        }
    }
    var found: std.ArrayListUnmanaged([]u8) = .empty;
    errdefer {
        for (found.items) |name| gpa.free(name);
        found.deinit(gpa);
    }
    for (affixes.items) |stem| {
        if (!dictionaries.contains(stem)) continue;
        try found.append(gpa, try gpa.dupe(u8, stem));
    }
    std.mem.sort([]u8, found.items, {}, lessThan);
    return try found.toOwnedSlice(gpa);
}

pub fn freeLanguages(gpa: std.mem.Allocator, names: [][]u8) void {
    for (names) |name| gpa.free(name);
    gpa.free(names);
}

/// The dictionary files of `language` in `folder`, as `languages` found
/// them (extensions in any case); null when either is missing.
pub fn dictionaryFiles(gpa: std.mem.Allocator, io: std.Io, folder: []const u8, language: []const u8) !?[2][]u8 {
    var names: [2][]u8 = undefined;
    for ([_][]const u8{ ".aff", ".dic" }, 0..) |extension, index| {
        const name = try std.mem.concat(gpa, u8, &.{ language, extension });
        defer gpa.free(name);
        names[index] = try database.findEntry(gpa, io, folder, name, .file) orelse {
            if (index == 1) gpa.free(names[0]);
            return null;
        };
    }
    return names;
}

fn lessThan(_: void, a: []u8, b: []u8) bool {
    return std.mem.lessThan(u8, a, b);
}

const testing = std.testing;

test "the delimiter tables are CSpellChecker's, character for character" {
    // SpellChecker.cpp: _T( " \t\n\r`~!@#$%^&*()-_=+\\|[{]};:'\",<.>/?" ),
    // _T( " \t\n\r" ) and _T( "\n\r" ).
    try testing.expectEqualSlices(u8, &.{
        ' ', '\t', '\n', '\r', '`', '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '-', '_',
        '=', '+', '\\', '|', '[', '{', ']', '}', ';', ':', '\'', '"', ',', '<', '.', '>', '/', '?',
    }, spelling_delimiters);
    try testing.expectEqualStrings(" \t\n\r", word_delimiters);
    try testing.expectEqualStrings("\n\r", ignore_symbols);
    // Every other printable ASCII byte is part of a word, digits included,
    // and so is every byte of a multi-byte character.
    for (0..256) |value| {
        const byte: u8 = @intCast(value);
        const listed = std.mem.indexOfScalar(u8, spelling_delimiters, byte) != null;
        try testing.expectEqual(listed, isSpellingDelimiter(byte));
    }
    try testing.expect(!isSpellingDelimiter('0') and !isSpellingDelimiter(0xD0));
}

test "GetWord splits at every delimiter, apostrophes and hyphens included" {
    const text = "  Don't stop-now, <br>Кубань 1943?!";
    var words: std.ArrayListUnmanaged([]const u8) = .empty;
    defer words.deinit(testing.allocator);
    var at: usize = 0;
    while (nextWord(text, at)) |word| : (at = word.offset + word.len) try words.append(testing.allocator, word.of(text));
    const wanted = [_][]const u8{ "Don", "t", "stop", "now", "br", "Кубань", "1943" };
    try testing.expectEqual(wanted.len, words.items.len);
    for (wanted, words.items) |want, got| try testing.expectEqualStrings(want, got);
    try testing.expectEqual(@as(?Word, .{ .offset = 2, .len = 3 }), nextWord(text, 0));
    try testing.expectEqual(@as(?Word, null), nextWord(" .,\r\n", 0));
    try testing.expectEqual(@as(?Word, null), nextWord("", 0));
}

/// A speller that knows a fixed list, for the pass without a dictionary.
const FakeEngine = struct {
    known: []const []const u8,
    added: ?[]const u8 = null,

    fn engine(self: *FakeEngine) Engine {
        return .{ .context = self, .vtable = &.{ .spell = spellFn, .suggest = suggestFn, .add = addFn } };
    }

    fn spellFn(context: *anyopaque, word: []const u8) bool {
        const self: *FakeEngine = @ptrCast(@alignCast(context));
        for (self.known) |known| if (std.mem.eql(u8, known, word)) return true;
        return false;
    }

    fn suggestFn(_: *anyopaque, allocator: std.mem.Allocator, _: []const u8) error{OutOfMemory}![]const []const u8 {
        return allocator.alloc([]const u8, 0);
    }

    fn addFn(context: *anyopaque, word: []const u8) void {
        const self: *FakeEngine = @ptrCast(@alignCast(context));
        self.added = word;
    }
};

test "the pass lists each unknown word where it stands; the user dictionary accepts its own" {
    var fake: FakeEngine = .{ .known = &.{ "the", "tank", "is", "ready" } };
    var user = try UserDictionary.parse(testing.allocator, "#LID 1049\r\nPanzer\r\n");
    defer user.deinit(testing.allocator);
    const text = "The tank is redy; the Panzer is redy.";
    const found = try misspellings(testing.allocator, fake.engine(), &user, text);
    defer testing.allocator.free(found);
    try testing.expectEqual(@as(usize, 3), found.len);
    try testing.expectEqualStrings("The", found[0].of(text));
    try testing.expectEqual(@as(usize, 12), found[1].offset);
    try testing.expectEqualStrings("redy", found[1].of(text));
    try testing.expectEqual(@as(usize, 32), found[2].offset);
}

test "custom.dic: the legacy header is a comment, words one per line, an added word appended" {
    var legacy = try UserDictionary.parse(testing.allocator, "#LID 1049\r\n");
    defer legacy.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), legacy.words.count());

    var words = try UserDictionary.parse(testing.allocator, "\xEF\xBB\xBFPanzer\n  Kuban \n\n# note\nStug\nStug\n\xC0\xFF\n");
    defer words.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 3), words.words.count());
    try testing.expect(words.contains("Panzer") and words.contains("Kuban") and words.contains("Stug"));
    try testing.expect(!words.contains("panzer"));

    const io = testing.io;
    const folder = "zig-out/local-test/elk/spell-core";
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, folder) catch {};
    try cwd.createDirPath(io, folder);
    // The legacy file under its old name is the one found, and kept CRLF.
    try cwd.writeFile(io, .{ .sub_path = folder ++ "/CUSTOM.DIC", .data = "#LID 1049\r\n" });
    const path = try userDictionaryPath(testing.allocator, io, folder);
    defer testing.allocator.free(path);
    try testing.expectEqualStrings("CUSTOM.DIC", std.fs.path.basename(path));
    try appendUserWord(testing.allocator, io, path, "Panzerfaust");
    var reloaded = try UserDictionary.load(testing.allocator, io, path);
    defer reloaded.deinit(testing.allocator);
    try testing.expect(reloaded.contains("Panzerfaust"));
    const bytes = try cwd.readFileAlloc(io, path, testing.allocator, .limited(1024));
    defer testing.allocator.free(bytes);
    try testing.expectEqualStrings("#LID 1049\r\nPanzerfaust\r\n", bytes);

    // A new file in an empty folder.
    const fresh = folder ++ "/new/custom.dic";
    try appendUserWord(testing.allocator, io, fresh, "Kuban");
    try appendUserWord(testing.allocator, io, fresh, "Stug");
    const written = try cwd.readFileAlloc(io, fresh, testing.allocator, .limited(1024));
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Kuban\nStug\n", written);
    var missing = try UserDictionary.load(testing.allocator, io, folder ++ "/none/custom.dic");
    defer missing.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), missing.words.count());
}

test "a dictionary folder's languages are its .aff and .dic pairs" {
    const io = testing.io;
    const folder = "zig-out/local-test/elk/spell-languages";
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, folder) catch {};
    try cwd.createDirPath(io, folder);
    for ([_][]const u8{ "ru_RU.aff", "ru_RU.dic", "en_US.AFF", "en_US.DIC", "de_DE.aff", "readme.txt" }) |name| {
        var buffer: [128]u8 = undefined;
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.bufPrint(&buffer, "{s}/{s}", .{ folder, name }), .data = "" });
    }
    const found = (try languages(testing.allocator, io, folder)).?;
    defer freeLanguages(testing.allocator, found);
    try testing.expectEqual(@as(usize, 2), found.len);
    try testing.expectEqualStrings("en_US", found[0]);
    try testing.expectEqualStrings("ru_RU", found[1]);
    const files = (try dictionaryFiles(testing.allocator, io, folder, "en_US")).?;
    defer for (files) |file| testing.allocator.free(file);
    try testing.expectEqualStrings("en_US.DIC", std.fs.path.basename(files[1]));
    try testing.expectEqual(@as(?[2][]u8, null), try dictionaryFiles(testing.allocator, io, folder, "de_DE"));
    try testing.expectEqual(@as(?[][]u8, null), try languages(testing.allocator, io, folder ++ "/missing"));
}
