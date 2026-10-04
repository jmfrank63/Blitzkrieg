//! Single instance (05-11, D-34, PARITY F13): a second launch hands its map to
//! the running editor and exits; the running editor opens it through the
//! unsaved-changes guard. The MFC editor did this with WM_COPYDATA
//! (MainFrm.cpp:1200, editor.cpp:125), which is Windows-only.
//!
//! Mechanism (the agent's discretion, CONTEXT D-34): a per-user local stream
//! socket - a Unix domain socket on macOS and Linux, and on Windows too (AF_UNIX
//! has been there since Windows 10, and `std.Io.net.UnixAddress` speaks it
//! natively), so one implementation serves every target and its tests run on
//! every target. A Windows named pipe would be a second back end with nothing
//! to add for one line of text.
//!
//! Protocol: one line, one path. The second launch connects, writes
//! `<absolute path>\n` (an empty line means "bring the window to the front"
//! and nothing else) and waits for `ok\n` (queued), `no\n` (refused), `busy\n`
//! (the owner serves its limit of connections) or `mod\n` (see below). No
//! commands travel the socket (T-05-11-01); the receiving editor validates the
//! path like any open (`panels_logic.dropVerdict`, then the guard and the
//! bridge's own read), so a hostile line can at worst name a file the editor
//! would open anyway, or be refused with a status note.
//!
//! A launch that named a mod (`-mod=`) sends `<mod>`, a unit separator (0x1f)
//! and then the path, with an empty mod for `-mod=None`. A map opens under the
//! object database of the mod that is loaded, so an owner with another mod
//! answers `mod\n` and queues nothing; the second launch then runs its own
//! editor instead of having its mod silently dropped (WR-A03).
//!
//! Endpoint: `<user root>/mapeditor/instance.sock`, the user root being the one
//! the engine uses (Platform/Paths.cpp: `$XDG_DATA_HOME/Nival/Blitzkrieg`, else
//! `$HOME/.local/share/Nival/Blitzkrieg`; `%APPDATA%\Nival\Blitzkrieg\` on
//! Windows) - computed here from the environment because the check has to run
//! before the window and the engine exist. A path longer than a socket address
//! holds (104 bytes on macOS) falls back to a short file under the per-user
//! temporary folder - a folder of its own there, `bk-mapeditor-<uid>`, made with
//! mode 0700 and refused when it is open to others, so that nobody else can
//! bind the name first or reach the socket (WR-A04) - named after a hash of
//! the user root.
//!
//! A stale or hung peer never blocks a start (T-05-11-02): connecting to a
//! socket file nobody listens on fails; one whose accept thread is hung answers
//! nothing within `timeout_ms`. Either way the second launch removes that one
//! socket file (a per-user path, never anything else), binds a fresh one and
//! starts normally. A listener that accepted the connection and then closed it
//! unanswered, or said `busy`, is alive: the second launch leaves its socket
//! file alone and starts without single-instance instead (WR-A01).
//!
//! What "hung" means here is the owner's accept thread, nothing more. An owner
//! whose MAIN loop is frozen (a long synchronous Create Random Map or Update
//! Map, or a deadlock) is not detected: the helper thread still answers `ok`,
//! up to `queue_capacity` lines queue and the second launch exits 0 without
//! the map opening until the loop runs again. Telling a long job from a
//! deadlock needs a heartbeat and a guess at how long is too long, and a wrong
//! guess starts a second editor over the first one's files and recovery copies,
//! so there is none (WR-A05).
//!
//! Threads: the owner's accept loop runs on its own thread and hands each
//! connection to a thread of its own (four at most), which answers `ok` as soon
//! as the line is queued, so neither a busy or loading main thread nor a client
//! that connects and says nothing keeps a second launch waiting; the main loop
//! only `poll`s the queue each frame. A watchdog per connection shuts a silent
//! client's stream down after the timeout. The second launch does its whole
//! hand-off on a worker thread and gives up on it after the timeout: the std has
//! no connect or read timeout, and on Windows neither a connect to a listener
//! that never accepts nor a read on it is woken by anything but the peer.
//!
//! std-only (no SDL, no engine, no ImGui), so its tests run in
//! `zig build test-map-editor-panels` on every target.
const std = @import("std");
const builtin = @import("builtin");

/// A worker thread's stack: these threads only pass a line, so a small one
/// elsewhere. Not on Linux: glibc carves the static TLS out of each new
/// thread's stack, and the std alone keeps 256 KiB there (its signal stack), so
/// a 128-256 KiB stack fails to start (EINVAL). The default there is only
/// reserved address space, committed as it is touched.
fn workerStack(size: usize) std.Thread.SpawnConfig {
    return .{ .stack_size = if (builtin.os.tag == .linux) std.Thread.SpawnConfig.default_stack_size else size };
}
const Io = std.Io;
const net = Io.net;

/// The longest path a line may carry. Paths in the editor are at most
/// `PathSlot.max_path` (4096); a line past this is refused, not cut.
pub const max_line = 2048;
/// Lines waiting for the main loop. A second launch is a human action: eight
/// pending is a flood, answered `no`.
pub const queue_capacity = 8;
/// Connections served at once; one more is closed without an answer.
pub const max_connections = 4;
/// How long a second launch waits for the owner's answer, and how long the
/// owner waits for a connected client's line.
pub const default_timeout_ms = 1500;
/// How long `deinit` waits for the accept thread to notice its wake-up
/// connection before it gives the thread up (WR-A02).
pub const accept_wait_ms = 1000;
/// A socket address holds 104 bytes on macOS, 108 on Linux (the std's own
/// limit); one byte for the terminator and a margin of one.
pub const posix_path_limit = 102;
/// Where an endpoint path is built.
pub const path_capacity = 512;

pub const Options = struct {
    timeout_ms: u32 = default_timeout_ms,
    /// A refused connection might be an owner between `bind` and `listen`:
    /// ask again this many times, this far apart, before calling it stale.
    stale_retries: u8 = 2,
    stale_retry_ms: u32 = 60,
    /// The endpoint's folder is the private fallback one (`Endpoint`): made with
    /// mode 0700 and refused if it is open to anyone else (WR-A04).
    private_folder: bool = false,
    /// The mod the owner starts with (null: none); `Instance.setMod` keeps it
    /// current afterwards.
    mod: ?[]const u8 = null,
};

// -- The endpoint ------------------------------------------------------------

/// What `endpointPath` reads of the environment, as plain slices so the rule is
/// testable without one.
pub const Env = struct {
    xdg_data_home: ?[]const u8 = null,
    home: ?[]const u8 = null,
    appdata: ?[]const u8 = null,
    tmpdir: ?[]const u8 = null,
    /// The user's id, for the name of the private folder the fallback endpoint
    /// lives in (WR-A04); null on Windows, which never uses the fallback.
    uid: ?u32 = null,

    pub fn fromEnviron(gpa: std.mem.Allocator, environ: std.process.Environ, storage: *EnvStorage) Env {
        storage.* = .{};
        return .{
            .xdg_data_home = storage.get(gpa, environ, 0, "XDG_DATA_HOME"),
            .home = storage.get(gpa, environ, 1, "HOME"),
            .appdata = storage.get(gpa, environ, 2, "APPDATA"),
            .tmpdir = storage.get(gpa, environ, 3, "TMPDIR"),
            .uid = currentUid(),
        };
    }
};

fn currentUid() ?u32 {
    return switch (builtin.os.tag) {
        .windows, .wasi => null,
        .linux => std.os.linux.getuid(),
        else => std.c.getuid(),
    };
}

