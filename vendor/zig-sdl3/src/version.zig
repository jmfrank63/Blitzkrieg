const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Functionality to query the current SDL version, both as headers the app was compiled against, and a library the app is linked to.
pub const Version = packed struct {
    value: c_int,
    /// This is the version number macro for the current SDL version.
    ///
    /// ## Version
    /// This field is available since SDL 3.2.0.
    pub const compiled_against = Version{ .value = c.SDL_VERSION };

    /// A string describing the source at a particular point in development.
    ///
    /// ## Remarks
    /// This string is often generated from revision control's state at build time.
    ///
    /// This string can be quite complex and does not follow any standard.
    /// For example, it might be something like "SDL-prerelease-3.1.1-47-gf687e0732".
    /// It might also be user-defined at build time, so it's best to treat it as a clue in debugging forensics and not something the app will parse in any way.
    ///
    /// ## Version
    /// This field is available since SDL 3.2.0.
    // pub const revision = c.SDL_REVISION; // This should exist but does not?

    /// Check if the SDL version is at least greater than the given one.
    ///
    /// ## Function Parameters
    /// * `major` - Major version to compare against.
    /// * `minor` - Minor version to compare against.
    /// * `micro` - Micro version to compare against.
    ///
    /// ## Return Value
    /// If the SDL version is at least greater than the given one.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn atLeast(
        major: u32,
        minor: u32,
        micro: u32,
    ) bool {
        const ret = c.SDL_VERSION_ATLEAST(
            @as(c_int, @intCast(major)),
            @as(c_int, @intCast(minor)),
            @as(c_int, @intCast(micro)),
        );
        return ret;
    }

    /// Get the version of SDL that is linked against your program.
    ///
    /// ## Return Value
    /// Returns the version of the linked library.
    ///
    /// ## Remarks
    /// If you are linking to SDL dynamically, then it is possible that the current version will be different than the version you compiled against.
    /// This function returns the current version, while `Version.compiled_version` is the version you compiled with.
    ///
    /// ## Thread Safety
    /// This function may be called safely at any time, even before `init.init()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get() Version {
        const ret = c.SDL_GetVersion();
        return Version{ .value = ret };
    }

    /// Extracts the major version from a version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version number.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMajor(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MAJOR(
            self.value,
        );
        return @intCast(ret);
    }

    /// Extracts the minor version from a version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version number.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMinor(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MINOR(
            self.value,
        );
        return @intCast(ret);
    }

    /// Extracts the micro version from a version number.
    ///
    /// ## Function Parameters
    /// * `self`: The version number.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMicro(
        self: Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MICRO(
            self.value,
        );
        return @intCast(ret);
    }

    /// Get the code revision of SDL that is linked against your program.
    ///
    /// ## Remarks
    ///
    /// Returns an arbitrary string, uniquely identifying the exact revision of the SDL library in use, or `null` possibly.
    ///
    /// This value is the revision of the code you are linked with and may be different from the code you are compiling with,
    /// which is found in the constant `Version.compiled_revision`.
    ///
    /// The revision is arbitrary string (a hash value) uniquely identifying the exact revision of the SDL library in use,
    /// and is only useful in comparing against other revisions. It is NOT an incrementing number.
    ///
    /// If SDL wasn't built from a git repository with the appropriate tools, this will return an empty string.
    ///
    /// You shouldn't use this function for anything but logging it for debugging purposes. The string is not intended to be reliable in any way.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getRevision() ?[:0]const u8 {
        const ret = c.SDL_GetRevision();
        const converted_ret = std.mem.span(ret);
        if (std.mem.eql(u8, converted_ret, ""))
            return null;
        return converted_ret;
    }

    /// Turns the version numbers into a numeric value.
    ///
    /// ## Function Parameters
    /// * `major` - The major version number.
    /// * `minor` - The minor version number.
    /// * `micro` - The micro version number.
    ///
    /// ## Return Value
    /// An SDL version composed of the numbers provided.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn make(
        major: u32,
        minor: u32,
        micro: u32,
    ) Version {
        const ret = c.SDL_VERSIONNUM(
            @as(c_int, @intCast(major)),
            @as(c_int, @intCast(minor)),
            @as(c_int, @intCast(micro)),
        );
        return Version{ .value = ret };
    }
};

// Test version functionality.
test "Version" {
    errors.refAllDeclsRecursive(@This());

    try std.testing.expectEqual(true, Version.getRevision() != null);
    try std.testing.expectEqual(true, Version.atLeast(3, 0, 1));
    try std.testing.expectEqual(false, Version.atLeast(4, 0, 0));
    try std.testing.expectEqual(Version.compiled_against, Version.get());

    const sample_version = Version{ .value = 3_002_001 };
    try std.testing.expectEqual(sample_version, Version.make(3, 2, 1));

    try std.testing.expectEqual(3, sample_version.getMajor());
    try std.testing.expectEqual(2, sample_version.getMinor());
    try std.testing.expectEqual(1, sample_version.getMicro());
}
