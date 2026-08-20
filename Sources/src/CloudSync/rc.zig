//! rclone rc JSON client.
//!
//! **Every method in this file blocks the calling thread.** `_async` makes the
//! *rclone job* asynchronous server-side; the initiating POST and each
//! `job/status` POST are ordinary synchronous HTTP requests that occupy the
//! caller until the daemon answers or the deadline expires. Calling any of
//! this from the game's main thread is a bug — P02-M02 provides the worker
//! thread that owns the socket, and `poll()` reads a snapshot instead.
//!
//! Deadlines. A wedged daemon must fail a call rather than hold it forever,
//! because a caller cannot recover from a hang it is never told about. Each
//! call is therefore split into two bounded phases — connect, then
//! send/receive — and each phase runs as a concurrent `std.Io` task raced
//! against a timer. Whichever finishes first wins; the loser is cancelled and
//! joined before the phase returns, so no task outlives the call that spawned
//! it. Expiry becomes `RcError.Timeout`.
//!
//! Why not `SO_RCVTIMEO`: on POSIX a receive timeout surfaces as `EAGAIN`, and
//! `std.Io.Threaded`'s `netRead` classifies `EAGAIN` as a programmer bug —
//! `std.debug.panic("programmer bug caused syscall error: AGAIN")` in Debug.
//! Cancellation is the mechanism `std.Io` actually supports for this, and on
//! POSIX it is delivered as `SIGIO`, which interrupts the blocked `readv`.
//!
//! Allocation failure is folded into `RcError.Transport`: the packet contract
//! fixes the error set at five members, and a call that could not be built is
//! a call that did not reach the daemon.

const std = @import("std");
const Io = std.Io;

const Connection = std.http.Client.Connection;

pub const Endpoint = struct {
    host: []const u8,
    port: u16,
    user: []const u8,
    pass: []const u8,
};

pub const RcError = error{ Transport, Unauthorized, RcFailed, BadJson, Timeout };

/// Per-call budget. `connect_ms` bounds establishing the TCP connection,
/// `read_ms` bounds sending the request and receiving the whole reply.
pub const Deadline = struct {
    connect_ms: u32 = 5_000,
    read_ms: u32 = 30_000,
};

/// An rc failure: HTTP 500 plus a JSON body such as
/// `{"error":"bisync aborted","status":500}`. It is not a transport problem
/// and must never be reported as one.
pub const RcFailure = struct {
    message: []const u8,
    status: i64,
};

pub const JobId = i64;

/// A parsed rc reply. Owns its JSON arena; `deinit` frees it.
pub const Reply = struct {
    parsed: std.json.Parsed(std.json.Value),
    value: std.json.Value,

    pub fn deinit(self: *Reply) void {
        self.parsed.deinit();
        self.* = undefined;
    }
};

/// The state of an `_async` job. Field lifetimes are tied to the embedded
/// `Reply`, so `deinit` must outlive every use of `error_text` and `output`.
pub const JobStatus = struct {
    reply: Reply,
    finished: bool,
    success: bool,
    error_text: []const u8,
    /// The whole `output` object. For bisync, `output.output` holds the run
    /// log, which is the only place an abort explains itself.
    output: std.json.Value,

    pub fn outputText(self: JobStatus) ?[]const u8 {
        const obj = switch (self.output) {
            .object => |o| o,
            else => return null,
        };
        const nested = obj.get("output") orelse return null;
        return switch (nested) {
            .string => |s| s,
            else => null,
        };
    }

    pub fn deinit(self: *JobStatus) void {
        self.reply.deinit();
        self.* = undefined;
    }
};

