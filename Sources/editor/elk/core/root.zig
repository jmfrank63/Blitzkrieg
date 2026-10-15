//! The ELK core (D-02): std-only and headless, no UI, no engine, no C. This
//! is the text database of the Blitzkrieg localisation kit - `elk.xml`, the
//! `<element>_data_base/` folders and their text, state and description
//! files - read and written exactly as the MFC ELK in Sources/src/ELK does
//! (D-05, D-06), so old and new ELK can share a database. See
//! docs/superpowers/specs/2026-09-30-portable-elk-design.md.
pub const text_codec = @import("text_codec.zig");
pub const state = @import("state.zig");
pub const project_file = @import("project_file.zig");
pub const database = @import("database.zig");
pub const tree = @import("tree.zig");
pub const filter = @import("filter.zig");
pub const stats = @import("stats.zig");
pub const search = @import("search.zig");
pub const spell = @import("spell.zig");
pub const packages = @import("packages.zig");
pub const zip_writer = @import("zip_writer.zig");
pub const sheet = @import("sheet.zig");
pub const xlsx = @import("xlsx.zig");
pub const xls = @import("xls.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