/// Owned copies of the four variables, so `Env` can hold slices (the
/// `Environ.getAlloc` API allocates). Freed by `deinit`.
pub const EnvStorage = struct {
    values: [4]?[]u8 = .{ null, null, null, null },

    fn get(self: *EnvStorage, gpa: std.mem.Allocator, environ: std.process.Environ, slot: usize, name: []const u8) ?[]const u8 {
        const value = environ.getAlloc(gpa, name) catch return null;
        if (value.len == 0) {
            gpa.free(value);
            return null;
        }
        self.values[slot] = value;
        return value;
    }

    pub fn deinit(self: *EnvStorage, gpa: std.mem.Allocator) void {
        for (self.values) |maybe| if (maybe) |value| gpa.free(value);
        self.* = .{};
    }
};

/// Where the endpoint is, and whether its folder has to be made private: the
/// short fallback in the shared temporary folder lives in a folder of its own
/// (`prepareFolder`).
pub const Endpoint = struct { path: []const u8, private_folder: bool = false };

/// `<user root>/mapeditor/instance.sock` (see the file's header), or null when
/// the environment names no user root or the result does not fit `buffer`.
/// `os` is a parameter so the Windows form is tested on every host.
pub fn endpointPath(buffer: []u8, env: Env, os: std.Target.Os.Tag) ?[]const u8 {
    return (endpointFor(buffer, env, os) orelse return null).path;
}

pub fn endpointFor(buffer: []u8, env: Env, os: std.Target.Os.Tag) ?Endpoint {
    if (os == .windows) {
        const appdata = env.appdata orelse return null;
        const text = std.fmt.bufPrint(buffer, "{s}{s}Nival\\Blitzkrieg\\mapeditor\\instance.sock", .{ appdata, if (std.mem.endsWith(u8, appdata, "\\")) "" else "\\" }) catch return null;
        return .{ .path = text };
    }
    var root_buffer: [path_capacity]u8 = undefined;
    const root: []const u8 = if (env.xdg_data_home) |xdg|
        std.fmt.bufPrint(&root_buffer, "{s}/Nival/Blitzkrieg", .{std.mem.trimEnd(u8, xdg, "/")}) catch return null
    else if (env.home) |home|
        std.fmt.bufPrint(&root_buffer, "{s}/.local/share/Nival/Blitzkrieg", .{std.mem.trimEnd(u8, home, "/")}) catch return null
    else
        return null;
    const preferred = std.fmt.bufPrint(buffer, "{s}/mapeditor/instance.sock", .{root}) catch return null;
    if (preferred.len <= posix_path_limit) return .{ .path = preferred };
    // Too long for a socket address: a short name in the temporary folder,
    // still one per user root. That folder may be shared (/tmp is world-
    // writable on Linux), where a plain `bk-mapeditor-<hash>.sock` could be
    // bound first by anyone: so the name sits in a folder of its own, one per
    // user, made private by `prepareFolder` (WR-A04).
    //
    // $TMPDIR is tried first, but on macOS it is a long /var/folders/... name and
    // a custom one can be longer still: when the name does not fit a socket
    // address either, /tmp is the shorter place (the same per-user folder and
    // the same checks). When nothing fits there is no endpoint, and the editor
    // starts without single-instance, rather than hand the std a path it panics
    // on.
    const candidates = [_][]const u8{ std.mem.trimEnd(u8, env.tmpdir orelse "/tmp", "/"), "/tmp" };
    for (candidates) |tmp| {
        const text = std.fmt.bufPrint(buffer, "{s}/bk-mapeditor-{d}/{x:0>16}.sock", .{ tmp, env.uid orelse 0, std.hash.Fnv1a_64.hash(root) }) catch continue;
        if (text.len <= posix_path_limit) return .{ .path = text, .private_folder = true };
    }
    return null;
}

/// Makes the endpoint's folder if it is not there, for the fallback endpoint
/// (WR-A04): created with mode 0700, and an existing one must be a directory
/// with no group or world access. Another user's folder of that name cannot
/// be written to by us, and one they left open fails the mode check, so a name
/// squatted in a shared temporary folder is refused rather than trusted.
fn prepareFolder(io: Io, directory: []const u8) bool {
    if (builtin.os.tag == .windows) return true;
    return preparePosixFolder(io, directory);
}

fn preparePosixFolder(io: Io, directory: []const u8) bool {
    Io.Dir.cwd().createDir(io, directory, @enumFromInt(0o700)) catch |err| switch (err) {
        error.PathAlreadyExists => {},
        else => return false,
    };
    const stat = Io.Dir.cwd().statFile(io, directory, .{}) catch return false;
    return stat.kind == .directory and stat.permissions.toMode() & 0o077 == 0;
}

// -- One line, one path ------------------------------------------------------

/// The longest mod folder name a line may carry (the app keeps 64 bytes for it).
pub const max_mod_len = 64;
/// Between the mod and the path of an open that names a mod: the one control
/// character a line may hold.
pub const mod_separator: u8 = 0x1f;

/// What a line asks for.
pub const Request = union(enum) {
    /// An empty line: bring the window forward, open nothing.
    raise,
    open: []const u8,
    /// A launch that named a mod (`-mod=`, "" for `-mod=None`): the open only
    /// goes through when the owner has that mod loaded (WR-A03).
    open_in_mod: struct { mod: []const u8, path: []const u8 },
};

fn hasControl(text: []const u8) bool {
    for (text) |byte| {
        if (byte < 0x20 or byte == 0x7f) return true;
    }
    return false;
}

/// A line read off the socket (without its newline), or null when it is not
/// one this protocol sends: a control character in it (a second line, an
/// escape sequence; the one mod separator is allowed) or more than `max_line`
/// bytes. A trailing CR is dropped.
pub fn parseLine(line: []const u8) ?Request {
    const body = std.mem.trimEnd(u8, line, "\r");
    if (body.len > max_line) return null;
    if (std.mem.indexOfScalar(u8, body, mod_separator)) |cut| {
        const mod = body[0..cut];
        const path = body[cut + 1 ..];
        if (mod.len > max_mod_len or path.len == 0 or hasControl(mod) or hasControl(path)) return null;
        return .{ .open_in_mod = .{ .mod = mod, .path = path } };
    }
    if (hasControl(body)) return null;
    if (body.len == 0) return .raise;
    return .{ .open = body };
}

/// `<path>\n` in `buffer` for the client to send; null when the path would not
/// parse back (a control character, too long) or does not fit.
pub fn frameLine(buffer: []u8, path: []const u8) ?[]const u8 {
    // The separator would make parseLine read a mod where there is none.
    if (std.mem.indexOfScalar(u8, path, mod_separator) != null) return null;
    if (parseLine(path) == null) return null;
    if (path.len + 1 > buffer.len) return null;
    @memcpy(buffer[0..path.len], path);
    buffer[path.len] = '\n';
    return buffer[0 .. path.len + 1];
}

/// `<mod>` 0x1f `<path>\n` in `buffer`, for a launch that named a mod (an empty
/// `mod` is `-mod=None`); null when it would not parse back or does not fit.
pub fn frameOpenInMod(buffer: []u8, mod: []const u8, path: []const u8) ?[]const u8 {
    if (path.len == 0 or mod.len > max_mod_len) return null;
    const total = mod.len + 1 + path.len + 1;
    if (total > buffer.len) return null;
    @memcpy(buffer[0..mod.len], mod);
    buffer[mod.len] = mod_separator;
    @memcpy(buffer[mod.len + 1 ..][0..path.len], path);
    buffer[total - 1] = '\n';
    const framed = buffer[0 .. total - 1];
    if (parseLine(framed) == null) return null;
    return buffer[0..total];
}

