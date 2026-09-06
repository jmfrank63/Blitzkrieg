//! The rc config state machine, driven without a terminal.
//!
//! rclone's interactive configuration — the path every OAuth backend needs —
//! is a state machine over `config/create`: each reply carries `State`, an
//! `Option` describing the question, and `Error` when the previous answer
//! was rejected; each continuation posts the answer back. Two properties of
//! that protocol are load-bearing and easy to get wrong:
//!
//! - **Request opt keys are lowercase; reply keys are capitalised.** They
//!   are not the same spelling. Posting `State`/`Result` is silently
//!   ignored and the machine never advances — a failure that looks like a
//!   hang, not a rejection.
//! - **Every continuation resends the complete envelope** — `name`, `type`
//!   and the full `parameters` map, not just the continuation flags.
//!   rclone rebuilds the remote's configuration from each request; a
//!   continuation carrying only `state` and `result` loses the parameters
//!   and configures something incomplete.
//!
//! The machine itself is opaque data. Nothing here interprets a state
//! string or knows a provider's name: the loop must carry a backend added
//! after this code was written. A question is surfaced through the same
//! conversion the catalogue form uses — the reply's `Option` block is the
//! same rclone struct, serialised by the same code, so it goes through
//! `catalogue.parse` and `form.fieldFromOption` rather than a second
//! hand-written mapping.
//!
//! The flow runs on the worker like every other rc call, bounded by the
//! client's per-POST deadline. The worker owns the waiting-for-a-human
//! part; this module owns one exchange at a time.

const std = @import("std");
const rc = @import("rc.zig");
const catalogue = @import("catalogue.zig");
const form = @import("form.zig");

const Allocator = std.mem.Allocator;

/// A question the machine asks: the option rendered exactly the way the
/// catalogue form renders one, plus rclone's in-band error text when the
/// previous answer was rejected ("" otherwise) and the opaque state it
/// belongs to. Borrows the flow's per-step storage: valid until the next
/// `step` or `deinit`.
pub const Question = struct {
    field: form.Field,
    error_text: []const u8,
    state: []const u8,
};

pub const Step = union(enum) {
    done,
    question: Question,
};

pub const StepError = rc.RcError || Allocator.Error || error{
    /// The reply did not have the config-machine shape.
    BadReply,
    /// The machine ended with an in-band error and no further state;
    /// `lastError` carries rclone's text.
    ConfigFailed,
    /// The machine hopped through ask-nothing states without settling.
    Runaway,
};

/// How many ask-nothing states one `step` follows before calling the
/// machine runaway. Real machines hop once or twice; a bound this size is
/// never met by one that terminates.
const max_auto_continues = 16;

