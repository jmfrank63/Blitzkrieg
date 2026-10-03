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
//! and nothing else) and waits for `ok\n` (queued) or `no\n` (refused). No
//! commands travel the socket (T-05-11-01); the receiving editor validates the
//! path like any open (`panels_logic.dropVerdict`, then the guard and the
//! bridge's own read), so a hostile line can at worst name a file the editor
//! would open anyway, or be refused with a status note.
//!
//! Endpoint: `<user root>/mapeditor/instance.sock`, the user root being the one
//! the engine uses (Platform/Paths.cpp: `$XDG_DATA_HOME/Nival/Blitzkrieg`, else
//! `$HOME/.local/share/Nival/Blitzkrieg`; `%APPDATA%\Nival\Blitzkrieg\` on
//! Windows) - computed here from the environment because the check has to run
//! before the window and the engine exist. A path longer than a socket address
//! holds (104 bytes on macOS) falls back to a short file under the per-user
//! temporary folder, named after a hash of the user root.
//!
//! A stale or hung peer never blocks a start (T-05-11-02): connecting to a
//! socket file nobody listens on fails; one whose owner is hung answers
//! nothing within `timeout_ms`. Either way the second launch removes that one
//! socket file (a per-user path, never anything else), binds a fresh one and
//! starts normally.
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
};

// -- The endpoint ------------------------------------------------------------

/// What `endpointPath` reads of the environment, as plain slices so the rule is
/// testable without one.
pub const Env = struct {
    xdg_data_home: ?[]const u8 = null,
    home: ?[]const u8 = null,
    appdata: ?[]const u8 = null,
    tmpdir: ?[]const u8 = null,

    pub fn fromEnviron(gpa: std.mem.Allocator, environ: std.process.Environ, storage: *EnvStorage) Env {
        storage.* = .{};
        return .{
            .xdg_data_home = storage.get(gpa, environ, 0, "XDG_DATA_HOME"),
            .home = storage.get(gpa, environ, 1, "HOME"),
            .appdata = storage.get(gpa, environ, 2, "APPDATA"),
            .tmpdir = storage.get(gpa, environ, 3, "TMPDIR"),
        };
    }
};

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

/// `<user root>/mapeditor/instance.sock` (see the file's header), or null when
/// the environment names no user root or the result does not fit `buffer`.
/// `os` is a parameter so the Windows form is tested on every host.
pub fn endpointPath(buffer: []u8, env: Env, os: std.Target.Os.Tag) ?[]const u8 {
    if (os == .windows) {
        const appdata = env.appdata orelse return null;
        return std.fmt.bufPrint(buffer, "{s}{s}Nival\\Blitzkrieg\\mapeditor\\instance.sock", .{ appdata, if (std.mem.endsWith(u8, appdata, "\\")) "" else "\\" }) catch null;
    }
    var root_buffer: [path_capacity]u8 = undefined;
    const root: []const u8 = if (env.xdg_data_home) |xdg|
        std.fmt.bufPrint(&root_buffer, "{s}/Nival/Blitzkrieg", .{std.mem.trimEnd(u8, xdg, "/")}) catch return null
    else if (env.home) |home|
        std.fmt.bufPrint(&root_buffer, "{s}/.local/share/Nival/Blitzkrieg", .{std.mem.trimEnd(u8, home, "/")}) catch return null
    else
        return null;
    const preferred = std.fmt.bufPrint(buffer, "{s}/mapeditor/instance.sock", .{root}) catch return null;
    if (preferred.len <= posix_path_limit) return preferred;
    // Too long for a socket address: a short name in the per-user temporary
    // folder, still one per user root.
    const tmp = std.mem.trimEnd(u8, env.tmpdir orelse "/tmp", "/");
    return std.fmt.bufPrint(buffer, "{s}/bk-mapeditor-{x:0>16}.sock", .{ tmp, std.hash.Fnv1a_64.hash(root) }) catch null;
}

// -- One line, one path ------------------------------------------------------