// -- The owner's side --------------------------------------------------------

const Entry = struct { len: u16 = 0, bytes: [max_line]u8 = undefined };

/// What the main loop gets from `poll`.
pub const Received = union(enum) {
    raise,
    /// The path, in the buffer the caller handed to `poll`.
    open: []const u8,
};

/// The running editor's end: a listening socket, the thread that answers it and
/// the queue between that thread and the main loop.
pub const Instance = struct {
    gpa: std.mem.Allocator,
    io: Io,
    server: net.Server,
    path_storage: [path_capacity]u8 = undefined,
    path_len: usize = 0,
    timeout_ms: u32,
    /// The loaded mod's folder name, guarded by `mutex` (`setMod`).
    mod_storage: [max_mod_len]u8 = undefined,
    mod_len: usize = 0,
    thread: ?std.Thread = null,
    stopping: std.atomic.Value(bool) = .init(false),
    /// Set by the accept thread as it returns, so `deinit` can wait for it with
    /// a deadline instead of `join`ing a thread that may never be woken.
    serve_done: std.atomic.Value(bool) = .init(false),
    /// The socket file's inode when it was bound, to tell it from a file a
    /// later launch put at the same path; null when it could not be read.
    bound_inode: ?Io.File.INode = null,
    /// Connection threads running; `max_connections` at most (one more is closed unanswered).
    connections: std.atomic.Value(u32) = .init(0),
    mutex: Io.Mutex = .init,
    queue: [queue_capacity]Entry = undefined,
    head: usize = 0,
    count: usize = 0,

    pub fn path(self: *const Instance) []const u8 {
        return self.path_storage[0..self.path_len];
    }

    /// The mod this editor has loaded now (null or "" for none); the main loop
    /// keeps it current, each frame, because File > Mod changes it. A name
    /// past `max_mod_len` is cut like the app's own.
    pub fn setMod(self: *Instance, mod: ?[]const u8) void {
        const name = mod orelse "";
        const len = @min(name.len, max_mod_len);
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        @memcpy(self.mod_storage[0..len], name[0..len]);
        self.mod_len = len;
    }

    fn hasMod(self: *Instance, mod: []const u8) bool {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        return std.mem.eql(u8, self.mod_storage[0..self.mod_len], mod);
    }

    /// The oldest line the second launches have sent, if any: a path copied
    /// into `out` or a raise. Called by the main loop once per frame.
    pub fn poll(self: *Instance, out: *[max_line]u8) ?Received {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.count == 0) return null;
        const entry = &self.queue[self.head];
        self.head = (self.head + 1) % queue_capacity;
        self.count -= 1;
        if (entry.len == 0) return .raise;
        @memcpy(out[0..entry.len], entry.bytes[0..entry.len]);
        return .{ .open = out[0..entry.len] };
    }

    /// Stops answering, removes the socket file and frees the instance. The
    /// accept thread is woken by a connection of our own and waited for up to
    /// `accept_wait_ms`: the wake-up goes through the socket file, which may be
    /// gone or replaced by now (a tmp cleaner, a launch that judged this one
    /// stale), and then the accept never returns. Past the wait the thread is
    /// detached and the instance, with its listening socket, left as it is - the
    /// process is on its way out - rather than closed under the blocked accept
    /// (WR-A02). Only a socket file that is still the one bound here is removed:
    /// another editor's live socket at that path is not ours to unlink. A
    /// connection thread still stuck on a silent client (its watchdog could not
    /// wake the read) is waited for up to twice the timeout; past that the
    /// instance is left allocated rather than freed under it.
    pub fn deinit(self: *Instance) void {
        self.stopping.store(true, .release);
        var accept_ended = true;
        if (self.thread) |thread| {
            if (net.UnixAddress.init(self.path())) |address| {
                if (address.connect(self.io)) |stream| stream.close(self.io) else |_| {}
            } else |_| {}
            const accept_deadline: Deadline = .in(self.io, accept_wait_ms);
            while (!self.serve_done.load(.acquire) and !accept_deadline.passed(self.io)) {
                self.io.sleep(.fromMilliseconds(10), .awake) catch break;
            }
            if (self.serve_done.load(.acquire)) {
                thread.join();
            } else {
                thread.detach();
                accept_ended = false;
            }
        }
        if (self.ownsSocketFile()) Io.Dir.cwd().deleteFile(self.io, self.path()) catch {};
        if (!accept_ended) return;
        self.server.deinit(self.io);
        const connections_deadline: Deadline = .in(self.io, self.timeout_ms *| 2);
        while (self.connections.load(.acquire) != 0 and !connections_deadline.passed(self.io)) {
            self.io.sleep(.fromMilliseconds(10), .awake) catch break;
        }
        if (self.connections.load(.acquire) != 0) return;
        const gpa = self.gpa;
        gpa.destroy(self);
    }

    fn enqueue(self: *Instance, request: Request) bool {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.count == queue_capacity) return false;
        const slot = &self.queue[(self.head + self.count) % queue_capacity];
        switch (request) {
            .raise => slot.len = 0,
            .open => |text| {
                slot.len = @intCast(text.len);
                @memcpy(slot.bytes[0..text.len], text);
            },
            // The mod was checked by `serveOne`; the main loop gets the path.
            .open_in_mod => |named| {
                slot.len = @intCast(named.path.len);
                @memcpy(slot.bytes[0..named.path.len], named.path);
            },
        }
        self.count += 1;
        return true;
    }

    /// Whether the file at the endpoint is still the socket this instance bound
    /// (or cannot be told apart from it, which keeps the old behaviour).
    fn ownsSocketFile(self: *const Instance) bool {
        const bound = self.bound_inode orelse return true;
        const now = socketFileInode(self.io, self.path()) orelse return false;
        return now == bound;
    }

    fn serve(self: *Instance) void {
        defer self.serve_done.store(true, .release);
        while (!self.stopping.load(.acquire)) {
            const stream = self.server.accept(self.io) catch |err| switch (err) {
                error.SocketNotListening, error.Canceled => return,
                // A passing failure (a connection that died before accept,
                // a descriptor shortage): try again, never spin.
                else => {
                    self.io.sleep(.fromMilliseconds(50), .awake) catch return;
                    continue;
                },
            };
            // Each connection on its own thread: a client that connects and says
            // nothing must not keep the accept loop from the next launch.
            if (self.connections.load(.acquire) >= max_connections) {
                // Said, not just closed: a launch that heard nothing would call
                // this live owner stale (WR-A01).
                answerAndClose(self.io, stream, "busy\n");
                continue;
            }
            _ = self.connections.fetchAdd(1, .acq_rel);
            const thread = std.Thread.spawn(workerStack(256 * 1024), serveConnection, .{ self, stream }) catch {
                _ = self.connections.fetchSub(1, .acq_rel);
                stream.close(self.io);
                continue;
            };
            thread.detach();
        }
    }

    fn serveConnection(self: *Instance, stream: net.Stream) void {
        defer _ = self.connections.fetchSub(1, .acq_rel);
        self.serveOne(stream);
    }

    fn serveOne(self: *Instance, stream: net.Stream) void {
        defer stream.close(self.io);
        var watchdog: Watchdog = .{ .io = self.io, .stream = stream, .timeout_ms = self.timeout_ms };
        watchdog.start();
        defer _ = watchdog.finish();
        var read_buffer: [max_line + 2]u8 = undefined;
        var reader = stream.reader(self.io, &read_buffer);
        const line = reader.interface.takeDelimiterExclusive('\n') catch |err| {
            // A line longer than the buffer is a hostile client that is still
            // connected: it is told no. Anything else (it left, the watchdog cut
            // it) has nobody to answer.
            if (err == error.StreamTooLong) writeAnswer(self.io, stream, "no\n");
            return;
        };
        // Our own wake-up connection (deinit) sends nothing and is closed.
        if (self.stopping.load(.acquire)) return;
        const answer: []const u8 = blk: {
            const request = parseLine(line) orelse break :blk "no\n";
            // A map opens under the object database of the mod that is
            // loaded: another mod's launch is told, not queued (WR-A03).
            if (request == .open_in_mod and !self.hasMod(request.open_in_mod.mod)) break :blk "mod\n";
            break :blk if (self.enqueue(request)) "ok\n" else "no\n";
        };
        writeAnswer(self.io, stream, answer);
    }
};