/// One interactive `config/create` in flight. `init` copies the name and
/// backend; `step` posts one exchange (plus any ask-nothing hops) and
/// returns either the next question or completion. The caller passes the
/// same full parameters map to every step — the envelope is resent whole
/// each time, which is the protocol's requirement, not a convenience.
pub const Flow = struct {
    gpa: Allocator,
    client: *rc.Client,
    name: []u8,
    backend: []u8,
    /// The `State` of the most recent reply, owned; null before the first
    /// exchange and after completion.
    state: ?[]u8 = null,
    /// rclone's text when the machine ended in failure.
    last_error: ?[]u8 = null,
    /// Per-question storage: the strings the current `Question` borrows.
    /// Reset at every exchange.
    step_arena: std.heap.ArenaAllocator,
    /// The synthetic one-option catalogue the current question's field
    /// borrows from; replaced at every exchange.
    question_cat: ?catalogue.Catalogue = null,

    pub fn init(
        gpa: Allocator,
        client: *rc.Client,
        name: []const u8,
        backend: []const u8,
    ) Allocator.Error!Flow {
        const owned_name = try gpa.dupe(u8, name);
        errdefer gpa.free(owned_name);
        const owned_backend = try gpa.dupe(u8, backend);
        errdefer gpa.free(owned_backend);
        return .{
            .gpa = gpa,
            .client = client,
            .name = owned_name,
            .backend = owned_backend,
            .step_arena = .init(gpa),
        };
    }

    pub fn deinit(self: *Flow) void {
        if (self.question_cat) |*cat| cat.deinit();
        if (self.state) |owned| self.gpa.free(owned);
        if (self.last_error) |owned| self.gpa.free(owned);
        self.step_arena.deinit();
        self.gpa.free(self.backend);
        self.gpa.free(self.name);
        self.* = undefined;
    }

    /// rclone's text for the most recent `error.ConfigFailed`.
    pub fn lastError(self: *const Flow) []const u8 {
        return self.last_error orelse "";
    }

    /// Drive one visible step: `result` is null to open the machine and
    /// the answer to the previous question afterwards. States that ask
    /// nothing are followed internally — they are the machine's own
    /// business, not a question to render.
    pub fn step(self: *Flow, parameters: std.json.Value, result: ?[]const u8) StepError!Step {
        var pending: ?[]const u8 = result;
        var hops: usize = 0;
        while (true) {
            switch (try self.exchange(parameters, pending)) {
                .done => return .done,
                .question => |q| return .{ .question = q },
                .auto_continue => {
                    hops += 1;
                    if (hops > max_auto_continues) return error.Runaway;
                    pending = "";
                },
            }
        }
    }

    /// One exchange's outcome for the async path: like `Step`, plus the
    /// ask-nothing hop the synchronous `step` follows internally — an
    /// async caller must begin a new exchange for it, so it crosses.
    pub const Exchange = union(enum) {
        done,
        question: Question,
        auto_continue,
    };

    /// Begin one exchange as an rc `_async` job. The continuation that
    /// starts a browser dance blocks server-side until the consent
    /// callback lands — and rclone aborts the dance the moment the
    /// request's connection dies, so a synchronous POST under the
    /// per-POST deadline would take the dance with it. The caller polls
    /// the job (each poll its own bounded POST) and hands the finished
    /// output to `finishStep`; while the job runs, `config/oauthstatus`
    /// says whether a consent URL is waiting.
    pub fn beginStepAsync(
        self: *Flow,
        parameters: std.json.Value,
        result: ?[]const u8,
    ) StepError!rc.JobId {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try self.buildEnvelope(&object, parameters, result);
        return try self.client.callAsync("config/create", .{ .object = object });
    }

    /// Complete an exchange begun by `beginStepAsync` from the finished
    /// job's `output` — the same reply object the synchronous call
    /// returns.
    pub fn finishStep(self: *Flow, output: std.json.Value) StepError!Exchange {
        return self.parseReply(output);
    }

    fn exchange(self: *Flow, parameters: std.json.Value, result: ?[]const u8) StepError!Exchange {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try self.buildEnvelope(&object, parameters, result);

        var reply = try self.client.call("config/create", .{ .object = object });
        defer reply.deinit();
        return self.parseReply(reply.value);
    }

    /// The complete envelope, every time: `name`, `type` and the full
    /// `parameters` map, continuation flags included when a state is being
    /// answered. rclone rebuilds the remote from each request; an envelope
    /// missing its parameters configures something incomplete.
    fn buildEnvelope(
        self: *Flow,
        object: *std.json.ObjectMap,
        parameters: std.json.Value,
        result: ?[]const u8,
    ) StepError!void {
        try object.put(self.gpa, "name", .{ .string = self.name });
        try object.put(self.gpa, "type", .{ .string = self.backend });
        try object.put(self.gpa, "parameters", parameters);
        // The nested opt map lives in the step arena: it must outlive the
        // call it travels in, and the next reply's parse resets the arena
        // only after the call has returned.
        const opt_alloc = self.step_arena.allocator();
        var opt: std.json.ObjectMap = .empty;
        // Lowercase on purpose: see the module doc. `obscure` matches the
        // non-interactive config path — password-typed parameters arrive
        // from the credentials document in plaintext and rclone transforms
        // them itself.
        try opt.put(opt_alloc, "nonInteractive", .{ .bool = true });
        try opt.put(opt_alloc, "obscure", .{ .bool = true });
        if (result) |r| {
            const state = self.state orelse return error.BadReply;
            try opt.put(opt_alloc, "continue", .{ .bool = true });
            try opt.put(opt_alloc, "state", .{ .string = state });
            try opt.put(opt_alloc, "result", .{ .string = r });
        }
        try object.put(self.gpa, "opt", .{ .object = opt });
    }

    fn parseReply(self: *Flow, value: std.json.Value) StepError!Exchange {
        // The previous question's storage dies with the reply replacing it.
        if (self.question_cat) |*cat| {
            cat.deinit();
            self.question_cat = null;
        }
        _ = self.step_arena.reset(.retain_capacity);

        const top = switch (value) {
            .object => |o| o,
            else => return error.BadReply,
        };
        const state_text: []const u8 = if (top.get("State")) |v| switch (v) {
            .string => |s| s,
            else => return error.BadReply,
        } else "";
        const error_text: []const u8 = if (top.get("Error")) |v| switch (v) {
            .string => |s| s,
            else => "",
        } else "";

        if (self.state) |owned| self.gpa.free(owned);
        self.state = null;
        if (state_text.len != 0) self.state = try self.gpa.dupe(u8, state_text);

        if (state_text.len == 0) {
            if (error_text.len != 0) {
                try self.setLastError(error_text);
                return error.ConfigFailed;
            }
            return .done;
        }

        const option_value = top.get("Option") orelse std.json.Value.null;
        if (option_value == .null) return .auto_continue;

        // The Option block is the catalogue's option shape — the same
        // rclone struct, serialised by the same code — so it goes through
        // the catalogue parser and the shared field conversion. The
        // synthetic wrapper exists only to reuse that parser verbatim.
        const alloc = self.step_arena.allocator();
        var doc: std.Io.Writer.Allocating = .init(alloc);
        var json: std.json.Stringify = .{ .writer = &doc.writer };
        wrap: {
            doc.writer.writeAll("{\"providers\":[{\"Name\":\"question\",\"Options\":[") catch break :wrap;
            json.write(option_value) catch break :wrap;
            doc.writer.writeAll("]}]}") catch break :wrap;

            var cat = catalogue.parse(self.gpa, doc.written()) catch |err| switch (err) {
                error.OutOfMemory => return error.OutOfMemory,
                error.BadJson => return error.BadReply,
            };
            errdefer cat.deinit();
            const entry = cat.backend("question") orelse return error.BadReply;
            if (entry.options.len != 1) return error.BadReply;

            // No provider filtering here: a machine question is already
            // addressed to this configuration, so every example applies.
            const owned_error = try alloc.dupe(u8, error_text);
            const field = try form.fieldFromOption(alloc, &entry.options[0], "");
            // Nothing fallible past this point: the errdefer above must
            // not fire once the catalogue is stored.
            self.question_cat = cat;
            return .{ .question = .{
                .field = field,
                .error_text = owned_error,
                .state = self.state.?,
            } };
        }
        return error.OutOfMemory;
    }

    fn setLastError(self: *Flow, text: []const u8) Allocator.Error!void {
        const owned = try self.gpa.dupe(u8, text);
        if (self.last_error) |previous| self.gpa.free(previous);
        self.last_error = owned;
    }
};

