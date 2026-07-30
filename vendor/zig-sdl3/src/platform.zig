const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Constant only true if compiling for AIX.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const aix = @hasDecl(c, "SDL_PLATFORM_AIX");

/// Constant only true if compiling for Android.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const android = @hasDecl(c, "SDL_PLATFORM_ANDROID");

/// Constant only true if compiling for Apple platforms.
///
/// ## Remarks
/// iOS, macOS, etc will additionally define a more specific platform macro.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const apple = @hasDecl(c, "SDL_PLATFORM_APPLE");

/// Constant only true if compiling for BSDi.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const bsdi = @hasDecl(c, "SDL_PLATFORM_BSDI");

/// Constant only true if compiling for Cygwin.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const cygwin = @hasDecl(c, "SDL_PLATFORM_CYGWIN");

/// Constant only true if compiling for Emscripten.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const emscripten = @hasDecl(c, "SDL_PLATFORM_EMSCRIPTEN");

/// Constant only true if compiling for FreeBSD.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const free_bsd = @hasDecl(c, "SDL_PLATFORM_FREEBSD");

/// Constant only true if compiling for Microsoft GDK on any platform.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const gdk = @hasDecl(c, "SDL_PLATFORM_GDK");

/// Constant only true if compiling for Haiku OS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const haiku = @hasDecl(c, "SDL_PLATFORM_HAIKU");

/// Constant only true if compiling for HP-UX.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const hp_ux = @hasDecl(c, "SDL_PLATFORM_HPUX");

/// Constant only true if compiling for iOS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const ios = @hasDecl(c, "SDL_PLATFORM_IOS");

/// Constant only true if compiling for IRIX.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const irix = @hasDecl(c, "SDL_PLATFORM_IRIX");

/// Constant only true if compiling for Linux.
///
/// ## Remarks
/// Note that Android, although ostensibly a Linux-based system, will not define this.
/// It sets `platform.android` to true instead.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const linux = @hasDecl(c, "SDL_PLATFORM_LINUX");

/// Constant only true if compiling for macOS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const macos = @hasDecl(c, "SDL_PLATFORM_MACOS");

/// Constant only true if compiling for Nintendo 3DS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const n3ds = @hasDecl(c, "SDL_PLATFORM_3DS");

/// Constant only true if compiling for NetBSD.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const net_bsd = @hasDecl(c, "SDL_PLATFORM_NETBSD");

/// Constant only true if compiling for OpenBSD.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const open_bsd = @hasDecl(c, "SDL_PLATFORM_OPENBSD");

/// Constant only true if compiling for OS/2.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const os2 = @hasDecl(c, "SDL_PLATFORM_OS2");

/// Constant only true if compiling for Tru64 (OSF/1).
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const osf = @hasDecl(c, "SDL_PLATFORM_OSF");

/// Constant only true if compiling for Sony PlayStation 2.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const ps2 = @hasDecl(c, "SDL_PLATFORM_PS2");

/// Constant only true if compiling for Sony PSP.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const psp = @hasDecl(c, "SDL_PLATFORM_PSP");

/// Constant only true if compiling for QNX Neutrino.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const qnx_nto = @hasDecl(c, "SDL_PLATFORM_QNXNTO");

/// Constant only true if compiling for RISC OS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const risc_os = @hasDecl(c, "SDL_PLATFORM_RISCOS");

/// Constant only true if compiling for SunOS/Solaris.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const solaris = @hasDecl(c, "SDL_PLATFORM_SOLARIS");

/// Constant only true if compiling for tvOS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const tv_os = @hasDecl(c, "SDL_PLATFORM_TVOS");

/// Constant only true if compiling for a Unix-like system.
///
/// ## Remarks
/// Other platforms, like Linux, might define this in addition to their primary define.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const unix = @hasDecl(c, "SDL_PLATFORM_UNIX");

/// Constant only true if compiling for VisionOS.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const vision_os = @hasDecl(c, "SDL_PLATFORM_VISIONOS");

/// Constant only true if compiling for Sony Vita.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const vita = @hasDecl(c, "SDL_PLATFORM_VITA");

/// Constant only true if compiling for Microsoft GDK for Windows.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const win_gdk = @hasDecl(c, "SDL_PLATFORM_WINGDK");

/// Constant only true if compiling for desktop Windows.
///
/// ## Remarks
/// Despite the "32", this also covers 64-bit Windows; as an informal convention, its system layer tends to still be referred to as "the Win32 API".
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const win32 = @hasDecl(c, "SDL_PLATFORM_WIN32");

/// Constant only true if compiling for Windows.
///
/// ## Remarks
/// This also covers several other platforms, like Microsoft GDK, Xbox, WinRT, etc.
/// Each will have their own more-specific platform macros, too.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const windows = @hasDecl(c, "SDL_PLATFORM_WINDOWS");

/// Constant only true if compiling for Xbox One.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const xbox_one = @hasDecl(c, "SDL_PLATFORM_XBOXONE");

/// Constant only true if compiling for Xbox Series.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const xbox_series = @hasDecl(c, "SDL_PLATFORM_XBOXSERIES");

// Not sure how to do the WinAPI phone here.

/// Get the name of the platform.
///
/// ## Return Value
/// Returns the name of the platform.
/// If the correct platform name is not available, returns a string beginning with the text "Unknown".
///
/// ## Remarks
/// Here are the names returned for some (but not all) supported platforms:
/// * "Windows"
/// * "macOS"
/// * "Linux"
/// * "iOS"
/// * "Android"
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn get() [:0]const u8 {
    return std.mem.span(c.SDL_GetPlatform());
}

// Test platform.
test "Platform" {
    errors.refAllDeclsRecursive(@This());

    _ = aix;
    _ = android;
    _ = apple;
    _ = bsdi;
    _ = cygwin;
    _ = emscripten;
    _ = free_bsd;
    _ = gdk;
    _ = haiku;
    _ = hp_ux;
    _ = ios;
    _ = irix;
    _ = linux;
    _ = macos;
    _ = n3ds;
    _ = net_bsd;
    _ = open_bsd;
    _ = os2;
    _ = osf;
    _ = ps2;
    _ = psp;
    _ = qnx_nto;
    _ = risc_os;
    _ = solaris;
    _ = tv_os;
    _ = unix;
    _ = vision_os;
    _ = vita;
    _ = win32;
    _ = windows;
    _ = win_gdk;
    _ = xbox_one;
    _ = xbox_series;
    _ = get();
}