/// The identity of the file at `path` itself, null when there is none. Not
/// followed: on Windows an AF_UNIX socket file is a reparse point, and opening it
/// to follow fails (IO_REPARSE_TAG_NOT_HANDLED), which left `bound_inode` unknown
/// and let `deinit` remove a later owner's socket file. On the other systems a
/// socket file is no link, so this is the plain stat.
fn socketFileInode(io: Io, path: []const u8) ?Io.File.INode {
    const stat = Io.Dir.cwd().statFile(io, path, .{ .follow_symlinks = false }) catch return null;
    return stat.inode;
}

/// One short answer, flushed; a client that is gone is not an error.
fn writeAnswer(io: Io, stream: net.Stream, answer: []const u8) void {
    var write_buffer: [8]u8 = undefined;
    var writer = stream.writer(io, &write_buffer);
    writer.interface.writeAll(answer) catch return;
    writer.interface.flush() catch return;
}

fn answerAndClose(io: Io, stream: net.Stream, answer: []const u8) void {
    writeAnswer(io, stream, answer);
    stream.close(io);
}

/// A point `ms` from now on the monotonic clock. The waits here poll in short
/// sleeps; they read this rather than count their sleeps, because on a loaded
/// machine a 10 ms sleep can take several times that, and a wait that counts
/// them runs several times its length (CI 37151247396: a 1 s accept wait in
/// `deinit` took over 4 s on the macos-14 runner).
const Deadline = struct {
    at: Io.Clock.Timestamp,

    fn in(io: Io, ms: u32) Deadline {
        return .{ .at = Io.Clock.Timestamp.fromNow(io, .{ .raw = .fromMilliseconds(ms), .clock = .awake }) };
    }

    fn passed(self: Deadline, io: Io) bool {
        return Io.Clock.Timestamp.now(io, .awake).compare(.gte, self.at);
    }
};

/// Cuts a connection that has not finished by its deadline: a thread that
/// sleeps in small steps and, if `finish` has not been called by then, shuts
/// the stream down, which ends the blocked read or write with an error.
const Watchdog = struct {
    io: Io,
    stream: net.Stream,
    timeout_ms: u32,
    done: std.atomic.Value(bool) = .init(false),
    fired: std.atomic.Value(bool) = .init(false),
    thread: ?std.Thread = null,

    fn start(self: *Watchdog) void {
        self.thread = std.Thread.spawn(workerStack(128 * 1024), run, .{self}) catch null;
    }

    /// True when the deadline passed first.
    fn finish(self: *Watchdog) bool {
        self.done.store(true, .release);
        if (self.thread) |thread| thread.join();
        self.thread = null;
        return self.fired.load(.acquire);
    }

    fn run(self: *Watchdog) void {
        const deadline: Deadline = .in(self.io, self.timeout_ms);
        while (!self.done.load(.acquire)) {
            if (deadline.passed(self.io)) {
                self.fired.store(true, .release);
                self.stream.shutdown(self.io, .both) catch {};
                return;
            }
            self.io.sleep(.fromMilliseconds(10), .awake) catch return;
        }
    }
};

// -- The second launch's side and the choice between the two ------------------

/// What a hand-off attempt came to.
pub const Handoff = enum {
    /// The owner queued the line.
    acked,
    /// The owner answered `no` (a full queue, a line it would not take).
    refused,
    /// Nothing listens at the endpoint.
    no_listener,
    /// The connection was made and nothing came back within the timeout: a
    /// listener that never accepts, or an accept thread that is hung.
    no_reply,
    /// The connection was made and then closed or reset with no answer (or an
    /// answer this protocol does not have): somebody is listening, it just did
    /// not take the line. Not stale (WR-A01).
    dropped,
    /// The owner answered `busy`: it is serving its limit of connections.
    busy,
    /// The owner has another mod loaded than the one this launch named; nothing
    /// was queued (WR-A03).
    other_mod,
};

/// Whether a failed hand-off means the endpoint is stale (T-05-11-02): nobody
/// listens, or the one who does never answers. Anything that connected and was
/// closed on, or was answered, is a live listener - deleting its socket file
/// would leave two editors with the first one unreachable (WR-A01).
pub fn endpointIsStale(result: Handoff) bool {
    return result == .no_listener or result == .no_reply;
}

/// The whole hand-off - connect, write the line, read the answer - on its own
/// thread, so that nothing a hung owner does can hold the second launch: on
/// Windows a connection to a listening socket whose owner never accepts does not
/// complete, and a blocked read there is not woken by a shutdown. The waiter polls
/// for the result and, past the deadline, abandons the job; whichever of the two
/// finishes last frees it (an abandoned worker stays blocked on the dead peer until
/// the peer or this process goes, which costs one idle thread).
const HandoffJob = struct {
    io: Io,
    path_storage: [path_capacity]u8 = undefined,
    path_len: usize = 0,
    line_storage: [max_line + 2]u8 = undefined,
    line_len: usize = 0,
    state: std.atomic.Value(u8) = .init(running),
    result: Handoff = .no_reply,

    const running: u8 = 0;
    const done: u8 = 1;
    const abandoned: u8 = 2;
    /// An abandoned job can outlive its caller (and a test's own allocator), so
    /// the job comes from the process-wide allocator.
    const allocator = std.heap.smp_allocator;

    fn run(self: *HandoffJob) void {
        const io = self.io;
        const address = net.UnixAddress.init(self.path_storage[0..self.path_len]) catch {
            self.finish(.no_listener);
            return;
        };
        const stream = address.connect(io) catch {
            self.finish(.no_listener);
            return;
        };
        // Connected, so somebody listens: unless the deadline passes with no
        // word at all (`handoffOnce`'s `no_reply`), a failure from here on is
        // the peer closing on us, not an absent or hung owner.
        var result: Handoff = .dropped;
        exchange: {
            var write_buffer: [max_line + 2]u8 = undefined;
            var writer = stream.writer(io, &write_buffer);
            writer.interface.writeAll(self.line_storage[0..self.line_len]) catch break :exchange;
            writer.interface.flush() catch break :exchange;
            var read_buffer: [16]u8 = undefined;
            var reader = stream.reader(io, &read_buffer);
            const answer = reader.interface.takeDelimiterExclusive('\n') catch break :exchange;
            if (std.mem.eql(u8, answer, "ok")) {
                result = .acked;
            } else if (std.mem.eql(u8, answer, "no")) {
                result = .refused;
            } else if (std.mem.eql(u8, answer, "busy")) {
                result = .busy;
            } else if (std.mem.eql(u8, answer, "mod")) {
                result = .other_mod;
            }
        }
        stream.close(io);
        self.finish(result);
    }

    fn finish(self: *HandoffJob, result: Handoff) void {
        self.result = result;
        // Abandoned while it worked: nobody is waiting for this.
        if (self.state.cmpxchgStrong(running, done, .acq_rel, .acquire) != null) allocator.destroy(self);
    }
};