/// The `role` value that marks a consent card rather than a field
/// question: the dialog opens the card's `url` in the platform browser
/// and shows a waiting state instead of an edit row.
pub const consent_role = "consent";

/// The consent URL a running dance is waiting on, out of a
/// `config/oauthstatus` reply — or null when no dance is pending. The
/// URL carries a state secret: it may cross to the dialog and the
/// browser, and must never reach a log.
pub fn pendingAuthUrl(reply: std.json.Value) ?[]const u8 {
    const top = switch (reply) {
        .object => |o| o,
        else => return null,
    };
    const status = top.get("status") orelse return null;
    if (status != .string or !std.mem.eql(u8, status.string, "running")) return null;
    const url = top.get("authUrl") orelse return null;
    return switch (url) {
        .string => |s| if (s.len == 0) null else s,
        else => null,
    };
}

/// A consent card as wire JSON: `{"role":"consent","url":...}`. The same
/// mailbox the field questions travel in, told apart by `role`. Owned by
/// the caller — and never logged, because the URL is a credential.
pub fn consentJson(gpa: Allocator, url: []const u8) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };
    consent: {
        json.beginObject() catch break :consent;
        json.objectField("role") catch break :consent;
        json.write(consent_role) catch break :consent;
        json.objectField("url") catch break :consent;
        json.write(url) catch break :consent;
        json.endObject() catch break :consent;
        return out.toOwnedSlice();
    }
    return error.OutOfMemory;
}

/// A question as the form's wire JSON — the same keys
/// `bk_cloudsync_catalogue_form` writes per field, plus `error` — so the
/// dialog renders a machine prompt with the renderer it already has.
/// Owned by the caller.
pub fn questionJson(gpa: Allocator, question: *const Question) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };
    writeQuestion(&json, question) catch return error.OutOfMemory;
    return out.toOwnedSlice();
}

fn writeQuestion(json: *std.json.Stringify, question: *const Question) !void {
    const field = question.field;
    try json.beginObject();
    try json.objectField("role");
    try json.write(@tagName(field.role));
    try json.objectField("name");
    try json.write(field.name);
    try json.objectField("label");
    try json.write(field.label);
    try json.objectField("help");
    try json.write(field.help);
    try json.objectField("widget");
    try json.write(@tagName(field.widget));
    try json.objectField("kind");
    try json.write(@tagName(field.kind));
    try json.objectField("required");
    try json.write(field.required);
    try json.objectField("secret");
    try json.write(field.secret);
    try json.objectField("is_password");
    try json.write(field.is_password);
    try json.objectField("placeholder");
    try json.write(field.placeholder);
    try json.objectField("examples");
    try json.beginArray();
    for (field.examples) |example| {
        try json.beginObject();
        try json.objectField("value");
        try json.write(example.value);
        try json.objectField("help");
        try json.write(example.help);
        try json.endObject();
    }
    try json.endArray();
    try json.objectField("error");
    try json.write(question.error_text);
    try json.endObject();
}
