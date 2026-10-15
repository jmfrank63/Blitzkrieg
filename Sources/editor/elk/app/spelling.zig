//! Spell check's state in the app (D-11), no UI: the dictionary folder and
//! language from elk.cfg, the Hunspell loaded for them or the reason there
//! is none, and the user dictionary `custom.dic` beside elk.cfg. The pass
//! itself is the core's (`elk.spell.misspellings`) over the translation the
//! session holds. Like the MFC ELK without Word, ELK works without a
//! dictionary: the commands are disabled and `reason` says why.
const std = @import("std");
const elk = @import("elk_core");
const Hunspell = @import("hunspell.zig").Hunspell;

const spell = elk.spell;
pub const Word = spell.Word;

/// The dictionary folder elk.cfg names none: `dictionaries` beside ELK.
pub const default_folder_name = "dictionaries";

pub const Spelling = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    hunspell: ?Hunspell = null,
    /// The folder's languages, sorted; `language` indexes it when loaded.
    languages: [][]u8 = &.{},
    language: ?usize = null,
    user: spell.UserDictionary = .{},
    /// Null keeps added words for this run only (a scripted run with no
    /// elk.cfg).
    user_path: ?[]u8 = null,
    reason_storage: [1024]u8 = undefined,
    reason_len: usize = 0,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) Spelling {
        return .{ .gpa = gpa, .io = io };
    }

    pub fn deinit(self: *Spelling) void {
        self.unload();
        self.user.deinit(self.gpa);
        if (self.user_path) |path| self.gpa.free(path);
    }

    fn unload(self: *Spelling) void {
        if (self.hunspell) |*loaded| loaded.close();
        self.hunspell = null;
        spell.freeLanguages(self.gpa, self.languages);
        self.languages = &.{};
        self.language = null;
    }

    /// Loads `language` from `folder`, the first language there when the
    /// folder has not got it (`DEFAULT_LANGUAGE_INDEX`), and the user
    /// dictionary from `user_folder` (elk.cfg's folder). On failure spell
    /// check is off and `reason` says why.
    pub fn configure(self: *Spelling, folder: []const u8, wanted: []const u8, user_folder: ?[]const u8) void {
        self.unload();
        self.reason_len = 0;
        self.loadUser(user_folder) catch |err| return self.off("the user dictionary in {s} does not read ({s})", .{ user_folder orelse "", @errorName(err) });
        if (folder.len == 0) return self.off("no dictionary folder is set", .{});
        const found = spell.languages(self.gpa, self.io, folder) catch |err| return self.off("the dictionary folder {s} does not read ({s})", .{ folder, @errorName(err) });
        self.languages = found orelse return self.off("the dictionary folder {s} does not open", .{folder});
        if (self.languages.len == 0) return self.off("{s} holds no dictionary (a <language>.aff and <language>.dic pair)", .{folder});
        const index = for (self.languages, 0..) |name, at| {
            if (std.mem.eql(u8, name, wanted)) break at;
        } else 0;
        const name = self.languages[index];
        const files = (spell.dictionaryFiles(self.gpa, self.io, folder, name) catch null) orelse return self.off("{s}'s dictionary files are gone from {s}", .{ name, folder });
        defer for (files) |file| self.gpa.free(file);
        var loaded = Hunspell.open(self.gpa, files[0], files[1]) catch return self.off("Hunspell did not load {s}", .{name});
        if (!loaded.isUtf8()) {
            defer loaded.close();
            return self.off("{s}'s dictionary is in {s}; ELK reads UTF-8 dictionaries only", .{ name, loaded.encoding() });
        }
        self.hunspell = loaded;
        self.language = index;
        for (self.user.words.keys()) |word| self.hunspell.?.engine().add(word);
    }

    fn loadUser(self: *Spelling, user_folder: ?[]const u8) !void {
        self.user.deinit(self.gpa);
        self.user = .{};
        if (self.user_path) |path| self.gpa.free(path);
        self.user_path = null;
        const folder = user_folder orelse return;
        self.user_path = try spell.userDictionaryPath(self.gpa, self.io, folder);
        self.user = try spell.UserDictionary.load(self.gpa, self.io, self.user_path.?);
    }

    /// The reason is formatted before the languages go: it may name one.
    fn off(self: *Spelling, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.reason_storage, "Spell check is off: " ++ format ++ ".", args) catch self.reason_storage[0..];
        self.reason_len = text.len;
        self.unload();
    }

    pub fn available(self: *const Spelling) bool {
        return self.hunspell != null;
    }

    /// Why spell check is off; "" when it is on.
    pub fn reason(self: *const Spelling) []const u8 {
        return self.reason_storage[0..self.reason_len];
    }

    pub fn languageName(self: *const Spelling) []const u8 {
        return self.languages[self.language orelse return ""];
    }

    /// The misspelt words of `text`, allocated in `allocator`; none while
    /// spell check is off.
    pub fn check(self: *Spelling, allocator: std.mem.Allocator, text: []const u8) ![]Word {
        const loaded = &(self.hunspell orelse return allocator.alloc(Word, 0));
        return spell.misspellings(allocator, loaded.engine(), &self.user, text);
    }

    pub fn suggest(self: *Spelling, allocator: std.mem.Allocator, word: []const u8) ![]const []const u8 {
        const loaded = &(self.hunspell orelse return allocator.alloc([]const u8, 0));
        return loaded.engine().suggest(allocator, word);
    }

    /// Add to dictionary: the word is accepted from now on and written to
    /// custom.dic, so it stays accepted after a restart.
    pub fn addWord(self: *Spelling, word: []const u8) !void {
        if (!try self.user.add(self.gpa, word)) return;
        if (self.hunspell) |*loaded| loaded.engine().add(word);
        if (self.user_path) |path| try spell.appendUserWord(self.gpa, self.io, path, word);
    }
};