fn handoffOnce(io: Io, address: *const net.UnixAddress, line: []const u8, timeout_ms: u32) Handoff {
    const job = HandoffJob.allocator.create(HandoffJob) catch return .no_listener;
    if (address.path.len > job.path_storage.len or line.len > job.line_storage.len) {
        HandoffJob.allocator.destroy(job);
        return .no_listener;
    }
    job.* = .{ .io = io };
    @memcpy(job.path_storage[0..address.path.len], address.path);
    job.path_len = address.path.len;
    @memcpy(job.line_storage[0..line.len], line);
    job.line_len = line.len;
    const thread = std.Thread.spawn(workerStack(256 * 1024), HandoffJob.run, .{job}) catch {
        HandoffJob.allocator.destroy(job);
        return .no_listener;
    };
    thread.detach();
    const deadline: Deadline = .in(io, timeout_ms);
    while (job.state.load(.acquire) != HandoffJob.done and !deadline.passed(io)) {
        io.sleep(.fromMilliseconds(5), .awake) catch break;
    }
    if (job.state.cmpxchgStrong(HandoffJob.running, HandoffJob.abandoned, .acq_rel, .acquire) == null) return .no_reply;
    const result = job.result;
    HandoffJob.allocator.destroy(job);
    return result;
}

fn handoffWithRetry(io: Io, address: *const net.UnixAddress, line: []const u8, options: Options) Handoff {
    var result = handoffOnce(io, address, line, options.timeout_ms);
    var tries: u8 = 0;
    // An owner that has bound but not yet begun to listen refuses once; give it
    // a moment before the file is called stale. An owner at its connection limit
    // is asked again too. A hung owner is not retried.
    while ((result == .no_listener or result == .busy) and tries < options.stale_retries) : (tries += 1) {
        io.sleep(.fromMilliseconds(options.stale_retry_ms), .awake) catch break;
        result = handoffOnce(io, address, line, options.timeout_ms);
    }
    return result;
}

fn startServer(gpa: std.mem.Allocator, io: Io, address: *const net.UnixAddress, endpoint: []const u8, options: Options) !*Instance {
    const server = try address.listen(io, .{});
    const instance = gpa.create(Instance) catch {
        var closing = server;
        closing.deinit(io);
        return error.OutOfMemory;
    };
    instance.* = .{ .gpa = gpa, .io = io, .server = server, .timeout_ms = options.timeout_ms };
    instance.setMod(options.mod);
    @memcpy(instance.path_storage[0..endpoint.len], endpoint);
    instance.path_len = endpoint.len;
    instance.bound_inode = socketFileInode(io, endpoint);
    instance.thread = std.Thread.spawn(workerStack(256 * 1024), Instance.serve, .{instance}) catch {
        instance.server.deinit(io);
        Io.Dir.cwd().deleteFile(io, endpoint) catch {};
        gpa.destroy(instance);
        return error.ThreadQuotaExceeded;
    };
    return instance;
}

pub const Acquired = union(enum) {
    /// This process owns the endpoint and answers it.
    primary: *Instance,
    /// Another editor took the line: the caller exits 0.
    handed_off,
    /// No endpoint could be had (no user root, an unsupported system): the
    /// editor starts as it would without this feature. The text says why.
    unavailable: []const u8,
};

/// Binds the endpoint at `endpoint` or hands `line` to whoever owns it. A
/// stale or hung owner's socket file is removed and the bind tried again.
pub fn acquireAt(gpa: std.mem.Allocator, io: Io, endpoint: []const u8, line: []const u8, options: Options) Acquired {
    if (endpoint.len > path_capacity) return .{ .unavailable = "the endpoint path is too long" };
    // The std checks a Unix address against Linux's 108 bytes only and panics
    // past the shorter sun_path of macOS (104): the limit is ours to keep.
    if (builtin.os.tag != .windows and endpoint.len > posix_path_limit) return .{ .unavailable = "the endpoint path is too long for a socket" };
    const address = net.UnixAddress.init(endpoint) catch return .{ .unavailable = "the endpoint path is too long for a socket" };
    if (std.fs.path.dirname(endpoint)) |directory| {
        if (options.private_folder) {
            if (!prepareFolder(io, directory)) return .{ .unavailable = "the folder for the endpoint is not private to this user" };
        } else {
            Io.Dir.cwd().createDirPath(io, directory) catch {};
        }
    }
    var attempt: u8 = 0;
    while (attempt < 2) : (attempt += 1) {
        if (startServer(gpa, io, &address, endpoint, options)) |instance| {
            return .{ .primary = instance };
        } else |err| switch (err) {
            error.AddressInUse => {},
            else => return .{ .unavailable = @errorName(err) },
        }
        // Something that is not a socket at the path (a leftover regular file)
        // is stale without asking; the std panics on a connect to one. A socket
        // file is asked. (Windows reports its AF_UNIX files as ordinary ones,
        // so the shortcut is for the other systems.)
        const not_a_socket = builtin.os.tag != .windows and blk: {
            const stat = Io.Dir.cwd().statFile(io, endpoint, .{}) catch break :blk false;
            break :blk stat.kind != .unix_domain_socket;
        };
        if (!not_a_socket) {
            const result = handoffWithRetry(io, &address, line, options);
            switch (result) {
                .acked, .refused => return .handed_off,
                // A live owner that did not take the line: its socket file stays,
                // and this launch runs without single-instance rather than exit
                // with the map unopened (WR-A01).
                .busy => return .{ .unavailable = "the running editor is busy" },
                .dropped => return .{ .unavailable = "the running editor did not take the line" },
                // The map would have opened under the other mod's objects, or
                // the -mod= been dropped: this launch runs on its own instead.
                .other_mod => return .{ .unavailable = "the running editor has another mod loaded" },
                .no_listener, .no_reply => {},
            }
        }
        // Stale or hung: take the file away and bind our own.
        Io.Dir.cwd().deleteFile(io, endpoint) catch |err| switch (err) {
            error.FileNotFound => {},
            else => return .{ .unavailable = "a stale socket file could not be removed" },
        };
    }
    return .{ .unavailable = "the endpoint stayed busy" };
}

/// `acquireAt` at the per-user endpoint of the running environment. Null-like
/// `unavailable` when no endpoint path can be built.
pub fn acquire(gpa: std.mem.Allocator, io: Io, environ: std.process.Environ, line: []const u8, options: Options) Acquired {
    var storage: EnvStorage = .{};
    defer storage.deinit(gpa);
    const env = Env.fromEnviron(gpa, environ, &storage);
    var buffer: [path_capacity]u8 = undefined;
    const endpoint = endpointFor(&buffer, env, builtin.os.tag) orelse return .{ .unavailable = "no user folder to keep the endpoint in" };
    var chosen = options;
    chosen.private_folder = endpoint.private_folder;
    return acquireAt(gpa, io, endpoint.path, line, chosen);
}

// -- Tests ---------------------------------------------------------------------