/// What a line asks for.
pub const Request = union(enum) {
    /// An empty line: bring the window forward, open nothing.
    raise,
    open: []const u8,
};

/// A line read off the socket (without its newline), or null when it is not
/// one this protocol sends: a control character in it (a second line, an
/// escape sequence) or more than `max_line` bytes. A trailing CR is dropped.
pub fn parseLine(line: []const u8) ?Request {
    const body = std.mem.trimEnd(u8, line, "\r");
    if (body.len > max_line) return null;
    for (body) |byte| {
        if (byte < 0x20 or byte == 0x7f) return null;
    }
    if (body.len == 0) return .raise;
    return .{ .open = body };
}

/// `<path>\n` in `buffer` for the client to send; null when the path would not
/// parse back (a control character, too long) or does not fit.
pub fn frameLine(buffer: []u8, path: []const u8) ?[]const u8 {
    if (parseLine(path) == null) return null;
    if (path.len + 1 > buffer.len) return null;
    @memcpy(buffer[0..path.len], path);
    buffer[path.len] = '\n';
    return buffer[0 .. path.len + 1];
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
    thread: ?std.Thread = null,
    stopping: std.atomic.Value(bool) = .init(false),
    /// Connection threads running; `max_connections` at most (one more is closed unanswered).
    connections: std.atomic.Value(u32) = .init(0),
    mutex: Io.Mutex = .init,
    queue: [queue_capacity]Entry = undefined,
    head: usize = 0,
    count: usize = 0,

    pub fn path(self: *const Instance) []const u8 {
        return self.path_storage[0..self.path_len];
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
    /// accept thread is woken by a connection of our own and joined. A
    /// connection thread still stuck on a silent client (its watchdog could not
    /// wake the read) is waited for up to twice the timeout; past that the
    /// instance is left allocated - the process is on its way out - rather than
    /// freed under it.
    pub fn deinit(self: *Instance) void {
        self.stopping.store(true, .release);
        if (self.thread) |thread| {
            if (net.UnixAddress.init(self.path())) |address| {
                if (address.connect(self.io)) |stream| stream.close(self.io) else |_| {}
            } else |_| {}
            thread.join();
        }
        self.server.deinit(self.io);
        Io.Dir.cwd().deleteFile(self.io, self.path()) catch {};
        var waited: u32 = 0;
        while (self.connections.load(.acquire) != 0 and waited < self.timeout_ms * 2) : (waited += 10) {
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
        }
        self.count += 1;
        return true;
    }

    fn serve(self: *Instance) void {
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
                stream.close(self.io);
                continue;
            }
            _ = self.connections.fetchAdd(1, .acq_rel);
            const thread = std.Thread.spawn(.{ .stack_size = 256 * 1024 }, serveConnection, .{ self, stream }) catch {
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
        const line = reader.interface.takeDelimiterExclusive('\n') catch return;
        // Our own wake-up connection (deinit) sends nothing and is closed.
        if (self.stopping.load(.acquire)) return;
        const answer: []const u8 = blk: {
            const request = parseLine(line) orelse break :blk "no\n";
            break :blk if (self.enqueue(request)) "ok\n" else "no\n";
        };
        var write_buffer: [8]u8 = undefined;
        var writer = stream.writer(self.io, &write_buffer);
        writer.interface.writeAll(answer) catch return;
        writer.interface.flush() catch return;
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
        self.thread = std.Thread.spawn(.{ .stack_size = 128 * 1024 }, run, .{self}) catch null;
    }

    /// True when the deadline passed first.
    fn finish(self: *Watchdog) bool {
        self.done.store(true, .release);
        if (self.thread) |thread| thread.join();
        self.thread = null;
        return self.fired.load(.acquire);
    }

    fn run(self: *Watchdog) void {
        const step_ms: u32 = 10;
        var waited: u32 = 0;
        while (!self.done.load(.acquire)) {
            if (waited >= self.timeout_ms) {
                self.fired.store(true, .release);
                self.stream.shutdown(self.io, .both) catch {};
                return;
            }
            self.io.sleep(.fromMilliseconds(step_ms), .awake) catch return;
            waited += step_ms;
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
    /// Something accepted the connection and said nothing in time.
    no_reply,
};

/// Whether a failed hand-off means the endpoint is stale (T-05-11-02): nobody
/// listens, or the one who does is hung. Anything the owner answered is not.
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
        var result: Handoff = .no_reply;
        exchange: {
            var write_buffer: [max_line + 2]u8 = undefined;
            var writer = stream.writer(io, &write_buffer);
            writer.interface.writeAll(self.line_storage[0..self.line_len]) catch break :exchange;
            writer.interface.flush() catch break :exchange;
            var read_buffer: [16]u8 = undefined;
            var reader = stream.reader(io, &read_buffer);
            const answer = reader.interface.takeDelimiterExclusive('\n') catch break :exchange;
            if (std.mem.eql(u8, answer, "ok")) result = .acked else if (std.mem.eql(u8, answer, "no")) result = .refused;
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
    const thread = std.Thread.spawn(.{ .stack_size = 256 * 1024 }, HandoffJob.run, .{job}) catch {
        HandoffJob.allocator.destroy(job);
        return .no_listener;
    };
    thread.detach();
    var waited: u32 = 0;
    const step_ms: u32 = 5;
    while (waited <= timeout_ms) {
        if (job.state.load(.acquire) == HandoffJob.done) break;
        io.sleep(.fromMilliseconds(step_ms), .awake) catch break;
        waited += step_ms;
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
    // a moment before the file is called stale. A hung owner is not retried.
    while (result == .no_listener and tries < options.stale_retries) : (tries += 1) {
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
    @memcpy(instance.path_storage[0..endpoint.len], endpoint);
    instance.path_len = endpoint.len;
    instance.thread = std.Thread.spawn(.{ .stack_size = 256 * 1024 }, Instance.serve, .{instance}) catch {
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
    const address = net.UnixAddress.init(endpoint) catch return .{ .unavailable = "the endpoint path is too long for a socket" };
    if (std.fs.path.dirname(endpoint)) |directory| Io.Dir.cwd().createDirPath(io, directory) catch {};
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
            if (!endpointIsStale(result)) return .handed_off;
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
    const endpoint = endpointPath(&buffer, env, builtin.os.tag) orelse return .{ .unavailable = "no user folder to keep the endpoint in" };
    return acquireAt(gpa, io, endpoint, line, options);
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
    const a = endpointPath(&buffer, .{ .home = long_home, .tmpdir = "/var/folders/xy/abc/T/" }, .macos).?;
    try std.testing.expect(a.len <= posix_path_limit);
    try std.testing.expect(std.mem.startsWith(u8, a, "/var/folders/xy/abc/T/bk-mapeditor-"));
    try std.testing.expect(std.mem.endsWith(u8, a, ".sock"));
    // Stable for one user root, different for another.
    var other_buffer: [path_capacity]u8 = undefined;
    const again = endpointPath(&other_buffer, .{ .home = long_home, .tmpdir = "/var/folders/xy/abc/T/" }, .macos).?;
    try std.testing.expectEqualStrings(a, again);
    var third_buffer: [path_capacity]u8 = undefined;
    const other = endpointPath(&third_buffer, .{ .home = long_home ++ "2", .tmpdir = "/var/folders/xy/abc/T/" }, .macos).?;
    try std.testing.expect(!std.mem.eql(u8, a, other));
    // With no TMPDIR it is /tmp.
    try std.testing.expect(std.mem.startsWith(u8, endpointPath(&buffer, .{ .home = long_home }, .linux).?, "/tmp/bk-mapeditor-"));
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

test "stale-peer policy: nobody listening and a hung owner are stale, anything answered is not" {
    try std.testing.expect(endpointIsStale(.no_listener));
    try std.testing.expect(endpointIsStale(.no_reply));
    try std.testing.expect(!endpointIsStale(.acked));
    try std.testing.expect(!endpointIsStale(.refused));
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