/// A dictionary pair made for the tests and the smoke, so no third-party
/// dictionary is needed to prove the spell check: UTF-8, a plural suffix,
/// and the letters Hunspell tries for its suggestions.
pub const test_language = "en_TEST";
pub const test_affix =
    "# ELK's test dictionary, written for its tests (D-11).\r\n" ++
    "SET UTF-8\r\n" ++
    "TRY esianrtolcdugmphbyfvkwz\r\n" ++
    "SFX S Y 1\r\n" ++
    "SFX S 0 s .\r\n";
pub const test_dictionary =
    "9\r\n" ++
    "the\r\n" ++
    "tank/S\r\n" ++
    "is\r\n" ++
    "ready\r\n" ++
    "and\r\n" ++
    "medal/S\r\n" ++
    "good\r\n" ++
    "word/S\r\n" ++
    "Кубань\r\n";

/// Writes the test dictionary pair into `folder`.
pub fn writeTestDictionary(io: std.Io, folder: []const u8) !void {
    var dir = try std.Io.Dir.cwd().createDirPathOpen(io, folder, .{});
    defer dir.close(io);
    try dir.writeFile(io, .{ .sub_path = test_language ++ ".aff", .data = test_affix });
    try dir.writeFile(io, .{ .sub_path = test_language ++ ".dic", .data = test_dictionary });
}

const testing = std.testing;
const scratch = "zig-out/local-test/elk/spelling";

fn words(text: []const u8, found: []const Word, out: *[8][]const u8) []const []const u8 {
    for (found, 0..) |word, index| out[index] = word.of(text);
    return out[0..found.len];
}

test "Hunspell on the test dictionary: good words pass, a misspelling is flagged with suggestions" {
    const io = testing.io;
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, scratch ++ "/check") catch {};
    try writeTestDictionary(io, scratch ++ "/check/dictionaries");
    var spelling = Spelling.init(testing.allocator, io);
    defer spelling.deinit();
    spelling.configure(scratch ++ "/check/dictionaries", "", null);
    try testing.expect(spelling.available());
    try testing.expectEqualStrings("", spelling.reason());
    try testing.expectEqualStrings(test_language, spelling.languageName());

    // The affix rule, the capital at a sentence's start, Cyrillic and numbers.
    const text = "The tanks are redy, and teh medals: Кубань 1943.";
    const found = try spelling.check(testing.allocator, text);
    defer testing.allocator.free(found);
    var out: [8][]const u8 = undefined;
    const flagged = words(text, found, &out);
    try testing.expectEqual(@as(usize, 3), flagged.len);
    try testing.expectEqualStrings("are", flagged[0]);
    try testing.expectEqualStrings("redy", flagged[1]);
    try testing.expectEqualStrings("teh", flagged[2]);
    try testing.expectEqual(@as(usize, 14), found[1].offset);

    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const for_redy = try spelling.suggest(arena.allocator(), "redy");
    try testing.expect(for_redy.len != 0);
    try testing.expectEqualStrings("ready", for_redy[0]);
    const for_teh = try spelling.suggest(arena.allocator(), "teh");
    try testing.expectEqualStrings("the", for_teh[0]);
}