test "endpoint: the user root's mapeditor folder, the engine's own root rules" {
    var buffer: [path_capacity]u8 = undefined;
    try std.testing.expectEqualStrings(
        "/data/Nival/Blitzkrieg/mapeditor/instance.sock",
        endpointPath(&buffer, .{ .xdg_data_home = "/data", .home = "/home/me" }, .linux).?,
    );
    try std.testing.expectEqualStrings(
        "/Users/me/.local/share/Nival/Blitzkrieg/mapeditor/instance.sock",
        endpointPath(&buffer, .{ .home = "/Users/me/" }, .macos).?,
    );
    // The Windows form follows SDL_GetPrefPath("Nival", "Blitzkrieg").
    try std.testing.expectEqualStrings(
        "C:\\Users\\me\\AppData\\Roaming\\Nival\\Blitzkrieg\\mapeditor\\instance.sock",
        endpointPath(&buffer, .{ .appdata = "C:\\Users\\me\\AppData\\Roaming" }, .windows).?,
    );
    try std.testing.expectEqualStrings(
        "C:\\a\\Nival\\Blitzkrieg\\mapeditor\\instance.sock",
        endpointPath(&buffer, .{ .appdata = "C:\\a\\" }, .windows).?,
    );
    // No user root, no endpoint.
    try std.testing.expect(endpointPath(&buffer, .{}, .macos) == null);
    try std.testing.expect(endpointPath(&buffer, .{ .home = "/h" }, .windows) == null);
}

test "endpoint: a root too long for a socket address falls back to a short per-user name" {
    var buffer: [path_capacity]u8 = undefined;
    const long_home = "/Users/a-very-long-user-name-indeed/with/some/more/folders/to/make/it/long";
    const a = endpointPath(&buffer, .{ .home = long_home, .tmpdir = "/var/folders/xy/abc/T/", .uid = 501 }, .macos).?;
    try std.testing.expect(a.len <= posix_path_limit);
    try std.testing.expect(std.mem.startsWith(u8, a, "/var/folders/xy/abc/T/bk-mapeditor-501/"));
    try std.testing.expect(std.mem.endsWith(u8, a, ".sock"));
    // The fallback asks for a private folder; the preferred endpoint does not (WR-A04).
    var flag_buffer: [path_capacity]u8 = undefined;
    try std.testing.expect(endpointFor(&flag_buffer, .{ .home = long_home, .uid = 501 }, .linux).?.private_folder);
    try std.testing.expect(!endpointFor(&flag_buffer, .{ .home = "/h" }, .linux).?.private_folder);
    // Stable for one user root, different for another.
    var other_buffer: [path_capacity]u8 = undefined;
    const again = endpointPath(&other_buffer, .{ .home = long_home, .tmpdir = "/var/folders/xy/abc/T/", .uid = 501 }, .macos).?;
    try std.testing.expectEqualStrings(a, again);
    var third_buffer: [path_capacity]u8 = undefined;
    const other = endpointPath(&third_buffer, .{ .home = long_home ++ "2", .tmpdir = "/var/folders/xy/abc/T/", .uid = 501 }, .macos).?;
    try std.testing.expect(!std.mem.eql(u8, a, other));
    // With no TMPDIR it is /tmp.
    try std.testing.expect(std.mem.startsWith(u8, endpointPath(&buffer, .{ .home = long_home }, .linux).?, "/tmp/bk-mapeditor-"));
    // A TMPDIR too long for a socket address falls back to /tmp (the macOS
    // sun_path holds 104 bytes), and a path that fits nowhere is no endpoint.
    const long_tmp = "/var/folders/xy/abcdefghijklmnopqrstuvwxyz0123456789abcdef/T/and/more/folders/yet";
    const short = endpointPath(&buffer, .{ .home = long_home, .tmpdir = long_tmp, .uid = 501 }, .macos).?;
    try std.testing.expect(short.len <= posix_path_limit);
    try std.testing.expect(std.mem.startsWith(u8, short, "/tmp/bk-mapeditor-501/"));
    var tiny: [24]u8 = undefined;
    try std.testing.expect(endpointPath(&tiny, .{ .home = long_home, .uid = 501 }, .macos) == null);
}

test "framing: one line, one path - a control character, a second line and a flood are refused" {
    var buffer: [max_line + 4]u8 = undefined;
    try std.testing.expectEqualStrings("/maps/a.bzm\n", frameLine(&buffer, "/maps/a.bzm").?);
    try std.testing.expectEqualStrings("\n", frameLine(&buffer, "").?);
    try std.testing.expect(frameLine(&buffer, "/maps/a.bzm\n/etc/passwd") == null);
    try std.testing.expect(frameLine(&buffer, "/maps/a\x1b[2J.bzm") == null);
    var long: [max_line + 1]u8 = undefined;
    @memset(&long, 'x');
    try std.testing.expect(frameLine(&buffer, &long) == null);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(frameLine(&tiny, "/maps/a.bzm") == null);

    try std.testing.expectEqualStrings("/maps/a.bzm", parseLine("/maps/a.bzm").?.open);
    try std.testing.expectEqualStrings("C:\\maps\\a.bzm", parseLine("C:\\maps\\a.bzm\r").?.open);
    try std.testing.expect(parseLine("").? == .raise);
    try std.testing.expect(parseLine("\r").? == .raise);
    try std.testing.expect(parseLine("a\x00b") == null);
    try std.testing.expect(parseLine("a\tb") == null);
    try std.testing.expect(parseLine(&long) == null);
}

test "framing: a mod travels before the path, and the separator is the only control character let through (WR-A03)" {
    var buffer: [max_line + 4]u8 = undefined;
    const framed = frameOpenInMod(&buffer, "MyMod", "/maps/a.bzm").?;
    try std.testing.expectEqualStrings("MyMod\x1f/maps/a.bzm\n", framed);
    const parsed = parseLine(framed[0 .. framed.len - 1]).?.open_in_mod;
    try std.testing.expectEqualStrings("MyMod", parsed.mod);
    try std.testing.expectEqualStrings("/maps/a.bzm", parsed.path);
    // -mod=None is an empty mod, and still a mod.
    try std.testing.expectEqualStrings("\x1f/maps/a.bzm\n", frameOpenInMod(&buffer, "", "/maps/a.bzm").?);
    try std.testing.expectEqualStrings("", parseLine("\x1f/maps/a.bzm").?.open_in_mod.mod);
    // Nothing else rides along: a second separator, a control character in
    // either part, no path, a mod too long, a plain path holding the separator.
    try std.testing.expect(parseLine("m\x1fa\x1fb") == null);
    try std.testing.expect(parseLine("m\x1b\x1f/maps/a.bzm") == null);
    try std.testing.expect(parseLine("m\x1f/maps/a\x1b.bzm") == null);
    try std.testing.expect(parseLine("m\x1f") == null);
    try std.testing.expect(frameOpenInMod(&buffer, "x" ** (max_mod_len + 1), "/maps/a.bzm") == null);
    try std.testing.expect(frameOpenInMod(&buffer, "m", "") == null);
    try std.testing.expect(frameLine(&buffer, "m\x1f/maps/a.bzm") == null);
}

