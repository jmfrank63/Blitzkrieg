const std = @import("std");

const module_defs = [_][]const u8{
	"Sources/src/AILogic/AILogic.def",
	"Sources/src/Anim/Animation.def",
	"Sources/src/GameTT/GameTT.def",
	"Sources/src/GFX/GFX.def",
	"Sources/src/GFXGPU/GFXGPU.def",
	"Sources/src/Image/Image.def",
	"Sources/src/Input/Input.def",
	"Sources/src/Net/net.def",
	"Sources/src/Scene/Scene.def",
	"Sources/src/SFX/Sound.def",
	"Sources/src/UI/UI.def",
};

test "playable module definitions export exactly one descriptor" {
	const cwd = std.Io.Dir.cwd();
	for (module_defs) |path| {
		const contents = try cwd.readFileAlloc(std.testing.io, path, std.testing.allocator, .limited(1024 * 1024));
		defer std.testing.allocator.free(contents);
		try std.testing.expectEqual(@as(usize, 1), std.mem.count(u8, contents, "GetModuleDescriptor"));
		try std.testing.expect(std.mem.indexOf(u8, contents, "EXPORTS") != null);
	}
}

test "non-Windows targets do not consume module definition files" {
	const cwd = std.Io.Dir.cwd();
	const build = try cwd.readFileAlloc(std.testing.io, "build.zig", std.testing.allocator, .limited(4 * 1024 * 1024));
	defer std.testing.allocator.free(build);
	for ([_][]const u8{ "Image/Image.def", "Input/Input.def", "Anim/Animation.def", "UI/UI.def", "GFX/GFX.def", "GFXGPU/GFXGPU.def" }) |suffix| {
		const guarded = try std.fmt.allocPrint(std.testing.allocator, ".win32_module_definition = if (target.result.os.tag == .windows) b.path(\"Sources/src/{s}\") else null", .{suffix});
		defer std.testing.allocator.free(guarded);
		try std.testing.expect(std.mem.indexOf(u8, build, guarded) != null);
	}
}