pub const Client = struct {
    gpa: std.mem.Allocator,
    io: Io,
    http: std.http.Client,
    port: u16,
    deadline: Deadline,
    /// Owned copy of the endpoint host; `Io.net.HostName` borrows it.
    host: []u8,
    /// `Basic <base64(user:pass)>`, built once here rather than per call.
    auth_header: []u8,
    /// `http://<host>:<port>`.
    url_prefix: []u8,
    failure: ?RcFailure,
    failure_message: ?[]u8,

    pub fn init(gpa: std.mem.Allocator, io: Io, endpoint: Endpoint) !Client {
        const host = try gpa.dupe(u8, endpoint.host);
        errdefer gpa.free(host);

        const credentials = try std.fmt.allocPrint(gpa, "{s}:{s}", .{ endpoint.user, endpoint.pass });
        defer gpa.free(credentials);

        const encoder = std.base64.standard.Encoder;
        const prefix = "Basic ";
        const auth_header = try gpa.alloc(u8, prefix.len + encoder.calcSize(credentials.len));
        errdefer gpa.free(auth_header);
        @memcpy(auth_header[0..prefix.len], prefix);
        _ = encoder.encode(auth_header[prefix.len..], credentials);

        const url_prefix = try std.fmt.allocPrint(gpa, "http://{s}:{d}", .{ endpoint.host, endpoint.port });
        errdefer gpa.free(url_prefix);

        return .{
            .gpa = gpa,
            .io = io,
            .http = .{ .allocator = gpa, .io = io },
            .port = endpoint.port,
            .deadline = .{},
            .host = host,
            .auth_header = auth_header,
            .url_prefix = url_prefix,
            .failure = null,
            .failure_message = null,
        };
    }

    pub fn deinit(self: *Client) void {
        self.clearFailure();
        self.http.deinit();
        self.gpa.free(self.url_prefix);
        self.gpa.free(self.auth_header);
        self.gpa.free(self.host);
        self.* = undefined;
    }

    /// The detail of the most recent `error.RcFailed`. Cleared at the start of
    /// every call.
    pub fn lastFailure(self: *const Client) ?RcFailure {
        return self.failure;
    }

    /// POST `params` to `http://host:port/<method>` and parse the reply.
    pub fn call(self: *Client, method: []const u8, params: std.json.Value) RcError!Reply {
        const body = self.encodeParams(params, false) catch return error.Transport;
        defer self.gpa.free(body);
        return self.post(method, body);
    }

    /// As `call`, but injects `"_async": true` so rclone runs the operation as
    /// a job and answers immediately with its id.
    pub fn callAsync(self: *Client, method: []const u8, params: std.json.Value) RcError!JobId {
        const body = self.encodeParams(params, true) catch return error.Transport;
        defer self.gpa.free(body);

        var reply = try self.post(method, body);
        defer reply.deinit();

        const object = switch (reply.value) {
            .object => |o| o,
            else => return error.BadJson,
        };
        const id = object.get("jobid") orelse return error.BadJson;
        return switch (id) {
            .integer => |i| i,
            .float => |f| @intFromFloat(f),
            else => error.BadJson,
        };
    }

    pub fn jobStatus(self: *Client, id: JobId) RcError!JobStatus {
        var params: std.json.ObjectMap = .empty;
        defer params.deinit(self.gpa);
        params.put(self.gpa, "jobid", .{ .integer = id }) catch return error.Transport;

        const body = std.json.Stringify.valueAlloc(
            self.gpa,
            std.json.Value{ .object = params },
            .{},
        ) catch return error.Transport;
        defer self.gpa.free(body);

        var reply = try self.post("job/status", body);
        errdefer reply.deinit();

        const object = switch (reply.value) {
            .object => |o| o,
            else => return error.BadJson,
        };
        return .{
            .reply = reply,
            .finished = boolField(object, "finished"),
            .success = boolField(object, "success"),
            .error_text = stringField(object, "error"),
            .output = object.get("output") orelse .null,
        };
    }

    fn boolField(object: std.json.ObjectMap, name: []const u8) bool {
        const v = object.get(name) orelse return false;
        return switch (v) {
            .bool => |b| b,
            else => false,
        };
    }

    fn stringField(object: std.json.ObjectMap, name: []const u8) []const u8 {
        const v = object.get(name) orelse return "";
        return switch (v) {
            .string => |s| s,
            else => "",
        };
    }

    fn encodeParams(self: *Client, params: std.json.Value, force_async: bool) ![]u8 {
        var owned: std.json.ObjectMap = .empty;
        defer owned.deinit(self.gpa);

        switch (params) {
            .object => |source| {
                var it = source.iterator();
                while (it.next()) |entry| try owned.put(self.gpa, entry.key_ptr.*, entry.value_ptr.*);
            },
            .null => {},
            // rc only accepts a JSON object as the request body.
            else => return error.InvalidParams,
        }
        if (force_async) try owned.put(self.gpa, "_async", .{ .bool = true });

        return std.json.Stringify.valueAlloc(self.gpa, std.json.Value{ .object = owned }, .{});
    }

    fn post(self: *Client, method: []const u8, body: []u8) RcError!Reply {
        self.clearFailure();
        const connection = try self.connectWithDeadline();
        return self.exchangeWithDeadline(connection, method, body);
    }

    // -- deadline plumbing ---------------------------------------------------

    const ConnectResult = RcError!*Connection;
    const ConnectOutcome = union(enum) { connected: ConnectResult, expired: void };
    const ExchangeResult = RcError!Reply;
    const ExchangeOutcome = union(enum) { done: ExchangeResult, expired: void };

    fn sleepMs(io: Io, ms: u32) void {
        const duration: Io.Clock.Duration = .{
            .raw = .fromMilliseconds(@intCast(ms)),
            .clock = .awake,
        };
        // Cancellation is the normal way out when the other task wins.
        duration.sleep(io) catch {};
    }

    fn connectWithDeadline(self: *Client) RcError!*Connection {
        var slots: [2]ConnectOutcome = undefined;
        var select: Io.Select(ConnectOutcome) = .init(self.io, &slots);

        select.concurrent(.connected, doConnect, .{self}) catch return error.Transport;
        select.concurrent(.expired, sleepMs, .{ self.io, self.deadline.connect_ms }) catch {
            self.drainConnect(&select);
            return error.Transport;
        };

        const first = select.await() catch {
            self.drainConnect(&select);
            return error.Transport;
        };
        switch (first) {
            .connected => |result| {
                // Only the timer is left, and its result carries no resources.
                select.cancelDiscard();
                return result;
            },
            .expired => {
                self.drainConnect(&select);
                return error.Timeout;
            },
        }
    }

    fn drainConnect(self: *Client, select: *Io.Select(ConnectOutcome)) void {
        while (select.cancel()) |leftover| switch (leftover) {
            .connected => |result| if (result) |connection| {
                self.http.connection_pool.release(connection, self.io);
            } else |_| {},
            .expired => {},
        };
    }

    fn exchangeWithDeadline(
        self: *Client,
        connection: *Connection,
        method: []const u8,
        body: []u8,
    ) RcError!Reply {
        var slots: [2]ExchangeOutcome = undefined;
        var select: Io.Select(ExchangeOutcome) = .init(self.io, &slots);

        select.concurrent(.done, doExchange, .{ self, connection, method, body }) catch {
            self.http.connection_pool.release(connection, self.io);
            return error.Transport;
        };
        select.concurrent(.expired, sleepMs, .{ self.io, self.deadline.read_ms }) catch {
            drainExchange(&select);
            return error.Transport;
        };

        const first = select.await() catch {
            drainExchange(&select);
            return error.Transport;
        };
        switch (first) {
            .done => |result| {
                select.cancelDiscard();
                return result;
            },
            .expired => {
                drainExchange(&select);
                return error.Timeout;
            },
        }
    }

    fn drainExchange(select: *Io.Select(ExchangeOutcome)) void {
        while (select.cancel()) |leftover| switch (leftover) {
            .done => |result| if (result) |reply| {
                var owned = reply;
                owned.deinit();
            } else |_| {},
            .expired => {},
        };
    }

    // -- the blocking work itself -------------------------------------------

    fn doConnect(self: *Client) ConnectResult {
        const host = Io.net.HostName.init(self.host) catch return error.Transport;
        return self.http.connect(host, self.port, .plain) catch error.Transport;
    }

    fn doExchange(
        self: *Client,
        connection: *Connection,
        method: []const u8,
        body: []u8,
    ) ExchangeResult {
        var url_buffer: [512]u8 = undefined;
        const url = std.fmt.bufPrint(&url_buffer, "{s}/{s}", .{ self.url_prefix, method }) catch
            return error.Transport;
        const uri = std.Uri.parse(url) catch return error.Transport;

        var request = self.http.request(.POST, uri, .{
            .connection = connection,
            // One connection per call: a cancelled call leaves the protocol
            // state undefined, and a pooled undefined connection is a landmine.
            .keep_alive = false,
            .redirect_behavior = .not_allowed,
            .headers = .{
                .authorization = .{ .override = self.auth_header },
                .content_type = .{ .override = "application/json" },
            },
        }) catch {
            // The connection was acquired but never handed to a Request, so
            // nothing else will give it back.
            self.http.connection_pool.release(connection, self.io);
            return error.Transport;
        };
        defer request.deinit();

        request.sendBodyComplete(body) catch return error.Transport;

        var redirect_buffer: [64]u8 = undefined;
        var response = request.receiveHead(&redirect_buffer) catch |err| switch (err) {
            error.ReadFailed => return readFailure(connection),
            else => return error.Transport,
        };

        const status = response.head.status;
        var transfer_buffer: [4096]u8 = undefined;
        const reader = response.reader(&transfer_buffer);
        const payload = reader.allocRemaining(self.gpa, .limited(8 * 1024 * 1024)) catch |err| switch (err) {
            error.ReadFailed => return readFailure(connection),
            else => return error.Transport,
        };
        defer self.gpa.free(payload);

        if (status == .unauthorized or status == .proxy_auth_required) {
            return error.Unauthorized;
        }

        var parsed = std.json.parseFromSlice(std.json.Value, self.gpa, payload, .{}) catch
            return error.BadJson;

        if (status.class() != .success) {
            // An rc failure is HTTP 500 with a JSON body. Keep the message: the
            // status alone says nothing a caller can act on.
            self.recordFailure(parsed.value, status);
            parsed.deinit();
            return error.RcFailed;
        }

        return .{ .parsed = parsed, .value = parsed.value };
    }

    /// Recover the socket-level reason behind `error.ReadFailed`. A cancelled
    /// task is how the deadline unwinds, and the phase that raced it reports
    /// `Timeout`; anything else here is a genuine transport fault.
    fn readFailure(connection: *Connection) RcError {
        // Read directly rather than through `getReadError`, which unwraps the
        // optional and would panic when `ReadFailed` came from the HTTP body
        // framing rather than from the socket.
        const err = connection.stream_reader.err orelse return error.Transport;
        return switch (err) {
            error.Timeout => error.Timeout,
            else => error.Transport,
        };
    }

    fn recordFailure(self: *Client, value: std.json.Value, status: std.http.Status) void {
        var message: []const u8 = "";
        var code: i64 = @intFromEnum(status);
        if (value == .object) {
            if (value.object.get("error")) |field| {
                if (field == .string) message = field.string;
            }
            if (value.object.get("status")) |field| switch (field) {
                .integer => |i| code = i,
                .float => |f| code = @intFromFloat(f),
                else => {},
            };
        }
        if (self.gpa.dupe(u8, message)) |owned| {
            self.failure_message = owned;
            self.failure = .{ .message = owned, .status = code };
        } else |_| {
            self.failure = .{ .message = "", .status = code };
        }
    }

    fn clearFailure(self: *Client) void {
        if (self.failure_message) |owned| self.gpa.free(owned);
        self.failure_message = null;
        self.failure = null;
    }
};