test "single instance: a launch naming another mod is told so and queues nothing; the same mod is handed over (WR-A03)" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "mod");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);
    const owner = switch (acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 2000, .mod = "ModA" })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer owner.deinit();

    var line_buffer: [max_line + 4]u8 = undefined;
    const address = try net.UnixAddress.init(endpoint);
    // Another mod, none, and a plain launch with no -mod= at all.
    try std.testing.expectEqual(Handoff.other_mod, handoffOnce(io, &address, frameOpenInMod(&line_buffer, "ModB", "/maps/b.bzm").?, 2000));
    try std.testing.expectEqual(Handoff.other_mod, handoffOnce(io, &address, frameOpenInMod(&line_buffer, "", "/maps/b.bzm").?, 2000));
    var out: [max_line]u8 = undefined;
    try std.testing.expect(owner.poll(&out) == null);
    switch (acquireAt(gpa, io, endpoint, frameOpenInMod(&line_buffer, "ModB", "/maps/b.bzm").?, .{ .timeout_ms = 2000, .stale_retries = 0 })) {
        .unavailable => {},
        else => return error.TestUnexpectedResult,
    }
    // The same mod goes through, and so does a launch that named none.
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, frameOpenInMod(&line_buffer, "ModA", "/maps/a.bzm").?, 2000));
    try std.testing.expectEqualStrings("/maps/a.bzm", owner.poll(&out).?.open);
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, "/maps/plain.bzm\n", 2000));
    try std.testing.expectEqualStrings("/maps/plain.bzm", owner.poll(&out).?.open);
    // File > Mod changed the owner's mod: the answer follows.
    owner.setMod("ModB");
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, frameOpenInMod(&line_buffer, "ModB", "/maps/b.bzm").?, 2000));
    try std.testing.expectEqualStrings("/maps/b.bzm", owner.poll(&out).?.open);
    owner.setMod(null);
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, frameOpenInMod(&line_buffer, "", "/maps/n.bzm").?, 2000));
}

test "stale-peer policy: nobody listening and a hung owner are stale, anything answered is not" {
    try std.testing.expect(endpointIsStale(.no_listener));
    try std.testing.expect(endpointIsStale(.no_reply));
    try std.testing.expect(!endpointIsStale(.acked));
    try std.testing.expect(!endpointIsStale(.refused));
    // Connected and closed on, or told busy: alive, so not stale (WR-A01).
    try std.testing.expect(!endpointIsStale(.dropped));
    try std.testing.expect(!endpointIsStale(.busy));
}

/// A socket path for a test: in zig-out/local-test, absolute where the platform
/// needs it and relative where an absolute one would not fit a socket address.
fn testEndpoint(io: Io, buffer: *[path_capacity]u8, name: []const u8) []const u8 {
    Io.Dir.cwd().createDirPath(io, "zig-out/local-test") catch {};
    var cwd_buffer: [path_capacity]u8 = undefined;
    if (std.process.currentPath(io, &cwd_buffer)) |cwd_len| {
        const absolute = std.fmt.bufPrint(buffer, "{s}{c}zig-out{c}local-test{c}si-{s}.sock", .{ cwd_buffer[0..cwd_len], std.fs.path.sep, std.fs.path.sep, std.fs.path.sep, name }) catch "";
        if (absolute.len != 0 and (builtin.os.tag == .windows or absolute.len <= posix_path_limit)) return absolute;
    } else |_| {}
    return std.fmt.bufPrint(buffer, "zig-out/local-test/si-{s}.sock", .{name}) catch unreachable;
}

fn removeTestEndpoint(io: Io, endpoint: []const u8) void {
    Io.Dir.cwd().deleteFile(io, endpoint) catch {};
}

test "single instance: a second launch hands its path over, the owner polls it, and the second exits" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "handoff");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);

    const first = acquireAt(gpa, io, endpoint, "/maps/first.bzm\n", .{ .timeout_ms = 2000 });
    const owner = switch (first) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer owner.deinit();

    var out: [max_line]u8 = undefined;
    try std.testing.expect(owner.poll(&out) == null);

    // The second launch: it does not become an owner, it hands the line over.
    const second = acquireAt(gpa, io, endpoint, "/maps/second.bzm\n", .{ .timeout_ms = 2000 });
    try std.testing.expect(second == .handed_off);
    const third = acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 2000 });
    try std.testing.expect(third == .handed_off);

    const got = owner.poll(&out).?;
    try std.testing.expectEqualStrings("/maps/second.bzm", got.open);
    try std.testing.expect(owner.poll(&out).? == .raise);
    try std.testing.expect(owner.poll(&out) == null);
}

test "single instance: a hostile line is answered no and queues nothing, a flood stops at the queue's size" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "hostile");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);
    const owner = switch (acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 2000 })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer owner.deinit();

    const address = try net.UnixAddress.init(endpoint);
    // A line with a control character: refused, nothing queued.
    try std.testing.expectEqual(Handoff.refused, handoffOnce(io, &address, "/maps/a\x1b.bzm\n", 2000));
    var out: [max_line]u8 = undefined;
    try std.testing.expect(owner.poll(&out) == null);
    // More lines than the queue holds: the extra ones are refused.
    var accepted: usize = 0;
    var refused: usize = 0;
    for (0..queue_capacity + 3) |_| switch (handoffOnce(io, &address, "/maps/x.bzm\n", 2000)) {
        .acked => accepted += 1,
        .refused => refused += 1,
        else => return error.TestUnexpectedResult,
    };
    try std.testing.expectEqual(queue_capacity, accepted);
    try std.testing.expectEqual(@as(usize, 3), refused);
    // Draining makes room again.
    try std.testing.expect(owner.poll(&out) != null);
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, "/maps/y.bzm\n", 2000));
}

test "single instance: a client that connects and says nothing does not keep the owner from the next launch" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "silent");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);
    const owner = switch (acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 400 })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer owner.deinit();

    const address = try net.UnixAddress.init(endpoint);
    // Two silent clients: connected, never a line.
    const silent_a = try address.connect(io);
    defer silent_a.close(io);
    const silent_b = try address.connect(io);
    defer silent_b.close(io);
    // A real launch is answered at once all the same.
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, "/maps/real.bzm\n", 2000));
    var out: [max_line]u8 = undefined;
    try std.testing.expectEqualStrings("/maps/real.bzm", owner.poll(&out).?.open);
}

test "single instance: an owner at its connection limit says busy and keeps its socket (WR-A01)" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "busy");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);
    const owner = switch (acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 3000 })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer owner.deinit();

    const address = try net.UnixAddress.init(endpoint);
    // Every connection slot held by a client that says nothing.
    var silent: [max_connections]net.Stream = undefined;
    for (&silent) |*stream| stream.* = try address.connect(io);
    defer for (silent) |stream| stream.close(io);
    var waited: u32 = 0;
    while (owner.connections.load(.acquire) < max_connections and waited < 2000) : (waited += 10) {
        try io.sleep(.fromMilliseconds(10), .awake);
    }
    try std.testing.expectEqual(@as(u32, max_connections), owner.connections.load(.acquire));
    // The next launch is told so - not left to read its silence as a dead owner.
    try std.testing.expectEqual(Handoff.busy, handoffOnce(io, &address, "/maps/late.bzm\n", 2000));
    switch (acquireAt(gpa, io, endpoint, "/maps/late.bzm\n", .{ .timeout_ms = 2000, .stale_retries = 0 })) {
        .unavailable => {},
        else => return error.TestUnexpectedResult,
    }
    // Its socket file is still the owner's, not removed or replaced.
    try std.testing.expectEqual(owner.bound_inode, socketFileInode(io, endpoint));
    try std.testing.expect(owner.bound_inode != null);
}

const Closer = struct {
    io: Io,
    server: *net.Server,

    /// Accepts one connection and closes it unanswered.
    fn run(self: *Closer) void {
        if (self.server.accept(self.io)) |stream| stream.close(self.io) else |_| {}
    }
};

