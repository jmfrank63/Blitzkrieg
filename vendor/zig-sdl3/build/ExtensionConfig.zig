const std = @import("std");

/// SDL3 module.
sdl3: *std.Build.Module,
/// C translation step used for SDL3.
translate_c: *std.Build.Step.TranslateC,
/// Optional SDL lib to link against.
sdl_dep_lib: ?*std.Build.Step.Compile,
/// How to link the extension library.
linkage: std.builtin.LinkMode,
/// If to use the system include path.
system_include_path: ?std.Build.LazyPath,
