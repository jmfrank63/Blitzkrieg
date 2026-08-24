//! The form model: one backend catalogue entry turned into a list of
//! widgets, in Zig, testable without a UI.
//!
//! This is the piece that makes the credentials dialog generic. Nothing
//! here names a provider, a field, or a vendor: every decision is derived
//! from catalogue data, so a newer rclone brings new backends and new
//! options with no change on our side. The one field the catalogue cannot
//! describe is the remote root — the schema carries it separately from the
//! options (the S3 bucket is a path component, not a parameter) — so the
//! model appends it with a label and help text we own, and leaves it
//! optional: which backends require one is exactly the per-backend
//! knowledge this plan forbids, and the writability test discovers it with
//! a real error from the service instead of a guess from us.
//!
//! The widget rules, in precedence order:
//!
//! - `IsPassword` or `Sensitive` is a **masked** field — a secret is never
//!   offered as a droplist, whatever else the option carries.
//! - `Examples` with `Exclusive` is a **closed droplist**; `Examples`
//!   without it an **editable** one. The examples that count are the ones
//!   left after provider filtering — an option can stay applicable while
//!   every one of its examples belongs to another vendor.
//! - Everything else is **text**, including types this build has never
//!   heard of; the catalogue's `kind` rides along so a renderer can refine
//!   a boolean or a number without the model guessing.
//!
//! Options and examples are filtered by their `Provider` expressions
//! through `catalogue.matchProvider` — the single implementation of the
//! rule — and the selected provider is itself just an option value, so a
//! vendor change is answered by calling `buildForm` again, not by patching
//! the previous result. `Hide` is a bitmask: only the configurator bit
//! removes a field, because bit 1 means hidden from the command line and
//! dropping everything non-zero would wrongly hide those options.
//!
//! `Advanced` splits the form instead of dropping fields — s3 has 78
//! options against 14 basic ones, and rendering all 78 is a wall, not a
//! form. The renderer shows `basic` expanded and `advanced` collapsed.

const std = @import("std");
const catalogue = @import("catalogue.zig");

const Allocator = std.mem.Allocator;

/// How a field renders. Derived from the catalogue by the precedence in
/// the module doc; a renderer may refine `.text` using `Field.kind`.
pub const Widget = enum { text, masked, droplist_closed, droplist_editable };

/// What a field is: an option from the catalogue, or the remote root the
/// schema carries beside the options.
pub const Role = enum { option, remote_root };

/// The label and help for the remote-root field — the one field whose text
/// we own, because the catalogue does not describe the remote path. The
/// renderer may localise them; these are the canonical fallback.
pub const remote_root_label = "Folder on the service";
pub const remote_root_help =
    "Where the game keeps its saves on the service - an S3 bucket " ++
    "(optionally with a path inside it), a directory, or empty when the " ++
    "connection details already name one. If the service needs a folder " ++
    "and this is missing, the connection test will say so.";

pub const Field = struct {
    role: Role = .option,
    /// `Option.Name` — also the key the save path uses. Empty for the
    /// remote-root field, which is not an option.
    name: []const u8 = "",
    /// What the renderer shows: the option name, or our remote-root label.
    /// The catalogue has no display names, and inventing them would go
    /// stale against upstream.
    label: []const u8 = "",
    /// The tooltip source, straight from the catalogue's `Help`.
    help: []const u8 = "",
    widget: Widget = .text,
    /// The catalogue's type classification, carried so a renderer can
    /// refine a `.text` widget into a toggle or a number box.
    kind: catalogue.Kind = .text,
    /// Must-fill: the catalogue marked it `Required` *and* its effective
    /// default is empty. rclone accepts an unset required option with a
    /// non-empty default — three of the 66 required options carry one —
    /// so a blank there is not an error, and marking it required would
    /// make those backends impossible to configure. Fields under another
    /// vendor's `Provider` expression never reach the form at all, so
    /// their requirements cannot block: validation is against the active
    /// filtered form, not the raw catalogue.
    required: bool = false,
    advanced: bool = false,
    /// The withheld classification, persisted per field at save time.
    secret: bool = false,
    is_password: bool = false,
    /// `DefaultStr`: shown as placeholder text, and the save path must not
    /// persist a typed value equal to it — a default that changes upstream
    /// must follow upstream.
    placeholder: []const u8 = "",
    /// The provider-filtered examples; droplist values when the widget is
    /// a droplist, suggestions otherwise.
    examples: []const catalogue.Example = &.{},
};

/// A built form and the arena its slices live in. Field strings and
/// example values borrow from the catalogue, so the catalogue must outlive
/// the form.
pub const Form = struct {
    arena: std.heap.ArenaAllocator,
    /// Shown expanded. The remote-root field is always its last entry.
    basic: []const Field,
    /// Collapsed by default.
    advanced: []const Field,

    pub fn deinit(self: *Form) void {
        self.arena.deinit();
        self.* = undefined;
    }
};

pub const BuildError = Allocator.Error || error{UnknownBackend};

/// Derive the form for `backend_name` under `selected_provider` (empty for
/// "no vendor chosen yet", which shows every conditional field — rclone's
/// own rule). A vendor change is a re-derivation: call this again.
pub fn buildForm(
    gpa: Allocator,
    cat: *const catalogue.Catalogue,
    backend_name: []const u8,
    selected_provider: []const u8,
) BuildError!Form {
    const backend = cat.backend(backend_name) orelse return error.UnknownBackend;

    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    var basic: std.ArrayList(Field) = .empty;
    var advanced: std.ArrayList(Field) = .empty;

    for (backend.options) |*option| {
        // Only the configurator bit hides; bit 1 is the command line's.
        if (option.hiddenFromConfigurator()) continue;
        if (!option.appliesTo(selected_provider)) continue;

        const examples = try filteredExamples(alloc, option, selected_provider);
        const field: Field = .{
            .role = .option,
            .name = option.name,
            .label = option.name,
            .help = option.help,
            .widget = widgetFor(option, examples),
            .kind = option.kind,
            .required = option.required and option.default_str.len == 0,
            .advanced = option.advanced,
            .secret = option.isSecret(),
            .is_password = option.is_password,
            .placeholder = option.default_str,
            .examples = examples,
        };
        if (option.advanced) {
            try advanced.append(alloc, field);
        } else {
            try basic.append(alloc, field);
        }
    }

    try basic.append(alloc, .{
        .role = .remote_root,
        .label = remote_root_label,
        .help = remote_root_help,
        .widget = .text,
        // Deliberately not required: see the module doc.
        .required = false,
    });

    return .{
        .arena = arena,
        .basic = basic.items,
        .advanced = advanced.items,
    };
}

/// The widget precedence from the module doc: masked beats droplist beats
/// text, and only the examples that survived provider filtering can open a
/// droplist.
fn widgetFor(option: *const catalogue.Option, examples: []const catalogue.Example) Widget {
    if (option.isSecret()) return .masked;
    if (examples.len != 0) {
        return if (option.exclusive) .droplist_closed else .droplist_editable;
    }
    return .text;
}

fn filteredExamples(
    alloc: Allocator,
    option: *const catalogue.Option,
    selected_provider: []const u8,
) Allocator.Error![]const catalogue.Example {
    var kept: std.ArrayList(catalogue.Example) = .empty;
    for (option.examples) |example| {
        if (!example.appliesTo(selected_provider)) continue;
        try kept.append(alloc, example);
    }
    return kept.items;
}
