//! SDL3's C API for the app, from the headers of the SDL the engine links
//! (the staged one, sdl_dynamic in build.zig). Not vendor/zig-sdl3's module:
//! that one links libc, which on MSVC is Zig's static release CRT, and the
//! engine's statics need the debug DLL CRT - measured on the Windows job, the
//! two collide (lld-link: duplicate symbol: _cexit, libucrt.lib against
//! ucrtd.lib). The same `c` namespace, so host.zig reads as it would with it.
pub const c = @import("sdl_c");