test "single instance: a live listener that closes on us is not stale - its socket file stays (WR-A01)" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "dropped");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);
    const address = try net.UnixAddress.init(endpoint);
    var server = try address.listen(io, .{});
    defer server.deinit(io);
    const listening = socketFileInode(io, endpoint) orelse return error.TestUnexpectedResult;
    var closer: Closer = .{ .io = io, .server = &server };
    const thread = try std.Thread.spawn(.{}, Closer.run, .{&closer});
    defer thread.join();

    const result = acquireAt(gpa, io, endpoint, "/maps/a.bzm\n", .{ .timeout_ms = 2000, .stale_retries = 0 });
    switch (result) {
        .unavailable => {},
        .primary => |instance| {
            instance.deinit();
            return error.TestUnexpectedResult;
        },
        .handed_off => return error.TestUnexpectedResult,
    }
    // Not removed, not replaced: the file is still there for the listener.
    try std.testing.expectEqual(@as(?Io.File.INode, listening), socketFileInode(io, endpoint));
}

test "single instance: deinit neither hangs on a replaced socket nor removes the replacement (WR-A02)" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "replaced");
    var moved_buffer: [path_capacity + 8]u8 = undefined;
    const moved = try std.fmt.bufPrint(&moved_buffer, "{s}.old", .{endpoint});
    removeTestEndpoint(io, endpoint);
    removeTestEndpoint(io, moved);
    defer removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, moved);

    // The first owner. A detached accept thread keeps its memory alive past
    // this test, so it does not come from the leak-checking allocator.
    const first = switch (acquireAt(std.heap.smp_allocator, io, endpoint, "\n", .{ .timeout_ms = 1000 })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    // It knows its own socket file, so it can tell a replacement from it (on
    // Windows that file is a reparse point and must be read as one).
    try std.testing.expect(first.bound_inode != null);
    // Its socket file goes away from the path (a tmp cleaner, a launch that
    // judged it stale) and another editor binds there.
    try Io.Dir.rename(Io.Dir.cwd(), endpoint, Io.Dir.cwd(), moved, io);
    const second = switch (acquireAt(gpa, io, endpoint, "\n", .{ .timeout_ms = 1000 })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    defer second.deinit();

    const started = Io.Clock.Timestamp.now(io, .awake);
    first.deinit();
    const elapsed_ms = @divTrunc(started.durationTo(Io.Clock.Timestamp.now(io, .awake)).raw.nanoseconds, std.time.ns_per_ms);
    // It gave up on its accept thread after about the wait; it did not hang.
    try std.testing.expect(elapsed_ms < accept_wait_ms + 3000);
    // The replacement's socket file is still its own, and it still answers.
    const address = try net.UnixAddress.init(endpoint);
    try std.testing.expectEqual(Handoff.acked, handoffOnce(io, &address, "/maps/b.bzm\n", 2000));
    var out: [max_line]u8 = undefined;
    try std.testing.expectEqualStrings("/maps/b.bzm", second.poll(&out).?.open);
}

test "single instance: the fallback endpoint's folder is made private, and one left open is refused (WR-A04)" {
    if (builtin.os.tag == .windows) return error.SkipZigTest;
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    // <folder>/s.sock, the whole path no longer than a plain test socket's, so it
    // fits the sun_path of macOS (104 bytes) wherever the checkout is.
    const sock = testEndpoint(io, &endpoint_buffer, "private/s");
    const folder = std.fs.path.dirname(sock).?;
    Io.Dir.cwd().deleteFile(io, sock) catch {};
    Io.Dir.cwd().deleteDir(io, folder) catch {};
    defer Io.Dir.cwd().deleteDir(io, folder) catch {};
    defer removeTestEndpoint(io, sock);

    // Made by the first launch, with no access for anyone else.
    const owner = switch (acquireAt(gpa, io, sock, "\n", .{ .timeout_ms = 1000, .private_folder = true })) {
        .primary => |instance| instance,
        else => return error.TestUnexpectedResult,
    };
    const stat = try Io.Dir.cwd().statFile(io, folder, .{});
    try std.testing.expect(stat.permissions.toMode() & 0o077 == 0);
    owner.deinit();
    removeTestEndpoint(io, sock);

    // A folder that is open to others (somebody squatting the name, a leftover)
    // is not trusted with the socket.
    Io.Dir.cwd().deleteDir(io, folder) catch {};
    try Io.Dir.cwd().createDir(io, folder, @enumFromInt(0o777));
    try Io.Dir.cwd().setFilePermissions(io, folder, @enumFromInt(0o755), .{});
    switch (acquireAt(gpa, io, sock, "\n", .{ .timeout_ms = 1000, .private_folder = true })) {
        .unavailable => {},
        .primary => |instance| {
            instance.deinit();
            return error.TestUnexpectedResult;
        },
        .handed_off => return error.TestUnexpectedResult,
    }
}

test "single instance: a path longer than a socket address holds is unavailable, not a panic (WR-A04)" {
    if (builtin.os.tag == .windows) return error.SkipZigTest;
    const long = "/tmp/bk-test-too-long-for-sun-path/" ++ "x" ** 80 ++ ".sock";
    try std.testing.expect(long.len > posix_path_limit and long.len < path_capacity);
    switch (acquireAt(std.testing.allocator, std.testing.io, long, "\n", .{ .timeout_ms = 100 })) {
        .unavailable => {},
        .primary => |instance| {
            instance.deinit();
            return error.TestUnexpectedResult;
        },
        .handed_off => return error.TestUnexpectedResult,
    }
}

test "single instance: a stale socket file never blocks a start - the next launch takes the endpoint" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "stale");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);

    // A dead owner leaves its socket file behind: bind a socket and close it
    // without removing the file (posix) - on Windows the same, by leaving the
    // file of a listener that was dropped.
    {
        const address = try net.UnixAddress.init(endpoint);
        var server = try address.listen(io, .{});
        server.deinit(io);
    }
    // A plain file at the path is just as stale.
    const owner = switch (acquireAt(gpa, io, endpoint, "/maps/a.bzm\n", .{ .timeout_ms = 500, .stale_retries = 1, .stale_retry_ms = 10 })) {
        .primary => |instance| instance,
        .handed_off => return error.TestUnexpectedResult,
        .unavailable => |why| {
            std.debug.print("stale endpoint test: unavailable: {s}\n", .{why});
            return error.TestUnexpectedResult;
        },
    };
    owner.deinit();

    // And a regular file where the socket should be.
    if (Io.Dir.cwd().createFile(io, endpoint, .{ .truncate = true })) |file| {
        file.close(io);
        const again = switch (acquireAt(gpa, io, endpoint, "/maps/a.bzm\n", .{ .timeout_ms = 500, .stale_retries = 0 })) {
            .primary => |instance| instance,
            else => return error.TestUnexpectedResult,
        };
        again.deinit();
    } else |_| {}
}

test "single instance: a hung owner - one that accepts and never answers - is timed out and replaced" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var endpoint_buffer: [path_capacity]u8 = undefined;
    const endpoint = testEndpoint(io, &endpoint_buffer, "hung");
    removeTestEndpoint(io, endpoint);
    defer removeTestEndpoint(io, endpoint);

    // The hung owner: listening, its kernel queue takes the connection, nobody
    // ever accepts it.
    const address = try net.UnixAddress.init(endpoint);
    var hung = try address.listen(io, .{});
    defer hung.deinit(io);

    const started = Io.Clock.Timestamp.now(io, .awake);
    const result = acquireAt(gpa, io, endpoint, "/maps/a.bzm\n", .{ .timeout_ms = 200, .stale_retries = 0 });
    const elapsed_ms = @divTrunc(started.durationTo(Io.Clock.Timestamp.now(io, .awake)).raw.nanoseconds, std.time.ns_per_ms);
    switch (result) {
        .primary => |instance| instance.deinit(),
        else => return error.TestUnexpectedResult,
    }
    // Not a silent hang: it gave up after about the timeout.
    try std.testing.expect(elapsed_ms < 3000);
}