test "Add to dictionary clears the flag, lands in custom.dic and holds after a restart" {
    const io = testing.io;
    const cwd = std.Io.Dir.cwd();
    const root = scratch ++ "/add";
    cwd.deleteTree(io, root) catch {};
    try writeTestDictionary(io, root ++ "/dictionaries");
    // The legacy file, copied beside elk.cfg under its old name.
    try cwd.createDirPath(io, root ++ "/user");
    try cwd.writeFile(io, .{ .sub_path = root ++ "/user/CUSTOM.DIC", .data = "#LID 1049\r\n" });
    const text = "redy tank";
    {
        var spelling = Spelling.init(testing.allocator, io);
        defer spelling.deinit();
        spelling.configure(root ++ "/dictionaries", test_language, root ++ "/user");
        const before = try spelling.check(testing.allocator, text);
        defer testing.allocator.free(before);
        try testing.expectEqual(@as(usize, 1), before.len);
        try spelling.addWord("redy");
        const after = try spelling.check(testing.allocator, text);
        defer testing.allocator.free(after);
        try testing.expectEqual(@as(usize, 0), after.len);
    }
    const bytes = try cwd.readFileAlloc(io, root ++ "/user/CUSTOM.DIC", testing.allocator, .limited(1024));
    defer testing.allocator.free(bytes);
    try testing.expectEqualStrings("#LID 1049\r\nredy\r\n", bytes);
    // A new run reads it back; Hunspell takes the word's capitalised form too.
    var again = Spelling.init(testing.allocator, io);
    defer again.deinit();
    again.configure(root ++ "/dictionaries", test_language, root ++ "/user");
    const found = try again.check(testing.allocator, "Redy redy tank");
    defer testing.allocator.free(found);
    try testing.expectEqual(@as(usize, 0), found.len);
}

test "without a usable dictionary spell check is off with the reason" {
    const io = testing.io;
    const cwd = std.Io.Dir.cwd();
    const root = scratch ++ "/off";
    cwd.deleteTree(io, root) catch {};
    try cwd.createDirPath(io, root ++ "/empty");
    var spelling = Spelling.init(testing.allocator, io);
    defer spelling.deinit();

    spelling.configure("", "", null);
    try testing.expect(!spelling.available());
    try testing.expectEqualStrings("Spell check is off: no dictionary folder is set.", spelling.reason());
    const none = try spelling.check(testing.allocator, "redy");
    defer testing.allocator.free(none);
    try testing.expectEqual(@as(usize, 0), none.len);

    spelling.configure(root ++ "/missing", "", null);
    try testing.expect(!spelling.available());
    try testing.expect(std.mem.indexOf(u8, spelling.reason(), "does not open") != null);

    spelling.configure(root ++ "/empty", "", null);
    try testing.expect(std.mem.indexOf(u8, spelling.reason(), "holds no dictionary") != null);

    // A dictionary in another encoding is named, not misread.
    try cwd.createDirPath(io, root ++ "/latin");
    try cwd.writeFile(io, .{ .sub_path = root ++ "/latin/de_OLD.aff", .data = "SET ISO8859-1\r\n" });
    try cwd.writeFile(io, .{ .sub_path = root ++ "/latin/de_OLD.dic", .data = "1\r\nwort\r\n" });
    spelling.configure(root ++ "/latin", "de_OLD", null);
    try testing.expect(!spelling.available());
    try testing.expect(std.mem.indexOf(u8, spelling.reason(), "ISO8859-1") != null);

    // The wanted language missing: the first one, here the Latin-1 one;
    // the UTF-8 one chosen, back on.
    try writeTestDictionary(io, root ++ "/latin");
    spelling.configure(root ++ "/latin", "fr_FR", null);
    try testing.expect(std.mem.indexOf(u8, spelling.reason(), "de_OLD") != null);
    spelling.configure(root ++ "/latin", test_language, null);
    try testing.expect(spelling.available());
    try testing.expectEqualStrings(test_language, spelling.languageName());
}
