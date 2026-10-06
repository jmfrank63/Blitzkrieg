const std = @import("std");
const build_support = @import("tools/zig/build_support.zig");
const package_policy = @import("tools/zig/verify_runtime.zig");
const season_textures_plan = @import("tools/zig/season_textures.zig");

/// Single source of truth for the game version. Bump the patch component with
/// every change. The version is embedded into Game.exe as a Win32 VERSIONINFO
/// resource and displayed on the title screen.
const game_version = std.SemanticVersion{ .major = 2, .minor = 0, .patch = 0 };

const cflags_debug = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-Wno-deprecated-non-prototype",
};
const portable_cflags = &.{
    "-include",                     "Sources/src/Platform/PortableCrt.h",
    "-Wno-switch",                  "-Wno-enum-compare",
    "-Wno-deprecated-declarations", "-Wno-comment",
    "-Wno-pointer-to-int-cast",     "-Wno-implicit-float-conversion",
    "-Wno-c++11-narrowing",         "-Wno-c23-extensions",
    "-Wno-extra-tokens",            "-Wno-extra-qualification",
    "-Wno-logical-not-parentheses", "-D__stdcall=",
    "-DBK_STDCALL=",
    "-Wno-macro-redefined",         "-fPIC",
};
const portable_cppflags = portable_cflags.* ++ [_][]const u8{ "-std=c++17", "-nostdinc++", "-Wno-invalid-constexpr" };
const portable_cppflags_release = portable_cflags_release ++ [_][]const u8{ "-std=c++17", "-nostdinc++", "-Wno-invalid-constexpr" };

// The portable flag set is shared by both variants because the debug macros
// (_DEBUG, _DO_ASSERT_SLOW, _STL_RANGE_CHECK) gate code that has never been
// compiled outside Windows. The release macros are a different matter: without
// them a macOS release build still ran with assert() live and _FINALRELEASE
// undefined, so development-only code stayed in -- the unit name in the status
// bar, for one, carried its "id N,(frozen flags),CState," debug prefix.
const portable_cflags_release = portable_cflags.* ++ [_][]const u8{ "-DNDEBUG", "-D_FINALRELEASE" };

const cflags_release = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-Wno-deprecated-non-prototype",
};

const cppflags_debug = &.{
    // C++17 to match the mac build (clang's default there); the MSVC target
    // otherwise defaults to C++14, which empties <filesystem> out of the STL.
    // _HAS_AUTO_PTR_ETC keeps std::random_shuffle, which LegacyAlgorithm.h
    // uses to preserve the engine's historical shuffle sequences.
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",

    "-fms-compatibility",
    "-fms-compatibility",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-non-pod-varargs",
    "-Wno-extra-tokens",
    "-Wno-parentheses-equality",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

// -Dubsan-trap: compile UBSan checks as trap instructions (ud2) instead of the
// zig runtime that prints a panic and aborts. Traps raise an illegal-instruction
// exception at the faulting line, which GUI debuggers (vsdbg) break on directly.
var ubsan_trap = false;
var build_target_os: std.Target.Os.Tag = .windows;
var build_host_os: std.Target.Os.Tag = .windows;
// Windows has two ABIs and only one of them is Visual Studio. The MSVC
// helpers below used to gate on the OS alone, so a MinGW target picked up
// Visual Studio include paths and the msvcrt/ucrt import libraries it has no
// business linking - and failed when that toolchain was not installed.
var build_target_msvc: bool = true;
const cppflags_debug_trap = &(cppflags_debug.* ++ .{"-fsanitize-trap=undefined"});

const cppflags_release = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",
    "-fms-compatibility",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-non-pod-varargs",
    "-Wno-extra-tokens",
    "-Wno-parentheses-equality",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_beta_debug = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-D_DO_BETA_CHECK",
    "-fms-extensions",
    "-fms-compatibility",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-non-pod-varargs",
    "-Wno-extra-tokens",
    "-Wno-parentheses-equality",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_beta_release = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-D_DO_BETA_CHECK",
    "-fms-extensions",
    "-fms-compatibility",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-non-pod-varargs",
    "-Wno-extra-tokens",
    "-Wno-parentheses-equality",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cflags_sfx_debug = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-DSFX_USE_OPEN_AUDIO_BACKEND",
    "-Wno-deprecated-non-prototype",
};

const cflags_sfx_release = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-DSFX_USE_OPEN_AUDIO_BACKEND",
    "-Wno-deprecated-non-prototype",
};

const cppflags_sfx_debug = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-DSFX_USE_OPEN_AUDIO_BACKEND",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-microsoft-cast",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
    "-Wno-pointer-compare",
};

const cppflags_sfx_release = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-DSFX_USE_OPEN_AUDIO_BACKEND",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-microsoft-cast",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
    "-Wno-pointer-compare",
};

const zlib_sources = &.{
    "Sources/src/zlib/adler32.c",
    "Sources/src/zlib/compress.c",
    "Sources/src/zlib/crc32.c",
    "Sources/src/zlib/deflate.c",
    "Sources/src/zlib/gzio.c",
    "Sources/src/zlib/infblock.c",
    "Sources/src/zlib/infcodes.c",
    "Sources/src/zlib/inffast.c",
    "Sources/src/zlib/inflate.c",
    "Sources/src/zlib/inftrees.c",
    "Sources/src/zlib/infutil.c",
    "Sources/src/zlib/trees.c",
    "Sources/src/zlib/uncompr.c",
    "Sources/src/zlib/zutil.c",
};

const libpng_sources = &.{
    "Sources/src/libpng/png.c",
    "Sources/src/libpng/pngerror.c",
    "Sources/src/libpng/pnggccrd.c",
    "Sources/src/libpng/pngget.c",
    "Sources/src/libpng/pngmem.c",
    "Sources/src/libpng/pngpread.c",
    "Sources/src/libpng/pngread.c",
    "Sources/src/libpng/pngrio.c",
    "Sources/src/libpng/pngrtran.c",
    "Sources/src/libpng/pngrutil.c",
    "Sources/src/libpng/pngset.c",
    "Sources/src/libpng/pngtest.c",
    "Sources/src/libpng/pngtrans.c",
    "Sources/src/libpng/pngvcrd.c",
    "Sources/src/libpng/pngwio.c",
    "Sources/src/libpng/pngwrite.c",
    "Sources/src/libpng/pngwtran.c",
    "Sources/src/libpng/pngwutil.c",
};

const misc_sources = &.{
    "Sources/src/Platform/Debug.cpp",
    "Sources/src/PlatformABI/PlatformClient.cpp",
    "Sources/src/Platform/DynamicLibrary.cpp",
    "Sources/src/Platform/LegacyVariant.cpp",
    "Sources/src/Platform/Paths.cpp",
    "Sources/src/Platform/Sync.cpp",
    "Sources/src/Platform/System.cpp",
    "Sources/src/Misc/FileUtils.cpp",
    "Sources/src/Misc/FreeIDs.cpp",
    "Sources/src/Misc/GRect.cpp",
    "Sources/src/Misc/HPTimer.cpp",
    "Sources/src/Misc/Spline.cpp",
    "Sources/src/Misc/StrProc.cpp",
    "Sources/src/Misc/Win32Random.cpp",
    "Sources/src/Misc/StdAfx.cpp",
    "Sources/src/Misc/BasicObjectFactory.cpp",
    "Sources/src/Misc/Manipulator.cpp",
    "Sources/src/Misc/MemorySystem.cpp",
    "Sources/src/Misc/Thread.cpp",
};

const image_sources = &.{
    "Sources/src/Image/ImageBMP.cpp",
    "Sources/src/Image/ImageMMP.cpp",
    "Sources/src/Image/ImageObjectFactory.cpp",
    "Sources/src/Image/ImagePNG.cpp",
    "Sources/src/Image/ImageProcessor.cpp",
    "Sources/src/Image/ImageReal.cpp",
    "Sources/src/Image/DxtCodec.cpp",
    "Sources/src/Image/ImageScale.cpp",
    "Sources/src/Image/ImageTGA.cpp",
    "Sources/src/Image/RectsComposition.cpp",
    "Sources/src/Image/GlobalsLoader.cpp",
    "Sources/src/Image/StdAfx.cpp",
};

const lualib_c_sources = &.{
    "Sources/src/LuaLib/LuaSrc/lapi.c",
    "Sources/src/LuaLib/LuaSrc/lcode.c",
    "Sources/src/LuaLib/LuaSrc/ldebug.c",
    "Sources/src/LuaLib/LuaSrc/ldo.c",
    "Sources/src/LuaLib/LuaSrc/lfunc.c",
    "Sources/src/LuaLib/LuaSrc/lgc.c",
    "Sources/src/LuaLib/LuaSrc/llex.c",
    "Sources/src/LuaLib/LuaSrc/lmem.c",
    "Sources/src/LuaLib/LuaSrc/lobject.c",
    "Sources/src/LuaLib/LuaSrc/lparser.c",
    "Sources/src/LuaLib/LuaSrc/lstate.c",
    "Sources/src/LuaLib/LuaSrc/lstring.c",
    "Sources/src/LuaLib/LuaSrc/ltable.c",
    "Sources/src/LuaLib/LuaSrc/ltm.c",
    "Sources/src/LuaLib/LuaSrc/lundump.c",
    "Sources/src/LuaLib/LuaSrc/lvm.c",
    "Sources/src/LuaLib/LuaSrc/lzio.c",
};

const lualib_cpp_sources = &.{
    "Sources/src/LuaLib/Script.cpp",
};

const net_sources = &.{
    "Sources/src/Net/GlobalsLoader.cpp",
    "Sources/src/Net/StdAfx.cpp",
    "Sources/src/Net/NetA4.cpp",
    "Sources/src/Net/NetAcks.cpp",
    "Sources/src/Net/NetConnection.cpp",
    "Sources/src/Net/NetDriverConsts.cpp",
    "Sources/src/Net/NetLogin.cpp",
    "Sources/src/Net/NetLowest.cpp",
    "Sources/src/Net/NetPeer2Peer.cpp",
    "Sources/src/Net/NetServerInfo.cpp",
    "Sources/src/Net/NetStream.cpp",
    "Sources/src/Net/NetObjectFactory.cpp",
    "Sources/src/Net/Streams.cpp",
};

const buildversion_sources = &.{
    "Sources/src/buildversion/StdAfx.cpp",
    "Sources/src/buildversion/BuildVersion.cpp",
    "Sources/src/buildversion/main.cpp",
    "Sources/src/buildversion/StringTokenizer.cpp",
};

const betakeygen_sources = &.{
    "Sources/src/betakeygen/StdAfx.cpp",
    "Sources/src/betakeygen/BetaKey.cpp",
    "Sources/src/betakeygen/main.cpp",
};

const input_sources = &.{
    "Sources/src/Input/GlobalsLoader.cpp",
    "Sources/src/Input/StdAfx.cpp",
    "Sources/src/Input/InputCodes.cpp",
    "Sources/src/Input/InputAPI.cpp",
    "Sources/src/Input/InputBinder.cpp",
    "Sources/src/Input/InputObjectFactory.cpp",
    "Sources/src/Input/InputSlider.cpp",
    "Sources/src/Input/Visitors.cpp",
};

const formats_sources = &.{
    "Sources/src/Formats/StdAfx.cpp",
    "Sources/src/Formats/fmtAIGeneral.cpp",
    "Sources/src/Formats/fmtAnimation.cpp",
    "Sources/src/Formats/fmtEffect.cpp",
    "Sources/src/Formats/fmtFont.cpp",
    "Sources/src/Formats/fmtMap.cpp",
    "Sources/src/Formats/fmtMesh.cpp",
    "Sources/src/Formats/fmtSound.cpp",
    "Sources/src/Formats/fmtSprite.cpp",
    "Sources/src/Formats/fmtTerrain.cpp",
    "Sources/src/Formats/fmtUnitCreation.cpp",
    "Sources/src/Formats/fmtVSO.cpp",
};

const anim_sources = &.{
    "Sources/src/Anim/GlobalsLoader.cpp",
    "Sources/src/Anim/StdAfx.cpp",
    "Sources/src/Anim/MeshAnimation.cpp",
    "Sources/src/Anim/SpriteAnimation.cpp",
    "Sources/src/Anim/AnimationManager.cpp",
    "Sources/src/Anim/AnimObjectFactory.cpp",
    "Sources/src/Anim/MatrixEffectorJogging.cpp",
    "Sources/src/Anim/MatrixEffectorLeveling.cpp",
};

const common_sources = &.{
    "Sources/src/Common/StdAfx.cpp",
    "Sources/src/Common/MapObject.cpp",
    "Sources/src/Common/MOBridge.cpp",
    "Sources/src/Common/MOBuilding.cpp",
    "Sources/src/Common/MOEntrenchment.cpp",
    "Sources/src/Common/MOObject.cpp",
    "Sources/src/Common/MOProjectile.cpp",
    "Sources/src/Common/MOSquad.cpp",
    "Sources/src/Common/MOUnit.cpp",
    "Sources/src/Common/MOUnitInfantry.cpp",
    "Sources/src/Common/MOUnitMechanical.cpp",
    "Sources/src/Common/Passangers.cpp",
    "Sources/src/Common/UISquadElement.cpp",
    "Sources/src/Common/WorldBase.cpp",
    "Sources/src/Common/InterfaceScreenBase.cpp",
};

const ui_sources = &.{
    "Sources/src/UI/GlobalsLoader.cpp",
    "Sources/src/UI/StdAfx.cpp",
    "Sources/src/UI/UIBasic.cpp",
    "Sources/src/UI/UIBasicM.cpp",
    "Sources/src/UI/UIInternal.cpp",
    "Sources/src/UI/UIInternalM.cpp",
    "Sources/src/UI/UIColorTextScroll.cpp",
    "Sources/src/UI/UIButton.cpp",
    "Sources/src/UI/UIConsole.cpp",
    "Sources/src/UI/UICreditsScroller.cpp",
    "Sources/src/UI/UIDialog.cpp",
    "Sources/src/UI/UIEdit.cpp",
    "Sources/src/UI/UIMessageBox.cpp",
    "Sources/src/UI/UIMiniMap.cpp",
    "Sources/src/UI/UINumberIndicator.cpp",
    "Sources/src/UI/UIScreen.cpp",
    "Sources/src/UI/UIScrollText.cpp",
    "Sources/src/UI/UISlider.cpp",
    "Sources/src/UI/UIStatusBar.cpp",
    "Sources/src/UI/UITimeCounter.cpp",
    "Sources/src/UI/UIVideoButton.cpp",
    "Sources/src/UI/UIComplexScroll.cpp",
    "Sources/src/UI/UIComboBox.cpp",
    "Sources/src/UI/UIList.cpp",
    "Sources/src/UI/UIListSorter.cpp",
    "Sources/src/UI/UIMedals.cpp",
    "Sources/src/UI/UIObjectiveScreen.cpp",
    "Sources/src/UI/UIObjMap.cpp",
    "Sources/src/UI/UIShortcutBar.cpp",
    "Sources/src/UI/UITree.cpp",
    "Sources/src/UI/MaskManager.cpp",
    "Sources/src/UI/UIMask.cpp",
    "Sources/src/UI/UIObjectFactory.cpp",
};

const fontgen_sources = &.{
    "Sources/src/FontGen/GlobalsLoader.cpp",
    "Sources/src/FontGen/StdAfx.cpp",
    "Sources/src/FontGen/FontGen.cpp",
};

const sfx_cpp_sources = &.{
    "Sources/src/SFX/AudioBackend.cpp",
    "Sources/src/SFX/AudioBackendOpen.cpp",
    "Sources/src/SFX/GlobalsLoader.cpp",
    "Sources/src/SFX/StdAfx.cpp",
    "Sources/src/SFX/SampleSounds.cpp",
    "Sources/src/SFX/SoundsSerialize.cpp",
    "Sources/src/SFX/StreamingSound.cpp",
    "Sources/src/SFX/SoundEngine.cpp",
    "Sources/src/SFX/SoundManager.cpp",
    "Sources/src/SFX/SoundObjectFactory.cpp",
    "Sources/src/SFX/StreamFadeOff.cpp",
};

const sfx_c_sources = &.{
    "Sources/src/SFX/AudioBackendXiphVorbis.c",
    "Sources/sdk/xiph/ogg-1.3.5/src/bitwise.c",
    "Sources/sdk/xiph/ogg-1.3.5/src/framing.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/analysis.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/bitrate.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/block.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/codebook.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/envelope.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/floor0.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/floor1.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/info.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/lookup.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/lpc.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/lsp.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/mapping0.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/mdct.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/psy.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/registry.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/res0.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/sharedbook.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/smallft.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/synthesis.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/vorbisfile.c",
    "Sources/sdk/xiph/vorbis-1.3.7/lib/window.c",
};

const gfx_sources = &.{
    "Sources/src/GFX/GlobalsLoader.cpp",
    "Sources/src/GFX/StdAfx.cpp",
    "Sources/src/GFX/GFXObjectFactory.cpp",
    "Sources/src/GFX/GraphicsEngine.cpp",
    "Sources/src/GFX/VideoCheck.cpp",
    "Sources/src/GFX/Texture.cpp",
    "Sources/src/GFX/TextureManager.cpp",
    "Sources/src/GFX/GeometryBuffer.cpp",
    "Sources/src/GFX/GeometryManager.cpp",
    "Sources/src/GFX/GeometryMesh.cpp",
    "Sources/src/GFX/RangeAllocs.cpp",
    "Sources/src/GFX/Clipping.cpp",
    "Sources/src/GFX/Font.cpp",
    "Sources/src/GFX/FontManager.cpp",
    "Sources/src/GFX/GFXTextVisitors.cpp",
    "Sources/src/GFX/Text.cpp",
};

const gfx_gpu_sources = &.{
    "Sources/src/GFXGPU/GraphicsEngineGpu.cpp",
    "Sources/src/GFXGPU/TextureGpu.cpp",
    "Sources/src/GFXGPU/GeometryBufferGpu.cpp",
    "Sources/src/GFXGPU/MeshGpu.cpp",
    "Sources/src/GFXGPU/MeshManagerGpu.cpp",
    "Sources/src/GFXGPU/GlobalsLoader.cpp",
    "Sources/src/GFXGPU/GfxGpuObjectFactory.cpp",
};

fn auditDefaultRendererInputs(files: []const []const u8) void {
    const forbidden = [_][]const u8{ "GraphicsEngine.cpp", "Texture.cpp", "GeometryBuffer.cpp", "d3d9", "dxguid", "Specific.h" };
    for (files) |file| {
        for (forbidden) |token| {
            if (std.mem.indexOf(u8, file, token) != null) {
                std.log.err("SDL GPU renderer input audit failed: {s} contains {s}", .{ file, token });
                @panic("default renderer contains legacy D3D input");
            }
        }
    }
}

const randommapgen_sources = &.{
    "Sources/src/RandomMapGen/StdAfx.cpp",
    "Sources/src/RandomMapGen/BetaSpline.cpp",
    "Sources/src/RandomMapGen/PNoise.cpp",
    "Sources/src/RandomMapGen/TerrainBuilder.cpp",
    "Sources/src/RandomMapGen/TerrainGenerator.cpp",
    "Sources/src/RandomMapGen/IB_Methods.cpp",
    "Sources/src/RandomMapGen/IB_StaticMethods.cpp",
    "Sources/src/RandomMapGen/LA_Methods.cpp",
    "Sources/src/RandomMapGen/MapInfo_CheckSums.cpp",
    "Sources/src/RandomMapGen/MapInfo_Consts.cpp",
    "Sources/src/RandomMapGen/MapInfo_Methods.cpp",
    "Sources/src/RandomMapGen/MapInfo_StaticMethods.cpp",
    "Sources/src/RandomMapGen/MapInfo_StaticMethods_MiniMapCreation.cpp",
    "Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp",
    "Sources/src/RandomMapGen/MapInfo_StaticMethods_SoundsCreation.cpp",
    "Sources/src/RandomMapGen/MiniMap_Methods.cpp",
    "Sources/src/RandomMapGen/Polygons_Methods.cpp",
    "Sources/src/RandomMapGen/Registry_Sources.cpp",
    "Sources/src/RandomMapGen/Resource_Functions.cpp",
    "Sources/src/RandomMapGen/Resource_Methods.cpp",
    "Sources/src/RandomMapGen/Resource_StaticMethods.cpp",
    "Sources/src/RandomMapGen/RMG_Consts.cpp",
    "Sources/src/RandomMapGen/RMG_Methods.cpp",
    "Sources/src/RandomMapGen/RMG_StaticMethods.cpp",
    "Sources/src/RandomMapGen/RP_Methods.cpp",
    "Sources/src/RandomMapGen/VA_Methods.cpp",
    "Sources/src/RandomMapGen/VA_StaticMethods.cpp",
    "Sources/src/RandomMapGen/VSO_Methods.cpp",
    "Sources/src/RandomMapGen/VSO_StaticMethods.cpp",
};

const main_sources = &.{
    "Sources/src/Main/StdAfx.cpp",
    "Sources/src/Main/iMainInternal.cpp",
    "Sources/src/Main/MainLoopCommands.cpp",
    "Sources/src/Main/MainObjectFactory.cpp",
    "Sources/src/Main/RandomMapHelper.cpp",
    "Sources/src/Main/GameTimerInternal.cpp",
    "Sources/src/Main/GameDB.cpp",
    "Sources/src/Main/GameStats.cpp",
    "Sources/src/Main/RPGStats.cpp",
    "Sources/src/Main/FilesInspector.cpp",
    "Sources/src/Main/InitGlobalVarConsts.cpp",
    "Sources/src/Main/Initialization.cpp",
    "Sources/src/Main/LoadDLLs.cpp",
    "Sources/src/Main/TextManager.cpp",
    "Sources/src/Main/TextObject.cpp",
    "Sources/src/Main/AILogicCommand.cpp",
    "Sources/src/Main/AILogicCommandInternal.cpp",
    "Sources/src/Main/MultiPlayerTransceiver.cpp",
    "Sources/src/Main/SinglePlayerTransceiver.cpp",
    "Sources/src/Main/ChatMessages.cpp",
    "Sources/src/Main/GameCreationMessages.cpp",
    "Sources/src/Main/MessagesStore.cpp",
    "Sources/src/Main/ServersListMessages.cpp",
    "Sources/src/Main/assert.cpp",
    "Sources/src/Main/GameCreation.cpp",
    "Sources/src/Main/GamePlaying.cpp",
    "Sources/src/Main/LanChat.cpp",
    "Sources/src/Main/MultiplayerInternal.cpp",
    "Sources/src/Main/ServerInfo.cpp",
    "Sources/src/Main/ServersList.cpp",
    "Sources/src/Main/CommandsHistory.cpp",
    "Sources/src/Main/PlayerScenarioInfo.cpp",
    "Sources/src/Main/PlayerSkill.cpp",
    "Sources/src/Main/ScenarioStatistics.cpp",
    "Sources/src/Main/ScenarioTracker2Internal.cpp",
    "Sources/src/Main/UserProfile.cpp",
    "Sources/src/Main/BetaKey.cpp",
};

const game_sources = &.{
    "Sources/src/Game/GlobalsLoader.cpp",
    "Sources/src/Game/StdAfx.cpp",
    "Sources/src/Game/GameMain.cpp",
    "Sources/src/Game/main.cpp",
    "Sources/src/Game/GameFrame.cpp",
    "Sources/src/Game/SysKeys.cpp",
    "Sources/src/Game/MouseCapture.cpp",
    "Sources/src/Main/CloudSyncFacade.cpp",
    "Sources/src/Platform/CloudSyncLoader.cpp",
    "Sources/src/Platform/SDLApplication.cpp",
};
const windows_game_sources = &.{
    "Sources/src/Game/WindowsMain.cpp",
    "Sources/src/Game/WinFrame.cpp",
};

// P00-M01 keeps this manifest next to the source declarations. The audit
// derives playable paths from these declarations and fails if a new source
// array is not classified, so additions cannot silently escape inventory.
const runtime_platform_playable_source_arrays = &.{
    "zlib_sources",    "libpng_sources", "misc_sources",    "image_sources",   "lualib_c_sources",     "lualib_cpp_sources",
    "net_sources",     "input_sources",  "formats_sources", "anim_sources",    "common_sources",       "ui_sources",
    "sfx_cpp_sources", "sfx_c_sources",  "gfx_sources",     "gfx_gpu_sources", "randommapgen_sources", "main_sources",
    "game_sources",    "windows_game_sources",
};
const runtime_platform_non_playable_source_arrays = &.{
    "buildversion_sources", "betakeygen_sources", "fontgen_sources",
};
const runtime_platform_playable_link_module_names = &.{
    "module",     "game_module",    "net_module", "input_module", "sfx_module",
    "gfx_module", "gfx_gpu_module",
};
const runtime_platform_playable_build_functions = &.{
    "addLegacyProjectDll", "addGame", "addNet", "addInput", "addSFX", "addGFX", "addGFXGPU",
};

const cppflags_game_debug = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_SEH",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_game_release = &.{
    "-std=c++17",
    "-D_HAS_AUTO_PTR_ETC=1",
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-comment",
    "-Wno-enum-compare",
    "-Wno-microsoft-enum-forward-reference",
    "-Wno-return-type",
    "-Wno-address-of-temporary",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

pub fn build(b: *std.Build) void {
    // The default target follows the host CPU on Linux as it already did on
    // macOS. This branch used to hardcode x86_64, so a plain `zig build` on an
    // arm64 Linux host silently cross-compiled for x86_64 and then failed to
    // find /usr/lib/x86_64-linux-gnu/libstdc++.so.6.
    const default_target: std.Target.Query = switch (b.graph.host.result.os.tag) {
        .linux => switch (b.graph.host.result.cpu.arch) {
            .aarch64 => .{
                .cpu_arch = .aarch64,
                .os_tag = .linux,
                .abi = .gnu,
            },
            else => .{
                .cpu_arch = .x86_64,
                .os_tag = .linux,
                .abi = .gnu,
            },
        },
        .windows => .{
            .cpu_arch = .x86_64,
            .os_tag = .windows,
            .abi = .msvc,
        },
        .macos => switch (b.graph.host.result.cpu.arch) {
            .x86_64 => .{
                .cpu_arch = .x86_64,
                .os_tag = .macos,
            },
            .aarch64 => .{
                .cpu_arch = .aarch64,
                .os_tag = .macos,
            },
            else => .{
                .cpu_arch = .x86_64,
                .os_tag = .macos,
            },
        },
        else => .{
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .gnu,
        },
    };

    var selected_target = b.standardTargetOptions(.{
        .default_target = default_target,
    });
    // The Linux build compiles against the host's GCC libstdc++ headers and
    // links its shared libstdc++ (linkCxxRuntime), and those headers assume
    // the host's glibc - GCC 13's <ext/atomicity.h> unconditionally includes
    // <sys/single_threaded.h>, which needs glibc 2.32+. An unversioned
    // -Dtarget=x86_64-linux-gnu makes zig model its older cross-default
    // glibc, whose bundled headers reject that include with an #error. When
    // building on a Linux host for Linux, adopt the host's detected glibc
    // version unless the command line pinned one explicitly.
    if (b.graph.host.result.os.tag == .linux and
        selected_target.result.os.tag == .linux and
        selected_target.result.abi.isGnu() and
        selected_target.query.glibc_version == null)
    {
        var query = selected_target.query;
        query.glibc_version = b.graph.host.result.os.version_range.linux.glibc;
        selected_target = b.resolveTargetQuery(query);
    }
    // The editor's single-instance socket (Sources/editor/app/single_instance.zig) is an
    // AF_UNIX stream socket, which Windows has had since Windows 10 version 1803 (build
    // 17063, "redstone 4"). The std keeps `net.UnixAddress` behind
    // `builtin.os.version_range.windows.isAtLeast(.win10_rs4)`, and a target that names no
    // Windows version leaves that unknown - read as false - so a plain
    // -Dtarget=x86_64-windows-msvc build would have a second launch that never reaches the
    // first (its socket tests fail with AddressFamilyUnsupported). Name the oldest Windows
    // this project builds for unless the command line pinned a version.
    if (selected_target.result.os.tag == .windows and selected_target.query.os_version_min == null) {
        var query = selected_target.query;
        query.os_version_min = .{ .windows = .win10_rs4 };
        selected_target = b.resolveTargetQuery(query);
    }
    const platform = build_support.classify(selected_target.result) catch @panic("unsupported target; supported triples are x86_64-windows-msvc, x86_64-windows-gnu, x86_64-linux-gnu, aarch64-linux-gnu, x86_64-macos, and aarch64-macos");
    build_target_os = selected_target.result.os.tag;
    build_target_msvc = build_support.usesMsvc(platform);
    build_host_os = b.graph.host.result.os.tag;
    // Runtime eligibility follows the host OS and CPU. Windows can execute
    // an MSVC-targeted binary even when the Zig host itself reports the GNU
    // Windows ABI (the common Scoop Zig installation does); ABI selection is
    // still enforced by the target and linker configuration below.
    const native_target = selected_target.result.os.tag == b.graph.host.result.os.tag and
        selected_target.result.cpu.arch == b.graph.host.result.cpu.arch;
    // Preserve an explicitly selected target for project artifacts, but let
    // dependencies see Zig's native target query when the OS and CPU match.
    // SDL distinguishes native macOS builds from cross-compiles using the
    // target query; forwarding `-Dtarget=aarch64-macos` verbatim on an Apple
    // Silicon host incorrectly makes SDL require an explicit SDK sysroot.
    const dependency_target = if (native_target and
        selected_target.result.abi == b.graph.host.result.abi) b.graph.host else selected_target;
    // The project's own macOS modules link SDL's Apple frameworks transitively.
    // Zig only populates SDK framework search paths for a *native* target query,
    // so an explicit `-Dtarget=aarch64-macos` yields "searched paths: none" at
    // link time. Reuse the native query on Apple hosts; Windows and Linux keep
    // the explicitly selected target unchanged.
    const target = if (selected_target.result.os.tag == .macos) dependency_target else selected_target;
    const test_mode_text = b.option([]const u8, "test-mode", "Test execution mode: compile or run") orelse switch (build_support.defaultTestMode(native_target)) {
        .compile => "compile",
        .run => "run",
    };
    const test_mode = build_support.parseTestMode(test_mode_text) catch @panic("invalid -Dtest-mode; expected compile or run");
    build_support.validateTestMode(test_mode, native_target) catch @panic("-Dtest-mode=run requires a matching native target; use -Dtest-mode=compile for cross targets");
    const platform_policy = build_support.policy(platform, native_target);
    const build_support_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/build_support.zig"),
        .target = target,
        .optimize = .Debug,
    });
    const build_support_tests = b.addTest(.{ .root_module = build_support_module });
    const build_support_tests_run = b.addRunArtifact(build_support_tests);
    const build_support_step = b.step("test-build-support", "Validate the supported cross-platform target policy");
    build_support_step.dependOn(&build_support_tests.step);
    if (test_mode == .run) build_support_step.dependOn(&build_support_tests_run.step);

    const runtime_platform_audit_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/runtime_platform_audit_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const runtime_platform_audit_tests = b.addTest(.{ .root_module = runtime_platform_audit_module });
    const runtime_platform_audit_run = b.addRunArtifact(runtime_platform_audit_tests);
    const runtime_platform_audit_step = b.step("test-runtime-platform-audit", "Audit playable source platform dependencies");
    runtime_platform_audit_step.dependOn(&runtime_platform_audit_tests.step);
    if (test_mode == .run) runtime_platform_audit_step.dependOn(&runtime_platform_audit_run.step);

    // Data/Scenarios against GOG Blitzkrieg 1.2 and the chapter screen's rules
    // (docs/superpowers/specs/2026-09-25-revive-random-missions-design.md).
    // Reads Data, so it runs from the repository root.
    const mission_data_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/mission_data_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const mission_data_tests = b.addTest(.{ .root_module = mission_data_module });
    const mission_data_run = b.addRunArtifact(mission_data_tests);
    mission_data_run.setCwd(b.path("."));
    const mission_data_step = b.step("test-mission-data", "Check Data/Scenarios against GOG 1.2 and the chapter rules");
    mission_data_step.dependOn(&mission_data_tests.step);
    if (test_mode == .run) mission_data_step.dependOn(&mission_data_run.step);

    const platform_linkage_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/platform_linkage_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const platform_linkage_tests = b.addTest(.{ .root_module = platform_linkage_module });
    const platform_linkage_step = b.step("test-platform-linkage", "Validate one target-correct PlatformRuntime and staged linkage policy");
    platform_linkage_step.dependOn(&platform_linkage_tests.step);

    const standard_optimize = b.standardOptimizeOption(.{});
    const build_variant = b.option([]const u8, "build-variant", "Output variant: default, debug, or release. Also selects the optimisation unless -Doptimize is given") orelse "default";
    if (!std.mem.eql(u8, build_variant, "default") and
        !std.mem.eql(u8, build_variant, "debug") and
        !std.mem.eql(u8, build_variant, "release"))
    {
        @panic("-Dbuild-variant must be default, debug, or release");
    }
    // The variant names what the staged tree is for, so it has to choose the
    // optimisation as well. It used to be a directory suffix and nothing else,
    // which meant zig-out/game/<platform>-release held an unoptimised build
    // compiled with _DEBUG. An explicit -Doptimize still wins.
    const optimize = if (b.user_input_options.contains("optimize"))
        standard_optimize
    else if (std.mem.eql(u8, build_variant, "release"))
        std.builtin.OptimizeMode.ReleaseFast
    else if (std.mem.eql(u8, build_variant, "debug"))
        std.builtin.OptimizeMode.Debug
    else
        standard_optimize;
    const library_arch = build_support.libraryArch(platform);
    const toolchain = ToolchainIncludes{
        .msvc_include = b.option([]const u8, "msvc-include", "MSVC C/C++ include directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\include",
        .windows_sdk_include = b.option([]const u8, "windows-sdk-include", "Windows SDK library include directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.26100.0",
        .msvc_lib = b.option([]const u8, "msvc-lib", "MSVC library directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\lib",
        .windows_sdk_lib = b.option([]const u8, "windows-sdk-lib", "Windows SDK library directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0",
        .library_arch = library_arch,
    };
    if (platform == .windows_x64 and b.graph.host.result.os.tag != .windows and
        (!b.user_input_options.contains("msvc-include") or
            !b.user_input_options.contains("windows-sdk-include") or
            !b.user_input_options.contains("msvc-lib") or
            !b.user_input_options.contains("windows-sdk-lib")))
    {
        @panic("Windows target on a non-Windows host requires explicit MSVC/Windows SDK paths: pass -Dmsvc-include, -Dwindows-sdk-include, -Dmsvc-lib, and -Dwindows-sdk-lib.");
    }

    const platform_abi_layout_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    platform_abi_layout_module.addIncludePath(b.path("Sources/src"));
    platform_abi_layout_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_abi_layout_test.cpp"), .flags = &.{"-std=c++17"} });
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_abi_layout_module, toolchain);
        addMsvcLibraryPaths(b, platform_abi_layout_module, toolchain);
        linkMsvcRuntime(platform_abi_layout_module, .Debug);
    }
    const platform_abi_layout_test = b.addExecutable(.{ .name = "platform-abi-layout-test", .root_module = platform_abi_layout_module });
    if (platform == .windows_x64) {
        platform_abi_layout_test.subsystem = .console;
        platform_abi_layout_test.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const platform_abi_layout_run = b.addRunArtifact(platform_abi_layout_test);

    const platform_abi_compile_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/platform_abi_compile_test.zig"),
        .target = target,
        .optimize = .Debug,
    });
    platform_abi_compile_module.addIncludePath(b.path("Sources/src"));
    const platform_abi_compile_tests = b.addTest(.{ .root_module = platform_abi_compile_module });
    const platform_abi_compile_run = b.addRunArtifact(platform_abi_compile_tests);
    const platform_abi_layout_step = b.step("test-platform-abi-layout", "Validate the versioned platform C ABI layout and C import");
    platform_abi_layout_step.dependOn(&platform_abi_layout_test.step);
    platform_abi_layout_step.dependOn(&platform_abi_compile_tests.step);
    if (test_mode == .run) {
        platform_abi_layout_step.dependOn(&platform_abi_layout_run.step);
        platform_abi_layout_step.dependOn(&platform_abi_compile_run.step);
    }

    // The shipped runtime follows the build's optimisation like every other
    // module. Pinning it to Debug left a release install with a debug copy of the
    // layer everything else calls -- clock, file I/O, sockets, heap, debug output
    // -- and on Windows it also pinned the CRT, so a release build there would
    // have mixed debug and release CRTs across the DLL boundary.
    const platform_runtime_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    platform_runtime_module.addIncludePath(b.path("Sources/src"));
    addLinuxCxxIncludePaths(b, platform_runtime_module);
    platform_runtime_module.addCMacro("BK_PLATFORM_RUNTIME_BUILD", "1");
    platform_runtime_module.addCSourceFile(.{ .file = b.path("Sources/src/PlatformABI/PlatformRuntime.cpp"), .flags = &.{"-std=c++17"} });
    platform_runtime_module.addCSourceFile(.{ .file = b.path("Sources/src/Platform/Clock.cpp"), .flags = &.{"-std=c++17"} });
    platform_runtime_module.addCSourceFile(.{ .file = b.path("Sources/src/Platform/SocketWin32.cpp"), .flags = &.{"-std=c++17"} });
    platform_runtime_module.addCSourceFile(.{ .file = b.path("Sources/src/Platform/SocketPosix.cpp"), .flags = &.{"-std=c++17"} });
    linkCxxRuntime(platform_runtime_module, target);
    if (build_support.usesMsvc(platform)) {
        addMsvcIncludePaths(b, platform_runtime_module, toolchain);
        addMsvcLibraryPaths(b, platform_runtime_module, toolchain);
        linkMsvcRuntime(platform_runtime_module, optimize);
    }
    // Winsock is a property of the OS, not of the toolchain: MinGW needs it just
    // as much as MSVC does, and bundling the two together left the GNU ABI
    // build failing to link every WSA* symbol in SocketWin32.cpp.
    if (build_support.isWindows(platform)) platform_runtime_module.linkSystemLibrary("ws2_32", .{});
    applyLoaderPath(target, platform_runtime_module);
    addSharedObjectFinalizer(b, target, platform_runtime_module);
    const platform_runtime = b.addLibrary(.{
        .name = "PlatformRuntime",
        .linkage = .dynamic,
        .root_module = platform_runtime_module,
        .win32_module_definition = if (platform == .windows_x64) b.path("Sources/src/PlatformABI/PlatformRuntime.def") else null,
    });
    const platform_runtime_test_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    platform_runtime_test_module.addIncludePath(b.path("Sources/src"));
    addLinuxCxxIncludePaths(b, platform_runtime_test_module);
    platform_runtime_test_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_runtime_lifecycle_test.cpp"), .flags = &.{"-std=c++17"} });
    platform_runtime_test_module.linkLibrary(platform_runtime);
    linkCxxRuntime(platform_runtime_test_module, target);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_runtime_test_module, toolchain);
        addMsvcLibraryPaths(b, platform_runtime_test_module, toolchain);
        linkMsvcRuntime(platform_runtime_test_module, .Debug);
    }
    const platform_runtime_test = b.addExecutable(.{ .name = "platform-runtime-lifecycle-test", .root_module = platform_runtime_test_module });
    if (platform == .windows_x64) {
        platform_runtime_test.subsystem = .console;
        platform_runtime_test.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const platform_runtime_run = b.addRunArtifact(platform_runtime_test);
    const platform_runtime_step = b.step("test-platform-runtime", "Run shared platform runtime lifecycle tests");
    platform_runtime_step.dependOn(&platform_runtime.step);
    platform_runtime_step.dependOn(&platform_runtime_test.step);
    if (test_mode == .run) platform_runtime_step.dependOn(&platform_runtime_run.step);

    const consumer_a_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    consumer_a_module.addIncludePath(b.path("Sources/src"));
    addLinuxCxxIncludePaths(b, consumer_a_module);
    consumer_a_module.addCSourceFiles(.{ .files = &.{ "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/platform_test_consumer_a.cpp" }, .flags = &.{"-std=c++17"} });
    consumer_a_module.linkLibrary(platform_runtime);
    linkCxxRuntime(consumer_a_module, target);
    const consumer_a = b.addLibrary(.{ .name = "platform-consumer-a", .linkage = .dynamic, .root_module = consumer_a_module });
    const consumer_b_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    consumer_b_module.addIncludePath(b.path("Sources/src"));
    addLinuxCxxIncludePaths(b, consumer_b_module);
    consumer_b_module.addCSourceFiles(.{ .files = &.{ "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/platform_test_consumer_b.cpp" }, .flags = &.{"-std=c++17"} });
    consumer_b_module.linkLibrary(platform_runtime);
    linkCxxRuntime(consumer_b_module, target);
    const consumer_b = b.addLibrary(.{ .name = "platform-consumer-b", .linkage = .dynamic, .root_module = consumer_b_module });
    const client_test_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    client_test_module.addIncludePath(b.path("Sources/src"));
    addLinuxCxxIncludePaths(b, client_test_module);
    client_test_module.addCSourceFiles(.{ .files = &.{ "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/platform_client_test.cpp" }, .flags = &.{"-std=c++17"} });
    client_test_module.linkLibrary(platform_runtime);
    linkCxxRuntime(client_test_module, target);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, consumer_a_module, toolchain);
        addMsvcLibraryPaths(b, consumer_a_module, toolchain);
        addMsvcIncludePaths(b, consumer_b_module, toolchain);
        addMsvcLibraryPaths(b, consumer_b_module, toolchain);
        addMsvcIncludePaths(b, client_test_module, toolchain);
        addMsvcLibraryPaths(b, client_test_module, toolchain);
        linkMsvcRuntime(consumer_a_module, .Debug);
        linkMsvcRuntime(consumer_b_module, .Debug);
        linkMsvcRuntime(client_test_module, .Debug);
    }
    const client_test = b.addExecutable(.{ .name = "platform-client-test", .root_module = client_test_module });
    if (platform == .windows_x64) {
        client_test.subsystem = .console;
        client_test.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const client_run = b.addRunArtifact(client_test);
    client_run.addArtifactArg(consumer_a);
    client_run.addArtifactArg(consumer_b);
    const client_step = b.step("test-platform-client", "Run checked C++ platform client tests");
    client_step.dependOn(&platform_runtime.step);
    client_step.dependOn(&consumer_a.step);
    client_step.dependOn(&consumer_b.step);
    client_step.dependOn(&client_test.step);
    if (test_mode == .run) client_step.dependOn(&client_run.step);

    const platform_headers_step = b.step("test-platform-headers", "Validate portable compiler and legacy value types");
    if (test_mode == .run) {
        const platform_headers_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
        platform_headers_module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_headers_test.cpp"}, .flags = &.{} });
        platform_headers_module.addIncludePath(b.path("Sources/src"));
        addLinuxCxxIncludePaths(b, platform_headers_module);
        linkCxxRuntime(platform_headers_module, target);
        if (platform == .windows_x64) addMsvcIncludePaths(b, platform_headers_module, toolchain);
        const platform_headers_test = b.addExecutable(.{ .name = "platform-headers-test-run", .root_module = platform_headers_module });
        if (platform == .windows_x64) {
            addMsvcLibraryPaths(b, platform_headers_module, toolchain);
            linkMsvcRuntime(platform_headers_module, optimize);
            platform_headers_test.entry = .{ .symbol_name = "mainCRTStartup" };
        }
        const platform_headers_run = b.addRunArtifact(platform_headers_test);
        platform_headers_step.dependOn(&platform_headers_run.step);
    } else {
        const platform_headers_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
        platform_headers_module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_headers_test.cpp"}, .flags = &.{} });
        platform_headers_module.addIncludePath(b.path("Sources/src"));
        addLinuxCxxIncludePaths(b, platform_headers_module);
        linkCxxRuntime(platform_headers_module, target);
        if (platform == .windows_x64) addMsvcIncludePaths(b, platform_headers_module, toolchain);
        const platform_headers_object = b.addObject(.{ .name = "platform-headers-test", .root_module = platform_headers_module });
        platform_headers_step.dependOn(&platform_headers_object.step);
    }

    const platform_clock_module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, platform_clock_module);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_clock_module, toolchain);
        addMsvcLibraryPaths(b, platform_clock_module, toolchain);
        linkMsvcRuntime(platform_clock_module, .Debug);
    }
    platform_clock_module.addIncludePath(b.path("Sources/src/Misc"));
    platform_clock_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_clock_test.cpp",
            "Sources/src/Platform/Clock.cpp",
            "Sources/src/Misc/HPTimer.cpp",
        },
        .flags = &.{},
    });
    const platform_clock_test = b.addExecutable(.{ .name = "platform-clock-test", .root_module = platform_clock_module });
    platform_clock_test.subsystem = .console;
    if (platform == .windows_x64) platform_clock_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_clock_run = b.addRunArtifact(platform_clock_test);
    const platform_clock_step = b.step("test-platform-clock", "Run monotonic clock and high-resolution timer tests");
    platform_clock_step.dependOn(&platform_clock_test.step);
    if (test_mode == .run) platform_clock_step.dependOn(&platform_clock_run.step);

    const platform_sync_module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, platform_sync_module);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_sync_module, toolchain);
        addMsvcLibraryPaths(b, platform_sync_module, toolchain);
        linkMsvcRuntime(platform_sync_module, .Debug);
    }
    platform_sync_module.addIncludePath(b.path("Sources/src/Misc"));
    platform_sync_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_sync_test.cpp",
            "Sources/src/Platform/Sync.cpp",
            "Sources/src/Platform/Clock.cpp",
            "Sources/src/Misc/Thread.cpp",
        },
        .flags = if (platform == .windows_x64) &(cppflags_debug.* ++ .{"-DBLITZKRIEG_PLATFORM_SYNC_ONLY"}) else &.{"-DBLITZKRIEG_PLATFORM_SYNC_ONLY"},
    });
    const platform_sync_test = b.addExecutable(.{ .name = "platform-sync-test", .root_module = platform_sync_module });
    platform_sync_test.subsystem = .console;
    if (platform == .windows_x64) platform_sync_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_sync_run = b.addRunArtifact(platform_sync_test);
    const platform_sync_step = b.step("test-platform-sync", "Run synchronization and worker thread stress tests");
    platform_sync_step.dependOn(&platform_sync_test.step);
    if (test_mode == .run) platform_sync_step.dependOn(&platform_sync_run.step);

    const platform_debug_module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, platform_debug_module);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_debug_module, toolchain);
        addMsvcLibraryPaths(b, platform_debug_module, toolchain);
        linkMsvcRuntime(platform_debug_module, .Debug);
    }
    platform_debug_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_debug_test.cpp",
            "Sources/src/PlatformABI/PlatformClient.cpp",
            "Sources/src/Platform/Debug.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_debug else &.{},
    });
    platform_debug_module.linkLibrary(platform_runtime);
    linkCxxRuntime(platform_debug_module, target);
    const platform_debug_test = b.addExecutable(.{ .name = "platform-debug-test", .root_module = platform_debug_module });
    platform_debug_test.subsystem = .console;
    if (platform == .windows_x64) platform_debug_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_debug_run = b.addRunArtifact(platform_debug_test);
    const platform_debug_step = b.step("test-platform-debug", "Run portable diagnostic and debugger facade tests");
    platform_debug_step.dependOn(&platform_debug_test.step);
    platform_debug_step.dependOn(&platform_runtime.step);
    if (test_mode == .run) platform_debug_step.dependOn(&platform_debug_run.step);

    const platform_test_module_module = b.createModule(.{ .target = target, .optimize = .ReleaseFast });
    platform_test_module_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_test_module.cpp"), .flags = if (platform == .windows_x64) cppflags_release else &.{} });
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_test_module_module, toolchain);
        addMsvcLibraryPaths(b, platform_test_module_module, toolchain);
        linkMsvcRuntime(platform_test_module_module, .ReleaseFast);
    }
    const platform_test_module = b.addLibrary(.{ .name = "platform-test-module", .linkage = .dynamic, .root_module = platform_test_module_module });
    const sdl_dynamic_dep = b.dependency("sdl", .{
        .target = dependency_target,
        .optimize = .ReleaseFast,
        .preferred_linkage = .dynamic,
        .install_build_config_h = true,
    });
    const sdl_dynamic = sdl_dynamic_dep.artifact("SDL3");
    const platform_dynamic_module = b.createModule(.{ .target = target, .optimize = .ReleaseFast });
    addProjectIncludePaths(b, platform_dynamic_module);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_dynamic_module, toolchain);
        addMsvcLibraryPaths(b, platform_dynamic_module, toolchain);
        linkMsvcRuntime(platform_dynamic_module, .ReleaseFast);
        platform_dynamic_module.linkSystemLibrary("kernel32", .{});
    }
    platform_dynamic_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_dynamic_library_test.cpp",
            "Sources/src/PlatformABI/PlatformClient.cpp",
            "Sources/src/Platform/DynamicLibrary.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_release else &.{},
    });
    platform_dynamic_module.linkLibrary(platform_runtime);
    linkCxxRuntime(platform_dynamic_module, target);
    const platform_dynamic_test = b.addExecutable(.{ .name = "platform-dynamic-library-test", .root_module = platform_dynamic_module });
    platform_dynamic_test.subsystem = .console;
    if (platform == .windows_x64) platform_dynamic_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_dynamic_run = b.addRunArtifact(platform_dynamic_test);
    platform_dynamic_run.addArtifactArg(platform_test_module);
    const platform_dynamic_step = b.step("test-platform-dynamic-library", "Run portable dynamic library ownership tests");
    platform_dynamic_step.dependOn(&platform_dynamic_test.step);
    platform_dynamic_step.dependOn(&platform_test_module.step);
    platform_dynamic_step.dependOn(&platform_runtime.step);
    if (test_mode == .run) platform_dynamic_step.dependOn(&platform_dynamic_run.step);

    const platform_system_module = b.createModule(.{ .target = target, .optimize = .ReleaseFast });
    addProjectIncludePaths(b, platform_system_module);
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_system_module, toolchain);
        addMsvcLibraryPaths(b, platform_system_module, toolchain);
    }
    platform_system_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_system_test.cpp",
            "Sources/src/Platform/System.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_release else &.{},
    });
    platform_system_module.linkLibrary(sdl_dynamic);
    const platform_system_test = b.addExecutable(.{ .name = "platform-system-test", .root_module = platform_system_module });
    platform_system_test.subsystem = .console;
    if (platform == .windows_x64) platform_system_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_system_run = b.addRunArtifact(platform_system_test);
    const platform_system_step = b.step("test-platform-system", "Run portable system facade tests");
    platform_system_step.dependOn(&platform_system_test.step);
    if (test_mode == .run) platform_system_step.dependOn(&platform_system_run.step);

    const legacy_variant_module = b.createModule(.{ .target = target, .optimize = .Debug });
    legacy_variant_module.link_libc = !build_support.usesMsvc(platform);
    legacy_variant_module.link_libcpp = build_support.needsBundledLibcpp(platform);
    legacy_variant_module.addIncludePath(b.path("Sources/src"));
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, legacy_variant_module, toolchain);
        addMsvcLibraryPaths(b, legacy_variant_module, toolchain);
        linkMsvcRuntime(legacy_variant_module, .Debug);
        linkComSupport(legacy_variant_module, .Debug);
    }
    legacy_variant_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/legacy_variant_test.cpp",
            "Sources/src/Platform/LegacyVariant.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_debug else &.{},
    });
    const legacy_variant_test = b.addExecutable(.{ .name = "legacy-variant-test", .root_module = legacy_variant_module });
    legacy_variant_test.subsystem = .console;
    if (platform == .windows_x64) legacy_variant_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const legacy_variant_run = b.addRunArtifact(legacy_variant_test);
    const legacy_variant_step = b.step("test-legacy-variant", "Run portable legacy variant ownership and conversion tests");
    legacy_variant_step.dependOn(&legacy_variant_test.step);
    if (test_mode == .run) legacy_variant_step.dependOn(&legacy_variant_run.step);
    // Project-XML round trip (spec D-07): Sources/src/ResourceModel/xml.cpp over the 21 ResourceEditor
    // fixtures plus an unknown-node preservation check and the MFC editor's own saved projects. No engine modules are loaded, so no rdynamic;
    // the $ORIGIN rpath keeps the AGENTS.md Linux pattern.
    const resource_xml_roundtrip_module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
    });
    resource_xml_roundtrip_module.link_libc = !build_support.usesMsvc(platform);
    resource_xml_roundtrip_module.link_libcpp = !build_support.usesMsvc(platform);
    resource_xml_roundtrip_module.addIncludePath(b.path("Sources/src"));
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, resource_xml_roundtrip_module, toolchain);
        addMsvcLibraryPaths(b, resource_xml_roundtrip_module, toolchain);
        linkMsvcRuntime(resource_xml_roundtrip_module, .Debug);
    }
    resource_xml_roundtrip_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/resource_xml_roundtrip_test.cpp",
            "Sources/src/ResourceModel/xml.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_debug else &.{"-std=c++17"},
    });
    const resource_xml_roundtrip_test = b.addExecutable(.{ .name = "resource-xml-roundtrip-test", .root_module = resource_xml_roundtrip_module });
    resource_xml_roundtrip_test.subsystem = .console;
    if (platform == .windows_x64) resource_xml_roundtrip_test.entry = .{ .symbol_name = "mainCRTStartup" };
    applyLoaderPath(target, resource_xml_roundtrip_module);
    const resource_xml_roundtrip_run = b.addRunArtifact(resource_xml_roundtrip_test);
    resource_xml_roundtrip_run.setCwd(b.path("."));
    resource_xml_roundtrip_run.addArg("tools/zig/fixtures/resource_editor");
    resource_xml_roundtrip_run.addArg("zig-out/local-test/resource_editor/roundtrip");
    // The MFC editor's own saved projects, read only.
    resource_xml_roundtrip_run.addArg("Data/Editor/TestProjects");
    const resource_xml_roundtrip_step = b.step("test-resource-xml-roundtrip", "Round-trip the 21 ResourceEditor project XML fixtures and check unknown-node preservation");
    resource_xml_roundtrip_step.dependOn(&resource_xml_roundtrip_test.step);
    if (test_mode == .run) resource_xml_roundtrip_step.dependOn(&resource_xml_roundtrip_run.step);

    // DXT tolerance measurement (spec D-11 / R019, S03 T07): shipped _c.dds textures re-encoded by
    // NDxt and by the MFC-era S3TC encoder (Sources/src/ResourceModel/spike/legacy_dxt.*). The
    // measure step writes the tolerance JSON the D-11 comparator's DXT gate reads; the test step
    // measures again and fails unless the committed JSON is unchanged.
    const dxt_tolerance_module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
    });
    dxt_tolerance_module.link_libc = !build_support.usesMsvc(platform);
    dxt_tolerance_module.link_libcpp = !build_support.usesMsvc(platform);
    dxt_tolerance_module.addIncludePath(b.path("Sources/src"));
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, dxt_tolerance_module, toolchain);
        addMsvcLibraryPaths(b, dxt_tolerance_module, toolchain);
        linkMsvcRuntime(dxt_tolerance_module, .Debug);
    }
    dxt_tolerance_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/dxt_tolerance_test.cpp",
            "Sources/src/ResourceModel/dxt_gate.cpp",
            "Sources/src/ResourceModel/spike/legacy_dxt.cpp",
            "Sources/src/Image/DxtCodec.cpp",
        },
        .flags = if (platform == .windows_x64) cppflags_debug else &.{"-std=c++17"},
    });
    const dxt_tolerance_test = b.addExecutable(.{ .name = "dxt-tolerance-test", .root_module = dxt_tolerance_module });
    dxt_tolerance_test.subsystem = .console;
    if (platform == .windows_x64) dxt_tolerance_test.entry = .{ .symbol_name = "mainCRTStartup" };
    applyLoaderPath(target, dxt_tolerance_module);
    const dxt_tolerance_run = b.addRunArtifact(dxt_tolerance_test);
    dxt_tolerance_run.setCwd(b.path("."));
    dxt_tolerance_run.addArg("tools/zig/fixtures/resource_editor/dxt-tolerance.json");
    dxt_tolerance_run.addArg("zig-out/local-test/resource_editor/dxt");
    dxt_tolerance_run.addArg("--check");
    dxt_tolerance_run.has_side_effects = true;
    const dxt_tolerance_step = b.step("test-dxt-tolerance", "Re-measure the DXT tolerance on shipped _c.dds (DXT1/3/5, colour and alpha) and require dxt-tolerance.json unchanged");
    dxt_tolerance_step.dependOn(&dxt_tolerance_test.step);
    if (test_mode == .run) dxt_tolerance_step.dependOn(&dxt_tolerance_run.step);
    const dxt_measure_run = b.addRunArtifact(dxt_tolerance_test);
    dxt_measure_run.setCwd(b.path("."));
    dxt_measure_run.addArg("tools/zig/fixtures/resource_editor/dxt-tolerance.json");
    dxt_measure_run.addArg("zig-out/local-test/resource_editor/dxt");
    dxt_measure_run.has_side_effects = true;
    const dxt_measure_step = b.step("measure-dxt-tolerance", "Measure the DXT tolerance on shipped _c.dds textures and write tools/zig/fixtures/resource_editor/dxt-tolerance.json");
    dxt_measure_step.dependOn(&dxt_measure_run.step);

    const foundation_matrix_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/platform_build_matrix_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const foundation_matrix_tests = b.addTest(.{ .root_module = foundation_matrix_module });
    const foundation_matrix_run = b.addRunArtifact(foundation_matrix_tests);

    const stage_tests_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/stage_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const stage_tests = b.addTest(.{ .root_module = stage_tests_module });
    const stage_tests_run = b.addRunArtifact(stage_tests);
    // One test reads build.zig (every stage-game run goes through
    // addStageGameRun), so the run starts in the build root wherever zig build
    // was invoked from.
    stage_tests_run.setCwd(b.path("."));
    const stage_test_step = b.step("test-stage", "Run shell-free runtime staging tests");
    stage_test_step.dependOn(&stage_tests.step);
    if (test_mode == .run) stage_test_step.dependOn(&stage_tests_run.step);

    const package_tests_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/package_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const package_tests = b.addTest(.{ .root_module = package_tests_module });
    const package_tests_run = b.addRunArtifact(package_tests);
    const package_test_step = b.step("test-package", "Run release zip writer tests");
    package_test_step.dependOn(&package_tests.step);
    if (test_mode == .run) package_test_step.dependOn(&package_tests_run.step);

    const present_fit_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/GFXGPU/present_fit.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const present_fit_tests = b.addTest(.{ .root_module = present_fit_module });
    const present_fit_tests_run = b.addRunArtifact(present_fit_tests);
    const present_fit_step = b.step("test-present-fit", "Run the shrink-only present fit rect tests");
    present_fit_step.dependOn(&present_fit_tests.step);
    if (test_mode == .run) present_fit_step.dependOn(&present_fit_tests_run.step);

    const runtime_verify_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/verify_runtime.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const runtime_verify_tests = b.addTest(.{ .root_module = runtime_verify_module });
    const runtime_verify_run = b.addRunArtifact(runtime_verify_tests);
    const runtime_verify_step = b.step("verify-runtime", "Run the staged runtime layout verifier tests");
    runtime_verify_step.dependOn(&runtime_verify_tests.step);
    if (test_mode == .run) runtime_verify_step.dependOn(&runtime_verify_run.step);

    const shader_parser_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/compile_gfxgpu_shaders.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const shader_parser_tests = b.addTest(.{ .root_module = shader_parser_module });
    const shader_parser_tests_run = b.addRunArtifact(shader_parser_tests);
    const shader_tests_step = b.step("test-gfxgpu-shaders", "Run shader manifest parser tests");
    shader_tests_step.dependOn(&shader_parser_tests.step);
    if (test_mode == .run) shader_tests_step.dependOn(&shader_parser_tests_run.step);

    const hermeticity_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/build_hermeticity_test.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const hermeticity_test = b.addTest(.{ .root_module = hermeticity_module });
    const hermeticity_run = b.addRunArtifact(hermeticity_test);
    const hermeticity_step = b.step("audit-build-hermeticity", "Reject shell-dependent shader/build-tool steps");
    hermeticity_step.dependOn(&hermeticity_test.step);
    if (test_mode == .run) hermeticity_step.dependOn(&hermeticity_run.step);

    const test_platform_foundation = b.step("test-platform-foundation", "Run the portable foundation test matrix");
    test_platform_foundation.dependOn(build_support_step);
    test_platform_foundation.dependOn(platform_headers_step);
    test_platform_foundation.dependOn(stage_test_step);
    test_platform_foundation.dependOn(shader_tests_step);
    test_platform_foundation.dependOn(hermeticity_step);
    test_platform_foundation.dependOn(&foundation_matrix_tests.step);
    test_platform_foundation.dependOn(platform_abi_layout_step);
    test_platform_foundation.dependOn(platform_runtime_step);
    test_platform_foundation.dependOn(client_step);
    test_platform_foundation.dependOn(runtime_platform_audit_step);
    test_platform_foundation.dependOn(present_fit_step);
    test_platform_foundation.dependOn(platform_linkage_step);
    if (test_mode == .run) test_platform_foundation.dependOn(&foundation_matrix_run.step);
    const test_platform_core = b.step("test-platform-core", "Run the Phase 01 portable runtime core tests");
    test_platform_core.dependOn(test_platform_foundation);
    test_platform_core.dependOn(platform_clock_step);
    test_platform_core.dependOn(platform_sync_step);
    test_platform_core.dependOn(platform_debug_step);
    test_platform_core.dependOn(platform_dynamic_step);
    test_platform_core.dependOn(platform_system_step);
    const platform_foundation = b.step("platform-foundation", "Build the supported portable foundation matrix");
    platform_foundation.dependOn(test_platform_foundation);
    addGameCommandLineTest(b, target, test_mode, toolchain);
    addGameFrameTest(b, target, test_mode, toolchain, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addGameSystemKeysTest(b, target, test_mode, toolchain);
    addGameMouseCaptureTest(b, target, test_mode, toolchain);
    addGameLoopTest(b, target, test_mode, toolchain, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addSdlApplicationTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"), platform_runtime);
    addSdlEventTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"), platform_runtime);
    addInputCodesTest(b, target, test_mode, toolchain);
    addPlatformInputTest(b, target, test_mode, toolchain);
    addInputStateFixtureTest(b, target, test_mode, toolchain);
    const wheel_scroll_step = addWheelScrollTest(b, target, platform, test_mode, toolchain);
    // Task 7.3 review: in the foundation matrix, so every CI job builds it and
    // the run-mode jobs run it.
    test_platform_foundation.dependOn(wheel_scroll_step);
    addInputHeaderAuditTest(b, target, test_mode, toolchain);
    addInputTextRepeatTest(b, target, test_mode, toolchain);
    addInputControllerTest(b, target, test_mode, toolchain);
    addInputBindingsTest(b, target, test_mode, toolchain);
    addPlatformClipboardTest(b, target, test_mode, toolchain);
    addPlatformControllerTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addPlatformAudioTest(b, target, test_mode, toolchain);
    addAudioLifecycleFixtureTest(b, target, test_mode, toolchain);
    addAudioWorkerTest(b, target, test_mode, toolchain);
    addAudioStreamTest(b, target, test_mode, toolchain);
    addInputAudioGateTest(b, target, test_mode, toolchain);
    addPlatformSocketTypesTest(b, target, test_mode, toolchain);
    addPlatformNetworkTest(b, target, test_mode, toolchain);
    addPlatformSocketAbiTest(b, target, test_mode, toolchain, platform_runtime);
    addNetLowestTest(b, target, test_mode, toolchain);
    addNetworkWorkersTest(b, target, test_mode, toolchain);
    addNetworkSystemGateTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addRuntimeHeadersTest(b, target, test_mode, toolchain);
    addResourceModelScaffoldTest(b, target, test_mode, toolchain);
    addResourceModelReferencesTest(b, target, test_mode, toolchain);
    addResourceModelGridProjectionTest(b, target, test_mode, toolchain);
    addResourceModelFidelityTest(b, target, test_mode, toolchain);

    const sdl3_dep = b.dependency("sdl3", .{
        .target = dependency_target,
        .optimize = optimize,
        .c_sdl_preferred_linkage = .dynamic,
        .c_sdl_install_build_config_h = true,
        // Runtime shaders are precompiled into DXIL on Windows.  The
        // standalone shadercross CLI below remains enabled for generation,
        // while omitting the optional runtime extension avoids pulling the
        // libc++-based SPIR-V Cross library into the MSVC game DLLs.
        // Shadercross is used by the standalone shader-generation tool below;
        // the game runtime does not use the optional SDL shadercross module.
        // Keeping it out of the runtime graph prevents libc++ from being
        // mixed into the legacy libstdc++ module ABI on Linux.
        .ext_shadercross = false,
        // Windows shader generation uses the prebuilt DXC runtime below.  Do
        // not compile the source DXC backend into the MSVC SDL runtime
        // library: that backend is MinGW-oriented and is incompatible with
        // the MSVC target ABI.  The generated DXIL blobs still use DXC.
        .ext_shadercross_dxc = false,
    });
    const sdl3 = sdl3_dep.module("sdl3");
    const gfx_gpu_zig = addGfxGpuZig(b, target, optimize, sdl3);
    const editor_imgui = addEditorImgui(b, target, optimize, toolchain, sdl_dynamic_dep.path("include"));
    const editor_imgui_step = b.step("editor-imgui", "Build Dear ImGui with its SDL3 backends for the editor");
    editor_imgui_step.dependOn(&editor_imgui.step);
    const editor_imgui_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/kit/imgui/imgui.zig"),
        .target = target,
        .optimize = optimize,
    });
    editor_imgui_module.addIncludePath(b.path("vendor/dcimgui/src-docking"));
    editor_imgui_module.addIncludePath(b.path("Sources/editor/kit/imgui"));
    // cimgui.h includes <assert.h>. A consumer that does not link libc
    // (MapEditor, whose CRT is the engine's) gets no libc headers from Zig for
    // its @cImport on MSVC, so name the MSVC and UCRT ones here.
    addMsvcIncludePaths(b, editor_imgui_module, toolchain);
    editor_imgui_module.linkLibrary(editor_imgui);

    const editor_overlay_spike_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/editor_overlay_spike.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl3 },
            .{ .name = "gfxgpu", .module = gfx_gpu_zig.root_module },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
        },
    });
    // The spike links editor-imgui, a C++ static library, so the executable
    // needs the C++ runtime and - on MSVC - the CRT search paths as well: a
    // static library propagates the libraries it wants, not where to find
    // them. Without this the Linux jobs fail on __cxa_* and _Unwind_Resume,
    // and the MSVC job cannot find ucrtd.
    addMsvcLibraryPaths(b, editor_overlay_spike_module, toolchain);
    // On MSVC the library already names the CRT it wants and Zig supplies its
    // own libc for the executable; naming the CRT a second time here links two
    // of them (duplicate _cexit, _wctype, ...). Everywhere else the executable
    // is the one that has to pull the C++ runtime in.
    if (target.result.abi == .msvc) {
        // Zig supplies the CRT once the module asks for libc; the vendored
        // ImGui only adds the C++ standard library on top. Naming the CRT
        // again here linked two of them (duplicate _cexit, _wctype, ...).
        editor_overlay_spike_module.link_libc = true;

    } else {
        linkMsvcRuntime(editor_overlay_spike_module, optimize);
    }
    const editor_overlay_spike = b.addExecutable(.{ .name = "editor-overlay-spike", .root_module = editor_overlay_spike_module });
    if (target.result.os.tag == .windows) editor_overlay_spike.subsystem = .console;
    const editor_overlay_spike_install = b.addInstallArtifact(editor_overlay_spike, .{});
    const editor_overlay_spike_build_step = b.step("editor-overlay-spike-build", "Build the Map Editor ImGui overlay spike");
    editor_overlay_spike_build_step.dependOn(&editor_overlay_spike_install.step);
    addGameBootstrapSmoke(b, target, dependency_target, optimize, toolchain, gfx_gpu_zig, platform_runtime, sdl_dynamic_dep.path("include"), test_mode);
    const renderer = b.option([]const u8, "renderer", "Graphics renderer: sdl_gpu (default) or legacy (comparison)") orelse "sdl_gpu";
    if (!std.mem.eql(u8, renderer, "legacy") and !std.mem.eql(u8, renderer, "sdl_gpu")) {
        @panic("invalid -Drenderer value; expected legacy or sdl_gpu");
    }
    if (std.mem.eql(u8, renderer, "sdl_gpu")) auditDefaultRendererInputs(gfx_gpu_sources);
    _ = b.option(bool, "sdl-debug", "Enable SDL GPU debug validation") orelse false;
    const sdl3_verify_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/verify_sdl3.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "sdl3", .module = sdl3 }},
    });
    const sdl3_verify = b.addExecutable(.{
        .name = "verify-sdl3",
        .root_module = sdl3_verify_module,
    });
    const sdl3_verify_run = b.addRunArtifact(sdl3_verify);
    const sdl3_step = b.step("sdl3", "Build and verify the zig-sdl3 dependency");
    sdl3_step.dependOn(&sdl3_verify_run.step);

    const sdl3_build = b.lazyImport(@This(), "sdl3") orelse return;
    const shadercross_cli = sdl3_build.shadercross.cli(
        b,
        null,
        true,
        b.graph.host.result.os.tag != .windows,
    ) orelse return;
    if (b.graph.host.result.os.tag == .macos)
        addMacosSysrootPathsToModule(b, shadercross_cli.root_module);
    var dxc_runtime_path: ?[]const u8 = null;
    if (b.graph.host.result.os.tag == .windows) {
        const dxc_binary = b.lazyDependency("dxc_binary", .{}) orelse return;
        const dxc_arch = switch (b.graph.host.result.cpu.arch) {
            .x86 => "x86",
            .x86_64 => "x64",
            .aarch64 => "arm64",
            else => @panic("unsupported Windows shadercross host architecture"),
        };
        shadercross_cli.root_module.addCMacro("SDL_SHADERCROSS_DXC", "1");
        shadercross_cli.root_module.addIncludePath(dxc_binary.path("inc"));
        shadercross_cli.root_module.addObjectFile(dxc_binary.path(b.fmt("lib/{s}/dxcompiler.lib", .{dxc_arch})));
        shadercross_cli.root_module.addObjectFile(dxc_binary.path(b.fmt("lib/{s}/dxil.lib", .{dxc_arch})));
        dxc_runtime_path = dxc_binary.path(b.fmt("bin/{s}", .{dxc_arch})).getPath(b);
    }
    const shadercross_build_step = b.step("shadercross-build", "Build the pinned host SDL_shadercross tool");
    shadercross_build_step.dependOn(&shadercross_cli.step);

    const shadercross_verify_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/verify_shadercross.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const shadercross_verify = b.addExecutable(.{
        .name = "verify-shadercross",
        .root_module = shadercross_verify_module,
    });
    const shadercross_verify_run = b.addRunArtifact(shadercross_verify);
    shadercross_verify_run.addArtifactArg(shadercross_cli);
    if (dxc_runtime_path) |path| shadercross_verify_run.addPathDir(path);
    shadercross_verify_run.step.dependOn(shadercross_build_step);
    const shadercross_verify_step = b.step("verify-shadercross", "Verify shadercross CLI options and host installation");
    shadercross_verify_step.dependOn(&shadercross_verify_run.step);

    const shader_driver_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/compile_gfxgpu_shaders.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const shader_driver = b.addExecutable(.{
        .name = "compile-gfxgpu-shaders",
        .root_module = shader_driver_module,
    });
    const shader_driver_run = b.addRunArtifact(shader_driver);
    const shader_formats = b.option([]const u8, "shader-formats", "Comma-separated shader output formats: dxil,spirv,msl") orelse switch (target.result.os.tag) {
        .linux => "spirv",
        .macos => "msl",
        else => "dxil",
    };
    shader_driver_run.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_driver_run.addPathDir(path);
    shader_driver_run.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_driver_run.addArtifactArg(shadercross_cli);
    shader_driver_run.addArg("zig-out/shaders");
    shader_driver_run.addArg(shader_formats);
    // The driver reads the manifest and every .hlsl beside it but takes them as a
    // plain path argument, so none of its real inputs were visible to the build
    // cache: it was keyed on its argv alone and an edited shader was silently
    // never recompiled. Declaring the sources makes both this step and the
    // staging that consumes its output re-run exactly when a shader changes.
    const shader_sources = shaderSourceFiles(b) catch &[_][]const u8{};
    for (shader_sources) |source| shader_driver_run.addFileInput(b.path(source));

    const gfx_gpu_shaders_step = b.step("gfxgpu-shaders", "Compile deterministic GfxGpu shader blobs and manifest");
    gfx_gpu_shaders_step.dependOn(&shader_driver_run.step);
    // The driver parses the manifest this step compiles from, so the parser has
    // to build. Running its tests belongs to test-gfxgpu-shaders: depending on
    // the run put a test-runner handshake in front of every install-game, and a
    // build that merely failed to talk to that process failed shader
    // compilation, and the whole install, along with it.
    gfx_gpu_shaders_step.dependOn(&shader_parser_tests.step);

    const shader_determinism_a = b.addRunArtifact(shader_driver);
    shader_determinism_a.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_determinism_a.addPathDir(path);
    shader_determinism_a.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_determinism_a.addArtifactArg(shadercross_cli);
    shader_determinism_a.addArg("zig-out/shaders-determinism-a");
    shader_determinism_a.addArg(shader_formats);
    const shader_determinism_b = b.addRunArtifact(shader_driver);
    shader_determinism_b.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_determinism_b.addPathDir(path);
    shader_determinism_b.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_determinism_b.addArtifactArg(shadercross_cli);
    shader_determinism_b.addArg("zig-out/shaders-determinism-b");
    shader_determinism_b.addArg(shader_formats);

    const shader_compare_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/compare_trees.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const shader_compare = b.addExecutable(.{
        .name = "compare-shader-trees",
        .root_module = shader_compare_module,
    });
    const shader_compare_run = b.addRunArtifact(shader_compare);
    shader_compare_run.step.dependOn(&shader_determinism_a.step);
    shader_compare_run.step.dependOn(&shader_determinism_b.step);
    shader_compare_run.addArgs(&.{ "zig-out/shaders-determinism-a", "zig-out/shaders-determinism-b" });
    const shader_determinism_step = b.step("test-gfxgpu-shader-determinism", "Compare two clean shader compiler output directories");
    shader_determinism_step.dependOn(&shader_compare_run.step);

    const blitz64 = addBlitz64(b, target, optimize);
    // <os>/<arch>[/<variant>], so a second architecture for one OS lands beside
    // the first rather than on top of it: macos/arm64/release next to
    // macos/x86_64/release. The default variant stays unqualified, as it was when
    // this was a "-release"/"-debug" suffix on a single directory name.
    // The staged tree is named for what it holds, and the optimize mode is what
    // decides that: `--release=fast` stages release, a plain `zig build` stages
    // debug, whichever way the mode was reached. Naming the directory from
    // -Dbuild-variant instead left the unqualified variant covering both modes,
    // so a debug build and a release build staged over each other and the tree
    // said nothing about which one was in it. Deriving it from the mode also
    // means the name can never disagree with the contents.
    const variant_suffix = b.fmt("/{s}", .{if (optimize == .Debug) "debug" else "release"});
    const platform_root = b.fmt("{s}/{s}", .{ platform_policy.os_dir, platform_policy.arch_dir });
    const stage_root = b.fmt("zig-out/game/{s}{s}", .{ platform_root, variant_suffix });
    const package_root = b.fmt("zig-out/packages/{s}{s}", .{ platform_root, variant_suffix });
    const stage_game_name = platform_policy.executable_name;
    const stage_metadata_files = package_policy.required_metadata_files[0..];
    // rclone ships beside the game so cloud sync works on a machine with
    // nothing on PATH: daemon discovery already searches the executable's own
    // directory before PATH, so bundling is entirely a staging job and needs no
    // discovery change. stage.zig copies these names out of zig-out/bin, which
    // is why the binary is installed there first, below game-all.
    const rclone_bundle = build_support.bundledRclone(platform);
    const stage_runtime_files = stage_files: {
        const engine_files: []const []const u8 = switch (target.result.os.tag) {
            .windows => &[_][]const u8{ "Game.exe", "PlatformRuntime.dll", "StreamIO.dll", "StreamIOOptionsAbi.dll", "CloudSync.dll", "Anim.dll", "GFXGPU.dll", "SDL3.dll", "Image.dll", "Input.dll", "Net.dll", "SFX.dll", "UI.dll", "Scene.dll", "AILogic.dll", "GameTT.dll" },
            .linux => &[_][]const u8{ "Game", "libPlatformRuntime.so", "libStreamIO.so", "libStreamIOOptionsAbi.so", "libCloudSync.so", "libAnim.so", "libGFXGPU.so", "libGfxGpuZig.so", "libSDL3.so.0", "libImage.so", "libInput.so", "libNet.so", "libSFX.so", "libUI.so", "libScene.so", "libAILogic.so", "libGameTT.so" },
            .macos => &[_][]const u8{ "Game", "libPlatformRuntime.dylib", "libStreamIO.dylib", "libStreamIOOptionsAbi.dylib", "libCloudSync.dylib", "libAnim.dylib", "libGFXGPU.dylib", "libSDL3.dylib", "libImage.dylib", "libInput.dylib", "libNet.dylib", "libSFX.dylib", "libUI.dylib", "libScene.dylib", "libAILogic.dylib", "libGameTT.dylib" },
            else => &[_][]const u8{stage_game_name},
        };
        const files = b.allocator.alloc([]const u8, engine_files.len + 1) catch @panic("OOM");
        @memcpy(files[0..engine_files.len], engine_files);
        files[engine_files.len] = rclone_bundle.installed_name;
        break :stage_files files;
    };
    const stage_debug_files = if (target.result.os.tag == .windows)
        &[_][]const u8{ "Game.pdb", "StreamIO.pdb", "StreamIOOptionsAbi.pdb", "Anim.pdb", "GFXGPU.pdb", "Image.pdb", "Input.pdb", "Net.pdb", "SFX.pdb", "UI.pdb", "Scene.pdb", "AILogic.pdb", "GameTT.pdb" }
    else
        &[_][]const u8{};
    const gfx_gpu_abi_test_module = b.createModule(.{
        // An explicitly selected native Apple target still needs Zig's host
        // target query for framework discovery (CoreMedia, Metal, etc.).
        .target = dependency_target,
        .optimize = optimize,
    });
    gfx_gpu_abi_test_module.addCSourceFiles(.{
        .files = &.{"tools/zig/gfxgpu_abi_test.cpp"},
        .flags = cppflagsForTarget(target, optimize),
    });
    gfx_gpu_abi_test_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    addMsvcIncludePaths(b, gfx_gpu_abi_test_module, toolchain);
    addLinuxCxxIncludePaths(b, gfx_gpu_abi_test_module);
    addMsvcLibraryPaths(b, gfx_gpu_abi_test_module, toolchain);
    gfx_gpu_abi_test_module.linkLibrary(gfx_gpu_zig);
    linkMsvcRuntime(gfx_gpu_abi_test_module, optimize);
    const gfx_gpu_abi_test = b.addExecutable(.{
        .name = "gfxgpu-abi-test",
        .root_module = gfx_gpu_abi_test_module,
    });
    if (target.result.os.tag == .windows) {
        gfx_gpu_abi_test.subsystem = .console;
        gfx_gpu_abi_test.entry = .{ .symbol_name = "main" };
    }
    const gfx_gpu_abi_test_run = b.addRunArtifact(gfx_gpu_abi_test);
    const gfx_gpu_abi_test_step = b.step("gfxgpu-abi-test", "Run the C++ GfxGpu ABI test");
    gfx_gpu_abi_test_step.dependOn(&gfx_gpu_abi_test_run.step);

    const gfx_gpu_smoke_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/gfxgpu_smoke.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{ .{ .name = "sdl3", .module = sdl3 }, .{ .name = "gfxgpu", .module = gfx_gpu_zig.root_module } },
    });
    const gfx_gpu_smoke = b.addExecutable(.{
        .name = "gfxgpu-smoke",
        .root_module = gfx_gpu_smoke_module,
    });
    const gfx_gpu_smoke_run = b.addRunArtifact(gfx_gpu_smoke);
    const gfx_gpu_smoke_install = b.addInstallArtifact(gfx_gpu_smoke, .{});
    const gfx_gpu_smoke_build_step = b.step("gfxgpu-smoke-build", "Build the Zig SDL3 GPU shader smoke test");
    gfx_gpu_smoke_build_step.dependOn(&gfx_gpu_smoke_install.step);
    gfx_gpu_smoke_build_step.dependOn(gfx_gpu_shaders_step);
    gfx_gpu_smoke_run.step.dependOn(&gfx_gpu_smoke_install.step);
    gfx_gpu_smoke_run.step.dependOn(gfx_gpu_shaders_step);
    gfx_gpu_smoke_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{ .dest_dir = .{ .override = .bin } }).step);
    gfx_gpu_smoke_run.setCwd(b.path("."));
    if (target.result.os.tag == .linux) {
        gfx_gpu_smoke_run.setEnvironmentVariable("LD_LIBRARY_PATH", "zig-out/bin:zig-out/lib");
    }
    const gpu_driver = b.option([]const u8, "gpu-driver", "Native SDL_GPU driver expected by gfxgpu-smoke") orelse switch (target.result.os.tag) {
        .windows => "direct3d12",
        .linux => "vulkan",
        .macos => "metal",
        else => "",
    };
    if (gpu_driver.len != 0) {
        gfx_gpu_smoke_run.addArg("--driver");
        gfx_gpu_smoke_run.addArg(gpu_driver);
    }
    const gfx_gpu_smoke_step = b.step("gfxgpu-smoke", "Run the Zig SDL3 GPU shader smoke test");
    gfx_gpu_smoke_step.dependOn(&gfx_gpu_smoke_run.step);

    // Does this machine have a GPU device SDL can use? Nothing in CI has ever
    // created a real one - gfxgpu-factory-test fakes device creation and the
    // overlay spike is only built - so the answer is unknown rather than
    // known-bad. The probe prints it and always exits 0; it gates nothing.
    const gpu_device_probe_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/gpu_device_probe.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{ .{ .name = "sdl3", .module = sdl3 }, .{ .name = "gfxgpu", .module = gfx_gpu_zig.root_module } },
    });
    const gpu_device_probe = b.addExecutable(.{ .name = "gpu-device-probe", .root_module = gpu_device_probe_module });
    const gpu_device_probe_run = b.addRunArtifact(gpu_device_probe);
    gpu_device_probe_run.step.dependOn(&b.addInstallArtifact(gpu_device_probe, .{}).step);
    gpu_device_probe_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{ .dest_dir = .{ .override = .bin } }).step);
    gpu_device_probe_run.setCwd(b.path("."));
    if (target.result.os.tag == .linux) gpu_device_probe_run.setEnvironmentVariable("LD_LIBRARY_PATH", "zig-out/bin:zig-out/lib");
    const gpu_device_probe_step = b.step("gpu-device-probe", "Report whether this machine has a GPU device SDL can use");
    gpu_device_probe_step.dependOn(&gpu_device_probe_run.step);

    const editor_overlay_spike_run = b.addRunArtifact(editor_overlay_spike);
    editor_overlay_spike_run.step.dependOn(&editor_overlay_spike_install.step);
    editor_overlay_spike_run.step.dependOn(gfx_gpu_shaders_step);
    editor_overlay_spike_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{ .dest_dir = .{ .override = .bin } }).step);
    editor_overlay_spike_run.setCwd(b.path("."));
    if (target.result.os.tag == .linux) editor_overlay_spike_run.setEnvironmentVariable("LD_LIBRARY_PATH", "zig-out/bin:zig-out/lib");
    if (b.args) |args| editor_overlay_spike_run.addArgs(args);
    const editor_overlay_spike_step = b.step("editor-overlay-spike", "Run the Map Editor ImGui overlay spike");
    editor_overlay_spike_step.dependOn(&editor_overlay_spike_run.step);

    const gfx_reference_compare_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/compare_gfx_reference.zig"),
        .target = target,
        .optimize = optimize,
    });
    const gfx_reference_compare = b.addExecutable(.{
        .name = "compare-gfx-reference",
        .root_module = gfx_reference_compare_module,
    });
    const gfx_reference_compare_run = b.addRunArtifact(gfx_reference_compare);
    if (b.args) |args| gfx_reference_compare_run.addArgs(args);
    const gfx_reference_compare_step = b.step("compare-gfx-reference", "Compare two RGBA8 renderer reference captures");
    gfx_reference_compare_step.dependOn(&gfx_reference_compare_run.step);

    // The missing winter/Africa unit textures, derived from the summer ones:
    // `zig build season-textures -- Data [--dry-run] [--only Units/...]`.
    // A host tool; ReleaseFast writes the same bytes as Debug (no fast-math),
    // in a fraction of the time over all of Data/Units.
    const season_textures_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/season_textures.zig"),
        .target = b.graph.host,
        .optimize = .ReleaseFast,
    });
    const season_textures = b.addExecutable(.{
        .name = "season-textures",
        .root_module = season_textures_module,
    });
    const season_textures_run = b.addRunArtifact(season_textures);
    season_textures_run.setCwd(b.path("."));
    if (b.args) |args| season_textures_run.addArgs(args);
    const season_textures_step = b.step("season-textures", "Generate the missing winter/Africa unit textures in a Data tree");
    season_textures_step.dependOn(&season_textures_run.step);
    const season_textures_test_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/season_textures.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const season_textures_tests = b.addTest(.{ .root_module = season_textures_test_module });
    const season_textures_test_step = b.step("test-season-textures", "Run the season texture tool's codec and transform tests");
    season_textures_test_step.dependOn(&season_textures_tests.step);
    if (test_mode == .run) season_textures_test_step.dependOn(&b.addRunArtifact(season_textures_tests).step);
    // The same tool, run by every staging (install-game, and so
    // install-map-editor and every tier staged on it, package-game and
    // package-game-editors, with or without -Dcopy-data): see addSeasonData.
    const season_data = addSeasonData(b, season_textures);

    // ResourceEditor fixture generator. See
    // tools/zig/resource_editor_fixtures.zig for the design rationale and
    // the 21-row Fixture table it mirrors from Sources/src/editor/*Frm.cpp.
    const resource_editor_fixtures_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/resource_editor_fixtures.zig"),
        .target = b.graph.host,
        .optimize = .ReleaseFast,
    });
    const resource_editor_fixtures_exe = b.addExecutable(.{
        .name = "resource-editor-fixtures",
        .root_module = resource_editor_fixtures_module,
    });
    const resource_editor_fixtures_run = b.addRunArtifact(resource_editor_fixtures_exe);
    resource_editor_fixtures_run.setCwd(b.path("."));
    resource_editor_fixtures_run.addArg("--out");
    resource_editor_fixtures_run.addArg("tools/zig/fixtures/resource_editor");
    resource_editor_fixtures_run.addArg("--log");
    resource_editor_fixtures_run.addArg("zig-out/local-test/resource_editor/make-fixtures.log");
    const resource_editor_fixtures_step = b.step(
        "make-resource-fixtures",
        "Regenerate repo-owned ResourceEditor fixtures (21 extensions, project.<ext> + source art, deterministic)",
    );
    resource_editor_fixtures_step.dependOn(&resource_editor_fixtures_run.step);
    const resource_editor_fixtures_test_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/resource_editor_fixtures.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const resource_editor_fixtures_tests = b.addTest(.{ .root_module = resource_editor_fixtures_test_module });
    const resource_editor_fixtures_test_step = b.step(
        "test-resource-editor-fixtures",
        "Run the ResourceEditor fixture generator's in-memory tests",
    );
    resource_editor_fixtures_test_step.dependOn(&resource_editor_fixtures_tests.step);
    if (test_mode == .run) resource_editor_fixtures_test_step.dependOn(&b.addRunArtifact(resource_editor_fixtures_tests).step);
    // StreamIOOptionsAbi ships in the same directory as the shared SDL3
    // library and is loaded alongside it. It must share the game's one SDL3
    // image on every platform: a *static* SDL3 here is a second, private SDL
    // whose video subsystem is never initialized, so SDL_GetDisplays returns
    // nothing and the options menu degrades to a single monitor and no video
    // modes. On macOS the duplicate also collides at the Objective-C runtime
    // (duplicate class implementations, resolved arbitrarily by dyld).
    const options_bridge_sdl = sdl_dynamic;
    const options_bridge = addOptionsBridge(b, target, optimize, toolchain, platform_runtime, options_bridge_sdl);
    const options_bridge_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    options_bridge_test_module.addCSourceFile(.{
        .file = b.path("tools/zig/options_bridge_test.cpp"),
        .flags = cppflagsForOptimize(optimize),
    });
    addProjectIncludePaths(b, options_bridge_test_module);
    addMsvcIncludePaths(b, options_bridge_test_module, toolchain);
    addMsvcLibraryPaths(b, options_bridge_test_module, toolchain);
    linkMsvcRuntime(options_bridge_test_module, optimize);
    if (target.result.os.tag == .windows) {
        options_bridge_test_module.linkSystemLibrary("oleaut32", .{});
        options_bridge_test_module.linkSystemLibrary("comsuppw", .{});
    }
    const options_bridge_test = b.addExecutable(.{
        .name = "options-bridge-test",
        .root_module = options_bridge_test_module,
    });
    options_bridge_test.subsystem = .console;
    options_bridge_test.entry = .{ .symbol_name = "main" };
    const options_bridge_test_step = b.step("options-bridge-test", "Build portable options bridge contract tests");
    options_bridge_test_step.dependOn(&b.addInstallArtifact(options_bridge_test, .{}).step);


    // This module used to declare no C runtime at all and lean entirely on
    // linkMsvcRuntime below. That works when Zig supplies libc itself, but on a
    // Linux host linkCxxRuntime links the host's libstdc++ directly without
    // asking for libc, so the module compiled with no system headers and failed
    // on <stdio.h> - the long-standing red in the Linux CI job. MSVC still gets
    // its CRT from linkMsvcRuntime and must not have one forced here.
    const platform_module_test_module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
        .link_libc = !build_support.usesMsvc(platform),
        .link_libcpp = build_support.needsBundledLibcpp(platform),
    });
    const platform_module_test_flags: []const []const u8 = if (platform == .windows_x64) cppflagsForOptimize(.Debug) else &.{"-std=c++17"};
    platform_module_test_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_module_test.cpp"), .flags = platform_module_test_flags });
    addMsvcIncludePaths(b, platform_module_test_module, toolchain);
    addMsvcLibraryPaths(b, platform_module_test_module, toolchain);
    linkMsvcRuntime(platform_module_test_module, .Debug);
    const platform_module_test = b.addExecutable(.{ .name = "platform-module-test", .root_module = platform_module_test_module });
    if (platform == .windows_x64) {
        platform_module_test.subsystem = .console;
        platform_module_test.entry = .{ .symbol_name = "main" };
    }
    const platform_module_test_run = b.addRunArtifact(platform_module_test);
    platform_module_test_run.setCwd(b.path("."));
    const platform_module_test_step = b.step("test-platform-modules", "Run portable runtime module tests");
    // Every other test step gates its run on the mode; this one did not, so a
    // cross-compile of it tried to execute a foreign binary on the host and
    // failed -Dtest-mode=compile outright.
    platform_module_test_step.dependOn(&platform_module_test.step);
    if (test_mode == .run) platform_module_test_step.dependOn(&platform_module_test_run.step);
    const platform_storage_gate_module = b.createModule(.{ .target = target, .optimize = .Debug });
    var storage_gate_flags: std.ArrayListUnmanaged([]const u8) = .empty;
    storage_gate_flags.appendSlice(b.allocator, cppflagsForOptimize(.Debug)) catch @panic("OOM");
    storage_gate_flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    platform_storage_gate_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_storage_gate.cpp"), .flags = storage_gate_flags.items });
    addMsvcIncludePaths(b, platform_storage_gate_module, toolchain);
    addMsvcLibraryPaths(b, platform_storage_gate_module, toolchain);
    linkMsvcRuntime(platform_storage_gate_module, .Debug);
    const platform_storage_gate = b.addExecutable(.{ .name = "platform-storage-gate", .root_module = platform_storage_gate_module });
    platform_storage_gate.subsystem = .console;
    platform_storage_gate.entry = .{ .symbol_name = "main" };
    const platform_storage_gate_run = b.addRunArtifact(platform_storage_gate);
    platform_storage_gate_run.setCwd(b.path("."));
    const platform_storage_gate_step = b.step("test-platform-storage", "Run config/save/package storage gate");
    platform_storage_gate_step.dependOn(&platform_storage_gate_run.step);
    // Save-load spends its time in the zig structure reader; at Debug (-O0 +
    // safety) that alone made big-mission loads take ~1 min. The zig half of
    // StreamIO is unit-tested and ABI-thin, so it defaults to ReleaseFast even
    // in Debug builds. Pass -Dstreamio-fast=false when debugging streamio.zig
    // itself (the C++ bridge and CRT selection stay at the game's optimize
    // mode either way).
    const streamio_fast = b.option(bool, "streamio-fast", "Compile the StreamIO zig core ReleaseFast even in Debug builds") orelse true;
    const streamio_zig = addStreamIOZig(b, target, optimize, toolchain, options_bridge, platform_runtime, streamio_fast);
    // Cloud profile sync. Nothing loads it yet — the C++ facade arrives with
    // P06-M01 — so it is built and exercised by test-cloudsync-abi rather than
    // installed into the game layout; the packet that gives the game a reason
    // to load it is the packet that adds it to the staged runtime files.
    const cloudsync = addCloudSync(b, target, optimize, toolchain);
    const copy_data = b.option(bool, "copy-data", "Copy Data into install layout (the default)") orelse true;
    const use_prebuilt_shaders = b.option(bool, "use-prebuilt-shaders", "Skip gfxgpu-shaders and reuse existing zig-out/shaders outputs") orelse false;
    const startup_trace = b.option(bool, "startup-trace", "Emit Windows startup checkpoint markers to the debugger") orelse false;
    ubsan_trap = b.option(bool, "ubsan-trap", "Compile UBSan checks as traps so debuggers break at the faulting line (Debug only)") orelse false;
    const random_missions_sweep = b.option([]const u8, "random-missions-sweep", "test-random-missions: all, cover, cover-from=<n> (cover without its first n cases, to resume a cut-short run) or only=<text> (default all)") orelse "all";

    const zlib = addZlib(b, target, optimize, toolchain);
    const libpng = addLibpng(b, target, optimize, toolchain, zlib);
    const misc = addMisc(b, target, optimize, toolchain, sdl_dynamic_dep.path("include"));
    const image = addImage(b, target, optimize, toolchain, zlib, libpng, misc, platform_runtime, sdl_dynamic);
    const lualib = addLuaLib(b, target, optimize, toolchain);
    const net = addNet(b, target, optimize, toolchain, misc, platform_runtime, sdl_dynamic);
    const buildversion = if (platform == .windows_x64) addBuildVersion(b, target, optimize, toolchain, misc, platform_runtime, sdl_dynamic) else null;
    const betakeygen = if (platform == .windows_x64) addBetaKeyGen(b, target, optimize, toolchain, zlib, misc, platform_runtime, sdl_dynamic) else null;
    const input = addInput(b, target, optimize, toolchain, misc, platform_runtime, sdl_dynamic);
    addInputModuleTest(b, target, optimize, test_mode, toolchain, platform_runtime, input, misc, sdl_dynamic);
    const formats = addFormats(b, target, optimize, toolchain);
    const scene = addLegacyProjectDll(b, target, optimize, toolchain, "Scene", "Sources/src/Scene/Scene.vcxproj", "Sources/src/Scene/Scene.def", &.{ "Sources/src/Scene", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/libtheora-1.2.0/include" }, &.{ misc, formats }, platform_runtime, sdl_dynamic);
    const anim = addAnim(b, target, optimize, toolchain, misc, platform_runtime, formats, sdl_dynamic);
    const common = addCommon(b, target, optimize, toolchain);
    const ui = addUI(b, target, optimize, toolchain, misc, platform_runtime, common, lualib, sdl_dynamic);
    const fontgen = if (platform == .windows_x64) addFontGen(b, target, optimize, toolchain, image, common, formats, misc, platform_runtime, sdl_dynamic) else null;
    const sfx = addSFX(b, target, optimize, toolchain, misc, platform_runtime, common, sdl_dynamic);
    addSfxModuleTest(b, target, toolchain, platform_runtime, sfx, misc, sdl_dynamic, options_bridge, streamio_zig);
    const gfx_legacy = if (platform == .windows_x64) addGFX(b, target, optimize, toolchain, misc, platform_runtime, formats, sdl_dynamic) else null;
    const gfx_gpu = addGFXGPU(b, target, optimize, toolchain, misc, platform_runtime, formats, gfx_gpu_zig, sdl_dynamic, sdl_dynamic_dep.path("include"));
    if (!std.mem.eql(u8, renderer, "sdl_gpu") and platform != .windows_x64) @panic("legacy renderer is Windows-only; use -Drenderer=sdl_gpu");
    const gfx = if (std.mem.eql(u8, renderer, "sdl_gpu")) gfx_gpu else gfx_legacy.?;
    const randommapgen = addRandomMapGen(b, target, optimize, toolchain);
    const map_file = addMapFile(b, target, optimize, toolchain);
    addMapFileTest(b, target, optimize, toolchain, map_file, formats, randommapgen, misc, platform_runtime, streamio_zig, options_bridge, sdl_dynamic, test_mode);
    const ailogic = addLegacyProjectDll(b, target, optimize, toolchain, "AILogic", "Sources/src/AILogic/AILogic.vcxproj", "Sources/src/AILogic/AILogic.def", &.{ "Sources/src/AILogic", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/GameTT", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/vorbis-1.3.7/include" }, &.{ misc, lualib, formats, randommapgen, zlib }, platform_runtime, sdl_dynamic);
    const gamett = addLegacyProjectDll(b, target, optimize, toolchain, "GameTT", "Sources/src/GameTT/GameTT.vcxproj", "Sources/src/GameTT/GameTT.def", &.{ "Sources/src/GameTT", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/AILogic" }, &.{ misc, formats, common, randommapgen }, platform_runtime, sdl_dynamic);
    // Compile the game version directly into GameTT.dll so the title screen
    // shows the version string without relying on the Win32 version resource
    // API (which Zig's resinator does not produce correctly for runtime reads).
    gamett.root_module.addCMacro("BLITZKRIEG_VERSION", b.fmt("\"{d}.{d}.{d}\"", .{ game_version.major, game_version.minor, game_version.patch }));
    const main = addMain(b, target, optimize, toolchain);
    const editor_bridge = addEditorBridge(b, target, optimize, toolchain, common, sdl_dynamic_dep.path("include"));
    if (startup_trace) main.root_module.addCMacro("BK_STARTUP_TRACE", "1");
    const game = addGame(b, target, optimize, toolchain, main, misc, platform_runtime, lualib, zlib, randommapgen, formats, blitz64, startup_trace, renderer, platform, sdl_dynamic, sdl_dynamic_dep.path("include"));
    const package_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/package.zig"),
        .target = b.graph.host,
        .optimize = .ReleaseFast,
    });
    const stage_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/stage.zig"),
        .target = b.graph.host,
        .optimize = .ReleaseFast,
    });
    b.installArtifact(zlib);
    b.installArtifact(libpng);
    b.installArtifact(misc);
    b.installArtifact(image);
    b.installArtifact(lualib);
    b.installArtifact(net);
    if (buildversion) |artifact| b.installArtifact(artifact);
    if (betakeygen) |artifact| b.installArtifact(artifact);
    b.installArtifact(input);
    b.installArtifact(formats);
    b.installArtifact(anim);
    b.installArtifact(common);
    b.installArtifact(ui);
    if (fontgen) |artifact| b.installArtifact(artifact);
    b.installArtifact(sfx);
    b.installArtifact(gfx);
    b.installArtifact(randommapgen);
    b.installArtifact(main);
    b.installArtifact(options_bridge);
    b.installArtifact(platform_runtime);
    b.installArtifact(ailogic);
    b.installArtifact(gamett);
    b.installArtifact(streamio_zig);
    b.installArtifact(cloudsync);
    b.installArtifact(game);
    b.installArtifact(gfx_gpu_zig);

    const zlib_step = b.step("zlib", "Build the zlib static library");
    zlib_step.dependOn(&b.addInstallArtifact(zlib, .{}).step);

    const libpng_step = b.step("libpng", "Build the libpng static library");
    libpng_step.dependOn(&b.addInstallArtifact(libpng, .{}).step);

    const misc_step = b.step("misc", "Build the Misc static library");
    misc_step.dependOn(&b.addInstallArtifact(misc, .{}).step);

    const image_step = b.step("image", "Build the Image dynamic library");
    image_step.dependOn(&b.addInstallArtifact(image, .{}).step);

    const lualib_step = b.step("lualib", "Build the LuaLib static library");
    lualib_step.dependOn(&b.addInstallArtifact(lualib, .{}).step);

    const net_step = b.step("net", "Build the Net dynamic library");
    net_step.dependOn(&b.addInstallArtifact(net, .{}).step);

    const net_module_test_module = b.createModule(.{ .target = target, .optimize = .Debug });
    net_module_test_module.addCSourceFile(.{ .file = b.path("tools/zig/net_module_test.cpp"), .flags = cppflagsForTarget(target, .Debug) });
    addProjectIncludePaths(b, net_module_test_module);
    net_module_test_module.addIncludePath(b.path("Sources/src/Net"));
    addMsvcIncludePaths(b, net_module_test_module, toolchain);
    addMsvcLibraryPaths(b, net_module_test_module, toolchain);
    linkMsvcRuntime(net_module_test_module, .Debug);
    net_module_test_module.linkLibrary(misc);
    net_module_test_module.linkLibrary(platform_runtime);
    const net_module_test = b.addExecutable(.{ .name = "net-module-test", .root_module = net_module_test_module });
    if (target.result.os.tag == .windows) {
        net_module_test.subsystem = .console;
        net_module_test.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const net_module_test_run = b.addRunArtifact(net_module_test);
    net_module_test_run.setCwd(b.path("."));
    net_module_test_run.addArg(if (target.result.os.tag == .windows) "zig-out/bin/Net.dll" else "zig-out/lib/libNet.so");
    net_module_test_run.addPathDir(b.path("zig-out/bin").getPath(b));
    if (target.result.os.tag != .windows) net_module_test_run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    net_module_test_run.step.dependOn(&b.addInstallArtifact(net, .{}).step);
    net_module_test_run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    net_module_test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    net_module_test_run.step.dependOn(&b.addInstallArtifact(options_bridge, .{}).step);
    net_module_test_run.step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);
    const net_module_test_step = b.step("test-net-module", "Load the real Net module and verify its factory contract");
    net_module_test_step.dependOn(&net_module_test_run.step);

    if (buildversion) |artifact| {
        const buildversion_step = b.step("buildversion", "Build the BuildVersion console utility");
        buildversion_step.dependOn(&b.addInstallArtifact(artifact, .{}).step);
    }

    if (betakeygen) |artifact| {
        const betakeygen_step = b.step("betakeygen", "Build the BetaKeyGen console utility");
        betakeygen_step.dependOn(&b.addInstallArtifact(artifact, .{}).step);
    }

    const input_step = b.step("input", "Build the Input dynamic library");
    input_step.dependOn(&b.addInstallArtifact(input, .{}).step);

    const formats_step = b.step("formats", "Build the Formats static library");
    formats_step.dependOn(&b.addInstallArtifact(formats, .{}).step);

    const anim_step = b.step("anim", "Build the Anim dynamic library");
    anim_step.dependOn(&b.addInstallArtifact(anim, .{}).step);

    const common_step = b.step("common", "Build the Common static library");
    common_step.dependOn(&b.addInstallArtifact(common, .{}).step);

    const ui_step = b.step("ui", "Build the UI dynamic library");
    ui_step.dependOn(&b.addInstallArtifact(ui, .{}).step);

    if (fontgen) |artifact| {
        const fontgen_step = b.step("fontgen", "Build the FontGen console utility");
        fontgen_step.dependOn(&b.addInstallArtifact(artifact, .{}).step);
    }

    const sfx_step = b.step("sfx", "Build the SFX dynamic library");
    sfx_step.dependOn(&b.addInstallArtifact(sfx, .{}).step);

    const gfx_step = b.step("gfx", "Build the GFX dynamic library");
    gfx_step.dependOn(&b.addInstallArtifact(gfx, .{}).step);

    if (gfx_legacy) |artifact| {
        const gfx_legacy_step = b.step("gfx-legacy", "Build the legacy DirectX GFX dynamic library");
        gfx_legacy_step.dependOn(&b.addInstallArtifact(artifact, .{}).step);
    }

    const gfx_gpu_step = b.step("gfx-sdl-gpu", "Build the SDL GPU GFX adapter dynamic library");
    gfx_gpu_step.dependOn(&b.addInstallArtifact(gfx_gpu, .{}).step);

    const gfx_gpu_factory_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    gfx_gpu_factory_test_module.addCSourceFiles(.{
        .files = &.{ "tools/zig/gfxgpu_factory_test.cpp", "Sources/src/GFXGPU/GraphicsEngineGpu.cpp", "Sources/src/GFXGPU/TextureGpu.cpp", "Sources/src/GFXGPU/GeometryBufferGpu.cpp", "Sources/src/GFXGPU/MeshGpu.cpp" },
        // These are the real engine sources, so they need the portable CRT
        // shim that every other non-Windows build force-includes: bare
        // -std=c++17 leaves LARGE_INTEGER and QueryPerformanceCounter
        // undefined and the test cannot compile off Windows at all.
        .flags = cppflagsForOptimize(optimize),
    });
    addProjectIncludePaths(b, gfx_gpu_factory_test_module);
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFX"));
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    addMsvcIncludePaths(b, gfx_gpu_factory_test_module, toolchain);
    addLinuxCxxIncludePaths(b, gfx_gpu_factory_test_module);
    addMsvcLibraryPaths(b, gfx_gpu_factory_test_module, toolchain);
    addMacosSysrootPaths(b, gfx_gpu_factory_test_module, target);
    linkMsvcRuntime(gfx_gpu_factory_test_module, optimize);
    gfx_gpu_factory_test_module.linkLibrary(gfx_gpu_zig);
    // gfx_gpu_zig already brings the dynamic SDL3 in, the same copy the GFXGPU
    // module this harness dlopens was linked against. Linking the static SDL3
    // on top of it put two SDL runtimes into one process: the ObjC runtime
    // flagged the duplicate Metal classes on macOS, and on Linux and Windows
    // the run died in SDL before the harness printed anything. Take the same
    // SDL the module takes (addGFXGPU), so there is exactly one.
    if (target.result.os.tag == .macos) {
        gfx_gpu_factory_test_module.addIncludePath(sdl_dynamic_dep.path("include"));
    } else {
        linkSdlRuntime(gfx_gpu_factory_test_module, target, sdl_dynamic, sdl_dynamic_dep.path("include"));
    }
    gfx_gpu_factory_test_module.linkLibrary(formats);
    if (target.result.os.tag == .windows) {
        gfx_gpu_factory_test_module.linkSystemLibrary("user32", .{});
        // CommandLineToArgvW, see the entry-point note below.
        gfx_gpu_factory_test_module.linkSystemLibrary("shell32", .{});
    }
    const gfx_gpu_factory_test = b.addExecutable(.{
        .name = "gfxgpu-factory-test",
        .root_module = gfx_gpu_factory_test_module,
    });
    gfx_gpu_factory_test.subsystem = .console;
    // Mach-O and ELF entry points are not literally called "main"; forcing the
    // symbol left the test unlinkable anywhere but Windows. On Windows the
    // entry stays at main itself: the CRT's mainCRTStartup cannot be linked
    // here - the Zig GPU library brings Zig's libc objects and the engine
    // sources the MSVC runtime, and the startup object makes the two collide
    // on _wctype and friends. Entering at main skips the CRT's argv setup, so
    // the harness reads its command line through Win32 instead
    // (gfxgpu_factory_test.cpp, ModulePathFromCommandLine).
    if (target.result.os.tag == .windows) gfx_gpu_factory_test.entry = .{ .symbol_name = "main" };
    const gfx_gpu_factory_test_run = b.addRunArtifact(gfx_gpu_factory_test);
    gfx_gpu_factory_test_run.step.dependOn(&b.addInstallArtifact(gfx_gpu, .{}).step);
    // The module's own dependencies: on Windows a DLL's imports resolve from
    // the executable's directory and PATH, not from the DLL's, so the runtime
    // DLLs the module needs are installed and the install directory put on the
    // PATH (the pattern of the other module tests). ELF and Mach-O carry
    // loader-relative rpaths and need neither.
    gfx_gpu_factory_test_run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    gfx_gpu_factory_test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    gfx_gpu_factory_test_run.addPathDir(b.path("zig-out/bin").getPath(b));
    gfx_gpu_factory_test_run.setCwd(b.path("."));
    // The built module is GFXGPU.dll, libGFXGPU.dylib or libGFXGPU.so depending
    // on the host, so hand the test the artifact's own path rather than the
    // Windows file name.
    gfx_gpu_factory_test_run.addFileArg(gfx_gpu.getEmittedBin());
    const gfx_gpu_factory_test_step = b.step("gfxgpu-factory-test", "Load the SDL GPU GFX DLL and create its IGFX object");
    // Honour -Dtest-mode=compile like every other test step: this one ran the
    // artifact unconditionally, so it could not be built without executing it.
    gfx_gpu_factory_test_step.dependOn(&gfx_gpu_factory_test.step);
    // Installed as well, so a CI job that saw it crash can rerun the same
    // binary under a debugger from the repository root.
    gfx_gpu_factory_test_step.dependOn(&b.addInstallArtifact(gfx_gpu_factory_test, .{}).step);
    if (test_mode == .run) gfx_gpu_factory_test_step.dependOn(&gfx_gpu_factory_test_run.step);

    const randommapgen_step = b.step("randommapgen", "Build the RandomMapGen static library");
    randommapgen_step.dependOn(&b.addInstallArtifact(randommapgen, .{}).step);

    const main_step = b.step("main", "Build the Main static library");
    main_step.dependOn(&b.addInstallArtifact(main, .{}).step);

    const streamio_step = b.step("streamio", "Build the Zig StreamIO dynamic library");
    streamio_step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);

    const scene_step = b.step("scene", "Build the Scene x64 dynamic library");
    scene_step.dependOn(&b.addInstallArtifact(scene, .{}).step);

    const ailogic_step = b.step("ailogic", "Build the AILogic x64 dynamic library");
    ailogic_step.dependOn(&b.addInstallArtifact(ailogic, .{}).step);

    const gamett_step = b.step("gamett", "Build the GameTT x64 dynamic library");
    gamett_step.dependOn(&b.addInstallArtifact(gamett, .{}).step);

    const game_step = b.step("game", "Build the Game executable");
    game_step.dependOn(&b.addInstallArtifact(game, .{}).step);

    const gfx_gpu_zig_step = b.step("GfxGpuZig", "Build the Zig GPU renderer static library");
    gfx_gpu_zig_step.dependOn(&b.addInstallArtifact(gfx_gpu_zig, .{}).step);

    const game_all_step = b.step("game-all", "Build and install the playable game runtime set");
    game_all_step.dependOn(runtime_platform_audit_step);
    game_all_step.dependOn(present_fit_step);
    game_all_step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(game, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(options_bridge, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(scene, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(ailogic, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(gamett, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(anim, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(gfx_gpu_zig, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(gfx, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(image, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(input, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(net, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(sfx, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(ui, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(cloudsync, .{}).step);

    // Only the archive matching -Dtarget is ever asked for, and it is lazy, so
    // the ~31 MB download is not part of the eager dependency set.
    const rclone_dependency = b.lazyDependency(rclone_bundle.dependency, .{}) orelse return;
    const rclone_archive_member = rclone_dependency.path(rclone_bundle.archive_member);
    // Zig's zip extraction does not carry a member's unix mode, so the archive
    // copy of rclone arrives as 0644 and every copy after it inherits that:
    // Step.installFile and stage.zig's copyFile both preserve the source's
    // permissions. A staged rclone without the executable bit is found by
    // discovery and then rejected as .not_executable, which is a confusing way
    // to fail, so the bit goes on once here — before zig-out/bin — and staging
    // and packaging carry it from there. Windows has no such bit and no
    // `install`, so there it is a plain file copy.
    const install_rclone = install_rclone: {
        if (b.graph.host.result.os.tag == .windows)
            break :install_rclone b.addInstallBinFile(rclone_archive_member, rclone_bundle.installed_name);
        const mark_executable = b.addSystemCommand(&.{ "install", "-m", "0755" });
        mark_executable.addFileArg(rclone_archive_member);
        const executable_copy = mark_executable.addOutputFileArg(rclone_bundle.installed_name);
        break :install_rclone b.addInstallBinFile(executable_copy, rclone_bundle.installed_name);
    };
    game_all_step.dependOn(&install_rclone.step);

    const stage_tool = b.addExecutable(.{
        .name = "stage-game",
        .root_module = stage_module,
    });

    const stage_game_inputs: StageGameInputs = .{
        .tool = stage_tool,
        .game_all_step = game_all_step,
        .shaders_step = if (use_prebuilt_shaders) null else gfx_gpu_shaders_step,
        .shader_sources = shader_sources,
        .game_name = stage_game_name,
        .runtime_files = stage_runtime_files,
        .debug_files = stage_debug_files,
        .metadata_files = stage_metadata_files,
        .editors_supported = target.result.os.tag == .windows,
    };
    const install_game_cmd = addStageGameRun(b, stage_game_inputs, stage_root);
    if (!copy_data) install_game_cmd.addArg("--link-data");
    install_game_cmd.addArg("--season-data");
    install_game_cmd.addDirectoryArg(season_data);

    const install_game_step = b.step("install-game", "Create runnable game install layout with binaries and Data");
    install_game_step.dependOn(&install_game_cmd.step);

    // 03-08 Task 1's fixture mod (tools/zig/fixtures/editor_mod/EditorTestMod),
    // for the engine-tier test's TestModsListSetAndClear and the host-check's
    // -mod=EditorTestMod run below - never a dependency of install-map-editor
    // or any package step (the fixture is not AchtungPanzer2 and is tracked,
    // but it is still not part of what ships).
    const install_fixture_mod = b.addInstallDirectory(.{
        .source_dir = b.path("tools/zig/fixtures/editor_mod/EditorTestMod"),
        .install_dir = .{ .custom = stage_root["zig-out/".len..] },
        .install_subdir = "mods/EditorTestMod",
    });

    // After install-game, whose step it depends on: the tier's executable is
    // staged into the layout that step creates.
    addEditorBridgeTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode, &install_fixture_mod.step);
    // S04 T01: the resource bridge's smoke tier (test-resource-bridge). Reuses
    // the same engine static libraries addEditorBridgeTest links, and is staged
    // into the same install directory so NPlatform::Paths derives the right
    // roots. The step compiles unconditionally and runs only in test_mode==.run.
    addResourceBridge(b, target, optimize, toolchain, addResourceComparatorLib(b, target, optimize, toolchain, sdl_dynamic_dep.path("include")), editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode);
    // S03 T06: the D-11 comparator, hosted the same way; test-resource-model aggregates it.
    addResourceModelComparatorTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode);
    addResourceModelAggregateStep(b);
    addResourcesAllAggregateStep(b);
    // The editor's platforms: macOS on Apple Silicon and Intel, Linux x64 and
    // Windows x64 (MSVC); everywhere else there is no MapEditor.
    const map_editor_platform = (target.result.os.tag == .macos and (target.result.cpu.arch == .aarch64 or target.result.cpu.arch == .x86_64)) or
        (target.result.os.tag == .linux and target.result.cpu.arch == .x86_64) or
        (target.result.os.tag == .windows and target.result.cpu.arch == .x86_64 and target.result.abi == .msvc);
    // Captured (rather than discarded, as before) so the package steps below
    // can stage this exact build of MapEditor beside Game (D-08); null on
    // every other platform, where there is no MapEditor to package.
    const map_editor: ?MapEditorBuild = if (map_editor_platform) addMapEditor(b, target, optimize, toolchain, editor_imgui_module, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode, &install_fixture_mod.step) else null;
    const map_editor_exe: ?*std.Build.Step.Compile = if (map_editor) |built| built.exe else null;
    // M001 S05: ResourceEditor, on exactly MapEditor's platforms and staged
    // beside it; the package steps below stage this exact binary too.
    const resource_editor_exe: ?*std.Build.Step.Compile = if (map_editor_platform) addResourceEditor(b, target, optimize, toolchain, editor_imgui_module, addResourceComparatorLib(b, target, optimize, toolchain, sdl_dynamic_dep.path("include")), editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode) else null;
    addRandomMissionsTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode, random_missions_sweep);
    addRmgDeterminismTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode);
    addComposerRoundtripTest(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode);
    addPreviewSceneSpike(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main, lualib, zlib, platform_runtime, sdl_dynamic, sdl_dynamic_dep.path("include"), stage_root, install_game_step, test_mode);

    // Backwards-compatible alias for the older command used in project scripts.
    const game_install_step = b.step("game-install", "Create runnable game install layout with binaries and Data");
    game_install_step.dependOn(install_game_step);

    // Drives the shipped console bridge through the engine's own IConsoleBuffer
    // signature. It carries wchar_t, the Zig core stores UTF-16, and the vtable
    // slots match either way, so only an actual round trip catches a width
    // mismatch at that boundary.
    const console_bridge_test_module = b.createModule(.{ .target = target, .optimize = optimize });
    if (platform != .windows_x64) {
        console_bridge_test_module.link_libc = true;
        console_bridge_test_module.link_libcpp = true;
    }
    console_bridge_test_module.addCSourceFile(.{
        .file = b.path("tools/zig/console_bridge_test.cpp"),
        .flags = cppflagsForOptimize(optimize),
    });
    addProjectIncludePaths(b, console_bridge_test_module);
    addMsvcIncludePaths(b, console_bridge_test_module, toolchain);
    addMsvcLibraryPaths(b, console_bridge_test_module, toolchain);
    linkMsvcRuntime(console_bridge_test_module, optimize);
    const console_bridge_test = b.addExecutable(.{
        .name = "console-bridge-test",
        .root_module = console_bridge_test_module,
    });
    console_bridge_test.subsystem = .console;
    if (platform == .windows_x64) console_bridge_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const console_bridge_run = b.addRunArtifact(console_bridge_test);
    // The bridge dlopens its Zig core next to itself, so point at the staged
    // copy rather than the cache artefact, which sits alone.
    console_bridge_run.setCwd(b.path(stage_root));
    console_bridge_run.addArg(switch (target.result.os.tag) {
        .windows => "StreamIOOptionsAbi.dll",
        .macos => "./libStreamIOOptionsAbi.dylib",
        else => "./libStreamIOOptionsAbi.so",
    });
    // The bridge falls back to a bare dlopen of its core, so the loader needs
    // the staged directory on its search path.
    console_bridge_run.setEnvironmentVariable(switch (target.result.os.tag) {
        .macos => "DYLD_LIBRARY_PATH",
        .windows => "PATH",
        else => "LD_LIBRARY_PATH",
    }, b.pathFromRoot(stage_root));
    console_bridge_run.step.dependOn(install_game_step);
    // The terrain is built at integer screen coordinates and then scaled by
    // width/1024 against height/768, which is not a whole number on most
    // windows. Scaled vertices have to land on whole pixels or a point sampled
    // tile edge reads its neighbour out of the tileset.
    const scene_scale_module = b.createModule(.{ .target = target, .optimize = .Debug });
    if (platform != .windows_x64) {
        scene_scale_module.link_libc = true;
        scene_scale_module.link_libcpp = true;
    }
    scene_scale_module.addCSourceFile(.{
        .file = b.path("tools/zig/scene_screen_scale_test.cpp"),
        .flags = if (platform == .windows_x64) cppflagsForOptimize(.Debug) else &.{"-std=c++17"},
    });
    scene_scale_module.addIncludePath(b.path("Sources/src"));
    addMsvcIncludePaths(b, scene_scale_module, toolchain);
    addMsvcLibraryPaths(b, scene_scale_module, toolchain);
    linkMsvcRuntime(scene_scale_module, .Debug);
    const scene_scale_test = b.addExecutable(.{ .name = "scene-screen-scale-test", .root_module = scene_scale_module });
    scene_scale_test.subsystem = .console;
    if (platform == .windows_x64) scene_scale_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const scene_scale_run = b.addRunArtifact(scene_scale_test);
    // The noise texture is addressed in world tile units; neighbouring map
    // tiles must get adjacent coordinates or the pattern jumps at the seam.
    const noise_seam_module = b.createModule(.{ .target = target, .optimize = .Debug });
    if (platform != .windows_x64) {
        noise_seam_module.link_libc = true;
        noise_seam_module.link_libcpp = true;
    }
    noise_seam_module.addCSourceFile(.{
        .file = b.path("tools/zig/terrain_noise_seam_test.cpp"),
        .flags = if (platform == .windows_x64) cppflagsForOptimize(.Debug) else &.{"-std=c++17"},
    });
    addMsvcIncludePaths(b, noise_seam_module, toolchain);
    addMsvcLibraryPaths(b, noise_seam_module, toolchain);
    linkMsvcRuntime(noise_seam_module, .Debug);
    const noise_seam_test = b.addExecutable(.{ .name = "terrain-noise-seam-test", .root_module = noise_seam_module });
    noise_seam_test.subsystem = .console;
    if (platform == .windows_x64) noise_seam_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const noise_seam_run = b.addRunArtifact(noise_seam_test);
    // A layout's text shadow is a second draw of the same glyph. It reads as a
    // shadow only under light text; under dark text it doubles the letters.
    const text_shadow_module = b.createModule(.{ .target = target, .optimize = .Debug });
    if (platform != .windows_x64) {
        text_shadow_module.link_libc = true;
        text_shadow_module.link_libcpp = true;
    }
    text_shadow_module.addCSourceFile(.{
        .file = b.path("tools/zig/ui_text_shadow_test.cpp"),
        .flags = if (platform == .windows_x64) cppflagsForOptimize(.Debug) else &.{"-std=c++17"},
    });
    addMsvcIncludePaths(b, text_shadow_module, toolchain);
    addMsvcLibraryPaths(b, text_shadow_module, toolchain);
    linkMsvcRuntime(text_shadow_module, .Debug);
    const text_shadow_test = b.addExecutable(.{ .name = "ui-text-shadow-test", .root_module = text_shadow_module });
    text_shadow_test.subsystem = .console;
    if (platform == .windows_x64) text_shadow_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const text_shadow_run = b.addRunArtifact(text_shadow_test);
    const text_shadow_step = b.step("test-ui-text-shadow", "Check a text shadow is drawn only where it reads as a shadow");
    text_shadow_step.dependOn(&text_shadow_test.step);
    if (test_mode == .run) text_shadow_step.dependOn(&text_shadow_run.step);

    const noise_seam_step = b.step("test-terrain-noise", "Check terrain noise coordinates are continuous across patches");
    noise_seam_step.dependOn(&noise_seam_test.step);
    if (test_mode == .run) noise_seam_step.dependOn(&noise_seam_run.step);

    const scene_scale_step = b.step("test-scene-scale", "Check scaled terrain vertices land on whole pixels");
    scene_scale_step.dependOn(&scene_scale_test.step);
    if (test_mode == .run) scene_scale_step.dependOn(&scene_scale_run.step);

    const console_bridge_test_step = b.step("test-console-bridge", "Round-trip a wide string through the console bridge");
    console_bridge_test_step.dependOn(&console_bridge_test.step);
    if (test_mode == .run) console_bridge_test_step.dependOn(&console_bridge_run.step);

    const run_game_cmd = b.addSystemCommand(&.{stage_game_name});
    run_game_cmd.setCwd(b.path(stage_root));
    run_game_cmd.step.dependOn(install_game_step);
    if (b.args) |args| {
        run_game_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Build, install, and run Game.exe from install layout");
    run_step.dependOn(install_game_step);
    run_step.dependOn(&run_game_cmd.step);

    const verify_x64_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/verify_x64_runtime.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const verify_x64_tool = b.addExecutable(.{
        .name = "verify-x64-runtime",
        .root_module = verify_x64_module,
    });
    const verify_x64_cmd = b.addRunArtifact(verify_x64_tool);
    verify_x64_cmd.addArg(stage_root);
    verify_x64_cmd.step.dependOn(install_game_step);
    const verify_x64_step = b.step("verify-x64-runtime", "Validate the staged Windows x64 runtime");
    verify_x64_step.dependOn(&verify_x64_cmd.step);

    const endurance_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/verify_gfxgpu_endurance.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const endurance_tool = b.addExecutable(.{
        .name = "verify-gfxgpu-endurance",
        .root_module = endurance_module,
    });
    const endurance_cmd = b.addRunArtifact(endurance_tool);
    endurance_cmd.addArg(stage_root);
    endurance_cmd.step.dependOn(install_game_step);
    const endurance_step = b.step("verify-gfxgpu-endurance", "Run SDL GPU resize, restart, and endurance validation");
    endurance_step.dependOn(&endurance_cmd.step);

    // The tree the zip is built from. It was `<stage_root>/game`, which on a
    // case-insensitive filesystem is the staged `Game` executable sitting in
    // that same directory, so every macOS package run died in stage-game with
    // `NotDir` before it copied a byte. `package` collides with nothing the
    // layout stages, and the name says what the directory is for.
    const package_stage_root = b.fmt("{s}/package", .{stage_root});
    const stage_package_game_cmd = addStageGameRun(b, stage_game_inputs, package_stage_root);
    stage_package_game_cmd.addArg("--season-data");
    stage_package_game_cmd.addDirectoryArg(season_data);
    // D-08: the game package carries MapEditor beside Game on the editor's two
    // platforms. addFileArg (not a hand-built path string) both resolves the
    // exact binary this build produced and makes this Run step depend on it,
    // so `zig build package-game` always packages a freshly built MapEditor.
    if (map_editor_exe) |editor_exe| {
        stage_package_game_cmd.addArg("--map-editor");
        stage_package_game_cmd.addFileArg(editor_exe.getEmittedBin());
    }
    if (resource_editor_exe) |editor_exe| {
        stage_package_game_cmd.addArg("--resource-editor");
        stage_package_game_cmd.addFileArg(editor_exe.getEmittedBin());
    }

    const package_tool = b.addExecutable(.{
        .name = "package",
        .root_module = package_module,
    });
    const package_tool_run = b.addRunArtifact(package_tool);
    package_tool_run.addArg(package_stage_root);
    package_tool_run.addArg(b.fmt("{s}/Blitzkrieg-game.zip", .{package_root}));
    package_tool_run.step.dependOn(&stage_package_game_cmd.step);

    const package_game_step = b.step("package-game", "Create game-only installation zip package");
    package_game_step.dependOn(game_all_step);
    if (!use_prebuilt_shaders) package_game_step.dependOn(gfx_gpu_shaders_step);
    package_game_step.dependOn(&stage_package_game_cmd.step);
    package_game_step.dependOn(&package_tool_run.step);

    const stage_package_game_editors_cmd = addStageGameRun(b, stage_game_inputs, package_stage_root);
    stage_package_game_editors_cmd.addArg("--include-editors");
    stage_package_game_editors_cmd.addArg("--editors-only");
    // Same reasoning as package-game above: this run reuses package_stage_root
    // (already staged by stage_package_game_cmd, MapEditor included), but the
    // flag and its file dependency are added here too so this step's own graph
    // also depends on the exact MapEditor build, not only on the earlier step
    // having run at some point.
    if (map_editor_exe) |editor_exe| {
        stage_package_game_editors_cmd.addArg("--map-editor");
        stage_package_game_editors_cmd.addFileArg(editor_exe.getEmittedBin());
    }
    if (resource_editor_exe) |editor_exe| {
        stage_package_game_editors_cmd.addArg("--resource-editor");
        stage_package_game_editors_cmd.addFileArg(editor_exe.getEmittedBin());
    }
    stage_package_game_editors_cmd.step.dependOn(&package_tool_run.step);

    const package_tool_editors = b.addRunArtifact(package_tool);
    package_tool_editors.step.dependOn(&stage_package_game_editors_cmd.step);
    package_tool_editors.addArg(package_stage_root);
    package_tool_editors.addArg(b.fmt("{s}/Blitzkrieg-game-with-editors.zip", .{package_root}));

    const package_game_editors_step = b.step("package-game-editors", "Create installation zip package with editor tools");
    package_game_editors_step.dependOn(game_all_step);
    package_game_editors_step.dependOn(&package_tool_editors.step);

    const package_step = b.step("package", "Create both game-only and with-editors installation zip packages");
    package_step.dependOn(runtime_platform_audit_step);
    package_step.dependOn(package_game_step);
    package_step.dependOn(package_game_editors_step);

    const abi_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    abi_test_module.addIncludePath(b.path("Sources/src/Blitz64"));
    abi_test_module.addCSourceFiles(.{
        .files = &.{"tools/zig/blitz64_abi_test.cpp"},
        .flags = &.{"-std=c++17"},
    });
    addMsvcIncludePaths(b, abi_test_module, toolchain);
    addMsvcLibraryPaths(b, abi_test_module, toolchain);
    abi_test_module.linkLibrary(blitz64);
    linkMsvcRuntime(abi_test_module, optimize);
    const abi_test = b.addExecutable(.{
        .name = "blitz64-abi-test",
        .root_module = abi_test_module,
    });
    if (target.result.os.tag == .windows) {
        abi_test.subsystem = .console;
        abi_test.entry = .{ .symbol_name = "main" };
    }
    const run_abi_test = b.addRunArtifact(abi_test);
    const abi_test_step = b.step("blitz64-abi-test", "Run the Blitz64 C++ ABI smoke test");
    abi_test_step.dependOn(&run_abi_test.step);

    const blitz64_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/Blitz64/blitz64.zig"),
        .target = target,
        .optimize = optimize,
    });
    const blitz64_unit_tests = b.addTest(.{
        .root_module = blitz64_test_module,
    });
    const run_blitz64_unit_tests = b.addRunArtifact(blitz64_unit_tests);
    const streamio_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/StreamIOZig/streamio.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const streamio_unit_tests = b.addTest(.{ .root_module = streamio_test_module });
    const run_streamio_unit_tests = b.addRunArtifact(streamio_unit_tests);
    const test_streamio_step = b.step("test-streamio", "Run Zig StreamIO unit tests");
    test_streamio_step.dependOn(&streamio_unit_tests.step);
    if (test_mode == .run) test_streamio_step.dependOn(&run_streamio_unit_tests.step);
    const cloudsync_rc_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/rc_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_rc_unit_tests = b.addTest(.{ .root_module = cloudsync_rc_test_module });
    const run_cloudsync_rc_unit_tests = b.addRunArtifact(cloudsync_rc_unit_tests);
    const test_cloudsync_rc_step = b.step("test-cloudsync-rc", "Run Zig CloudSync rc client unit tests");
    test_cloudsync_rc_step.dependOn(&cloudsync_rc_unit_tests.step);
    if (test_mode == .run) test_cloudsync_rc_step.dependOn(&run_cloudsync_rc_unit_tests.step);
    const cloudsync_daemon_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/daemon_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_daemon_unit_tests = b.addTest(.{ .root_module = cloudsync_daemon_test_module });
    const run_cloudsync_daemon_unit_tests = b.addRunArtifact(cloudsync_daemon_unit_tests);
    const test_cloudsync_daemon_step = b.step("test-cloudsync-daemon", "Run Zig CloudSync rclone discovery unit tests");
    test_cloudsync_daemon_step.dependOn(&cloudsync_daemon_unit_tests.step);
    if (test_mode == .run) test_cloudsync_daemon_step.dependOn(&run_cloudsync_daemon_unit_tests.step);
    const cloudsync_plan_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/plan_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_plan_unit_tests = b.addTest(.{ .root_module = cloudsync_plan_test_module });
    const run_cloudsync_plan_unit_tests = b.addRunArtifact(cloudsync_plan_unit_tests);
    const test_cloudsync_plan_step = b.step("test-cloudsync-plan", "Run Zig CloudSync sync planning unit tests");
    test_cloudsync_plan_step.dependOn(&cloudsync_plan_unit_tests.step);
    if (test_mode == .run) test_cloudsync_plan_step.dependOn(&run_cloudsync_plan_unit_tests.step);
    const cloudsync_engine_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/engine_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_engine_unit_tests = b.addTest(.{ .root_module = cloudsync_engine_test_module });
    const run_cloudsync_engine_unit_tests = b.addRunArtifact(cloudsync_engine_unit_tests);
    const test_cloudsync_engine_step = b.step("test-cloudsync-engine", "Run Zig CloudSync sync engine tests");
    test_cloudsync_engine_step.dependOn(&cloudsync_engine_unit_tests.step);
    if (test_mode == .run) test_cloudsync_engine_step.dependOn(&run_cloudsync_engine_unit_tests.step);
    const cloudsync_creds_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/creds_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_creds_unit_tests = b.addTest(.{ .root_module = cloudsync_creds_test_module });
    const run_cloudsync_creds_unit_tests = b.addRunArtifact(cloudsync_creds_unit_tests);
    const test_cloudsync_creds_step = b.step("test-cloudsync-creds", "Run Zig CloudSync credentials tests");
    test_cloudsync_creds_step.dependOn(&cloudsync_creds_unit_tests.step);
    if (test_mode == .run) test_cloudsync_creds_step.dependOn(&run_cloudsync_creds_unit_tests.step);
    const cloudsync_backend_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/backend_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_backend_unit_tests = b.addTest(.{ .root_module = cloudsync_backend_test_module });
    const run_cloudsync_backend_unit_tests = b.addRunArtifact(cloudsync_backend_unit_tests);
    const test_cloudsync_backend_step = b.step("test-cloudsync-backend", "Run Zig CloudSync backend integration tests");
    test_cloudsync_backend_step.dependOn(&cloudsync_backend_unit_tests.step);
    if (test_mode == .run) test_cloudsync_backend_step.dependOn(&run_cloudsync_backend_unit_tests.step);
    const cloudsync_backup_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/backup_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_backup_unit_tests = b.addTest(.{ .root_module = cloudsync_backup_test_module });
    const run_cloudsync_backup_unit_tests = b.addRunArtifact(cloudsync_backup_unit_tests);
    const test_cloudsync_backup_step = b.step("test-cloudsync-backup", "Run Zig CloudSync config backup tests");
    test_cloudsync_backup_step.dependOn(&cloudsync_backup_unit_tests.step);
    if (test_mode == .run) test_cloudsync_backup_step.dependOn(&run_cloudsync_backup_unit_tests.step);
    // The catalogue tests read a committed snapshot of one rclone version's
    // `config/providers` reply rather than a live daemon, so they stay offline;
    // the fixture reaches `@embedFile` as an anonymous import because it lives
    // outside the module's own directory.
    const cloudsync_catalogue_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/catalogue_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    cloudsync_catalogue_test_module.addAnonymousImport("config_providers_fixture", .{
        .root_source_file = b.path("tools/zig/fixtures/config_providers.json"),
    });
    const cloudsync_catalogue_unit_tests = b.addTest(.{ .root_module = cloudsync_catalogue_test_module });
    const run_cloudsync_catalogue_unit_tests = b.addRunArtifact(cloudsync_catalogue_unit_tests);
    const test_cloudsync_catalogue_step = b.step("test-cloudsync-catalogue", "Run Zig CloudSync provider catalogue tests");
    test_cloudsync_catalogue_step.dependOn(&cloudsync_catalogue_unit_tests.step);
    if (test_mode == .run) test_cloudsync_catalogue_step.dependOn(&run_cloudsync_catalogue_unit_tests.step);
    // The form model derives widgets from the same committed snapshot the
    // catalogue tests read, so its fixture arrives the same way.
    const cloudsync_form_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/form_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    cloudsync_form_test_module.addAnonymousImport("config_providers_fixture", .{
        .root_source_file = b.path("tools/zig/fixtures/config_providers.json"),
    });
    const cloudsync_form_unit_tests = b.addTest(.{ .root_module = cloudsync_form_test_module });
    const run_cloudsync_form_unit_tests = b.addRunArtifact(cloudsync_form_unit_tests);
    const test_cloudsync_form_step = b.step("test-cloudsync-form", "Run Zig CloudSync form model tests");
    test_cloudsync_form_step.dependOn(&cloudsync_form_unit_tests.step);
    if (test_mode == .run) test_cloudsync_form_step.dependOn(&run_cloudsync_form_unit_tests.step);
    const cloudsync_worker_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/worker_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_worker_unit_tests = b.addTest(.{ .root_module = cloudsync_worker_test_module });
    const run_cloudsync_worker_unit_tests = b.addRunArtifact(cloudsync_worker_unit_tests);
    const test_cloudsync_worker_step = b.step("test-cloudsync-worker", "Run Zig CloudSync worker thread tests");
    test_cloudsync_worker_step.dependOn(&cloudsync_worker_unit_tests.step);
    if (test_mode == .run) test_cloudsync_worker_step.dependOn(&run_cloudsync_worker_unit_tests.step);
    const cloudsync_oauth_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/oauth_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_oauth_unit_tests = b.addTest(.{ .root_module = cloudsync_oauth_test_module });
    const run_cloudsync_oauth_unit_tests = b.addRunArtifact(cloudsync_oauth_unit_tests);
    const test_cloudsync_oauth_step = b.step("test-cloudsync-oauth", "Run Zig CloudSync config state machine tests");
    test_cloudsync_oauth_step.dependOn(&cloudsync_oauth_unit_tests.step);
    if (test_mode == .run) test_cloudsync_oauth_step.dependOn(&run_cloudsync_oauth_unit_tests.step);
    // The C ABI is proven from both sides in one step: the zig tests below
    // cover the discovery cache and its threading contract, and the C++ smoke
    // consumer links the real shared library and calls every export, which is
    // the only thing that can catch an export that compiles but is not
    // reachable from C++ (a missing .def entry, above all).
    const cloudsync_abi_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/cloudsync.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const cloudsync_abi_unit_tests = b.addTest(.{ .root_module = cloudsync_abi_test_module });
    const run_cloudsync_abi_unit_tests = b.addRunArtifact(cloudsync_abi_unit_tests);
    const cloudsync_abi_consumer_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    cloudsync_abi_consumer_module.addCSourceFiles(.{
        .files = &.{"tools/zig/cloudsync_abi_test.cpp"},
        // Deliberately C-runtime only: pulling MSVC's STL objects (locale,
        // iostreams, <thread>) into this consumer starts a RuntimeLibrary
        // fight with the mixed link line that the game itself never has to
        // win. The consumer proves the ABI, not the STL.
        .flags = &.{"-std=c++17"},
    });
    addMsvcIncludePaths(b, cloudsync_abi_consumer_module, toolchain);
    addMsvcLibraryPaths(b, cloudsync_abi_consumer_module, toolchain);
    cloudsync_abi_consumer_module.linkLibrary(cloudsync);
    linkMsvcRuntime(cloudsync_abi_consumer_module, optimize);
    applyLoaderPath(target, cloudsync_abi_consumer_module);
    const cloudsync_abi_consumer = b.addExecutable(.{
        .name = "cloudsync-abi-test",
        .root_module = cloudsync_abi_consumer_module,
    });
    if (target.result.os.tag == .windows) {
        cloudsync_abi_consumer.subsystem = .console;
        cloudsync_abi_consumer.entry = .{ .symbol_name = "main" };
    }
    const run_cloudsync_abi_consumer = b.addRunArtifact(cloudsync_abi_consumer);
    const test_cloudsync_abi_step = b.step("test-cloudsync-abi", "Run the CloudSync C ABI tests and C++ smoke consumer");
    test_cloudsync_abi_step.dependOn(&cloudsync_abi_unit_tests.step);
    test_cloudsync_abi_step.dependOn(&cloudsync_abi_consumer.step);
    if (test_mode == .run) {
        test_cloudsync_abi_step.dependOn(&run_cloudsync_abi_unit_tests.step);
        test_cloudsync_abi_step.dependOn(&run_cloudsync_abi_consumer.step);
    }
    // The facade loads the library at runtime, so unlike the ABI consumer it
    // links nothing: the two run modes prove the degraded path (no library
    // anywhere near the working directory) and the live path (cwd holding
    // the freshly built artifact).
    const cloudsync_facade_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    cloudsync_facade_test_module.addCSourceFiles(.{
        .files = &.{ "tools/zig/cloudsync_facade_test.cpp", "Sources/src/Main/CloudSyncFacade.cpp", "Sources/src/Platform/CloudSyncLoader.cpp" },
        .flags = &.{"-std=c++17"},
    });
    addMsvcIncludePaths(b, cloudsync_facade_test_module, toolchain);
    addMsvcLibraryPaths(b, cloudsync_facade_test_module, toolchain);
    linkMsvcRuntime(cloudsync_facade_test_module, optimize);
    applyLoaderPath(target, cloudsync_facade_test_module);
    const cloudsync_facade_test = b.addExecutable(.{
        .name = "cloudsync-facade-test",
        .root_module = cloudsync_facade_test_module,
    });
    if (target.result.os.tag == .windows) {
        cloudsync_facade_test.subsystem = .console;
        cloudsync_facade_test.entry = .{ .symbol_name = "main" };
    }
    // -fentry=main skips the CRT's argv setup, so the mode travels by env.
    const run_facade_absent = b.addRunArtifact(cloudsync_facade_test);
    run_facade_absent.setEnvironmentVariable("BK_FACADE_MODE", "absent");
    const run_facade_present = b.addRunArtifact(cloudsync_facade_test);
    run_facade_present.setEnvironmentVariable("BK_FACADE_MODE", "present");
    run_facade_present.setCwd(cloudsync.getEmittedBin().dirname());
    const test_cloudsync_facade_step = b.step("test-cloudsync-facade", "Run the CloudSync C++ facade tests");
    test_cloudsync_facade_step.dependOn(&cloudsync_facade_test.step);
    if (test_mode == .run) {
        test_cloudsync_facade_step.dependOn(&run_facade_absent.step);
        test_cloudsync_facade_step.dependOn(&run_facade_present.step);
    }
    const streamio_platform_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/streamio_platform_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "streamio", .module = streamio_test_module }},
    });
    const streamio_platform_tests = b.addTest(.{ .root_module = streamio_platform_module });
    const run_streamio_platform_tests = b.addRunArtifact(streamio_platform_tests);
    const streamio_platform_step = b.step("test-platform-files", "Run portable StreamIO host filesystem tests");
    streamio_platform_step.dependOn(&streamio_platform_tests.step);
    if (test_mode == .run) streamio_platform_step.dependOn(&run_streamio_platform_tests.step);

    const file_utils_module = b.createModule(.{ .target = target, .optimize = .Debug });
    file_utils_module.addIncludePath(b.path("Sources/src"));
    file_utils_module.addIncludePath(b.path("Sources/src/Misc"));
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, file_utils_module, toolchain);
        addMsvcLibraryPaths(b, file_utils_module, toolchain);
        linkMsvcRuntime(file_utils_module, .Debug);
    } else {
        // FileUtils.cpp is C++ (std::filesystem), so the host build needs the
        // C++ runtime as well; without it the target never compiled off Windows.
        file_utils_module.link_libc = true;
        file_utils_module.link_libcpp = true;
    }
    file_utils_module.addCSourceFiles(.{
        .files = &.{ "tools/zig/platform_file_utils_test.cpp", "Sources/src/Misc/FileUtils.cpp" },
        .flags = if (platform == .windows_x64) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"},
    });
    const file_utils_test = b.addExecutable(.{ .name = "platform-file-utils-test", .root_module = file_utils_module });
    file_utils_test.subsystem = .console;
    if (platform == .windows_x64) file_utils_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const file_utils_run = b.addRunArtifact(file_utils_test);
    const file_utils_step = b.step("test-file-utils", "Run portable legacy file utility tests");
    file_utils_step.dependOn(&file_utils_test.step);
    if (test_mode == .run) file_utils_step.dependOn(&file_utils_run.step);

    const paths_module = b.createModule(.{ .target = target, .optimize = .Debug });
    paths_module.addIncludePath(b.path("Sources/src"));
    paths_module.addCSourceFiles(.{
        .files = &.{ "tools/zig/platform_paths_test.cpp", "Sources/src/Platform/Paths.cpp" },
        .flags = if (platform == .windows_x64) &(cppflags_debug.* ++ .{ "-std=c++17", "-DBLITZKRIEG_PATHS_TEST" }) else &.{ "-std=c++17", "-DBLITZKRIEG_PATHS_TEST" },
    });
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, paths_module, toolchain);
        addMsvcLibraryPaths(b, paths_module, toolchain);
        linkMsvcRuntime(paths_module, .Debug);
    } else {
        paths_module.link_libc = true;
        // Paths.h includes <string> and Paths.cpp uses <filesystem>, so this
        // needs the C++ standard library. Without it the step never compiled
        // and the writable-root assertions had never once run.
        paths_module.link_libcpp = true;
    }
    const paths_test = b.addExecutable(.{ .name = "platform-paths-test", .root_module = paths_module });
    paths_test.subsystem = .console;
    if (platform == .windows_x64) paths_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const paths_run = b.addRunArtifact(paths_test);
    const paths_step = b.step("test-platform-paths", "Run portable data and writable root tests");
    paths_step.dependOn(&paths_test.step);
    if (test_mode == .run) paths_step.dependOn(&paths_run.step);
    const gfx_gpu_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/GFXGPU/root.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "sdl3", .module = sdl3 }},
    });
    const gfx_gpu_unit_tests = b.addTest(.{ .root_module = gfx_gpu_test_module });
    const run_gfx_gpu_unit_tests = b.addRunArtifact(gfx_gpu_unit_tests);
    const gfx_gpu_test_step = b.step("test-gfxgpu-core", "Run the Zig GPU renderer core tests");
    gfx_gpu_test_step.dependOn(&run_gfx_gpu_unit_tests.step);
    const gfx_gpu_compat_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/GFXGPU/compatibility_test.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "gfxgpu", .module = gfx_gpu_test_module }},
    });
    const gfx_gpu_compat_tests = b.addTest(.{ .root_module = gfx_gpu_compat_module });
    const run_gfx_gpu_compat_tests = b.addRunArtifact(gfx_gpu_compat_tests);
    const gfx_gpu_compat_step = b.step("test-gfxgpu-compatibility", "Run the Phase 8 compatibility matrix");
    gfx_gpu_compat_step.dependOn(&run_gfx_gpu_compat_tests.step);
    const test_gfxgpu_step = b.step("test-gfxgpu", "Run the GfxGpu core, C ABI, and SDL smoke tests");
    test_gfxgpu_step.dependOn(gfx_gpu_test_step);
    test_gfxgpu_step.dependOn(gfx_gpu_abi_test_step);
    test_gfxgpu_step.dependOn(gfx_gpu_smoke_step);
    const test_step = b.step("test", "Run Zig unit tests and the Blitz64 ABI smoke test");
    test_step.dependOn(wheel_scroll_step);
    // The editor kit tier: reusable editor plumbing (files, shipped, autosave,
    // history stack primitive, settings primitive, host, crt, imgui wrapper,
    // BK_EDITOR_AUTO schedule driver, pictures-cache, testlaunch) without any
    // engine-bridge dependency. T01 scaffolds the module; later S02 tasks move
    // submodules in. The kit must not import editor_core; editor_core imports
    // the kit so core submodules can be re-pointed at the kit later without a
    // second build.zig edit.
    const editor_kit_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/kit/root.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl3 },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
        },
    });
    // bridge.h: the engine's C ABI; kit/host.zig @cImports it so any editor
    // on the kit (ResourceEditor, future tier editors) shares one wrapper.
    editor_kit_module.addIncludePath(b.path("Sources/src/EditorBridge"));
    const editor_kit_tests = b.addTest(.{ .root_module = editor_kit_module });
    const editor_kit_tests_run = b.addRunArtifact(editor_kit_tests);
    const editor_kit_step = b.step("test-editor-kit", "Run the editor kit tests (reusable editor plumbing shared by the Zig editors)");
    editor_kit_step.dependOn(&editor_kit_tests.step);
    if (test_mode == .run) editor_kit_step.dependOn(&editor_kit_tests_run.step);
    test_step.dependOn(editor_kit_step);
    // The editor core tier: plain Zig against the fake bridge, so it runs on
    // every target, the MinGW job included.
    const editor_core_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/core/root.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        .imports = &.{.{ .name = "editor_kit", .module = editor_kit_module }},
    });
    const editor_core_tests = b.addTest(.{ .root_module = editor_core_module });
    const editor_core_tests_run = b.addRunArtifact(editor_core_tests);
    const editor_core_step = b.step("test-editor-core", "Run the Map Editor core tests against the fake bridge");
    editor_core_step.dependOn(&editor_core_tests.step);
    if (test_mode == .run) editor_core_step.dependOn(&editor_core_tests_run.step);
    test_step.dependOn(editor_core_step);
    // The resource editor core tier (S04 T03): plain Zig against the fake
    // resource bridge, so it builds on every target including
    // x86_64-windows-gnu where the engine C++ does not. Mirrors the map core's
    // addEditorCore wiring so Resource and Map stay symmetric on the Zig side.
    const resource_core_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/resource_core/root.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        .imports = &.{.{ .name = "editor_kit", .module = editor_kit_module }},
    });
    const resource_core_tests = b.addTest(.{ .root_module = resource_core_module });
    const resource_core_tests_run = b.addRunArtifact(resource_core_tests);
    const resource_core_step = b.step("test-resource-core", "Run the Resource Editor core tests against the fake resource bridge");
    resource_core_step.dependOn(&resource_core_tests.step);
    if (test_mode == .run) resource_core_step.dependOn(&resource_core_tests_run.step);
    test_step.dependOn(resource_core_step);
    // The resource app's pure logic tier (S05 T08): panels_logic.zig against
    // the fake resource bridge, built on every target like test-resource-core.
    // c_bridge.zig, the real ResBridge over resource_bridge.h, is compiled
    // here as an object so every C call it makes is type-checked even where
    // the engine C++ does not build; its BkRes* symbols resolve only where
    // ResourceEditor links the engine.
    const resource_app_logic_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/resource_app/panels_logic.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        // editor_kit for lifecycle.zig and settings.zig (S05 T09): autosave,
        // shipped, files and the shared settings keys.
        .imports = &.{
            .{ .name = "resource_core", .module = resource_core_module },
            .{ .name = "editor_kit", .module = editor_kit_module },
        },
    });
    const resource_app_logic_tests = b.addTest(.{ .root_module = resource_app_logic_module });
    const resource_app_logic_tests_run = b.addRunArtifact(resource_app_logic_tests);
    const resource_app_c_bridge_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/resource_app/c_bridge.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        .imports = &.{.{ .name = "resource_core", .module = resource_core_module }},
    });
    resource_app_c_bridge_module.addIncludePath(b.path("Sources/src/EditorBridge"));
    const resource_app_c_bridge_object = b.addObject(.{ .name = "resource-app-c-bridge", .root_module = resource_app_c_bridge_module });
    const resource_app_logic_step = b.step("test-resource-app-logic", "Run the Resource Editor app's pure logic tests against the fake resource bridge and compile its real bridge adapter");
    resource_app_logic_step.dependOn(&resource_app_logic_tests.step);
    resource_app_logic_step.dependOn(&resource_app_c_bridge_object.step);
    if (test_mode == .run) resource_app_logic_step.dependOn(&resource_app_logic_tests_run.step);
    test_step.dependOn(resource_app_logic_step);
    // The map view's pure parts (camera scrolling, the button-to-tool-event
    // mapping): plain Zig, no SDL or engine, so this runs without a GPU or a
    // staged installation.
    const view_math_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/app/view_math.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const view_math_tests = b.addTest(.{ .root_module = view_math_module });
    const view_math_tests_run = b.addRunArtifact(view_math_tests);
    const view_math_step = b.step("test-map-editor-view", "Run the map view's pure camera and input-mapping tests");
    view_math_step.dependOn(&view_math_tests.step);
    if (test_mode == .run) view_math_step.dependOn(&view_math_tests_run.step);
    // view.zig's own event-wiring tests (WINDOWS.md 2) need the app's SDL
    // headers, so they exist only where MapEditor builds.
    if (map_editor) |built| view_math_step.dependOn(built.view_test_step);
    test_step.dependOn(view_math_step);
    // The panels' pure parts (the file dialogs' hand-over and the file
    // actions, the palette's filter, directions, the title): plain Zig
    // against the core's fake bridge, no SDL, ImGui or engine.
    const panels_logic_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/app/panels_logic.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
        .imports = &.{
            .{ .name = "editor_core", .module = editor_core_module },
            .{ .name = "editor_kit", .module = editor_kit_module },
        },
    });
    const panels_logic_tests = b.addTest(.{ .root_module = panels_logic_module });
    const panels_logic_tests_run = b.addRunArtifact(panels_logic_tests);
    const panels_logic_step = b.step("test-map-editor-panels", "Run the panels' pure file-action, palette and properties tests");
    panels_logic_step.dependOn(&panels_logic_tests.step);
    if (test_mode == .run) panels_logic_step.dependOn(&panels_logic_tests_run.step);
    test_step.dependOn(panels_logic_step);
    // Test in game's argv construction and exit classification: plain Zig,
    // no sdl3 and no c_bridge (testlaunch.zig's own doc comment), so this
    // runs on every target with no engine, GPU or staged installation.
    const testlaunch_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/kit/testlaunch.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const testlaunch_tests = b.addTest(.{ .root_module = testlaunch_module });
    const testlaunch_tests_run = b.addRunArtifact(testlaunch_tests);
    const testlaunch_step = b.step("test-map-editor-testlaunch", "Run Test in game's argv-construction and exit-classification tests");
    testlaunch_step.dependOn(&testlaunch_tests.step);
    if (test_mode == .run) testlaunch_step.dependOn(&testlaunch_tests_run.step);
    test_step.dependOn(testlaunch_step);
    // BK_EDITOR_AUTO's schedule parser and TGA comparison: plain Zig, no
    // sdl3 and no c_bridge (auto_schedule.zig's own doc comment), so this
    // runs on every target with no engine, GPU or staged installation.
    const auto_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/kit/auto_schedule.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const auto_tests = b.addTest(.{ .root_module = auto_module });
    const auto_tests_run = b.addRunArtifact(auto_tests);
    const auto_step = b.step("test-map-editor-auto", "Run BK_EDITOR_AUTO's schedule parser and TGA comparison tests");
    auto_step.dependOn(&auto_tests.step);
    if (test_mode == .run) auto_step.dependOn(&auto_tests_run.step);
    test_step.dependOn(auto_step);
    test_step.dependOn(&run_blitz64_unit_tests.step);
    test_step.dependOn(&run_streamio_unit_tests.step);
    test_step.dependOn(&run_abi_test.step);
    if (target.result.cpu.arch == .x86_64) test_step.dependOn(&verify_x64_cmd.step);

    b.default_step = game_all_step;
}

/// What every stage-game run is built from. See addStageGameRun.
const StageGameInputs = struct {
    tool: *std.Build.Step.Compile,
    game_all_step: *std.Build.Step,
    /// null under -Duse-prebuilt-shaders, which reuses zig-out/shaders as is.
    shaders_step: ?*std.Build.Step,
    shader_sources: []const []const u8,
    game_name: []const u8,
    runtime_files: []const []const u8,
    debug_files: []const []const u8,
    metadata_files: []const []const u8,
    editors_supported: bool,
};

/// The one way to create a stage-game run (install-game, package-game,
/// package-game-editors). stage.zig copies the runtime out of zig-out/bin and
/// zig-out/lib and the shader blobs out of zig-out/shaders by plain path, so
/// the build graph cannot see that it reads them: the run has to be ordered
/// after the installs that write them by hand. Only install-game was. The two
/// package runs hung off package-game(-editors) beside game-all, not after
/// it, so in a tree with no zig-out/bin yet they raced the installs and died
/// in copyGameRuntime with FileNotFound - reliably under --release=fast, whose
/// optimised compiles lose that race - and in a tree that had one they
/// zipped whatever binaries an earlier build had left there, a Debug Game in
/// a release package included. stage_test.zig holds every stage-game run in
/// build.zig to this helper.
fn addStageGameRun(b: *std.Build, inputs: StageGameInputs, install_dir: []const u8) *std.Build.Step.Run {
    const run = b.addRunArtifact(inputs.tool);
    run.addArg(".");
    run.addArg(install_dir);
    addStageLayoutArgs(run, inputs.game_name, inputs.runtime_files, inputs.debug_files, inputs.metadata_files, inputs.editors_supported);
    run.step.dependOn(inputs.game_all_step);
    // Staging copies the third-party notice out of a plain path, the way it
    // copies the shader blobs, so an edited licence text has to be part of the
    // cache key or the staged and packaged copies keep the superseded notice.
    run.addFileInput(b.path(package_policy.third_party_notices_source));
    if (inputs.shaders_step) |shaders_step| {
        run.step.dependOn(shaders_step);
        // Staging copies the compiled shader blobs out of a plain path, so the same
        // sources have to be part of its cache key or an edited shader never reaches
        // the install layout. It cannot simply always run: it deletes and re-copies
        // the whole 2.7 GB Data tree.
        for (inputs.shader_sources) |source| run.addFileInput(b.path(source));
    }
    return run;
}

fn addStageLayoutArgs(run: anytype, game_name: []const u8, runtime_files: []const []const u8, debug_files: []const []const u8, metadata_files: []const []const u8, editors_supported: bool) void {
    run.addArg("--game-name");
    run.addArg(game_name);
    for (runtime_files) |name| {
        run.addArg("--runtime-file");
        run.addArg(name);
    }
    for (debug_files) |name| {
        run.addArg("--debug-file");
        run.addArg(name);
    }
    for (metadata_files) |name| {
        run.addArg("--metadata-file");
        run.addArg(name);
    }
    if (editors_supported) run.addArg("--editors-supported");
}

fn addRandomMapGen(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const randommapgen_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, randommapgen_module);
    addMsvcIncludePaths(b, randommapgen_module, toolchain);
    randommapgen_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    randommapgen_module.addIncludePath(b.path("Sources/src/Main"));
    randommapgen_module.addIncludePath(b.path("Sources/src/Common"));
    randommapgen_module.addIncludePath(b.path("Sources/src/Image"));
    // The randommapgen archive is linked into several runtime binaries (AILogic,
    // GameTT, Game, the editors). Its globals - SRMTemplateUnitsTable's string
    // array and lookup maps, the RMGC_* names - have default visibility, so on
    // ELF every copy interposes to the first-loaded instance while each module
    // still registers its own destructor for it: the one array is then destroyed
    // once per module at UnloadAllModules, and the double frees abort glibc at
    // exit ("double free or corruption"). Hiding visibility gives every binary
    // its own private copy - constructed once, destroyed once - which is what
    // the Windows build already does with per-DLL static data.
    const randommapgen_flags = std.mem.concat(
        b.allocator,
        []const u8,
        &.{ cppflagsForOptimize(optimize), &.{ "-fvisibility=hidden", "-fvisibility-inlines-hidden" } },
    ) catch @panic("out of memory");
    randommapgen_module.addCSourceFiles(.{
        .files = randommapgen_sources,
        .flags = randommapgen_flags,
    });

    return b.addLibrary(.{
        .name = "RandomMapGen",
        .linkage = .static,
        .root_module = randommapgen_module,
    });
}

fn addBlitz64(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const blitz64_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/Blitz64/blitz64.zig"),
        .target = target,
        .optimize = optimize,
    });
    return b.addLibrary(.{
        .name = "Blitz64",
        .linkage = .static,
        .root_module = blitz64_module,
    });
}

fn addOptionsBridge(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
    sdl: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    var flags: std.ArrayListUnmanaged([]const u8) = .empty;
    flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
    flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/StreamIOZig/options_bridge.cpp",
            "Sources/src/PlatformABI/PlatformClient.cpp",
            "Sources/src/Platform/DynamicLibrary.cpp",
            "Sources/src/Platform/Paths.cpp",
        },
        .flags = flags.items,
    });
    addProjectIncludePaths(b, module);
    addMsvcIncludePaths(b, module, toolchain);
    addMsvcLibraryPaths(b, module, toolchain);
    linkMsvcRuntime(module, optimize);
    module.linkLibrary(platform_runtime);
    module.linkLibrary(sdl);
    if (target.result.os.tag == .windows) module.linkSystemLibrary("comsuppw", .{});
    applyLoaderPath(target, module);
    addSharedObjectFinalizer(b, target, module);
    return b.addLibrary(.{ .name = "StreamIOOptionsAbi", .linkage = .dynamic, .root_module = module });
}

fn addStreamIOZig(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    options_bridge: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    streamio_fast: bool,
) *std.Build.Step.Compile {
    // The module optimize mode applies to the zig sources only; the C++
    // bridge below is compiled with cppflagsForOptimize(optimize) and the CRT
    // link stays keyed on the game's optimize mode, so a Debug game still
    // gets ucrtbased and a debuggable bridge.
    const zig_optimize = if (streamio_fast and optimize == .Debug) std.builtin.OptimizeMode.ReleaseFast else optimize;
    const streamio_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/StreamIOZig/streamio.zig"),
        .target = target,
        .optimize = zig_optimize,
        .link_libc = true,
    });
    var flags: std.ArrayListUnmanaged([]const u8) = .empty;
    flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
    flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    if (streamio_fast and optimize == .Debug) {
        // Optimize the bridge itself while keeping the _DEBUG/debug-STL
        // defines above (they must match the ucrtbased link); optimization
        // level does not affect that ABI.
        flags.appendSlice(b.allocator, &.{ "-O2", "-fno-sanitize=undefined" }) catch @panic("OOM");
    }
    streamio_module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/StreamIOZig/legacy_bridge.cpp",
            "Sources/src/PlatformABI/PlatformClient.cpp",
            "Sources/src/Platform/Debug.cpp",
        },
        .flags = flags.items,
    });
    addProjectIncludePaths(b, streamio_module);
    addMsvcIncludePaths(b, streamio_module, toolchain);
    addMsvcLibraryPaths(b, streamio_module, toolchain);
    streamio_module.linkLibrary(options_bridge);
    streamio_module.linkLibrary(platform_runtime);
    linkMsvcRuntime(streamio_module, optimize);
    // x86 exports carry stdcall decorations (_name@N) that do not exist on
    // x86_64, so the def file is per-arch.
    const def_path = if (target.result.cpu.arch == .x86)
        "Sources/src/StreamIOZig/StreamIO.def"
    else
        "Sources/src/StreamIOZig/StreamIO.x64.def";
    applyLoaderPath(target, streamio_module);
    addSharedObjectFinalizer(b, target, streamio_module);
    return b.addLibrary(.{
        .name = "StreamIO",
        .linkage = .dynamic,
        .root_module = streamio_module,
        .win32_module_definition = b.path(def_path),
    });
}

fn addCloudSync(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    // Pure zig, unlike StreamIO: there is no C++ bridge here, because the
    // whole point of the C ABI below is that C++ never sees a zig type.
    const cloudsync_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/CloudSync/cloudsync.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    addMsvcIncludePaths(b, cloudsync_module, toolchain);
    addMsvcLibraryPaths(b, cloudsync_module, toolchain);
    linkMsvcRuntime(cloudsync_module, optimize);
    // x86 exports carry a leading underscore that does not exist on x86_64,
    // so the def file is per-arch exactly as StreamIO's is.
    const def_path = if (target.result.cpu.arch == .x86)
        "Sources/src/CloudSync/CloudSync.def"
    else
        "Sources/src/CloudSync/CloudSync.x64.def";
    applyLoaderPath(target, cloudsync_module);
    return b.addLibrary(.{
        .name = "CloudSync",
        .linkage = .dynamic,
        .root_module = cloudsync_module,
        .win32_module_definition = b.path(def_path),
    });
}

fn addLegacyProjectDll(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    name: []const u8,
    project: []const u8,
    definition: []const u8,
    includes: []const []const u8,
    libraries: []const *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const contents = std.Io.Dir.cwd().readFileAlloc(b.graph.io, project, b.allocator, .limited(8 * 1024 * 1024)) catch |err| @panic(@errorName(err));
    var files: std.ArrayListUnmanaged([]const u8) = .empty;
    // libtheora/libogg are math-heavy decoders; at -O0 (Debug) clang emits code
    // ~2x slower than MSVC /Od, which breaks realtime video decode (measured
    // 60-90ms/frame vs the 40ms budget). Build just these sources optimized,
    // in a ReleaseFast module that links the SAME CRT as the rest of the game
    // (passing the project `optimize` to linkMsvcRuntime) so there's no
    // debug/release CRT mismatch.
    var xiph_files: std.ArrayListUnmanaged([]const u8) = .empty;
    var audio_files: std.ArrayListUnmanaged([]const u8) = .empty;
    var offset: usize = 0;
    const marker = "<ClCompile Include=\"";
    while (std.mem.indexOfPos(u8, contents, offset, marker)) |start| {
        const path_start = start + marker.len;
        const path_end = std.mem.indexOfPos(u8, contents, path_start, "\"") orelse break;
        const source = contents[path_start..path_end];
        const normalized_source = b.allocator.dupe(u8, source) catch @panic("OOM");
        std.mem.replaceScalar(u8, normalized_source, '\\', '/');
        if (std.mem.endsWith(u8, source, ".cpp") or std.mem.endsWith(u8, source, ".c")) {
            const placed = b.fmt("Sources/src/{s}/{s}", .{ name, normalized_source });
            if (std.mem.indexOf(u8, normalized_source, "xiph") != null) {
                xiph_files.append(b.allocator, placed) catch @panic("OOM");
            } else if (std.mem.indexOf(u8, source, "AudioBackend") != null) {
                // SFX's AudioBackend*.cpp/.c hold the whole miniaudio
                // implementation (mixer, dr_mp3, resampler) plus the vorbis
                // wrapper — the audio thread's realtime hot path. At -O0 +
                // UBSan the mp3 decode is borderline-realtime and the menu
                // music stutters; compile these TUs optimized even in Debug
                // (defines stay debug-ABI, same trick as legacy_bridge).
                audio_files.append(b.allocator, placed) catch @panic("OOM");
            } else {
                files.append(b.allocator, placed) catch @panic("OOM");
            }
        }
        offset = path_end + 1;
    }
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    addMsvcIncludePaths(b, module, toolchain);
    addMsvcLibraryPaths(b, module, toolchain);
    for (includes) |include| module.addIncludePath(b.path(include));
    if (std.mem.eql(u8, name, "AILogic")) {
        var flags: std.ArrayListUnmanaged([]const u8) = .empty;
        flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
        flags.append(b.allocator, "-include") catch @panic("OOM");
        flags.append(b.allocator, "Sources/src/AILogic/StdAfx.h") catch @panic("OOM");
        module.addCSourceFiles(.{ .files = files.items, .flags = flags.items });
    } else module.addCSourceFiles(.{ .files = files.items, .flags = cppflagsForOptimize(optimize) });
    if (audio_files.items.len > 0) {
        var audio_flags: std.ArrayListUnmanaged([]const u8) = .empty;
        audio_flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
        if (optimize == .Debug) {
            audio_flags.appendSlice(b.allocator, &.{ "-O2", "-fno-sanitize=undefined" }) catch @panic("OOM");
        }
        module.addCSourceFiles(.{ .files = audio_files.items, .flags = audio_flags.items });
    }
    if (xiph_files.items.len > 0) {
        // Static lib of just the decoder objects; it does NOT link a CRT itself
        // (no linkMsvcRuntime) — its CRT symbols resolve when Scene.dll links
        // against the game's CRT (ucrtbased in Debug), avoiding any mismatch.
        const xiph_module = b.createModule(.{ .target = target, .optimize = .ReleaseFast });
        addProjectIncludePaths(b, xiph_module);
        addMsvcIncludePaths(b, xiph_module, toolchain);
        for (includes) |include| xiph_module.addIncludePath(b.path(include));
        xiph_module.addCSourceFiles(.{ .files = xiph_files.items, .flags = cflagsForOptimize(.ReleaseFast) });
        const xiph_lib = b.addLibrary(.{ .name = b.fmt("{s}_xiph", .{name}), .linkage = .static, .root_module = xiph_module });
        module.linkLibrary(xiph_lib);
    }
    for (libraries) |library| module.linkLibrary(library);
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);
    linkMsvcRuntime(module, optimize);
    if (target.result.os.tag == .windows) {
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        linkComSupport(module, optimize);
    }
    applyLoaderPath(target, module);
    addSharedObjectFinalizer(b, target, module);
    const library = b.addLibrary(.{
        .name = name,
        .linkage = .dynamic,
        .root_module = module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path(definition) else null,
    });
    // These engine modules are loaded by Game, which owns the Main objects they
    // call back into (RPGStats typeinfo, for example). ELF permits those
    // undefined symbols in a shared object by default, which is what the Linux
    // build relies on; Mach-O rejects them unless asked to defer resolution to
    // load time, so opt macOS into the same contract.
    if (target.result.os.tag == .macos) library.linker_allow_shlib_undefined = true;
    return library;
}

fn addMain(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const main_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, main_module);
    addMsvcIncludePaths(b, main_module, toolchain);
    main_module.addIncludePath(b.path("Sources/src/Main"));
    main_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    main_module.addIncludePath(b.path("Sources/src/Common"));
    main_module.addIncludePath(b.path("Sources/src/UI"));
    main_module.addIncludePath(b.path("Sources/src/GFX"));
    main_module.addIncludePath(b.path("Sources/src/SFX"));
    main_module.addIncludePath(b.path("Sources/src/Net"));
    main_module.addIncludePath(b.path("Sources/src/Input"));
    main_module.addIncludePath(b.path("Sources/src/Anim"));
    main_module.addIncludePath(b.path("Sources/src/Image"));
    main_module.addCSourceFiles(.{
        .files = main_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "Main",
        .linkage = .static,
        .root_module = main_module,
    });
}

fn addGame(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    main: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    blitz64: *std.Build.Step.Compile,
    startup_trace: bool,
    renderer: []const u8,
    platform: build_support.PlatformTarget,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const game_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, game_module);
    addLinuxCxxIncludePaths(b, game_module);
    addMsvcIncludePaths(b, game_module, toolchain);
    addMsvcLibraryPaths(b, game_module, toolchain);
    game_module.addIncludePath(b.path("Sources/src/Game"));
    game_module.addIncludePath(sdl_include);
    game_module.addIncludePath(b.path("Sources/src/Main"));
    game_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    if (startup_trace) game_module.addCMacro("BK_STARTUP_TRACE", "1");
    game_module.addCSourceFiles(.{
        .files = if (target.result.os.tag == .windows) &(game_sources.* ++ windows_game_sources.*) else game_sources,
        .flags = cppflagsGameForOptimize(optimize),
    });
    game_module.linkLibrary(main);
    game_module.linkLibrary(misc);
    game_module.linkLibrary(platform_runtime);
    linkSdlImport(game_module, target, sdl_dynamic);
    game_module.linkLibrary(lualib);
    game_module.linkLibrary(zlib);
    game_module.linkLibrary(randommapgen);
    game_module.linkLibrary(formats);
    game_module.linkLibrary(blitz64);
    linkMsvcRuntime(game_module, optimize);
    if (target.result.os.tag == .windows) {
        game_module.linkSystemLibrary("version", .{});
        game_module.linkSystemLibrary("winmm", .{});
        game_module.linkSystemLibrary("odbc32", .{});
        game_module.linkSystemLibrary("odbccp32", .{});
    }
    if (target.result.os.tag == .macos) {
        // SDLApplication::SetAppIcon talks to AppKit through the Objective-C
        // runtime to give the bare executable a Dock icon.
        game_module.linkSystemLibrary("objc", .{});
        addMacosSysrootPaths(b, game_module, target);
    }
    if (target.result.os.tag == .windows and std.mem.eql(u8, renderer, "legacy")) {
        game_module.linkSystemLibrary("d3d9", .{});
    }
    if (target.result.os.tag == .windows) {
        game_module.linkSystemLibrary("shlwapi", .{});
        game_module.linkSystemLibrary("advapi32", .{});
        game_module.linkSystemLibrary("user32", .{});
        game_module.linkSystemLibrary("gdi32", .{});
        game_module.linkSystemLibrary("shell32", .{});
        linkComSupport(game_module, optimize);
    }
    if (target.result.os.tag == .windows) {
    // Splash screen, icon and bitmap resources: WinFrame.cpp creates the
    // IDD_SPLASH_SCREEN dialog from these — without them the loader shows a
    // bare white window. SplashResources.rc is an ASCII-only extract of
    // Game.rc (whose windows-1251 string tables the resource compiler cannot
    // process); winres.h comes from the SDK um directory.
    game_module.addWin32ResourceFile(.{
        .file = b.path("Sources/src/Game/SplashResources.rc"),
        .include_paths = &.{
            b.path("Sources/src/Game"),
            .{ .cwd_relative = b.fmt("{s}\\um", .{toolchain.windows_sdk_include}) },
            .{ .cwd_relative = b.fmt("{s}\\shared", .{toolchain.windows_sdk_include}) },
        },
    });

    // Game version VERSIONINFO resource. Bump game_version in build.zig then
    // update Sources/src/Game/GameVersion.rc FILEVERSION/PRODUCTVERSION to
    // match.
    game_module.addWin32ResourceFile(.{
        .file = b.path("Sources/src/Game/GameVersion.rc"),
        .include_paths = &.{
            b.path("Sources/src/Game"),
            .{ .cwd_relative = b.fmt("{s}\\um", .{toolchain.windows_sdk_include}) },
            .{ .cwd_relative = b.fmt("{s}\\shared", .{toolchain.windows_sdk_include}) },
        },
    });
    }

    applyLoaderPath(target, game_module);
    const game = b.addExecutable(.{
        .name = "Game",
        .root_module = game_module,
    });
    if (target.result.os.tag == .linux or target.result.os.tag == .macos) {
        // Legacy modules resolve RTTI owned by Main from the executable when
        // they are loaded with RTLD_NOW (for example SBuildingRPGStats).
        // macOS needs the same export set: its module dylibs defer those
        // symbols to load time, so they must be visible in the executable.
        game.rdynamic = true;
    }
    game.subsystem = switch (build_support.subsystem(platform, true)) {
        .windows => .windows,
        .console => .console,
    };
    switch (build_support.entryPoint(platform, true)) {
        .win_main_crt_startup => game.entry = .{ .symbol_name = "WinMainCRTStartup" },
        .main_crt_startup => game.entry = .{ .symbol_name = "mainCRTStartup" },
        .main => {},
    }
    return game;
}

fn addZlib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const zlib_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addLinuxCxxIncludePaths(b, zlib_module);
    addMsvcIncludePaths(b, zlib_module, toolchain);
    zlib_module.addIncludePath(b.path("Sources/src/zlib"));
    zlib_module.addCSourceFiles(.{
        .files = zlib_sources,
        .flags = cflagsForOptimize(optimize),
    });

    const zlib = b.addLibrary(.{
        .name = "zlib",
        .linkage = .static,
        .root_module = zlib_module,
    });

    return zlib;
}

fn addLibpng(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const libpng_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addLinuxCxxIncludePaths(b, libpng_module);
    addMsvcIncludePaths(b, libpng_module, toolchain);
    libpng_module.addIncludePath(b.path("Sources/src/libpng"));
    libpng_module.addIncludePath(b.path("Sources/src/zlib"));
    libpng_module.addCSourceFiles(.{
        .files = libpng_sources,
        .flags = cflagsForOptimize(optimize),
    });
    libpng_module.linkLibrary(zlib);

    return b.addLibrary(.{
        .name = "libpng",
        .linkage = .static,
        .root_module = libpng_module,
    });
}

fn addMisc(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const misc_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, misc_module);
    addMsvcIncludePaths(b, misc_module, toolchain);
    misc_module.addIncludePath(b.path("Sources/src/Misc"));
    misc_module.addIncludePath(b.path("Sources/src/zlib"));
    misc_module.addIncludePath(sdl_include);
    var misc_flags: std.ArrayListUnmanaged([]const u8) = .empty;
    misc_flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
    misc_flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    misc_module.addCSourceFiles(.{
        .files = misc_sources,
        .flags = misc_flags.items,
    });
    return b.addLibrary(.{
        .name = "Misc",
        .linkage = .static,
        .root_module = misc_module,
    });
}

fn addImage(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
    libpng: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const image_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, image_module);
    addMsvcIncludePaths(b, image_module, toolchain);
    addMsvcLibraryPaths(b, image_module, toolchain);
    image_module.addIncludePath(b.path("Sources/src/Image"));
    image_module.addIncludePath(b.path("Sources/src/zlib"));
    image_module.addIncludePath(b.path("Sources/src/libpng"));
    image_module.addCSourceFiles(.{
        .files = image_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    image_module.linkLibrary(misc);
    image_module.linkLibrary(platform_runtime);
    linkSdlImport(image_module, target, sdl_dynamic);
    image_module.linkLibrary(libpng);
    image_module.linkLibrary(zlib);
    linkMsvcRuntime(image_module, optimize);
    if (target.result.os.tag == .windows) image_module.linkSystemLibrary("user32", .{});

    applyLoaderPath(target, image_module);
    addSharedObjectFinalizer(b, target, image_module);
    return b.addLibrary(.{
        .name = "Image",
        .linkage = .dynamic,
        .root_module = image_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/Image/Image.def") else null,
    });
}

fn addLuaLib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const lualib_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, lualib_module);
    addMsvcIncludePaths(b, lualib_module, toolchain);
    lualib_module.addIncludePath(b.path("Sources/src/LuaLib"));
    lualib_module.addIncludePath(b.path("Sources/src/LuaLib/LuaSrc"));
    lualib_module.addCSourceFiles(.{
        .files = lualib_c_sources,
        .flags = cflagsForOptimize(optimize),
    });
    lualib_module.addCSourceFiles(.{
        .files = lualib_cpp_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "LuaLib",
        .linkage = .static,
        .root_module = lualib_module,
    });
}

fn addNet(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const net_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, net_module);
    addMsvcIncludePaths(b, net_module, toolchain);
    addMsvcLibraryPaths(b, net_module, toolchain);
    net_module.addIncludePath(b.path("Sources/src/Net"));
    net_module.addIncludePath(b.path("Sources/src/StreamIO"));
    net_module.addCSourceFiles(.{
        .files = net_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    net_module.linkLibrary(misc);
    net_module.linkLibrary(platform_runtime);
    linkSdlImport(net_module, target, sdl_dynamic);
    linkMsvcRuntime(net_module, optimize);
    if (target.result.os.tag == .windows) {
        net_module.linkSystemLibrary("ws2_32", .{});
        net_module.linkSystemLibrary("odbc32", .{});
        net_module.linkSystemLibrary("odbccp32", .{});
    }

    applyLoaderPath(target, net_module);
    addSharedObjectFinalizer(b, target, net_module);
    return b.addLibrary(.{
        .name = "Net",
        .linkage = .dynamic,
        .root_module = net_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/Net/net.def") else null,
    });
}

fn addBuildVersion(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const buildversion_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, buildversion_module);
    addMsvcIncludePaths(b, buildversion_module, toolchain);
    addMsvcLibraryPaths(b, buildversion_module, toolchain);
    buildversion_module.addIncludePath(b.path("Sources/src/buildversion"));
    buildversion_module.addCSourceFiles(.{
        .files = buildversion_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    buildversion_module.linkLibrary(misc);
    buildversion_module.linkLibrary(platform_runtime);
    linkSdlImport(buildversion_module, target, sdl_dynamic);
    linkMsvcRuntime(buildversion_module, optimize);
    if (target.result.os.tag == .windows) {
        buildversion_module.linkSystemLibrary("odbc32", .{});
        buildversion_module.linkSystemLibrary("odbccp32", .{});
    }

    const buildversion = b.addExecutable(.{
        .name = "BuildVersion",
        .root_module = buildversion_module,
    });
    buildversion.subsystem = .console;
    buildversion.entry = .{ .symbol_name = "mainCRTStartup" };
    return buildversion;
}

fn addBetaKeyGen(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const betakeygen_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, betakeygen_module);
    addMsvcIncludePaths(b, betakeygen_module, toolchain);
    addMsvcLibraryPaths(b, betakeygen_module, toolchain);
    betakeygen_module.addIncludePath(b.path("Sources/src/betakeygen"));
    betakeygen_module.addIncludePath(b.path("Sources/src/zlib"));
    betakeygen_module.addCSourceFiles(.{
        .files = betakeygen_sources,
        .flags = cppflagsBetaForOptimize(optimize),
    });
    betakeygen_module.linkLibrary(misc);
    betakeygen_module.linkLibrary(platform_runtime);
    linkSdlImport(betakeygen_module, target, sdl_dynamic);
    betakeygen_module.linkLibrary(zlib);
    linkMsvcRuntime(betakeygen_module, optimize);
    if (target.result.os.tag == .windows) {
        betakeygen_module.linkSystemLibrary("odbc32", .{});
        betakeygen_module.linkSystemLibrary("odbccp32", .{});
    }

    const betakeygen = b.addExecutable(.{
        .name = "BetaKeyGen",
        .root_module = betakeygen_module,
    });
    betakeygen.subsystem = .console;
    betakeygen.entry = .{ .symbol_name = "mainCRTStartup" };
    return betakeygen;
}

fn addInput(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const input_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, input_module);
    addMsvcIncludePaths(b, input_module, toolchain);
    addMsvcLibraryPaths(b, input_module, toolchain);
    input_module.addIncludePath(b.path("Sources/src/Input"));
    input_module.addCMacro("BK_INPUT_EVENT_ONLY", "1");
    input_module.addCSourceFiles(.{
        .files = input_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    input_module.linkLibrary(misc);
    input_module.linkLibrary(platform_runtime);
    linkSdlImport(input_module, target, sdl_dynamic);
    linkMsvcRuntime(input_module, optimize);
    if (target.result.os.tag == .windows) {
        input_module.linkSystemLibrary("winmm", .{});
        input_module.linkSystemLibrary("dinput8", .{});
        input_module.linkSystemLibrary("dxguid", .{});
        input_module.linkSystemLibrary("user32", .{});
        input_module.linkSystemLibrary("odbc32", .{});
        input_module.linkSystemLibrary("odbccp32", .{});
        linkComSupport(input_module, optimize);
    }

    applyLoaderPath(target, input_module);
    addSharedObjectFinalizer(b, target, input_module);
    return b.addLibrary(.{
        .name = "Input",
        .linkage = .dynamic,
        .root_module = input_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/Input/Input.def") else null,
    });
}

fn addInputModuleTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
    input: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) void {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Input"));
    module.addCSourceFiles(.{ .files = &.{ "tools/zig/input_module_test.cpp" }, .flags = if (target.result.os.tag == .windows) cppflagsForOptimize(optimize) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, optimize);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    module.linkLibrary(misc);
    module.linkLibrary(platform_runtime);
    const exe = b.addExecutable(.{ .name = "input-module-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("zig-out/bin"));
    run.addPathDir(b.path("zig-out/bin").getPath(b));
    run.addArg(if (target.result.os.tag == .windows) b.path("zig-out/bin/Input.dll").getPath(b) else if (target.result.os.tag == .macos) b.path("zig-out/lib/libInput.dylib").getPath(b) else b.path("zig-out/lib/libInput.so").getPath(b));
    const step = b.step("test-input-module", "Load and exercise the real Input module factory lifecycle");
    step.dependOn(&b.addInstallArtifact(input, .{}).step);
    step.dependOn(&b.addInstallArtifact(misc, .{}).step);
    step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addFormats(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const formats_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, formats_module);
    addMsvcIncludePaths(b, formats_module, toolchain);
    formats_module.addIncludePath(b.path("Sources/src/Formats"));
    formats_module.addIncludePath(b.path("Sources/src/Image"));
    formats_module.addIncludePath(b.path("Sources/src/Anim"));
    formats_module.addIncludePath(b.path("Sources/src/Common"));
    formats_module.addCSourceFiles(.{
        .files = formats_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "Formats",
        .linkage = .static,
        .root_module = formats_module,
    });
}

fn addMapFile(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const map_file_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, map_file_module);
    addMsvcIncludePaths(b, map_file_module, toolchain);
    map_file_module.addIncludePath(b.path("Sources/src/Formats"));
    map_file_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    map_file_module.addIncludePath(b.path("Sources/src/Common"));
    map_file_module.addIncludePath(b.path("Sources/src/Main"));
    map_file_module.addIncludePath(b.path("Sources/src/Image"));
    map_file_module.addCSourceFiles(.{
        .files = &.{ "Sources/src/MapFile/MapFile.cpp", "Sources/src/MapFile/MapEquivalence.cpp", "Sources/src/MapFile/MapOverlay.cpp", "Sources/src/MapFile/MapRecords.cpp", "Sources/src/MapFile/MapGeometry.cpp" },
        .flags = cppflagsForOptimize(optimize),
    });
    return b.addLibrary(.{
        .name = "MapFile",
        .linkage = .static,
        .root_module = map_file_module,
    });
}

fn addEditorBridge(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    // The engine's own object layer, CWorldBase, which world.cpp subclasses the
    // way the MFC editor's frame does. GameTT links the same static library.
    common: *std.Build.Step.Compile,
    // The headers only: BkEditorStart reads the window's size to set the mode.
    // Whatever links the bridge links SDL, as editor-bridge-test does.
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    addMsvcIncludePaths(b, module, toolchain);
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Formats"));
    module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    // Scene/SceneScreenScale.h's own bare #include "Globals.h" resolves
    // relative to Scene's directory first, same as every other module that
    // includes it (build.zig's Scene target carries this path too) - without
    // it, the zoom bridge's #include "../Scene/SceneScreenScale.h" fails to
    // find Globals.h even though StdAfx.h already pulls the same header in
    // through its own relative "../StreamIO/Globals.h" include.
    module.addIncludePath(b.path("Sources/src/StreamIO"));
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/EditorBridge/bridge.cpp",
            "Sources/src/EditorBridge/session.cpp",
            "Sources/src/EditorBridge/session_records.cpp",
            "Sources/src/EditorBridge/session_terrain.cpp",
            "Sources/src/EditorBridge/session_vso.cpp",
            "Sources/src/EditorBridge/session_groups.cpp",
            "Sources/src/EditorBridge/catalogue.cpp",
            "Sources/src/EditorBridge/filters.cpp",
            "Sources/src/EditorBridge/session_rmg.cpp",
            "Sources/src/EditorBridge/session_fields.cpp",
            "Sources/src/EditorBridge/session_layers.cpp",
            "Sources/src/EditorBridge/world.cpp",
            // S04 T01: the resource bridge's C ABI, stubbed today, filled in
            // by T02..T06. One archive beside bridge.cpp so a resource editor
            // process links EditorBridge and gets both ABIs.
            "Sources/src/EditorBridge/resource_bridge.cpp",
            // S04 T02: ResourceModel's Project+Tree model, pulled into the
            // bridge archive so resource_bridge.cpp links against Load, Save,
            // CTreeItemFactory and the typed root classes without needing a
            // separate static library. The test tiers that already compile
            // these sources into their own executables (test-resource-model,
            // -references, -fidelity) continue to do so; the compile cost
            // of a second copy is negligible against the integration shape
            // this archive gives the resource bridge.
            "Sources/src/ResourceModel/combos.cpp",
            "Sources/src/ResourceModel/factory.cpp",
            "Sources/src/ResourceModel/future_blob.cpp",
            "Sources/src/ResourceModel/key_frame_tree_item.cpp",
            "Sources/src/ResourceModel/localization.cpp",
            "Sources/src/ResourceModel/localization_item.cpp",
            "Sources/src/ResourceModel/editor_env.cpp",
            "Sources/src/ResourceModel/exporter.cpp",
            "Sources/src/ResourceModel/grid_projection.cpp",
            "Sources/src/ResourceModel/mfc_value.cpp",
            "Sources/src/ResourceModel/project.cpp",
            "Sources/src/ResourceModel/references.cpp",
            "Sources/src/ResourceModel/tree_item.cpp",
            "Sources/src/ResourceModel/variant.cpp",
            "Sources/src/ResourceModel/xml.cpp",
            "Sources/src/ResourceModel/items/stats_item.cpp",
            "Sources/src/ResourceModel/items/ai_tiles.cpp",
            "Sources/src/ResourceModel/items/bridge/bridge.cpp",
            "Sources/src/ResourceModel/items/building/building.cpp",
            "Sources/src/ResourceModel/items/campaign/campaign.cpp",
            "Sources/src/ResourceModel/items/chapter/chapter.cpp",
            "Sources/src/ResourceModel/items/effect/effect.cpp",
            "Sources/src/ResourceModel/items/fence/fence.cpp",
            "Sources/src/ResourceModel/items/gui/gui.cpp",
            "Sources/src/ResourceModel/items/infantry/infantry.cpp",
            "Sources/src/ResourceModel/items/medal/medal.cpp",
            "Sources/src/ResourceModel/items/mesh/mesh.cpp",
            "Sources/src/ResourceModel/items/mine/mine.cpp",
            "Sources/src/ResourceModel/items/mission/mission.cpp",
            "Sources/src/ResourceModel/items/object/object.cpp",
            "Sources/src/ResourceModel/items/particle/particle.cpp",
            "Sources/src/ResourceModel/items/river3d/river3d.cpp",
            "Sources/src/ResourceModel/items/road3d/road3d.cpp",
            "Sources/src/ResourceModel/items/sprite/sprite.cpp",
            "Sources/src/ResourceModel/items/squad/squad.cpp",
            "Sources/src/ResourceModel/items/tileset/tileset.cpp",
            "Sources/src/ResourceModel/items/trench/trench.cpp",
            "Sources/src/ResourceModel/items/weapon/weapon.cpp",
            // S06: the stats exporters. They write through the engine's
            // CTreeAccessor and stats structs, so they live here and not in
            // resource_model_sources, which the engine-free model tests build.
            "Sources/src/ResourceModel/items/stats_export.cpp",
            "Sources/src/ResourceModel/image_export.cpp",
            "Sources/src/ResourceModel/compose.cpp",
            "Sources/src/ResourceModel/items/weapon/weapon_export.cpp",
            "Sources/src/ResourceModel/items/mine/mine_export.cpp",
            "Sources/src/ResourceModel/items/medal/medal_export.cpp",
            "Sources/src/ResourceModel/items/chapter/chapter_export.cpp",
            "Sources/src/ResourceModel/items/campaign/campaign_export.cpp",
            "Sources/src/ResourceModel/items/mission/mission_export.cpp",
            "Sources/src/ResourceModel/items/trench/trench_export.cpp",
            "Sources/src/ResourceModel/items/squad/squad_export.cpp",
            "Sources/src/ResourceModel/items/sprite/sprite_export.cpp",
            "Sources/src/ResourceModel/items/infantry/infantry_export.cpp",
            "Sources/src/ResourceModel/items/mesh/mesh_export.cpp",
            "Sources/src/ResourceModel/items/object/object_export.cpp",
            "Sources/src/ResourceModel/items/fence/fence_export.cpp",
            "Sources/src/ResourceModel/items/building/building_export.cpp",
            "Sources/src/ResourceModel/items/bridge/bridge_export.cpp",
            "Sources/src/ResourceModel/items/particle/particle_export.cpp",
            "Sources/src/ResourceModel/items/effect/effect_export.cpp",
            "Sources/src/ResourceModel/items/road3d/road3d_export.cpp",
            "Sources/src/ResourceModel/items/river3d/river3d_export.cpp",
            "Sources/src/ResourceModel/items/tileset/tileset_export.cpp",
        },
        .flags = cppflagsForOptimize(optimize),
    });
    module.linkLibrary(common);
    return b.addLibrary(.{
        .name = "EditorBridge",
        .linkage = .static,
        .root_module = module,
    });
}

fn addAnim(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const anim_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, anim_module);
    addMsvcIncludePaths(b, anim_module, toolchain);
    addMsvcLibraryPaths(b, anim_module, toolchain);
    anim_module.addIncludePath(b.path("Sources/src/Anim"));
    anim_module.addIncludePath(b.path("Sources/src/Formats"));
    anim_module.addCSourceFiles(.{
        .files = anim_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    anim_module.linkLibrary(misc);
    anim_module.linkLibrary(platform_runtime);
    linkSdlImport(anim_module, target, sdl_dynamic);
    anim_module.linkLibrary(formats);
    linkMsvcRuntime(anim_module, optimize);
    if (target.result.os.tag == .windows) {
        anim_module.linkSystemLibrary("odbc32", .{});
        anim_module.linkSystemLibrary("odbccp32", .{});
    }
    if (target.result.os.tag == .windows) linkComSupport(anim_module, optimize);

    applyLoaderPath(target, anim_module);
    addSharedObjectFinalizer(b, target, anim_module);
    return b.addLibrary(.{
        .name = "Anim",
        .linkage = .dynamic,
        .root_module = anim_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/Anim/Animation.def") else null,
    });
}

fn addCommon(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const common_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, common_module);
    addMsvcIncludePaths(b, common_module, toolchain);
    common_module.addIncludePath(b.path("Sources/src/Common"));
    common_module.addIncludePath(b.path("Sources/src/AILogic"));
    common_module.addIncludePath(b.path("Sources/src/GameTT"));
    common_module.addIncludePath(b.path("Sources/src/Main"));
    common_module.addIncludePath(b.path("Sources/src/GFX"));
    common_module.addIncludePath(b.path("Sources/src/SFX"));
    common_module.addIncludePath(b.path("Sources/src/Input"));
    common_module.addIncludePath(b.path("Sources/src/Scene"));
    common_module.addIncludePath(b.path("Sources/src/UI"));
    common_module.addIncludePath(b.path("Sources/src/Anim"));
    common_module.addIncludePath(b.path("Sources/src/Image"));
    common_module.addIncludePath(b.path("Sources/src/StreamIO"));
    common_module.addCSourceFiles(.{
        .files = common_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "Common",
        .linkage = .static,
        .root_module = common_module,
    });
}

fn addUI(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    common: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const ui_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, ui_module);
    addMsvcIncludePaths(b, ui_module, toolchain);
    addMsvcLibraryPaths(b, ui_module, toolchain);
    ui_module.addIncludePath(b.path("Sources/src/UI"));
    ui_module.addIncludePath(b.path("Sources/src/Common"));
    ui_module.addIncludePath(b.path("Sources/src/LuaLib"));
    ui_module.addIncludePath(b.path("Sources/src/LuaLib/LuaSrc"));
    ui_module.addIncludePath(b.path("Sources/src/Image"));
    ui_module.addIncludePath(b.path("Sources/src/Input"));
    ui_module.addIncludePath(b.path("Sources/src/GFX"));
    ui_module.addIncludePath(b.path("Sources/src/SFX"));
    ui_module.addIncludePath(b.path("Sources/src/Scene"));
    ui_module.addIncludePath(b.path("Sources/src/Main"));
    ui_module.addCSourceFiles(.{
        .files = ui_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    ui_module.linkLibrary(misc);
    ui_module.linkLibrary(platform_runtime);
    linkSdlImport(ui_module, target, sdl_dynamic);
    ui_module.linkLibrary(common);
    ui_module.linkLibrary(lualib);
    linkMsvcRuntime(ui_module, optimize);
    if (target.result.os.tag == .windows) {
        ui_module.linkSystemLibrary("odbc32", .{});
        ui_module.linkSystemLibrary("odbccp32", .{});
    }
    if (target.result.os.tag == .windows) linkComSupport(ui_module, optimize);

    applyLoaderPath(target, ui_module);
    addSharedObjectFinalizer(b, target, ui_module);
    return b.addLibrary(.{
        .name = "UI",
        .linkage = .dynamic,
        .root_module = ui_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/UI/UI.def") else null,
    });
}

fn addFontGen(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    image: *std.Build.Step.Compile,
    common: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const fontgen_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, fontgen_module);
    addMsvcIncludePaths(b, fontgen_module, toolchain);
    addMsvcLibraryPaths(b, fontgen_module, toolchain);
    fontgen_module.addIncludePath(b.path("Sources/src/FontGen"));
    fontgen_module.addIncludePath(b.path("Sources/src/Image"));
    fontgen_module.addIncludePath(b.path("Sources/src/Common"));
    fontgen_module.addIncludePath(b.path("Sources/src/Formats"));
    fontgen_module.addCSourceFiles(.{
        .files = fontgen_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    fontgen_module.linkLibrary(image);
    fontgen_module.linkLibrary(common);
    fontgen_module.linkLibrary(formats);
    fontgen_module.linkLibrary(misc);
    fontgen_module.linkLibrary(platform_runtime);
    linkSdlImport(fontgen_module, target, sdl_dynamic);
    linkMsvcRuntime(fontgen_module, optimize);
    if (target.result.os.tag == .windows) {
        fontgen_module.linkSystemLibrary("user32", .{});
        fontgen_module.linkSystemLibrary("gdi32", .{});
        fontgen_module.linkSystemLibrary("odbc32", .{});
        fontgen_module.linkSystemLibrary("odbccp32", .{});
    }

    const fontgen = b.addExecutable(.{
        .name = "FontGen",
        .root_module = fontgen_module,
    });
    fontgen.subsystem = .console;
    fontgen.entry = .{ .symbol_name = "mainCRTStartup" };
    return fontgen;
}

fn addSFX(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    common: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const sfx_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, sfx_module);
    addMsvcIncludePaths(b, sfx_module, toolchain);
    addMsvcLibraryPaths(b, sfx_module, toolchain);
    sfx_module.addIncludePath(b.path("Sources/src/SFX"));
    sfx_module.addIncludePath(b.path("Sources/src/Common"));
    sfx_module.addIncludePath(b.path("Sources/src/StreamIO"));
    sfx_module.addIncludePath(b.path("Sources/src/Main"));
    sfx_module.addIncludePath(b.path("Sources/sdk/miniaudio"));
    sfx_module.addIncludePath(b.path("Sources/sdk/xiph/ogg-1.3.5/include"));
    sfx_module.addIncludePath(b.path("Sources/sdk/xiph/vorbis-1.3.7/include"));
    sfx_module.addIncludePath(b.path("Sources/sdk/xiph/vorbis-1.3.7/lib"));
    sfx_module.addCSourceFiles(.{
        .files = sfx_cpp_sources,
        .flags = cppflagsSfxForOptimize(optimize),
    });
    sfx_module.addCSourceFiles(.{
        .files = sfx_c_sources,
        .flags = cflagsSfxForOptimize(optimize),
    });
    sfx_module.linkLibrary(misc);
    sfx_module.linkLibrary(platform_runtime);
    linkSdlImport(sfx_module, target, sdl_dynamic);
    sfx_module.linkLibrary(common);
    linkMsvcRuntime(sfx_module, optimize);
    if (target.result.os.tag == .windows) {
        sfx_module.linkSystemLibrary("winmm", .{});
        sfx_module.linkSystemLibrary("odbc32", .{});
        sfx_module.linkSystemLibrary("odbccp32", .{});
        linkComSupport(sfx_module, optimize);
    }

    applyLoaderPath(target, sfx_module);
    addSharedObjectFinalizer(b, target, sfx_module);
    return b.addLibrary(.{
        .name = "SFX",
        .linkage = .dynamic,
        .root_module = sfx_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/SFX/Sound.def") else null,
    });
}

fn addSfxModuleTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
    sfx: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    options_bridge: *std.Build.Step.Compile,
    streamio_zig: *std.Build.Step.Compile,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFile(.{ .file = b.path("tools/zig/sfx_module_test.cpp"), .flags = cppflagsForTarget(target, .Debug) });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/SFX"));
    module.linkLibrary(misc);
    module.linkLibrary(platform_runtime);
    if (target.result.os.tag == .windows) {
        addMsvcIncludePaths(b, module, toolchain);
        addMsvcLibraryPaths(b, module, toolchain);
    }
    linkMsvcRuntime(module, .Debug);
    const exe = b.addExecutable(.{ .name = "sfx-module-test", .root_module = module });
    if (target.result.os.tag == .windows) {
        exe.subsystem = .console;
        exe.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.addArg(if (target.result.os.tag == .windows) "zig-out/bin/SFX.dll" else if (target.result.os.tag == .macos) "zig-out/lib/libSFX.dylib" else "zig-out/lib/libSFX.so");
    run.addPathDir(b.path("zig-out/bin").getPath(b));
    if (target.result.os.tag != .windows) run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    // dyld ignores LD_LIBRARY_PATH: without this the module's own
    // CGlobalsLoader cannot find libStreamIO, the singleton stays null and
    // CSoundEngine::Init crashes reading a global var.
    if (target.result.os.tag == .macos) run.setEnvironmentVariable("DYLD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    run.step.dependOn(&b.addInstallArtifact(sfx, .{}).step);
    run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    run.step.dependOn(&b.addInstallArtifact(options_bridge, .{}).step);
    run.step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);
    const step = b.step("test-sfx-module", "Load the real SFX module and verify its lifecycle contract");
    step.dependOn(&run.step);
}

fn addGFX(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const gfx_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, gfx_module);
    addMsvcIncludePaths(b, gfx_module, toolchain);
    addMsvcLibraryPaths(b, gfx_module, toolchain);
    gfx_module.addIncludePath(b.path("Sources/src/GFX"));
    gfx_module.addIncludePath(b.path("Sources/src/Image"));
    gfx_module.addIncludePath(b.path("Sources/src/Anim"));
    gfx_module.addCSourceFiles(.{
        .files = gfx_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    gfx_module.linkLibrary(misc);
    gfx_module.linkLibrary(platform_runtime);
    linkSdlImport(gfx_module, target, sdl_dynamic);
    gfx_module.linkLibrary(formats);
    linkMsvcRuntime(gfx_module, optimize);
    if (target.result.os.tag == .windows) {
        gfx_module.linkSystemLibrary("d3d9", .{});
        gfx_module.linkSystemLibrary("dxguid", .{});
        gfx_module.linkSystemLibrary("user32", .{});
        gfx_module.linkSystemLibrary("gdi32", .{});
        gfx_module.linkSystemLibrary("odbc32", .{});
        gfx_module.linkSystemLibrary("odbccp32", .{});
        linkComSupport(gfx_module, optimize);
    }

    applyLoaderPath(target, gfx_module);
    addSharedObjectFinalizer(b, target, gfx_module);
    return b.addLibrary(.{
        .name = "GFX",
        .linkage = .dynamic,
        .root_module = gfx_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/GFX/GFX.def") else null,
    });
}

fn addGFXGPU(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    gfx_gpu_zig: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const gfx_gpu_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, gfx_gpu_module);
    addMsvcIncludePaths(b, gfx_gpu_module, toolchain);
    addMsvcLibraryPaths(b, gfx_gpu_module, toolchain);
    gfx_gpu_module.addIncludePath(b.path("Sources/src/GFX"));
    gfx_gpu_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    gfx_gpu_module.addCSourceFiles(.{
        .files = gfx_gpu_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    gfx_gpu_module.linkLibrary(misc);
    gfx_gpu_module.linkLibrary(platform_runtime);
    gfx_gpu_module.linkLibrary(formats);
    gfx_gpu_module.linkLibrary(gfx_gpu_zig);
    if (target.result.os.tag == .macos) {
        // gfx_gpu_zig already links SDL3, so linking it again here emits a
        // second LC_LOAD_DYLIB for @rpath/libSDL3.dylib. ELF tolerates the
        // duplicate DT_NEEDED, but dyld rejects the image outright
        // ("duplicate linked dylib"), so take only the headers here.
        gfx_gpu_module.addIncludePath(sdl_include);
    } else {
        linkSdlRuntime(gfx_gpu_module, target, sdl_dynamic, sdl_include);
    }
    linkMsvcRuntime(gfx_gpu_module, optimize);
    if (target.result.os.tag == .windows) {
        gfx_gpu_module.linkSystemLibrary("user32", .{});
        gfx_gpu_module.linkSystemLibrary("gdi32", .{});
    }

    applyLoaderPath(target, gfx_gpu_module);
    addSharedObjectFinalizer(b, target, gfx_gpu_module);
    return b.addLibrary(.{
        .name = "GFXGPU",
        .linkage = .dynamic,
        .root_module = gfx_gpu_module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path("Sources/src/GFXGPU/GFXGPU.def") else null,
    });
}

fn addGfxGpuZig(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    sdl3: *std.Build.Module,
) *std.Build.Step.Compile {
    const gfx_gpu_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/GFXGPU/root.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "sdl3", .module = sdl3 }},
    });
    applyLoaderPath(target, gfx_gpu_module);

    return b.addLibrary(.{
        .name = "GfxGpuZig",
        .linkage = if (target.result.os.tag == .linux) .dynamic else .static,
        .root_module = gfx_gpu_module,
    });
}

// Dear ImGui (docking branch) with the dear_bindings C API and the SDL3 +
// SDL GPU backends, for the portable editors. Static: it lives inside the
// editor executable and shares the one dynamic SDL3 the game ships.
fn addEditorImgui(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    // Zig links the release CRT for an MSVC target even in Debug, so the C++
    // objects are built against the release CRT too: a debug build here
    // references the debug CRT and the two cannot be linked together.
    const cxx_optimize = if (target.result.abi == .msvc) .ReleaseFast else optimize;
    const module = b.createModule(.{
        .target = target,
        .optimize = cxx_optimize,
    });
    module.addIncludePath(b.path("vendor/dcimgui/src-docking"));
    module.addIncludePath(b.path("vendor/dcimgui/backends"));
    module.addIncludePath(b.path("Sources/editor/kit/imgui"));
    module.addIncludePath(sdl_include);
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    // On MSVC the CRT is the consumer's business: Zig links its own libc into
    // the Zig programs that use this library, and naming the MSVC CRT here as
    // well links two of them (duplicate _cexit, _wctype, ...).
    // On Linux the C++ runtime is the host's shared libstdc++ and libgcc_s,
    // which a static archive cannot hold as members (LLD rejects them); the
    // consuming executable links them itself, so only libc is named here.
    if (target.result.os.tag == .linux) {
        module.link_libc = true;
    } else if (target.result.abi != .msvc) linkMsvcRuntime(module, optimize);
    module.addCSourceFiles(.{
        .files = &.{
            "vendor/dcimgui/src-docking/imgui.cpp",
            "vendor/dcimgui/src-docking/imgui_demo.cpp",
            "vendor/dcimgui/src-docking/imgui_draw.cpp",
            "vendor/dcimgui/src-docking/imgui_tables.cpp",
            "vendor/dcimgui/src-docking/imgui_widgets.cpp",
            "vendor/dcimgui/src-docking/cimgui.cpp",
            "vendor/dcimgui/backends/imgui_impl_sdl3.cpp",
            "vendor/dcimgui/backends/imgui_impl_sdlgpu3.cpp",
            "Sources/editor/kit/imgui/imgui_backend.cpp",
        },
        // Plain C++17 everywhere: the project's MSVC flag set selects the DLL
        // CRT (-D_MT -D_DLL), which does not match the CRT Zig links into the
        // Zig programs that consume this library. ImGui needs none of it.
        .flags = &.{"-std=c++17"},
    });
    return b.addLibrary(.{ .name = "editor-imgui", .linkage = .static, .root_module = module });
}

fn cflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return switch (optimize) {
        .Debug => portable_cflags,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => &portable_cflags_release,
    };
    return switch (optimize) {
        .Debug => cflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_release,
    };
}

fn cppflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return switch (optimize) {
        .Debug => &portable_cppflags,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => &portable_cppflags_release,
    };
    return switch (optimize) {
        .Debug => if (ubsan_trap) cppflags_debug_trap else cppflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_release,
    };
}

fn cppflagsForTarget(target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) []const []const u8 {
    // Only real MSVC needs the -fms-extensions/-fms-compatibility flags; a
    // windows-gnu (mingw) target compiles with the plain libstdc++ flags,
    // and previously got the MSVC set solely because its os.tag is .windows.
    if (target.result.os.tag != .windows or target.result.abi != .msvc) return &.{"-std=c++17"};
    return cppflagsForOptimize(optimize);
}

// Debian multiarch library directory for a Linux target. This was hardcoded to
// the x86_64 triple, so an arm64 Linux host looked for its C++ runtime under
// /usr/lib/x86_64-linux-gnu and found nothing: the aarch64 job could not link
// libstdc++ at all. Only ever consulted for native Linux builds, where the
// target architecture is also the host's.
// Debian multiarch architecture name. libstdc++ keeps bits/c++config.h under
// /usr/include/<arch>-linux-gnu/c++/<version>, so a hardcoded x86_64 here left
// an arm64 host unable to find it and every <cstdint> include failed.
fn linuxArchName(arch: std.Target.Cpu.Arch) []const u8 {
    return switch (arch) {
        .aarch64 => "aarch64",
        else => "x86_64",
    };
}

fn linuxMultiarchDir(arch: std.Target.Cpu.Arch) []const u8 {
    return switch (arch) {
        .aarch64 => "/usr/lib/aarch64-linux-gnu",
        else => "/usr/lib/x86_64-linux-gnu",
    };
}

fn linkCxxRuntime(module: *std.Build.Module, target: std.Build.ResolvedTarget) void {
    switch (target.result.os.tag) {
        // Zig treats the abstract name `stdc++` as its libc++ switch. The
        // native Linux C++ headers in the supported WSL/CI toolchain are
        // libstdc++, so use the concrete soname to match headers and ABI.
        .linux => {
            if (build_host_os == .linux) {
                switch (target.result.cpu.arch) {
                    .aarch64 => {
                        module.addObjectFile(.{ .cwd_relative = "/usr/lib/aarch64-linux-gnu/libstdc++.so.6" });
                        module.addObjectFile(.{ .cwd_relative = "/usr/lib/aarch64-linux-gnu/libgcc_s.so.1" });
                    },
                    else => {
                        module.addObjectFile(.{ .cwd_relative = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6" });
                        module.addObjectFile(.{ .cwd_relative = "/usr/lib/x86_64-linux-gnu/libgcc_s.so.1" });
                    },
                }
            } else {
                module.linkSystemLibrary("stdc++", .{});
            }
        },
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
}

fn cppflagsBetaForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return &portable_cppflags;
    return switch (optimize) {
        .Debug => cppflags_beta_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_beta_release,
    };
}

fn cflagsSfxForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return portable_cflags;
    return switch (optimize) {
        .Debug => cflags_sfx_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_sfx_release,
    };
}

fn cppflagsSfxForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return switch (optimize) {
        .Debug => &portable_cppflags,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => &portable_cppflags_release,
    };
    return switch (optimize) {
        .Debug => cppflags_sfx_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_sfx_release,
    };
}

fn cppflagsGameForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    if (build_target_os != .windows) return switch (optimize) {
        .Debug => &portable_cppflags,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => &portable_cppflags_release,
    };
    return switch (optimize) {
        .Debug => cppflags_game_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_game_release,
    };
}

fn addProjectIncludePaths(b: *std.Build, module: *std.Build.Module) void {
    module.addIncludePath(b.path("Sources/src"));
    module.addIncludePath(b.path("Sources/src/Misc"));
    module.addIncludePath(b.path("Sources/src/Formats"));
    addLinuxCxxIncludePaths(b, module);
}

const ToolchainIncludes = struct {
    msvc_include: []const u8,
    windows_sdk_include: []const u8,
    msvc_lib: []const u8,
    windows_sdk_lib: []const u8,
    library_arch: []const u8,
};

// zig resolves macOS frameworks from the native SDK on its own, but a build
// driven with an explicit --sysroot (which is how CI invokes every macOS step)
// searches no framework directory at all, so anything that links SDL fails with
// "unable to find framework 'Cocoa'". Point the module at the sysroot's own
// framework and library directories.
fn addMacosSysrootPaths(b: *std.Build, module: *std.Build.Module, target: std.Build.ResolvedTarget) void {
    if (target.result.os.tag != .macos) return;
    addMacosSysrootPathsToModule(b, module);
}

/// The same for a module whose target is the host rather than a resolved one -
/// the shadercross tool. Split out because --sysroot is global to the build, so
/// a host tool needs the paths just as much as a cross-compiled one does.
fn addMacosSysrootPathsToModule(b: *std.Build, module: *std.Build.Module) void {
    const sysroot = b.sysroot orelse return;
    module.addFrameworkPath(.{ .cwd_relative = b.pathJoin(&.{ sysroot, "System/Library/Frameworks" }) });
    // libobjc and the rest of the system libraries live there as .tbd stubs.
    // Without this, linkSystemLibrary("objc") under --sysroot fails with
    // "unable to find dynamic system library 'objc' ... searched paths: none",
    // which is what the engine tier hit the first time CI built the game on
    // macOS - nothing else in CI had ever linked a system library there.
    //
    // Sysroot-relative, unlike the framework path above: zig prefixes --sysroot
    // onto a library path and does not onto a framework path. Measured - an
    // absolute one comes out as <sysroot>/<sysroot>/usr/lib and warns
    // "unable to open library directory", then fails to find anything.
    module.addLibraryPath(.{ .cwd_relative = "/usr/lib" });
}

fn addMsvcIncludePaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    if (!build_target_msvc) return;
    module.addSystemIncludePath(.{ .cwd_relative = toolchain.msvc_include });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/ucrt", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/shared", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/um", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/winrt", .{toolchain.windows_sdk_include}) });
}

fn addMsvcLibraryPaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    if (!build_target_msvc) return;
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/{s}", .{ toolchain.msvc_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/ucrt/{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/um/{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
}

/// macOS counterpart to the Linux libstdc++ policy below. Zig ships its own
/// libc++ and injects those headers whenever a module links it, so the macOS
/// build standardizes on that single header set and implementation rather than
/// mixing in the SDK's copy: having both visible redefines libc++'s configuration
/// macros and leaves <cstring>/<cerrno> using-declarations unresolved.
fn addMacosCxxIncludePaths(b: *std.Build, module: *std.Build.Module) void {
    if (build_target_os != .macos or b.graph.host.result.os.tag != .macos) return;
    module.link_libcpp = true;
}

/// The staged runtime layout puts Game and every shared library side by side
/// in one directory, so each binary resolves its dependencies relative to its
/// own location: `$ORIGIN` in ELF runpaths, `@loader_path` in Mach-O rpaths.
/// Without this the staged layout only resolves against the build-time
/// .zig-cache paths Zig records, and Game fails to launch
/// ("libPlatformRuntime.so: cannot open shared object file" on Linux).
// Every C++ shared object gets Platform/SharedObjectFinalize.cpp: Zig links no
// crtbegin into a shared object, so without it nothing calls __cxa_finalize at
// dlclose and the library's static destructors run at exit() in unmapped code.
// Linux only; dyld and the Windows loader handle this themselves.
fn addSharedObjectFinalizer(b: *std.Build, target: std.Build.ResolvedTarget, module: *std.Build.Module) void {
    if (target.result.os.tag != .linux) return;
    module.addCSourceFile(.{ .file = b.path("Sources/src/Platform/SharedObjectFinalize.cpp"), .flags = &.{"-std=c++17"} });
}

fn applyLoaderPath(target: std.Build.ResolvedTarget, module: *std.Build.Module) void {
    switch (target.result.os.tag) {
        .linux => module.addRPathSpecial("$ORIGIN"),
        .macos => module.addRPathSpecial("@loader_path"),
        else => {},
    }
}

fn addLinuxCxxIncludePaths(b: *std.Build, module: *std.Build.Module) void {
    addMacosCxxIncludePaths(b, module);
    if (build_target_os != .linux or b.graph.host.result.os.tag != .linux) return;
    module.addLibraryPath(.{ .cwd_relative = linuxMultiarchDir(b.graph.host.result.cpu.arch) });
    var versions = std.Io.Dir.openDirAbsolute(b.graph.io, "/usr/include/c++", .{ .iterate = true }) catch return;
    defer std.Io.Dir.close(versions, b.graph.io);
    var selected: ?[]const u8 = null;
    var iterator = versions.iterate();
    while (iterator.next(b.graph.io) catch null) |entry| {
        if (entry.kind != .directory) continue;
        if (selected == null or std.mem.order(u8, selected.?, entry.name) == .lt) {
            selected = b.allocator.dupe(u8, entry.name) catch @panic("OOM");
        }
    }
    const version = selected orelse return;
    const arch = linuxArchName(b.graph.host.result.cpu.arch);
    // Zig's Linux C++ driver injects libc++ system headers first. These
    // legacy modules share STL-bearing C++ objects across DLL boundaries, so
    // treat the native libstdc++ headers as ordinary include paths and keep one
    // ABI across Game, Main, Misc, and the loaded modules.
    module.addIncludePath(.{ .cwd_relative = b.fmt("/usr/include/c++/{s}", .{version}) });
    module.addSystemIncludePath(.{ .cwd_relative = "/usr/include" });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/include/{s}-linux-gnu", .{arch}) });
    module.addIncludePath(.{ .cwd_relative = b.fmt("/usr/include/{s}-linux-gnu/c++/{s}", .{ arch, version }) });
    module.addIncludePath(.{ .cwd_relative = b.fmt("/usr/include/c++/{s}/backward", .{version}) });
    var gcc_versions = std.Io.Dir.openDirAbsolute(b.graph.io, b.fmt("/usr/lib/gcc/{s}-linux-gnu", .{arch}), .{ .iterate = true }) catch return;
    defer std.Io.Dir.close(gcc_versions, b.graph.io);
    var selected_gcc: ?[]const u8 = null;
    var gcc_iterator = gcc_versions.iterate();
    while (gcc_iterator.next(b.graph.io) catch null) |entry| {
        if (entry.kind != .directory) continue;
        if (std.mem.eql(u8, entry.name, version)) {
            selected_gcc = b.allocator.dupe(u8, entry.name) catch @panic("OOM");
            break;
        }
        if (selected_gcc == null or std.mem.order(u8, selected_gcc.?, entry.name) == .lt) {
            selected_gcc = b.allocator.dupe(u8, entry.name) catch @panic("OOM");
        }
    }
    if (selected_gcc) |gcc_version| {
        module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/lib/gcc/{s}-linux-gnu/{s}/include", .{ arch, gcc_version }) });
    }
}

fn addPortableStreamioFilesTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    test_mode: build_support.TestMode,
) void {
    const streamio_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/StreamIOZig/streamio.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const streamio_platform_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/streamio_platform_test.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "streamio", .module = streamio_test_module }},
    });
    const streamio_platform_tests = b.addTest(.{ .root_module = streamio_platform_module });
    const run_streamio_platform_tests = b.addRunArtifact(streamio_platform_tests);
    const streamio_platform_step = b.step("test-platform-files", "Run portable StreamIO host filesystem tests");
    streamio_platform_step.dependOn(&streamio_platform_tests.step);
    if (test_mode == .run) streamio_platform_step.dependOn(&run_streamio_platform_tests.step);
}

fn addPortableModuleTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
        .link_libc = true,
    });
    module.addCSourceFile(.{
        .file = b.path("tools/zig/platform_module_test.cpp"),
        .flags = &.{"-std=c++17"},
    });
    const test_exe = b.addExecutable(.{ .name = "platform-module-test", .root_module = module });
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    const test_step = b.step("test-platform-modules", "Run portable runtime module tests");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addGameCommandLineTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
        .link_libc = true,
    });
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/Game/main.cpp",
            "tools/zig/game_command_line_test.cpp",
        },
        .flags = &.{"-std=c++17"},
    });
    module.addCMacro("BLITZKRIEG_COMMAND_LINE_TEST", "1");
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "game-command-line-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "main" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    const test_step = b.step("test-game-command-line", "Run portable game command-line tests");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addGameFrameTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Game"));
    // Debug.cpp reaches PlatformABI/platform_c.h by its path from Sources/src,
    // which nothing in this module put on the include path.
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/Game/GameFrame.cpp", "Sources/src/Game/MouseCapture.cpp", "Sources/src/Game/SysKeys.cpp", "Sources/src/PlatformABI/PlatformClient.cpp", "Sources/src/Platform/System.cpp", "tools/zig/game_frame_test.cpp" }, .flags = &.{ "-std=c++17" } });
    linkSdlImport(module, target, sdl_dynamic);
    // Debug.cpp speaks through the platform client, which lives in the
    // shared runtime; without it the module never linked at all.
    module.linkLibrary(platform_runtime);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            module.linkSystemLibrary("c++", .{});
            // SDLApplication reaches AppKit through the Objective-C runtime
            // (the Dock icon, the system pointer size), as the game does.
            module.linkSystemLibrary("objc", .{});
        },
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "game-frame-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "main" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    test_run.step.dependOn(&sdl_dynamic.step);
    test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    const sdl_runtime_dir = if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib";
    test_run.addPathDir(b.path(sdl_runtime_dir).getPath(b));
    if (target.result.os.tag != .windows) test_run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const test_step = b.step("test-game-frame", "Run the portable SDL game-frame contract test");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addGameSystemKeysTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Game"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Game/SysKeys.cpp", "tools/zig/game_system_keys_test.cpp" }, .flags = &.{ "-std=c++17" } });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "game-system-keys-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    const test_step = b.step("test-game-system-keys", "Run the portable game system-key policy test");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addGameMouseCaptureTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Game"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Game/MouseCapture.cpp", "tools/zig/game_mouse_capture_test.cpp" }, .flags = &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "game-mouse-capture-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    const test_step = b.step("test-game-mouse-capture", "Run the portable mouse-confinement policy test");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addGameLoopTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Game"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addIncludePath(b.path("Sources/src/GFXGPU"));
    // Debug.cpp reaches PlatformABI/platform_c.h by its path from Sources/src,
    // which nothing in this module put on the include path.
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/Game/SysKeys.cpp", "Sources/src/Game/GameFrame.cpp", "Sources/src/Game/MouseCapture.cpp", "Sources/src/PlatformABI/PlatformClient.cpp", "Sources/src/Platform/System.cpp", "tools/zig/game_loop_test.cpp" }, .flags = &.{ "-std=c++17" } });
    linkSdlImport(module, target, sdl_dynamic);
    // Debug.cpp speaks through the platform client, which lives in the
    // shared runtime; without it the module never linked at all.
    module.linkLibrary(platform_runtime);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            module.linkSystemLibrary("c++", .{});
            // SDLApplication reaches AppKit through the Objective-C runtime
            // (the Dock icon, the system pointer size), as the game does.
            module.linkSystemLibrary("objc", .{});
        },
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "game-loop-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    test_run.step.dependOn(&sdl_dynamic.step);
    test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    const sdl_runtime_dir = if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib";
    test_run.addPathDir(b.path(sdl_runtime_dir).getPath(b));
    if (target.result.os.tag != .windows) test_run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const test_step = b.step("test-game-loop", "Run the deterministic game loop policy test");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addSdlApplicationTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    platform_runtime: *std.Build.Step.Compile,
) void {
    const module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
        .link_libc = false,
    });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/Platform/SDLApplication.cpp",
            "Sources/src/Platform/Debug.cpp",
            "Sources/src/PlatformABI/PlatformClient.cpp",
            "Sources/src/Platform/System.cpp",
            "tools/zig/platform_window_test.cpp",
        },
        .flags = &.{"-std=c++17"},
    });
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);
    if (target.result.os.tag == .windows) {
        addMsvcIncludePaths(b, module, toolchain);
        addMsvcLibraryPaths(b, module, toolchain);
        linkMsvcRuntime(module, .Debug);
    } else if (target.result.os.tag == .linux) {
        module.linkSystemLibrary("stdc++", .{});
    } else if (target.result.os.tag == .macos) {
        module.linkSystemLibrary("c++", .{});
        // SDLApplication reaches AppKit through the Objective-C runtime
        // (the Dock icon, the system pointer size), as the game does.
        module.linkSystemLibrary("objc", .{});
    }
    const test_exe = b.addExecutable(.{ .name = "platform-window-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    test_run.step.dependOn(&platform_runtime.step);
    test_run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    test_run.step.dependOn(&sdl_dynamic.step);
    test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    const sdl_runtime_dir = if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib";
    test_run.addPathDir(b.path(sdl_runtime_dir).getPath(b));
    if (target.result.os.tag != .windows) test_run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const test_step = b.step("test-platform-window", "Run SDL application window lifecycle tests");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

fn addInputCodesTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Input"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Input/InputCodes.cpp", "tools/zig/input_codes_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-codes-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-codes", "Run portable legacy input code mapping tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformInputTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Input"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Input/InputCodes.cpp", "tools/zig/platform_input_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-input-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-platform-input", "Run portable keyboard and text event contract tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addInputStateFixtureTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Input"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Input/InputCodes.cpp", "tools/zig/input_state_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => { addMsvcIncludePaths(b, module, toolchain); addMsvcLibraryPaths(b, module, toolchain); linkMsvcRuntime(module, .Debug); },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-state-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-state", "Run event-fed keyboard and mouse state contract tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

/// The wheel and trackpad-swipe translation (Platform/WheelScroll.h): std-only
/// C++, no SDL and no engine module. Built on the platform-client test's
/// recipe, the C++ test test-platform-foundation already runs on every CI
/// job: libc except under MSVC, Zig's libc++ only for MinGW, the host
/// libstdc++ headers and soname on Linux (linkCxxRuntime) - never
/// linkSystemLibrary("stdc++"), which pulls Zig's libc++ headers in beside them.
fn addWheelScrollTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    platform: build_support.PlatformTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = !build_support.usesMsvc(platform), .link_libcpp = build_support.needsBundledLibcpp(platform) });
    addLinuxCxxIncludePaths(b, module);
    module.addCSourceFiles(.{ .files = &.{"tools/zig/wheel_scroll_test.cpp"}, .flags = &.{"-std=c++17"} });
    linkCxxRuntime(module, target);
    if (build_support.usesMsvc(platform)) {
        addMsvcIncludePaths(b, module, toolchain);
        addMsvcLibraryPaths(b, module, toolchain);
        linkMsvcRuntime(module, .Debug);
    }
    const exe = b.addExecutable(.{ .name = "wheel-scroll-test", .root_module = module });
    if (build_support.usesMsvc(platform)) {
        exe.subsystem = .console;
        exe.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-wheel-scroll", "Run the mouse wheel and trackpad swipe translation tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
    return step;
}

fn addInputHeaderAuditTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Input"));
    module.addCMacro("BK_INPUT_EVENT_ONLY", "1");
    module.addCSourceFiles(.{ .files = &.{ "tools/zig/input_headers_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-headers-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-headers", "Compile the portable Input header boundary audit");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addInputTextRepeatTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Input/InputCodes.cpp", "tools/zig/input_text_repeat_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-text-repeat-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-text-repeat", "Run deterministic Input text, repeat, and focus tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addInputControllerTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFiles(.{ .files = &.{ "tools/zig/input_controller_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-controller-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-controller", "Run deterministic Input controller mapping tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addInputBindingsTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFiles(.{ .files = &.{ "tools/zig/input_bindings_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-bindings-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-bindings", "Run deterministic Input binding and emulation tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformClipboardTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_clipboard_test.cpp"}, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-clipboard-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-platform-clipboard", "Run controller and clipboard contract tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformControllerTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = false });
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{ .files = &.{
        "Sources/src/Platform/SDLApplication.cpp",
        "Sources/src/Platform/Debug.cpp",
        "tools/zig/platform_controller_test.cpp",
    }, .flags = &.{"-std=c++17"} });
    linkSdlImport(module, target, sdl_dynamic);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-controller-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.step.dependOn(&sdl_dynamic.step);
    run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    run.addPathDir(b.path(if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib").getPath(b));
    if (target.result.os.tag != .windows) run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const step = b.step("test-platform-controller", "Run virtual controller name and lifetime tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformAudioTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_audio_test.cpp"}, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-audio-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-platform-audio", "Run portable audio initialization contract tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addAudioLifecycleFixtureTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addCSourceFiles(.{ .files = &.{"tools/zig/audio_lifecycle_fixture.cpp"}, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "audio-lifecycle-fixture", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-audio-lifecycle", "Run the miniaudio allocator and null-device lifecycle fixture");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addAudioWorkerTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, module);
    module.addCSourceFiles(.{ .files = &.{
        "Sources/src/Platform/Clock.cpp",
        "Sources/src/Platform/Sync.cpp",
        "Sources/src/Misc/Thread.cpp",
        "tools/zig/audio_worker_test.cpp",
    }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "audio-worker-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-audio-worker", "Run portable audio completion worker tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addAudioStreamTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, module);
    module.addCSourceFiles(.{ .files = &.{
        "Sources/src/Platform/Clock.cpp",
        "Sources/src/Platform/Sync.cpp",
        "tools/zig/audio_stream_test.cpp",
    }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "audio-stream-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-audio-stream", "Run portable audio stream lifetime tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addInputAudioGateTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{"tools/zig/input_audio_gate.cpp"}, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "input-audio-gate", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-input-audio-gate", "Run the portable input and audio lifecycle gate");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addRuntimeHeadersTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const header_names = [_][]const u8{
        "AILogic", "Anim", "Common", "Formats",      "Game",  "GameTT", "Image", "Input",
        "Main",    "Misc", "Net",    "RandomMapGen", "Scene", "SFX",    "UI",
    };
    const step = b.step("test-runtime-headers", "Compile each playable runtime StdAfx header independently");
    for (header_names, 0..) |header_name, index| {
        _ = header_name;
        const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = true });
        module.addIncludePath(b.path("Sources/src"));
        module.addIncludePath(b.path("Sources/src/Misc"));
        module.addIncludePath(b.path("Sources/src/StreamIO"));
        module.addIncludePath(b.path("Sources/src/Formats"));
        if (target.result.os.tag == .linux) addLinuxCxxIncludePaths(b, module);
        if (target.result.os.tag == .windows) addMsvcIncludePaths(b, module, toolchain);
        const index_flag = b.fmt("-DRUNTIME_HEADER_INDEX={d}", .{index});
        module.addCSourceFiles(.{
            .files = &.{"tools/zig/runtime_headers_test.cpp"},
            .flags = if (target.result.os.tag == .windows)
                &(cppflags_debug.* ++ .{index_flag})
            else
                &.{ "-std=c++17", index_flag },
        });
        const object = b.addObject(.{ .name = b.fmt("runtime-header-{d}", .{index}), .root_module = module });
        step.dependOn(&object.step);
    }
    _ = test_mode;
}

fn addPlatformSocketTypesTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_socket_types_test.cpp"}, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-socket-types-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-platform-socket-types", "Run portable socket ABI contract tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformNetworkTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SocketWin32.cpp", "Sources/src/Platform/SocketPosix.cpp", "tools/zig/platform_network_test.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
            module.linkSystemLibrary("ws2_32", .{});
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "platform-network-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-platform-network", "Run portable TCP and UDP socket tests");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addNetLowestTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Net"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/Platform/SocketWin32.cpp",
            "Sources/src/Platform/SocketPosix.cpp",
            "Sources/src/Platform/Debug.cpp",
            "Sources/src/Net/NetLowest.cpp",
            "Sources/src/Net/Streams.cpp",
            "tools/zig/netlowest_test.cpp",
        },
        .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"},
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
            module.linkSystemLibrary("ws2_32", .{});
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "netlowest-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-netlowest", "Run NetLowest loopback UDP fixture");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addNetworkWorkersTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Net"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/Platform/Clock.cpp",
            "Sources/src/Platform/Debug.cpp",
            "Sources/src/Platform/Sync.cpp",
            "Sources/src/Platform/SocketWin32.cpp",
            "Sources/src/Platform/SocketPosix.cpp",
            "Sources/src/Misc/Thread.cpp",
            "Sources/src/Net/NetLowest.cpp",
            "Sources/src/Net/Streams.cpp",
            "tools/zig/network_workers_test.cpp",
        },
        .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"},
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
            module.linkSystemLibrary("ws2_32", .{});
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "network-workers-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    const step = b.step("test-network-workers", "Run network worker cancellation and restart cycles");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addPlatformSocketAbiTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    platform_runtime: *std.Build.Step.Compile,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = target.result.os.tag != .windows });
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFile(.{ .file = b.path("tools/zig/platform_socket_abi_test.cpp"), .flags = &.{"-std=c++17"} });
    module.linkLibrary(platform_runtime);
    if (target.result.os.tag == .windows) {
        addMsvcIncludePaths(b, module, toolchain);
        addMsvcLibraryPaths(b, module, toolchain);
        linkMsvcRuntime(module, .Debug);
    } else if (target.result.os.tag == .linux) {
        module.linkSystemLibrary("stdc++", .{});
    } else if (target.result.os.tag == .macos) {
        module.linkSystemLibrary("c++", .{});
    }
    const exe = b.addExecutable(.{ .name = "platform-socket-abi-test", .root_module = module });
    if (target.result.os.tag == .windows) {
        exe.subsystem = .console;
        exe.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.addPathDir(b.path("zig-out/bin").getPath(b));
    if (target.result.os.tag != .windows) run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const step = b.step("test-platform-socket-abi", "Run the shared ABI generational socket contract");
    step.dependOn(&platform_runtime.step);
    step.dependOn(&exe.step);
    if (test_mode == .run) {
        run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
        step.dependOn(&run.step);
    }
}

fn addNetworkSystemGateTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = false });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addCSourceFiles(.{ .files = &.{ "tools/zig/network_system_gate.cpp", "Sources/src/Platform/SocketWin32.cpp", "Sources/src/Platform/SocketPosix.cpp", "Sources/src/Platform/System.cpp" }, .flags = if (target.result.os.tag == .windows) &(cppflags_debug.* ++ .{"-std=c++17"}) else &.{"-std=c++17"} });
    linkSdlImport(module, target, sdl_dynamic);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
            module.linkSystemLibrary("ws2_32", .{});
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "network-system-gate", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.step.dependOn(&sdl_dynamic.step);
    run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    const runtimeDir = if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib";
    run.addPathDir(b.path(runtimeDir).getPath(b));
    if (target.result.os.tag != .windows) run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const step = b.step("test-network-system-gate", "Run the portable network and system services gate");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addGameBootstrapSmoke(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    dependency_target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    gfx_gpu_zig: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{ .target = dependency_target, .optimize = optimize, .link_libc = true });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src"));
    module.addIncludePath(b.path("Sources/src/GFXGPU"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/game_bootstrap_smoke.cpp" }, .flags = &.{"-std=c++17"} });
    module.linkLibrary(gfx_gpu_zig);
    module.linkLibrary(platform_runtime);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "game-bootstrap-smoke", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.step.dependOn(&platform_runtime.step);
    run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    const step = b.step("test-game-bootstrap", "Run the SDL and GfxGpu game bootstrap smoke test");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addMapFileTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    streamio_zig: *std.Build.Step.Compile,
    options_bridge: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    test_mode: build_support.TestMode,
) void {
    // Shaped exactly like addInputModuleTest (build.zig:3449-3462), the
    // established way to build a C++ test that links the engine statics: no
    // link_libc, the engine's cppflags on Windows only, plain C++17 elsewhere.
    // Asking Zig for libc here got a second CRT on Windows (duplicate _cexit,
    // _invalid_parameter_noinfo, _wctype) and a second C++ standard library on
    // Linux (ambiguous std::integral_constant) - the same trap plan 1 hit with
    // the overlay spike.
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Formats"));
    module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addCSourceFiles(.{
        .files = &.{ "tools/zig/data_only_startup.cpp", "tools/zig/map_file_test.cpp" },
        // The engine's own cppflags on every target, not just Windows: this
        // test includes MapInfo_Types.h and the Common headers under it, which
        // need the PortableCrt force-include for __forceinline and the warning
        // suppressions the engine compiles itself with. A lighter test like
        // input_module_test gets away with plain C++17 because it only touches
        // the shallow headers.
        .flags = cppflagsForOptimize(optimize),
    });
    // The include and runtime recipe of gfxgpu-factory-test (build.zig:1940-1947),
    // which is the C++ executable this repository already runs on Linux CI.
    // linkSystemLibrary("stdc++") is what must not happen: Zig's Linux C++
    // driver then injects its own libc++ headers ahead of the native ones, and
    // two standard libraries in one translation unit is the
    // "std_abs.h: declaration conflicts with target of using declaration"
    // failure. addLinuxCxxIncludePaths exists to put the native libstdc++
    // headers in as ordinary include paths instead, keeping one ABI across the
    // engine modules.
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    linkMsvcRuntime(module, optimize);
    module.linkLibrary(map_file);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    module.linkLibrary(platform_runtime);
    // Misc's System.cpp reaches SDL for the clipboard and the error box. The
    // tier never opens a window; this is a link dependency, not a device one.
    linkSdlImport(module, target, sdl_dynamic);
    const exe = b.addExecutable(.{ .name = "map-file-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };

    // The test loads StreamIO by path at run time, so it is told where the
    // staged shared library is rather than relying on the loader's search path.
    // Zig installs a .dll next to the executables and a .dylib/.so under lib.
    const module_root = if (target.result.os.tag == .windows)
        b.path("zig-out/bin").getPath(b)
    else
        b.path("zig-out/lib").getPath(b);
    // Everything this tier writes goes under zig-out/local-test, which a bare
    // checkout does not have. The engine has no portable mkdir, and the Zig
    // StreamIO's CreateStorage does not create the path the way the legacy
    // Win32 CFileSystem did, so the build installs a file there and the
    // directory arrives with it.
    //
    // Mind the prose here: tools/zig/build_hermeticity_test.zig token-matches
    // this whole file against a list of shell and build-tool names, several of
    // which are also ordinary English verbs. A comment that happens to use one
    // fails the audit on every target, which is how this note came to be
    // written twice.
    const scratch = b.addWriteFiles();
    const scratch_keep = scratch.add(".keep", "scratch for the map file tier\n");
    const scratch_install = b.addInstallFileWithDir(scratch_keep, .{ .custom = "local-test" }, ".keep");
    const streamio_install = b.addInstallArtifact(streamio_zig, .{});
    // StreamIO imports StreamIOOptionsAbi and PlatformRuntime. ELF and Mach-O
    // find them by the rpath they carry; Windows has no rpath, so every one of
    // them has to be installed and on the PATH or the load fails with nothing
    // but "the file is there".
    const options_install = b.addInstallArtifact(options_bridge, .{});
    const sdl_install = b.addInstallArtifact(sdl_dynamic, .{});
    const platform_install = b.addInstallArtifact(platform_runtime, .{});
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.addArg(module_root);
    run.step.dependOn(&streamio_install.step);
    run.step.dependOn(&sdl_install.step);
    run.step.dependOn(&scratch_install.step);
    run.step.dependOn(&platform_install.step);
    run.step.dependOn(&options_install.step);
    // On Windows a DLL's imports resolve from the executable's directory and
    // PATH, and this executable runs out of the build cache - so the installed
    // runtime DLLs have to be findable. Without this the process died before
    // main with exit code 53, which is STATUS_DLL_NOT_FOUND (0xC0000135)
    // truncated the way Zig reports Windows crash codes. ELF and Mach-O carry
    // loader-relative rpaths and do not need it.
    run.addPathDir(b.path("zig-out/bin").getPath(b));
    const step = b.step("test-map-files", "Read and rewrite the shipped maps; check they are unchanged");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);

    const run_all = b.addRunArtifact(exe);
    run_all.setCwd(b.path("."));
    run_all.addArg(module_root);
    run_all.addArg("--all");
    run_all.step.dependOn(&streamio_install.step);
    run_all.step.dependOn(&sdl_install.step);
    run_all.step.dependOn(&scratch_install.step);
    run_all.step.dependOn(&platform_install.step);
    run_all.step.dependOn(&options_install.step);
    run_all.addPathDir(b.path("zig-out/bin").getPath(b));
    const step_all = b.step("test-map-files-all", "Sweep every shipped map, not just the CI sample");
    step_all.dependOn(&exe.step);
    if (test_mode == .run) step_all.dependOn(&run_all.step);

    // 04-13 (D-25 item 4): one record edit of each M2 collection a shipped map
    // has, undone, must write the unedited file byte for byte. Local, like
    // test-map-files-all: not in the default test step or CI.
    const run_m2_sweep = b.addRunArtifact(exe);
    run_m2_sweep.setCwd(b.path("."));
    run_m2_sweep.addArg(module_root);
    run_m2_sweep.addArg("--m2-sweep");
    run_m2_sweep.step.dependOn(&streamio_install.step);
    run_m2_sweep.step.dependOn(&sdl_install.step);
    run_m2_sweep.step.dependOn(&scratch_install.step);
    run_m2_sweep.step.dependOn(&platform_install.step);
    run_m2_sweep.step.dependOn(&options_install.step);
    run_m2_sweep.addPathDir(b.path("zig-out/bin").getPath(b));
    const step_m2_sweep = b.step("test-map-files-m2-sweep", "Edit every M2 collection of every Data/Maps map, undo it, and compare the bytes");
    step_m2_sweep.dependOn(&exe.step);
    if (test_mode == .run) step_m2_sweep.dependOn(&run_m2_sweep.step);
}

fn addEditorBridgeTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    // install-game stages every shared library the engine needs - StreamIO,
    // StreamIOOptionsAbi, PlatformRuntime, SDL3 and the rest - so the run step
    // installs none of them itself. The map file tier has to; it runs from the
    // build cache and there is nothing staged for it.
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
    // 03-08 Task 1: the fixture mod TestModsListSetAndClear needs, staged
    // beside the test binary - never a dependency of install_exe/install-game
    // itself (see its own doc comment at the call site).
    install_fixture_mod_step: *std.Build.Step,
) void {
    // The recipe of gfxgpu-factory-test, which is the C++ executable this
    // repository already runs on Linux CI. See the note in addMapFileTest.
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Formats"));
    module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{"tools/zig/editor_bridge_test.cpp"},
        .flags = cppflagsForOptimize(optimize),
    });
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    linkMsvcRuntime(module, optimize);
    // This executable hosts the same engine the game does, so on Windows it
    // needs the same imports the game executable links. Discovering them one
    // missing symbol at a time costs a CI round each: COM through _com_util and
    // _variant_t (Platform/LegacyVariant.h via Initialization.cpp) brought
    // VariantClear, _com_issue_error, CoCreateGuid and SysAllocString; the
    // version information in Misc brought GetFileVersionInfoSizeA,
    // GetFileVersionInfoA and VerQueryValueA. The list is addGame's, minus the
    // splash-screen resources, which a test has no window to show.
    if (target.result.os.tag == .windows) {
        linkComSupport(module, optimize);
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        module.linkSystemLibrary("shlwapi", .{});
        module.linkSystemLibrary("advapi32", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("gdi32", .{});
        module.linkSystemLibrary("shell32", .{});
    }
    module.linkLibrary(editor_bridge);
    module.linkLibrary(map_file);
    module.linkLibrary(main_lib);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    // Main brings Lua (the Script class) and zlib with it, the way addGame does.
    module.linkLibrary(lualib);
    module.linkLibrary(zlib);
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);

    const exe = b.addExecutable(.{ .name = "editor-bridge-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    // Engine modules resolve RTTI and the host globals from the executable,
    // as they do from Game and MapEditor.
    if (target.result.os.tag == .linux) exe.rdynamic = true;
    // Loader-relative, because this binary runs from the installation and not
    // from the build cache its link-time rpath points at.
    switch (target.result.os.tag) {
        .macos => exe.root_module.addRPathSpecial("@executable_path"),
        .linux => exe.root_module.addRPathSpecial("$ORIGIN"),
        else => {},
    }

    // Staged beside Game rather than added to the shipped file list: every
    // engine module derives its roots from the running executable's location,
    // so this has to live in the installation it starts - but a test binary has
    // no business in a release layout or a package, so it is installed on its
    // own and stage.zig never hears about it.
    const stage_suffix = stage_root["zig-out/".len..];
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);

    const run = b.addRunArtifact(exe);
    // Run from the installation, and tell it so: cwd is what the engine's
    // relative data names resolve against.
    run.setCwd(b.path(stage_root));
    run.addArg(".");
    // Where the test may write. Shipped Data is read-only for every tier: a run
    // that is killed halfway must not leave a map behind in the installation.
    run.addArg(b.pathFromRoot("zig-out/local-test"));
    // It reads the staged Data and engine, neither a file input of this step,
    // so a cached pass would say nothing about the installation now (the
    // same reason map-editor-smoke and test-map-editor-engine set this).
    run.has_side_effects = true;
    run.step.dependOn(&install_exe.step);
    run.step.dependOn(install_fixture_mod_step);
    const step = b.step("test-editor-bridge", "Open maps through the engine and check what it saves");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);

    // 04-13 (D-25 item 4): bridges, fences, entrenchments and the cascade
    // delete on every Data/Maps map, undone, must save the unedited bytes.
    // Local only, like test-map-files-m2-sweep: not in the default test step
    // or CI.
    const run_m2_sweep = b.addRunArtifact(exe);
    run_m2_sweep.setCwd(b.path(stage_root));
    run_m2_sweep.addArg(".");
    run_m2_sweep.addArg(b.pathFromRoot("zig-out/local-test"));
    run_m2_sweep.addArg("--m2-sweep");
    run_m2_sweep.has_side_effects = true;
    run_m2_sweep.step.dependOn(&install_exe.step);
    run_m2_sweep.step.dependOn(install_fixture_mod_step);
    const step_m2_sweep = b.step("test-editor-bridge-m2-sweep", "Draw, delete and undo bridges, fences, entrenchments and cascades on every Data/Maps map through the engine and compare the bytes");
    step_m2_sweep.dependOn(&exe.step);
    if (test_mode == .run) step_m2_sweep.dependOn(&run_m2_sweep.step);

    // 05-05: the players, the unit creation and Check Map's fixes through the engine,
    // alone (the full tier above runs them too, among everything else).
    const run_m3_players = b.addRunArtifact(exe);
    run_m3_players.setCwd(b.path(stage_root));
    run_m3_players.addArg(".");
    run_m3_players.addArg(b.pathFromRoot("zig-out/local-test"));
    run_m3_players.addArg("--m3-players-only");
    run_m3_players.has_side_effects = true;
    run_m3_players.step.dependOn(&install_exe.step);
    const step_m3_players = b.step("test-editor-bridge-m3-players", "Add and delete players, edit the unit creation and apply Check Map's fixes through the engine and compare the bytes");
    step_m3_players.dependOn(&exe.step);
    if (test_mode == .run) step_m3_players.dependOn(&run_m3_players.step);

    // 05-07: the Minimap panel's reads and Create Minimap Images through the engine,
    // alone (the full tier above runs them too, among everything else).
    const run_m3_minimap = b.addRunArtifact(exe);
    run_m3_minimap.setCwd(b.path(stage_root));
    run_m3_minimap.addArg(".");
    run_m3_minimap.addArg(b.pathFromRoot("zig-out/local-test"));
    run_m3_minimap.addArg("--m3-minimap-only");
    run_m3_minimap.has_side_effects = true;
    run_m3_minimap.step.dependOn(&install_exe.step);
    const step_m3_minimap = b.step("test-editor-bridge-m3-minimap", "Read the minimap's tiles, colours and markers and create its pictures through the engine and check them");
    step_m3_minimap.dependOn(&exe.step);
    if (test_mode == .run) step_m3_minimap.dependOn(&run_m3_minimap.step);

    // 05-08: Create Random Map through the engine's own generator, alone (the full
    // tier above runs it too, among everything else): the refusals, the seed, the
    // output folders, the mod stamp and the generated map's open.
    const run_m3_rmg = b.addRunArtifact(exe);
    run_m3_rmg.setCwd(b.path(stage_root));
    run_m3_rmg.addArg(".");
    run_m3_rmg.addArg(b.pathFromRoot("zig-out/local-test"));
    run_m3_rmg.addArg("--m3-rmg-only");
    run_m3_rmg.has_side_effects = true;
    run_m3_rmg.step.dependOn(&install_exe.step);
    run_m3_rmg.step.dependOn(install_fixture_mod_step);
    const step_m3_rmg = b.step("test-editor-bridge-m3-rmg", "Generate random maps through the engine with seeds and check the files, the refusals and the mod stamp");
    step_m3_rmg.dependOn(&exe.step);
    if (test_mode == .run) step_m3_rmg.dependOn(&run_m3_rmg.step);

    // 05-06: the Layers menu's probe (what each layer does in this renderer) and the
    // layer entries through the engine, alone (the full tier above runs them too).
    const run_m3_layers = b.addRunArtifact(exe);
    run_m3_layers.setCwd(b.path(stage_root));
    run_m3_layers.addArg(".");
    run_m3_layers.addArg(b.pathFromRoot("zig-out/local-test"));
    run_m3_layers.addArg("--m3-layers-only");
    run_m3_layers.has_side_effects = true;
    run_m3_layers.step.dependOn(&install_exe.step);
    const step_m3_layers = b.step("test-editor-bridge-m3-layers", "Measure what each Layers menu toggle does in the renderer and drive them through the engine");
    step_m3_layers.dependOn(&exe.step);
    if (test_mode == .run) step_m3_layers.dependOn(&run_m3_layers.step);

    // 05-05: the fixture maps the Check Map scenario and the railroad guard's game
    // proof open. `--craft <kind> <file>` writes one under zig-out/local-test
    // (CraftFixture in editor_bridge_test.cpp); the scenario steps depend on this
    // step. The two crafts run one after the other, so two engines never start at once.
    const craft_step = b.step("editor-craft-fixtures", "Craft the Check Map and short-railroad fixture maps under zig-out/local-test");
    const craft_kinds = [_][2][]const u8{
        .{ "short-railroad", "m3-short-railroad.bzm" },
        .{ "check-map", "m3-check-map.bzm" },
    };
    var previous_craft: ?*std.Build.Step = null;
    for (craft_kinds) |entry| {
        const run_craft = b.addRunArtifact(exe);
        run_craft.setCwd(b.path(stage_root));
        run_craft.addArg(".");
        run_craft.addArg(b.pathFromRoot("zig-out/local-test"));
        run_craft.addArg("--craft");
        run_craft.addArg(entry[0]);
        run_craft.addArg(entry[1]);
        run_craft.has_side_effects = true;
        run_craft.step.dependOn(&install_exe.step);
        if (previous_craft) |earlier| run_craft.step.dependOn(earlier);
        previous_craft = &run_craft.step;
        if (test_mode == .run) craft_step.dependOn(&run_craft.step);
    }
}

// S04 T01: the resource bridge's smoke tier. One executable linking the same
// static libraries test-editor-bridge links (the "five engine targets" the
// slice plan names: EditorBridge, MapFile, Main, RandomMapGen, Formats, Misc,
// PlatformRuntime and SDL - a bake of the game's engine), staged beside Game
// so NPlatform::Paths derives the right roots on every runner. On a host
// without a GPU (CI's Linux runner, three of the six) the executable prints
// "skipped: no GPU device" and exits 0 - the same gate editor_bridge_test
// uses, with BK_REQUIRE_ENGINE=1 turning a skip into a failure on the
// runners that do have a device (so a regression there cannot hide as a
// skip). The step gates test_mode == .run, so the configure-only build
// (zig build) compiles the executable but does not run it.
/// The D-11 comparator as a static library, for the engine-hosted tiers that
/// compare an export with a shipped file (test-resource-bridge's round trips).
/// It is compiled apart from them because its Scene sources take their StdAfx.h
/// from the Scene folder, which the tiers' own include order would not give.
fn addResourceComparatorLib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    module.addIncludePath(b.path("Sources/src/StreamIO"));
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/ResourceModel/comparator.cpp",
            "Sources/src/ResourceModel/dxt_gate.cpp",
            "Sources/src/Image/DxtCodec.cpp",
            "Sources/src/Scene/ParticleSourceData.cpp",
            "Sources/src/Scene/SmokinParticleSourceData.cpp",
            "Sources/src/Scene/Track.cpp",
        },
        .flags = cppflagsForOptimize(optimize),
    });
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMacosSysrootPaths(b, module, target);
    return b.addLibrary(.{ .name = "ResourceComparator", .linkage = .static, .root_module = module });
}

fn addResourceBridge(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    comparator_lib: *std.Build.Step.Compile,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Formats"));
    module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    module.addIncludePath(b.path("Sources/src/EditorBridge"));
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{"Sources/src/EditorBridge/resource_bridge_test.cpp"},
        .flags = cppflagsForOptimize(optimize),
    });
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    linkMsvcRuntime(module, optimize);
    if (target.result.os.tag == .windows) {
        linkComSupport(module, optimize);
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        module.linkSystemLibrary("shlwapi", .{});
        module.linkSystemLibrary("advapi32", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("gdi32", .{});
        module.linkSystemLibrary("shell32", .{});
    }
    module.linkLibrary(comparator_lib);
    module.linkLibrary(editor_bridge);
    module.linkLibrary(map_file);
    module.linkLibrary(main_lib);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    module.linkLibrary(lualib);
    module.linkLibrary(zlib);
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);

    const exe = b.addExecutable(.{ .name = "resource-bridge-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    // Linux loader pitfalls the Map Editor already solved (AGENTS.md):
    // rdynamic so engine modules resolve host RTTI and $ORIGIN rpath so the
    // staged binary finds libStreamIO etc. beside itself.
    if (target.result.os.tag == .linux) exe.rdynamic = true;
    switch (target.result.os.tag) {
        .macos => exe.root_module.addRPathSpecial("@executable_path"),
        .linux => exe.root_module.addRPathSpecial("$ORIGIN"),
        else => {},
    }

    const stage_suffix = stage_root["zig-out/".len..];
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);

    const run = b.addRunArtifact(exe);
    run.setCwd(b.path(stage_root));
    run.addArg(".");
    // T02: the Project+Tree sub-step needs the 21 fixture folders. The source
    // dir of the fixture tree is handed as argv[2]; the test writes its
    // temporary round-trip copies under argv[3] (zig-out/local-test/...).
    run.addArg(b.path("tools/zig/fixtures/resource_editor").getPath(b));
    run.addArg(b.path("zig-out/local-test/resource_editor/t02").getPath(b));
    // Reads the staged Data and modules, neither a file input of this step:
    // a cached pass would say nothing about the installation now.
    run.has_side_effects = true;
    run.step.dependOn(&install_exe.step);
    const step = b.step("test-resource-bridge", "Drive the resource bridge through the engine: every fixture, geometry, export, references and the preview captures");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

/// The static libraries MapEditor's executables link: the engine
/// addEditorBridgeTest hosts, with the bridge in front.
const MapEditorEngine = struct {
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
};

/// What addMapEditor hands back: the MapEditor executable (the package
/// steps stage its exact binary) and view.zig's test step, which
/// test-map-editor-view depends on.
const MapEditorBuild = struct {
    exe: *std.Build.Step.Compile,
    view_test_step: *std.Build.Step,
};

fn addMapEditor(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_imgui_module: *std.Build.Module,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
    // 03-08 Task 1: the fixture mod the -mod=EditorTestMod host-check run
    // below needs - never a dependency of install_exe/install-map-editor.
    install_fixture_mod_step: *std.Build.Step,
) MapEditorBuild {
    const engine: MapEditorEngine = .{
        .editor_bridge = editor_bridge,
        .map_file = map_file,
        .formats = formats,
        .randommapgen = randommapgen,
        .misc = misc,
        .main_lib = main_lib,
        .lualib = lualib,
        .zlib = zlib,
        .platform_runtime = platform_runtime,
        .sdl_dynamic = sdl_dynamic,
    };
    const app_kit = editorAppKit(b, target, optimize, toolchain, editor_imgui_module, sdl_include);
    const sdl_module = app_kit.sdl;
    const kit_module = app_kit.kit;
    // The editor core for the app's target. The core tier's module
    // (test-editor-core) is built for the host only. The core imports the
    // kit so later S02 tasks can re-point core submodules at the kit.
    const core_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/core/root.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "editor_kit", .module = kit_module }},
    });
    const stage_suffix = stage_root["zig-out/".len..];

    // view.zig's own tests (WINDOWS.md 2): real SDL events through the view's
    // wiring, the tools editing the core's fake bridge and the camera a fake
    // of the bridge's. The app's SDL module gives the event types and
    // constants; no SDL function is referenced, so none is linked. The core
    // is this target's, and editor_imgui a stand-in nothing the tests reach
    // uses. No engine, GPU or staged installation.
    const view_imgui_stub = b.createModule(.{
        .root_source_file = b.path("Sources/editor/app/testing/imgui_stub.zig"),
        .target = target,
        .optimize = .Debug,
    });
    const view_test_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/app/view.zig"),
        .target = target,
        .optimize = .Debug,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl_module },
            .{ .name = "editor_core", .module = core_module },
            .{ .name = "editor_kit", .module = kit_module },
            .{ .name = "editor_imgui", .module = view_imgui_stub },
        },
    });
    const view_tests = b.addTest(.{ .name = "map-editor-view-test", .root_module = view_test_module });
    const view_tests_run = b.addRunArtifact(view_tests);
    const view_test_step = b.step("test-map-editor-view-events", "Run view.zig's SDL event-wiring tests against fake bridges");
    view_test_step.dependOn(&view_tests.step);
    if (test_mode == .run) view_test_step.dependOn(&view_tests_run.step);

    const module = mapEditorModule(b, "Sources/editor/app/main.zig", target, optimize, toolchain, sdl_module, editor_imgui_module, core_module, kit_module, engine);
    const exe = b.addExecutable(.{ .name = "MapEditor", .root_module = module });
    // .windows: the packaged, player-facing binary opens no console on a
    // normal double-click (crt.attachParentConsole in main.zig keeps its
    // automated modes printing).
    configureMapEditorExecutable(exe, target, .windows);

    // Beside Game, because the engine's roots are the installation it runs in.
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);
    const install_step = b.step("install-map-editor", "Install MapEditor into the game installation");
    install_step.dependOn(&install_exe.step);

    // Launched from zig-out, not the installation: MapEditor finds its
    // modules, Data, SeasonData and Shaders beside its own executable
    // whatever the working directory is (a Windows shortcut, the Start menu
    // or Explorer start it elsewhere), and a relative map on the command line
    // is relative to where it was launched - here the stage path from zig-out,
    // in the engine's backslash form after it. Not the build root: on macOS
    // every Mach-O the build links carries Zig's cwd-relative build-cache
    // rpaths ahead of @executable_path, so from there
    // dyld loads a second libSDL3/libPlatformRuntime out of the cache
    // (03-16-SUMMARY.md) - a build-root-only quirk this check is not about.
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("zig-out"));
    run.addArgs(&.{ "--check", b.fmt("{s}\\Data\\Maps\\Multiplayer\\coldwinter.bzm", .{stage_suffix}), b.pathFromRoot("zig-out/local-test/map-editor-check.tga") });
    run.step.dependOn(&install_exe.step);
    const check_step = b.step("map-editor-host-check", "Start MapEditor on a shipped map and check ImGui draws over the engine's frame");
    // Only builds and installs the step in .compile mode, as test-editor-bridge
    // (addEditorBridgeTest) does - never runs a GPU-needing executable when the
    // caller only wants to know it compiles.
    check_step.dependOn(&install_exe.step);
    // The same map as an absolute path in the host's own form, as a person
    // types it or a shell expands it: forward slashes on macOS, which the
    // engine's file layer does not split on until MapEditor converts them.
    // Also launched from outside the installation, like `run` above; the
    // -mod= run below keeps the launch from inside it covered.
    const absolute_run = b.addRunArtifact(exe);
    absolute_run.setCwd(b.path("zig-out"));
    absolute_run.addArgs(&.{ "--check", b.pathFromRoot(b.fmt("{s}/Data/Maps/Multiplayer/coldwinter.bzm", .{stage_root})), b.pathFromRoot("zig-out/local-test/map-editor-check-absolute.tga") });
    absolute_run.step.dependOn(&install_exe.step);
    // After the relative run, so two engines never start at once.
    absolute_run.step.dependOn(&run.step);

    // 03-08 Task 1: -mod=EditorTestMod loads the fixture mod's data like the
    // game and the host check's own acceptance criterion greps this line.
    const mod_run = b.addRunArtifact(exe);
    mod_run.setCwd(b.path(stage_root));
    mod_run.addArgs(&.{ "-mod=EditorTestMod", "--check", "Data\\Maps\\Multiplayer\\coldwinter.bzm", b.pathFromRoot("zig-out/local-test/map-editor-check-mod.tga") });
    mod_run.step.dependOn(&install_exe.step);
    mod_run.step.dependOn(install_fixture_mod_step);
    // After the absolute-path run, so two engines never start at once.
    mod_run.step.dependOn(&absolute_run.step);
    if (test_mode == .run) {
        check_step.dependOn(&run.step);
        check_step.dependOn(&absolute_run.step);
        check_step.dependOn(&mod_run.step);
    }

    // The interactive loop itself, hidden and driven by smoke.zig's scripted
    // SDL events: paint, place, select, drag, turn, delete, undo all of it,
    // Save As and reopen.
    const smoke_run = b.addRunArtifact(exe);
    smoke_run.setCwd(b.path(stage_root));
    smoke_run.addArgs(&.{ "--smoke", "Data\\Maps\\Multiplayer\\coldwinter.bzm", b.pathFromRoot("zig-out/local-test/map-editor-smoke.bzm") });
    // What it reads - the staged Data and engine - is not a file input of the
    // step, so a cached pass would say nothing about the installation now.
    smoke_run.has_side_effects = true;
    smoke_run.step.dependOn(&install_exe.step);
    // D-26 (revised 2026-09-29): the script's last steps switch to the
    // fixture mod and back, closing the map - staged here, never a
    // dependency of install-map-editor.
    smoke_run.step.dependOn(install_fixture_mod_step);
    const smoke_step = b.step("map-editor-smoke", "Run MapEditor's interactive loop hidden under a scripted smoke on a shipped map");
    smoke_step.dependOn(&smoke_run.step);

    // The spec's editor-app tier (03-12-PLAN.md), one local command: paint,
    // place, save as a new map, shoot and compare the frame against a local
    // (never committed) reference, test-launch the game and wait for it to
    // exit 0, then quit. `--hidden`, like `--smoke`: the same loop, not a
    // person watching it. It starts a second engine process end to end, like
    // map-editor-game-reads-it. CI runs it on the two GPU runners (Windows,
    // macos-14) through map-editor-m3-auto, which depends on it (05-REVIEW WR-D05).
    const auto_dir = b.pathFromRoot("zig-out/local-test/map-editor-auto");
    const auto_saveas_path = b.fmt("{s}/auto.bzm", .{auto_dir});
    // Coordinates match smoke.zig's own script exactly (ground_a/
    // ground_between/ground_b, place_at): the same shipped map, the same
    // screen-centre-relative convention, so both scripts paint/place on the
    // same known-clear ground. BK_EDITOR_AUTO_GAME becomes the test game's
    // own BK_AUTO_UI: a shot (for a person to look at, unmeasured) then exit.
    const auto_run = b.addRunArtifact(exe);
    auto_run.setCwd(b.path(stage_root));
    auto_run.addArgs(&.{ "--hidden", "Data\\Maps\\Multiplayer\\coldwinter.bzm" });
    auto_run.setEnvironmentVariable("BK_EDITOR_AUTO_DIR", auto_dir);
    auto_run.setEnvironmentVariable("BK_EDITOR_AUTO", b.fmt(
        "3:key=2,4:press=c-120x-160,5:drag=c-95x-160,6:drag=c-70x-160,7:release=c-70x-160,8:key=3,9:click=c-40x-120,10:saveas={s},11:shot=edited,12:compare=edited,13:test,14:waitgame=240,15:exit",
        .{auto_saveas_path},
    ));
    auto_run.setEnvironmentVariable("BK_EDITOR_AUTO_GAME", "400:shot,440:exit");
    // What it reads - the staged Data and engine - is not a file input of the
    // step, so a cached pass would say nothing about the installation now.
    auto_run.has_side_effects = true;
    auto_run.step.dependOn(&install_exe.step);
    // After the smoke run, so two engines never start at once.
    auto_run.step.dependOn(&smoke_run.step);
    // Test in game (the schedule's own `test` action) leaves the game's own
    // screenshot dump in the stage root it ran from (main.zig's own
    // deleteAutoshots comment, --game-reads-it's analogous cleanup) - swept
    // up so a repeat run is not mistaken for a stale leftover. A Zig
    // artifact process (addRunArtifact), not an external interpreter -
    // build_hermeticity_test.zig forbids this file spawning one of those.
    const delete_matching_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/delete_matching_files.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const delete_matching = b.addExecutable(.{ .name = "delete-matching-files", .root_module = delete_matching_module });
    const cleanup_autoshots = b.addRunArtifact(delete_matching);
    cleanup_autoshots.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup_autoshots.step.dependOn(&auto_run.step);
    const auto_step = b.step("map-editor-auto", "Run BK_EDITOR_AUTO's editor-app scenario on a shipped map");
    auto_step.dependOn(&cleanup_autoshots.step);

    // Phase 4's M2 scenario (04-03): the same loop and the same shipped map,
    // scripted with the named commands and predicates (do=/expect=) instead
    // of coordinates, so each M2 plan appends its own segment here. The
    // schedule is a Zig array of entries joined with commas (research Q5) -
    // frames ascending, one entry per line, so a later plan's segment is a
    // block of lines and never an edit of one long string. No `test` action:
    // the M2 game-reads-it checks belong to the plans that add them.
    const auto_m2_dir = b.pathFromRoot("zig-out/local-test/map-editor-auto-m2");
    // 04-13: the copy-along leg's second folder, and the Lua fixture named
    // relative to the editor's working directory (the stage root): a do=
    // argument is at most 64 characters, which an absolute path is not.
    const auto_m2_along_dir = b.pathFromRoot("zig-out/local-test/map-editor-auto-m2-along");
    const auto_m2_fixture = fixture: {
        var up: std.ArrayListUnmanaged(u8) = .empty;
        for (0..std.mem.count(u8, stage_root, "/") + 1) |_| up.appendSlice(b.allocator, "../") catch @panic("OOM");
        up.appendSlice(b.allocator, "tools/zig/fixtures/m2_script.lua") catch @panic("OOM");
        break :fixture up.items;
    };
    const auto_m2_entries = [_][]const u8{
        // 04-03: camera anchors, the tracer. coldwinter already holds anchors
        // for players 0-3, so the segment works on player 4 and the neutral
        // anchor, both unset there: set player 4 at the view centre, undo it
        // (unset again), redo it, set the neutral one, go to player 0's
        // anchor, clear the neutral one, then save and shoot the markers.
        "3:do=camera_player:4",
        "4:expect=anchor_set:4",
        "5:expect=undo_depth:1",
        "6:key=Z+ctrl",
        "8:expect=anchor_unset:4",
        "9:key=Y+ctrl",
        "11:expect=anchor_set:4",
        "12:expect=undo_depth:1",
        "13:do=camera_neutral",
        "14:expect=anchor_set:neutral",
        "15:expect=undo_depth:2",
        "16:do=camera_goto:0",
        "17:do=camera_clear:neutral",
        "18:expect=anchor_unset:neutral",
        "19:expect=undo_depth:3",
        // 04-03 Task 3: the gestures and keys reach Select, which ignores them
        // (a tool that takes no right button or double click is not even
        // handed them), so nothing new is recorded. `text=` has no focused
        // field to reach here; it only proves the event is pushed and drawn
        // past without a failure.
        "20:tool=select",
        "21:rclick=c0x0",
        "22:dblclick=c10x10",
        "23:key=INSERT",
        "24:key=ESCAPE",
        "25:text=Area1",
        "27:expect=undo_depth:3",
        // 04-05: Roads & Rivers, on the empty snow above the view centre (the
        // camera stands at player 0's anchor after camera_goto:0). A road of
        // three clicks finished by a double click; a drag of its second
        // point, undone; Insert after that point (4 points), undone; then a
        // river of three clicks finished by Enter and a right-drag of 50
        // pixels down on its second point (opacity 1 -> 0.5); the markers
        // shot. These drags keep their press, motions and release in one
        // frame as 04-05 wrote them; since 04-06 a scripted press is held
        // across frames until its release (View.holdScripted), so a drag may
        // also span frames.
        "28:tool=roads_rivers",
        "29:do=vso_kind:road",
        "30:do=vso_width:3",
        "31:do=vso_opacity:100",
        "32:click=c-100x-250",
        "33:click=c50x-200",
        "34:dblclick=c200x-250",
        "36:expect=vso_delta:road:1",
        "37:expect=vso_points:road:3",
        "38:expect=undo_depth:4",
        "39:press=c50x-200",
        "39:drag=c50x-175",
        "39:drag=c50x-150",
        "39:release=c50x-150",
        "41:expect=undo_depth:5",
        "42:key=Z+ctrl",
        "44:expect=vso_delta:road:1",
        "45:expect=undo_depth:4",
        "46:key=INSERT",
        "48:expect=vso_points:road:4",
        "49:key=Z+ctrl",
        "51:expect=vso_points:road:3",
        "52:do=vso_kind:river",
        "53:click=c-260x-320",
        "54:click=c-230x-250",
        "55:click=c-250x-170",
        "56:key=ENTER",
        "58:expect=vso_delta:river:1",
        "59:rpress=c-230x-250",
        "59:rdrag=c-230x-225",
        "59:rdrag=c-230x-200",
        "59:rrelease=c-230x-200",
        "61:expect=undo_depth:6",
        // 04-13: in the width mode All the panel's width re-widths the selected
        // line (the river just finished) as one undo step (the MFC editor's
        // CW_ALL); undone, and the panel back to single and 3 tiles.
        "62:do=vso_width_mode:all",
        "62:do=vso_width:5",
        "63:expect=undo_depth:7",
        "63:key=Z+ctrl",
        "65:expect=undo_depth:6",
        "65:do=vso_width_mode:single",
        "65:do=vso_width:3",
        "66:shot=m2_roads",
        "66:compare=m2_roads",
        // 04-06: the Bridge tool (key 5) with W_WoodenBig_Heavy_01, dragged
        // along the world's x axis (down and to the right on screen, 2:1) over
        // the empty snow right of the view centre, between the new road and
        // the tanks below (the engine refuses a span on another object), the
        // press, the motions and the release in separate
        // frames (a scripted press is held until its release since 04-06):
        // one new bridges entry, selected; E rotates it to _02, Enter makes it
        // built during play; two undos and two redos walk both back and
        // forth; the bridge, its outline and its mark shot.
        "67:tool=bridge",
        "68:do=bridge_desc:W_WoodenBig_Heavy_01",
        "70:press=c-20x-190",
        "71:drag=c100x-130",
        "72:drag=c200x-80",
        "73:release=c260x-50",
        "75:expect=bridge_delta:1",
        "76:expect=undo_depth:7",
        "77:key=E",
        "79:expect=undo_depth:8",
        "80:key=ENTER",
        "82:expect=bridge_built",
        "83:expect=undo_depth:9",
        "84:key=Z+ctrl",
        "86:key=Z+ctrl",
        "88:expect=undo_depth:7",
        "89:key=Y+ctrl",
        "91:key=Y+ctrl",
        "93:expect=bridge_built",
        "94:expect=bridge_delta:1",
        "96:shot=m2_bridges",
        "96:compare=m2_bridges",
        // 04-07: the Fence tool (key 6) with W_FactoryFence, dragged along the
        // world's x axis on the snow between the bridge and the tanks: the
        // ghost is shot while the button is held (a scripted press is held
        // across frames), the release places one run as one undo step, undo
        // and redo walk it back and forth, the run is shot.
        "97:tool=fence",
        "98:do=fence_desc:W_FactoryFence",
        "100:press=c80x-65",
        "101:drag=c140x-35",
        "102:drag=c200x-5",
        "103:shot=m2_fence_ghost",
        "103:compare=m2_fence_ghost",
        "104:drag=c240x15",
        "105:release=c240x15",
        "107:expect=fence_delta:6",
        "108:expect=undo_depth:10",
        "109:key=Z+ctrl",
        "111:expect=fence_delta:0",
        "112:expect=undo_depth:9",
        "113:key=Y+ctrl",
        "115:expect=fence_delta:6",
        "116:expect=undo_depth:10",
        "118:shot=m2_fences",
        "118:compare=m2_fences",
        // 04-08: the Entrenchment tool (key 7) for player 0, an L of three
        // clicks on the snow left of the view centre, below the river: two
        // clicks, the pointer moved to the third point (the preview is shot
        // there), then the double click's own first click adds that point and
        // commits one entrenchment as one undo step; undo and redo walk it
        // back and forth; the trench is shot, selected.
        "119:tool=entrenchment",
        "120:do=trench_player:0",
        "121:click=c-330x-60",
        "122:click=c-170x-10",
        "123:drag=c-150x80",
        "125:shot=m2_trench_preview",
        "125:compare=m2_trench_preview",
        "126:dblclick=c-150x80",
        "128:expect=trench_delta:1",
        "129:expect=undo_depth:11",
        "130:key=Z+ctrl",
        "132:expect=trench_delta:0",
        "133:expect=undo_depth:10",
        "134:key=Y+ctrl",
        "136:expect=trench_delta:1",
        "137:expect=undo_depth:11",
        "139:shot=m2_trench",
        "139:compare=m2_trench",
        // 04-09: script IDs and reinforcement groups (coldwinter holds no
        // group, so New from 0 takes group 0). The Select tool clicks a tank of
        // the rows below the fence; the script_id command gives it 4244 and the
        // predicate reads it back; a new group takes 4244; Select objects
        // outlines the tank (shot); hiding the group hides that one object (the
        // status bar counts it, the Groups window is shot with the hidden tank);
        // unhiding, Remove and Delete are each one step and are undone; three
        // more undos walk everything back (group script ID, group, script ID),
        // and the window is shot again.
        "140:tool=select",
        "141:click=c-200x150",
        "143:do=script_id:4244",
        "144:expect=script_id:4244",
        "145:expect=undo_depth:12",
        "146:do=group_new:0",
        "147:expect=groups_delta:1",
        "148:expect=undo_depth:13",
        "149:do=group_add_id:0:4244",
        "150:expect=group_has:0:4244",
        "151:expect=undo_depth:14",
        "152:do=group_select:0",
        "154:shot=m2_groups_marked",
        "154:compare=m2_groups_marked",
        "155:do=group_hide:0:1",
        "156:expect=hidden_count:1",
        "157:do=groups_window:1",
        "160:shot=m2_groups_hidden",
        "160:compare=m2_groups_hidden",
        "161:do=group_hide:0:0",
        "162:expect=hidden_count:0",
        "163:do=group_remove_id:0:4244",
        "164:expect=undo_depth:15",
        "165:do=group_delete:0",
        "166:expect=groups_delta:0",
        "167:expect=undo_depth:16",
        "168:key=Z+ctrl",
        "170:expect=groups_delta:1",
        "171:key=Z+ctrl",
        "173:expect=group_has:0:4244",
        "174:expect=undo_depth:14",
        "175:key=Z+ctrl",
        "177:key=Z+ctrl",
        "179:key=Z+ctrl",
        "181:expect=groups_delta:0",
        "182:expect=undo_depth:11",
        "185:shot=m2_groups",
        "185:compare=m2_groups",
        // 04-10: script areas (key 8) and the script file. A rectangle named
        // m2_area is dragged, a circle named m2_ring after it (the drag's press,
        // motions and release in separate frames); the map now holds two areas
        // more. A click inside the rectangle selects it and a drag from its
        // centre handle moves it (one undo step, undone by the key); Rename gives
        // it m2_zone and the second undo takes that back. The map's script file is
        // named m2_script (a predicate reads it back), the Script window is opened
        // and everything is shot.
        "186:tool=script_areas",
        "187:do=area_shape:rect",
        "188:do=area_name:m2_area",
        "190:press=c120x120",
        "191:drag=c190x160",
        "192:drag=c260x200",
        "193:release=c260x200",
        "195:expect=area_named:m2_area",
        "196:expect=areas_delta:1",
        "197:expect=undo_depth:12",
        "198:do=area_shape:circle",
        "199:do=area_name:m2_ring",
        "201:press=c-120x210",
        "202:drag=c-90x225",
        "203:release=c-60x240",
        "205:expect=area_named:m2_ring",
        "206:expect=areas_delta:2",
        "207:expect=undo_depth:13",
        "208:click=c190x160",
        "210:press=c190x160",
        "211:drag=c205x170",
        "212:drag=c220x180",
        "213:release=c220x180",
        "215:expect=undo_depth:14",
        "216:key=Z+ctrl",
        "218:expect=undo_depth:13",
        "219:do=area_rename:0:m2_zone",
        "220:expect=area_named:m2_zone",
        "221:expect=undo_depth:14",
        "222:key=Z+ctrl",
        "224:expect=area_named:m2_area",
        "225:expect=undo_depth:13",
        "226:do=script_file:m2_script",
        "227:expect=script_file:m2_script",
        "228:expect=undo_depth:14",
        "229:do=script_dialog:1",
        "232:shot=m2_areas",
        "232:compare=m2_areas",
        "233:do=script_dialog:0",
        "233:do=groups_window:0",
        // 04-11: start commands. The Select tool clicks the tank the 04-09 segment
        // used; Unit > Add start command (the named command) makes a STOP command
        // for it, selected in the Start Commands window (coldwinter holds none, so
        // it is command 0); its type becomes MOVE_TO and its number 2.5, and the
        // view centre its target (a point; the red line from the tank to it is
        // shot with the window closed, then the window with it open); undo and
        // redo walk the target back and forth; Set target puts the Start Target
        // tool in hand for one click and it hands the Select tool back.
        "234:tool=select",
        "235:click=c-200x150",
        "237:do=startcmd_add",
        "238:expect=startcmds_delta:1",
        "239:expect=startcmd_units:0:1",
        "240:expect=undo_depth:15",
        "241:do=startcmd_type:MOVE_TO",
        "242:expect=startcmd_is:0:MOVE_TO",
        "243:expect=undo_depth:16",
        "244:do=startcmd_number:2.5",
        "245:expect=undo_depth:17",
        "246:do=startcmd_target_here",
        "247:expect=startcmd_target:0:pos",
        "248:expect=undo_depth:18",
        "250:shot=m2_startcmds",
        "250:compare=m2_startcmds",
        "251:do=startcmds_window:1",
        "254:shot=m2_startcmds_panel",
        "254:compare=m2_startcmds_panel",
        "255:do=startcmds_window:0",
        "256:key=Z+ctrl",
        "258:expect=undo_depth:17",
        "259:key=Y+ctrl",
        "261:expect=undo_depth:18",
        "262:expect=startcmd_target:0:pos",
        "264:do=startcmd_target_begin",
        "266:click=c300x60",
        "268:expect=undo_depth:19",
        "269:key=Z+ctrl",
        "271:expect=undo_depth:18",
        // 04-11: reserve positions. The Place tool puts a towed gun and a truck of
        // player 0 (the first towed gun the catalogue offers, found through the bridge,
        // and Sdkfz_8, a carrier whose 24000 towing force beats its weight) on the snow
        // left of the view centre; Unit > Artillery positions
        // mode (the named command) puts the Reserve Positions tool in hand; a click on
        // the gun, a click on the truck and a click on the ground form the choice,
        // Enter commits it as one position; undo and redo walk it back and forth and
        // the gun -> truck -> place line is shot. A placed unit is drawn above the
        // ground point it stands on, so the picking clicks land a little above the
        // points the placing clicks used.
        "273:tool=place",
        "274:do=placer_role:towed",
        "275:click=c-230x-140",
        "277:do=placer_name:Sdkfz_8",
        "278:click=c-130x-120",
        "280:expect=undo_depth:20",
        "281:do=reserve_mode",
        "282:click=c-230x-180",
        "284:expect=reserve_pending:gun",
        "285:click=c-130x-165",
        "287:expect=reserve_pending:truck",
        "288:click=c0x-200",
        "290:expect=reserve_pending:place",
        "291:key=ENTER",
        "293:expect=reserve_delta:1",
        "294:expect=undo_depth:21",
        "295:expect=reserve_pending:none",
        "296:key=Z+ctrl",
        "298:expect=reserve_delta:0",
        "299:key=Y+ctrl",
        "301:expect=reserve_delta:1",
        "303:shot=m2_reserve",
        "303:compare=m2_reserve",
        // 04-12: the AI general. The AI General tool on side 1 (coldwinter has two sides):
        // a click on open ground makes a defence parcel, a click inside it a reinforce
        // point, the undo key takes the point away and redo brings it back, Enter switches
        // the parcel to reinforce, a mobile script ID is added through the named command,
        // and the parcel, its arrow, the point and the panel are shot; then four undos take
        // the script ID, the type, the point and the parcel away again (the side count never
        // moves on an existing side).
        "304:tool=ai_general",
        "305:do=ai_side:1",
        "306:click=c80x-150",
        "308:expect=parcels:1:1",
        "309:expect=undo_depth:22",
        "310:click=c160x-150",
        "312:expect=undo_depth:23",
        "313:key=Z+ctrl",
        "315:expect=undo_depth:22",
        "316:key=Y+ctrl",
        "318:expect=undo_depth:23",
        "319:key=ENTER",
        "321:expect=undo_depth:24",
        "322:do=ai_mobile_add:4245",
        "323:expect=mobile_has:1:4245",
        "324:expect=undo_depth:25",
        "327:shot=m2_ai_general",
        "327:compare=m2_ai_general",
        "328:key=Z+ctrl",
        "330:key=Z+ctrl",
        "332:key=Z+ctrl",
        "334:key=Z+ctrl",
        "336:expect=parcels:1:0",
        "337:expect=undo_depth:21",
        b.fmt("339:saveas={s}/m2.bzm", .{auto_m2_dir}),
        "340:shot=m2_anchor",
        "340:compare=m2_anchor",
        // 04-13, the exit run (D-25.6): the saved user map gets its script the
        // way the Script dialog's Choose other gives one, without the file
        // picker (script_choose copies the fixture beside m2.bzm and names it),
        // is saved and shot; Save As into another folder asks to bring the
        // script along and the answer copies it (the copy-along question on a
        // user map); Test in game from there copies it beside the test map and
        // the test game's own BK_MAP_TRACE report says it ran the script.
        "341:do=script_file:none",
        "342:expect=script_file:none",
        b.fmt("343:do=script_choose:{s}", .{auto_m2_fixture}),
        "344:expect=script_file:m2_script",
        "344:expect=script_beside:m2_script",
        "345:expect=undo_depth:23",
        "346:save",
        "348:shot=m2_final",
        "348:compare=m2_final",
        b.fmt("350:saveas={s}/m2_along.bzm", .{auto_m2_along_dir}),
        "353:do=script_copy_along_yes",
        "354:expect=script_beside:m2_script",
        "354:expect=script_file:m2_script",
        "355:test",
        "356:waitgame=240",
        "357:expect=test_game_script:m2_script",
        "358:exit",
    };
    const auto_m2_run = b.addRunArtifact(exe);
    auto_m2_run.setCwd(b.path(stage_root));
    auto_m2_run.addArgs(&.{ "--hidden", "Data\\Maps\\Multiplayer\\coldwinter.bzm" });
    auto_m2_run.setEnvironmentVariable("BK_EDITOR_AUTO_DIR", auto_m2_dir);
    auto_m2_run.setEnvironmentVariable("BK_EDITOR_AUTO", std.mem.join(b.allocator, ",", &auto_m2_entries) catch @panic("OOM"));
    // 04-13: the test game's own schedule (a shot, then exit) and its
    // BK_MAP_TRACE, which the test_game_script predicate reads.
    auto_m2_run.setEnvironmentVariable("BK_EDITOR_AUTO_GAME", "400:shot,440:exit");
    auto_m2_run.setEnvironmentVariable("BK_EDITOR_AUTO_GAME_TRACE", "1");
    // What it reads - the staged Data and engine - is not a file input of the
    // step, so a cached pass would say nothing about the installation now.
    auto_m2_run.has_side_effects = true;
    auto_m2_run.step.dependOn(&install_exe.step);
    // After map-editor-auto (which itself runs after the smoke), so two
    // engines never start at once: this Zig has no ordering-only edge
    // (Build.Step has no mustRunAfter), so the M1 scenario runs first.
    auto_m2_run.step.dependOn(&cleanup_autoshots.step);
    // 04-13: a script left beside m2.bzm or m2_along.bzm by an earlier run
    // would turn Choose other into its Replace it? question and let the
    // copy-along check pass on a stale file, so both go first.
    for ([_][]const u8{ auto_m2_dir, auto_m2_along_dir }) |folder| {
        const stale_scripts = b.addRunArtifact(delete_matching);
        stale_scripts.addArgs(&.{ folder, "m2_script", ".lua" });
        auto_m2_run.step.dependOn(&stale_scripts.step);
    }
    const cleanup_autoshots_m2 = b.addRunArtifact(delete_matching);
    cleanup_autoshots_m2.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup_autoshots_m2.step.dependOn(&auto_m2_run.step);
    const auto_m2_step = b.step("map-editor-auto-m2", "Run BK_EDITOR_AUTO's M2 scenario (named commands and predicates) on a shipped map");
    auto_m2_step.dependOn(&cleanup_autoshots_m2.step);

    // Phase 5's M3 scenario (05-01, D-37/D-40.7): the same loop once more -
    // every M3 plan appends its own segment of named commands and predicates
    // to the array below. This plan's segment: the title (F15) and status
    // bar (V6) expect their own predicates, the brush combo's range (V4),
    // New Map (F1) and Save as BZM (F8). No `test` action and no
    // BK_EDITOR_AUTO_GAME: the game-reads-it checks belong to the plans that
    // add them.
    const auto_m3_dir = b.pathFromRoot("zig-out/local-test/map-editor-m3-auto");
    // The climb from the staged game root (the editor's cwd) up to zig-out/, one "../" per
    // component of stage_root: the save_bzm paths below are relative to that cwd, and a hand-written
    // count of two landed the maps in zig-out/game/<os>/local-test, outside the scratch folder
    // BK_EDITOR_AUTO_DIR names (05-REVIEW WR-D01). A do= argument is at most 64 characters.
    const auto_m3_up = climb: {
        var up: std.ArrayListUnmanaged(u8) = .empty;
        for (0..std.mem.count(u8, stage_root, "/")) |_| up.appendSlice(b.allocator, "../") catch @panic("OOM");
        break :climb up.items;
    };
    // The Check Map fixture the scenario opens (05-05): crafted by the bridge test's
    // `--craft` before the scenario starts.
    const auto_m3_check_map = b.pathFromRoot("zig-out/local-test/m3-check-map.bzm");
    // The shipped map the Layers frames open again mid-scenario (05-06): the staged copy's OS path.
    const auto_m3_coldwinter = b.pathFromRoot(b.fmt("{s}/Data/Maps/Multiplayer/coldwinter.bzm", .{stage_root}));
    const auto_m3_arnheim = b.pathFromRoot(b.fmt("{s}/Data/Maps/Multiplayer/arnheim.bzm", .{stage_root}));
    const craft_fixtures_step = &(b.top_level_steps.get("editor-craft-fixtures") orelse @panic("editor-craft-fixtures is defined by addEditorBridgeTest")).step;
    const auto_m3_entries = [_][]const u8{
        // The shipped map from the command line is open by now: the title
        // names it (F15), the status bar carries the MFC's own VIS/SCRIPT
        // pair and object line (V6).
        "3:expect=title:coldwinter",
        "5:expect=status:VIS:",
        "7:expect=status:SCRIPT",
        // The brush combo's own range (V4): 1 and 16 are both taken, and
        // neither is an edit - there is nothing to undo.
        "10:do=brush_size:1",
        "12:do=brush_size:16",
        "14:expect=undo_depth:0",
        // New Map (F1): the dialog's fields as one command; the title
        // follows to the never-saved name (F15).
        "18:do=map_new:8x8:summer:M3Auto",
        "25:expect=title:M3Auto",
        "27:expect=status:VIS:",
        // Save as BZM (F8): the command's own path argument delivers the
        // Save As; the relative path is from the staged game root (the
        // run's cwd), the same ground `saveas=` uses.
        b.fmt("32:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3.bzm", .{auto_m3_up}),
        // Saved: the title names the file it became, clean of the star.
        "40:expect=title:m3.bzm",
        "42:expect=title:8x8",
        // The new map's world replaces coldwinter's: the camera goes home
        // (zoom 0, re-synced), so the scripted cells below name real cells
        // of the 8x8 map.
        "44:key=HOME",
        // The Heights tool (D-18) on the fresh 8x8: a raise drag is one undo
        // step; a level drag (the mode is a named command, the gesture is
        // the same left drag) is one more. Generate hills (TR10): one
        // command, one undo step.
        "50:tool=heights",
        "52:press=c0x0",
        "54:drag=c10x0",
        "56:release=c10x0",
        "58:expect=undo_depth:1",
        "60:do=heights_mode:click_average",
        "62:press=c-20x0",
        "64:drag=c-40x0",
        "66:release=c-40x0",
        "68:expect=undo_depth:2",
        "72:do=heights_generate:hills:0.3:-3:3",
        "74:expect=undo_depth:3",
        // Update Map (M8/D-20) and Fill Entire Map (M1/D-22): one explicit
        // undoable command each - and the fill's tile 0 is answered by the
        // tile properties (TR2) after it.
        "78:do=map_update",
        "80:expect=undo_depth:4",
        "84:do=map_fill:0",
        "86:expect=undo_depth:5",
        // The two toggles (M9/M10): settings, never undo steps.
        "90:do=instant_update",
        "92:do=fit_grid",
        "94:expect=undo_depth:5",
        // Tile properties (TR2): tile 0 answers its name - the MFC's `> 0`
        // guard is not copied.
        "98:do=tile_info:0",
        "100:expect=status:0:",
        // The object filters (D-31, O2/O3/O4): the combo selects a shipped
        // filter (Data/Editor/filter.xml), the palette's count follows
        // (predicate over the filtered catalogue); Ctrl+click's assign puts
        // it in quick-toggle slot 0, unchecking and rechecking the slot
        // gates and ungates the same rows; the composer opens and closes by
        // its command.
        "120:do=filter_select:Buildings",
        "122:expect=palette_count:194",
        "124:do=filter_assign:0",
        "126:do=filter_select:none",
        "128:expect=palette_count:1068",
        "130:do=filter_toggle:0",
        "132:expect=palette_count:194",
        "134:do=filter_toggle:0",
        "136:expect=palette_count:1068",
        "138:do=filters_composer",
        "140:do=filters_composer",
        // The Fields tool (D-21, TR14-TR18): the field-set combo from the
        // storage scan, a scripted polygon, one apply - one undo step - and
        // undo back to the saved bytes (dirty 0). The vertices go through
        // the vertex commands (world points): the synthetic pointer's
        // ground answer barely moves per screen pixel at this zoom, so
        // clicks would collapse into one deduped point. The save first
        // marks the pre-fields document clean, the way the MFC's dialog
        // flow ran on a saved map.
        b.fmt("150:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3.bzm", .{auto_m3_up}),
        "152:do=fields_set:scenarios\\fieldsets\\summer\\field00",
        "154:tool=fields",
        "156:do=fields_vertex_add:64:64",
        "158:do=fields_vertex_add:192:64",
        "160:do=fields_vertex_add:192:192",
        "162:do=fields_vertex_add:64:192",
        "164:do=fields_apply",
        "166:expect=dirty:1",
        "168:key=Z+ctrl",
        "170:expect=dirty:0",
        // Multi-selection (D-25, O9-O12/O14/O16): two squads placed - the
        // placer selects each one it places - and the Selector takes over.
        // Undo depths run on from the fields segment's 5.
        "180:do=placer_name:US_sniper",
        "182:tool=place",
        "184:click=c-40x-60",
        "186:click=c0x-60",
        "188:expect=undo_depth:7",
        "189:expect=objects:2",
        // The screen rubber band (O10): a drag that starts on empty ground
        // draws the band, and its release selects every object whose
        // picture's centre it holds - the engine's own rectangle pick.
        "190:tool=select",
        "191:press=c-90x-150",
        "192:drag=c-20x-80",
        "193:drag=c50x-20",
        "194:release=c50x-20",
        "196:expect=selection_count:2",
        // The group move (O12): a drag FROM a selected object - on its
        // drawn picture, which the generated hills lift above the ground
        // point the placer clicked - moves the whole selection as ONE undo
        // step.
        "198:press=c-42x-102",
        "199:drag=c-32x-97",
        "200:drag=c-22x-92",
        "201:release=c-22x-92",
        "202:expect=undo_depth:8",
        "203:expect=selection_count:2",
        // Right-click alone deselects (O14); the Ctrl band's tile rectangle
        // re-selects both (O11: the scripted pointer has no Ctrl, so the
        // band's own read runs as a command).
        "204:rclick=c60x60",
        "206:expect=selection_count:0",
        "208:do=band_select:0-0-255-255",
        "210:expect=selection_count:2",
        // Delete takes the whole selection as one step (O16), and one undo
        // brings both back; the redo deletes them again, so the frames
        // below stage their own objects.
        "212:key=Delete",
        "214:expect=selection_count:0",
        "216:expect=undo_depth:9",
        "217:expect=objects:0",
        "218:key=Z+ctrl",
        "220:expect=undo_depth:8",
        "222:expect=objects:2",
        "224:key=Y+ctrl",
        "226:expect=undo_depth:9",
        "228:expect=objects:0",
        // The Properties window (D-26, O15/O17/O19): a fresh squad placed and
        // selected by the placer's own click; the player and health fields
        // commit as ONE undo step each.
        "230:do=placer_name:US_sniper",
        "231:tool=place",
        "232:click=c-40x-60",
        "233:expect=selection_count:1",
        "234:do=props_open:1",
        "236:do=props_set:player=1",
        "238:expect=undo_depth:11",
        "240:do=props_set:health=50",
        "242:expect=hp:@0:50",
        "244:expect=undo_depth:12",
        // Links (D-27, O13/O21): a house with rest slots (A_H01_1: 40)
        // placed off to the side, the band selects the pair (@0 the squad,
        // placed first, so the lower link ID; @1 the house), the garrison
        // follows CheckForInserting's rules as ONE step; undo and redo walk
        // it, and the units list's unlink takes it back.
        "250:do=placer_name:A_H01_1",
        "252:click=c160x-120",
        "254:do=band_select:0-0-255-255",
        "256:expect=selection_count:2",
        "258:do=link_make:@0=@1",
        "259:expect=link_with:@0=@1",
        "260:expect=undo_depth:14",
        "262:key=Z+ctrl",
        "264:expect=undo_depth:13",
        "265:expect=link_with:@0=0",
        "266:key=Y+ctrl",
        "268:expect=undo_depth:14",
        "269:expect=link_with:@0=@1",
        "274:do=link_unlink:@0",
        "276:expect=undo_depth:15",
        "277:expect=link_with:@0=0",
        // The direction wheel (D-28, O6) with nothing selected: a turn sets
        // the placement angle and edits nothing - Q/E stay beside it.
        "280:tool=select",
        "281:rclick=c60x60",
        "282:expect=selection_count:0",
        "283:do=wheel_turn:90",
        "284:expect=placer_angle:90",
        "286:do=wheel_turn:180",
        "288:expect=placer_angle:180",
        "290:expect=undo_depth:15",
        // The wheel with a selection (O6) turns it BY THE DELTA (the user's
        // ruling of 2026-10-03; 05-04 first turned it TO the wheel's angle):
        // a T-34 placed at the placer's 180 and left selected; the wheel is
        // set to 90 with nothing selected (the placer only), the T-34 is
        // selected again, and the wheel goes from 90 to 135 - a turn of +45,
        // so the T-34 faces 225, not the 135 a set-to-angle wheel would give.
        // ONE undo step, the undo turns it back to 180, the redo to 225; a
        // wheel turned to where it already stands turns nothing.
        "291:do=placer_name:T-34",
        "292:tool=place",
        "293:click=c-30x-30",
        "294:expect=selection_count:1",
        "295:expect=angle:@0:180",
        "296:expect=undo_depth:16",
        "297:tool=select",
        "298:rclick=c60x60",
        "299:expect=selection_count:0",
        "300:do=wheel_turn:90",
        "301:expect=placer_angle:90",
        "302:click=c-10x-78",
        "303:expect=selection_count:1",
        "304:expect=angle:@0:180",
        "305:do=wheel_turn:135",
        "306:expect=placer_angle:135",
        "307:expect=angle:@0:225",
        "308:expect=undo_depth:17",
        "309:key=Z+ctrl",
        "310:expect=angle:@0:180",
        "311:expect=undo_depth:16",
        "312:key=Y+ctrl",
        "313:expect=angle:@0:225",
        "314:expect=undo_depth:17",
        "315:do=wheel_turn:135",
        "316:expect=angle:@0:225",
        "317:expect=undo_depth:17",
        // The Damage tool (D-29, MT1) by its real clicks on the T-34's drawn
        // hull (the hills lift it above the point it was placed at): 25%, a
        // left click damages it ONE step, a right click heals it ONE step;
        // then the command form's hit and repair, ONE step each.
        "320:do=damage_percent:25",
        "321:tool=damage",
        "322:click=c-10x-78",
        "324:expect=hp:@0:75",
        "325:expect=undo_depth:18",
        "326:rclick=c-10x-78",
        "328:expect=hp:@0:100",
        "329:expect=undo_depth:19",
        "330:do=damage:@0:damage",
        "331:expect=hp:@0:75",
        "332:do=damage:@0:repair",
        "333:expect=hp:@0:100",
        "334:expect=undo_depth:21",
        // Players (D-30, M4): the new map has the two default entries; two
        // players are added before the neutral (one undo step each), the
        // unit creation of player 0 takes a relax time and an appear point
        // (one step each), and deleting player 0 turns the T-34 - its
        // object - over to the neutral, which is entry 2 of the 3 left.
        // Undo and redo walk it, the owner following.
        "336:expect=players:2",
        "338:do=player_add:0",
        "340:do=player_add:1",
        "342:expect=players:4",
        "344:expect=undo_depth:23",
        "346:do=unit_creation_player:0",
        "348:do=unit_creation_set:relax=77",
        "350:expect=unit_creation_is:relax=77",
        "352:expect=undo_depth:24",
        "354:do=appear_point_here",
        "356:expect=undo_depth:25",
        "358:expect=player_is:@0:0",
        "360:do=player_delete:0",
        "362:expect=players:3",
        "364:expect=player_is:@0:2",
        "366:expect=undo_depth:26",
        "368:key=Z+ctrl",
        "370:expect=players:4",
        "372:expect=player_is:@0:0",
        "374:key=Y+ctrl",
        "376:expect=players:3",
        "378:expect=player_is:@0:2",
        "380:key=Z+ctrl",
        "382:expect=players:4",
        // Check Map (D-33, M7/F4/S1) on a crafted fixture (coldwinter plus six defects:
        // a duplicate object, a link to a host that is not there, an owner of 99, a
        // party partys.xml lacks, an object whose type no database lists, and a road
        // with one control point). The new map is saved first so the open asks
        // nothing. A check edits nothing and writes its log; Save only SAYS the
        // checks failed; Fix all fixes what needs no asking as ONE undo step and
        // `remove` takes the unknown object and the short road too; undo walks both
        // back to the six findings.
        b.fmt("384:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3.bzm", .{auto_m3_up}),
        b.fmt("392:open={s}", .{auto_m3_check_map}),
        "406:do=check_map",
        "408:expect=check_findings:duplicate_object=1",
        "409:expect=check_findings:invalid_link=1",
        "410:expect=check_findings:player_index=1",
        "411:expect=check_findings:unknown_party=1",
        "412:expect=check_findings:unknown_object_type=1",
        "413:expect=check_findings:short_vso=1",
        "414:expect=check_findings:6",
        "416:expect=check_log_has:control",
        "418:expect=undo_depth:0",
        b.fmt("424:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3-checked.bzm", .{auto_m3_up}),
        "434:expect=status:checks",
        "436:expect=check_findings:6",
        "438:do=check_jump:0",
        "440:do=check_map_fix_all",
        "442:expect=check_findings:2",
        "444:expect=undo_depth:1",
        "446:do=check_map_fix_all:remove",
        "448:expect=check_findings:0",
        "450:expect=undo_depth:2",
        "452:do=undo",
        "453:do=undo",
        "454:expect=undo_depth:0",
        "456:do=check_map",
        "458:expect=check_findings:6",
        // The Minimap panel (05-07, D-14..D-17, MM1-MM4, M11). The crafted map is
        // saved under a name of its own (Save As: a user map beside which the
        // pictures can go), the panel is shown - nothing in it is an edit, the
        // document stays clean - and a click near its top-left corner moves the
        // camera: the shots before and after differ (the view's ground and the
        // panel's camera frame both moved). Create Minimap Images writes the
        // pictures beside the saved map (an explicit command, never part of
        // Save), the panel switches to Game mode and shows the fresh picture;
        // Editor mode is one click back.
        b.fmt("460:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3-minimap.bzm", .{auto_m3_up}),
        "470:expect=title:m3-minimap.bzm",
        "472:do=minimap_toggle:on",
        "474:expect=minimap_visible:1",
        "476:expect=minimap_mode:editor",
        "480:shot=m3-minimap-before",
        "482:do=minimap_click:8x8",
        "484:expect=minimap_moved",
        "488:shot=m3-minimap-after",
        "490:differ=m3-minimap-before/m3-minimap-after@0.5",
        "492:expect=dirty:0",
        "494:do=minimap_create",
        "498:expect=minimap_files",
        "500:expect=minimap_mode:game",
        "504:shot=m3-minimap-game",
        "506:do=minimap_mode:editor",
        "508:expect=minimap_mode:editor",
        "510:do=minimap_mode:game",
        "512:expect=dirty:0",
        // With the Heights tool active the panel draws the height gradient
        // (D-14: the MFC's grey ramp), not the terrain colours.
        "514:do=minimap_mode:editor",
        "516:tool=heights",
        "524:shot=m3-minimap-heights",
        "526:differ=m3-minimap-after/m3-minimap-heights@0.5",
        "528:tool=select",
        // The Layers menu (05-06, D-32; PARITY L1-L12, L14, L15). Renderer state:
        // nothing here is an edit - the document stays clean and there is nothing to
        // undo. The renderer's own answer (not the editor's memory) is read back after
        // every toggle; the layers whose effect is plain in a picture also differ from
        // the base shot. The starting menu is the MFC's own.
        "544:expect=dirty:0",
        "546:expect=layer:terrain:1",
        "547:expect=layer:terrain_noise:1",
        "548:expect=layer:black_stripes:1",
        "549:expect=layer:units:1",
        "550:expect=layer:objects:1",
        "551:expect=layer:shadows:1",
        "552:expect=layer:haze:1",
        "553:expect=layer:grid:0",
        "554:expect=layer:wireframe:0",
        "555:expect=layer:depth_complexity:0",
        "556:expect=layer:bounding_boxes:0",
        "557:expect=layer:war_fog:0",
        "558:expect=layer:units_passability:0",
        "559:expect=layer:fire_ranges:0",
        "562:shot=m3-layers-base",
        "566:do=layer_toggle:terrain",
        "568:expect=layer:terrain:0",
        "570:shot=m3-layers-terrain",
        "572:differ=m3-layers-base/m3-layers-terrain@5",
        "574:do=layer_toggle:terrain",
        "576:expect=layer:terrain:1",
        "578:do=layer_toggle:grid",
        "580:expect=layer:grid:1",
        "582:shot=m3-layers-grid",
        "584:differ=m3-layers-base/m3-layers-grid@0.05",
        "586:do=layer_toggle:grid",
        "588:expect=layer:grid:0",
        "590:do=layer_toggle:wireframe",
        "592:expect=layer:wireframe:1",
        "594:shot=m3-layers-wireframe",
        "596:differ=m3-layers-base/m3-layers-wireframe@5",
        "598:do=layer_toggle:wireframe",
        "600:expect=layer:wireframe:0",
        "602:do=layer_toggle:terrain_noise",
        "604:expect=layer:terrain_noise:0",
        "606:do=layer_toggle:terrain_noise",
        "608:expect=layer:terrain_noise:1",
        "610:do=layer_toggle:black_stripes",
        "612:expect=layer:black_stripes:0",
        "614:do=layer_toggle:black_stripes",
        "616:expect=layer:black_stripes:1",
        "618:do=layer_toggle:units",
        "620:expect=layer:units:0",
        "622:do=layer_toggle:units",
        "624:expect=layer:units:1",
        "626:do=layer_toggle:objects",
        "628:expect=layer:objects:0",
        "630:do=layer_toggle:objects",
        "632:expect=layer:objects:1",
        "634:do=layer_toggle:bounding_boxes",
        "636:expect=layer:bounding_boxes:1",
        "638:do=layer_toggle:bounding_boxes",
        "640:expect=layer:bounding_boxes:0",
        "642:do=layer_toggle:shadows",
        "644:expect=layer:shadows:0",
        "646:do=layer_toggle:shadows",
        "648:expect=layer:shadows:1",
        "650:do=layer_toggle:haze",
        "652:expect=layer:haze:0",
        "654:do=layer_toggle:haze",
        "656:expect=layer:haze:1",
        "658:do=layer_toggle:war_fog",
        "660:expect=layer:war_fog:1",
        "662:shot=m3-layers-war-fog",
        "664:differ=m3-layers-base/m3-layers-war-fog@5",
        "666:do=layer_toggle:war_fog",
        "668:expect=layer:war_fog:0",
        "670:do=layer_toggle:units_passability",
        "672:expect=layer:units_passability:1",
        "674:shot=m3-layers-units-passability",
        "676:differ=m3-layers-base/m3-layers-units-passability@0.05",
        "678:do=layer_toggle:units_passability",
        "680:expect=layer:units_passability:0",
        // Depth complexity is the one layer the GPU renderer cannot draw (the probe:
        // it paints the frame white): the menu greys it, the editor refuses it, and the
        // renderer keeps it off.
        "682:expect=layer:depth_complexity:0",
        "684:expect=dirty:0",
        "686:expect=undo_depth:0",
        // The desync fix (D-32): layers chosen before an open come back after it and
        // after a New Map - the MFC editor lost the grid and the noise to the new
        // terrain and its war fog to the open.
        "690:do=layer_toggle:grid",
        "692:do=layer_toggle:war_fog",
        "694:do=layer_toggle:terrain_noise",
        "696:do=layer_toggle:bounding_boxes",
        "698:expect=layer:grid:1",
        "699:expect=layer:war_fog:1",
        "700:expect=layer:terrain_noise:0",
        "701:expect=layer:bounding_boxes:1",
        "704:shot=m3-layers-chosen",
        b.fmt("708:open={s}", .{auto_m3_coldwinter}),
        "730:expect=title:coldwinter",
        "732:expect=layer:grid:1",
        "733:expect=layer:war_fog:1",
        "734:expect=layer:terrain_noise:0",
        "735:expect=layer:bounding_boxes:1",
        "736:expect=layer:terrain:1",
        "737:expect=layer:haze:1",
        "740:shot=m3-layers-after-open",
        "744:differ=m3-layers-base/m3-layers-after-open@1",
        "746:do=map_new:4x4:summer:M3Layers",
        "756:expect=title:M3Layers",
        "758:expect=layer:grid:1",
        "759:expect=layer:war_fog:1",
        "760:expect=layer:terrain_noise:0",
        "761:expect=layer:bounding_boxes:1",
        "764:shot=m3-layers-after-new",
        b.fmt("768:do=file_save_bzm:{s}local-test/map-editor-m3-auto/m3-layers.bzm", .{auto_m3_up}),
        "778:expect=title:m3-layers.bzm",
        b.fmt("780:open={s}", .{auto_m3_arnheim}),
        "806:expect=title:arnheim",
        // Unit Fire Ranges (L15), on a map with artillery (arnheim's StuG draws a ballistic
        // area; a rifleman's range is a line the minimap read leaves out): the selected
        // units' ranges follow the selection, a filter's follow the filter, and the mode is
        // asked again after an open (the AI forgot its groups with the map).
        "808:do=band_select:0-0-1023-1023",
        "810:do=fire_range:selected",
        "812:expect=layer:fire_ranges:1",
        "814:expect=fire_areas:1",
        "818:shot=m3-layers-fire",
        "822:do=band_select:0-0-0-0",
        "824:expect=selection_count:0",
        "827:expect=fire_areas:0",
        "829:do=fire_range:filter:All",
        "831:expect=fire_areas:1",
        "833:do=fire_range:filter:Buildings",
        "835:expect=fire_areas:0",
        "837:do=fire_range:filter:All",
        "839:expect=fire_areas:1",
        b.fmt("841:open={s}", .{auto_m3_arnheim}),
        "867:expect=title:arnheim",
        "869:expect=layer:fire_ranges:1",
        "871:expect=fire_areas:1",
        "873:do=fire_range:off",
        "875:expect=layer:fire_ranges:0",
        "877:expect=fire_areas:0",
        "879:do=layer_toggle:grid",
        "881:do=layer_toggle:war_fog",
        "883:do=layer_toggle:terrain_noise",
        "885:do=layer_toggle:bounding_boxes",
        "887:expect=layer:grid:0",
        "888:expect=layer:war_fog:0",
        "889:expect=layer:terrain_noise:1",
        "890:expect=layer:bounding_boxes:0",
        "893:expect=dirty:0",
        // Create Random Map (05-08, F10, D-01..D-05): the dialog opens and closes;
        // a fixed seed generates through the engine's own generator (the command is
        // synchronous - the window waits, D-03) into the run's user maps folder
        // (XDG_DATA_HOME below), the map opens as a normal document, and the seed
        // the generation reports is the one asked for.
        "895:do=rmg_dialog",
        "897:expect=rmg_dialog:1",
        "899:do=rmg_dialog:close",
        "901:expect=rmg_dialog:0",
        "902:do=rmg_set:template:scenarios\\templates\\summer\\template02",
        "903:do=rmg_set:context:scenarios\\chapters\\allies\\france\\context",
        "903:do=rmg_set:graph:0",
        "903:do=rmg_set:setting:any",
        "903:do=rmg_set:angle:0",
        // The setting combo greys out the settings this template cannot be
        // built in (a spring setting on a summer template): the generator would
        // find no terrain piece for them.
        "903:expect=rmg_setting_fits:scenarios\\settings\\spring_germany:0",
        "903:expect=rmg_setting_fits:scenarios\\settings\\summer_france:1",
        "903:do=rmg_set:level:1",
        "903:do=rmg_set:bzm:1",
        "903:do=rmg_set:dds:0",
        "903:do=rmg_set:overwrite:1",
        "903:do=rmg_set:name:m3_auto_rmg",
        "904:do=rmg_set:seed:777",
        "905:do=rmg_generate",
        "907:expect=rmg_seed:777",
        "945:expect=title:m3_auto_rmg.bzm",
        "947:expect=title:8x8",
        "949:expect=dirty:0",
        // The same generation as a person drives it: the dialog open with its fields
        // set one by one (shot), OK - the modal is announced at 0 of 19, the generator
        // runs one frame later with the window waiting, the result modal says the seed
        // (shot) - and Open map opens the file as a normal document.
        "951:do=rmg_dialog",
        "952:do=rmg_set:template:scenarios\\templates\\summer\\template02",
        "952:do=rmg_set:context:scenarios\\chapters\\allies\\france\\context",
        "952:do=rmg_set:graph:2",
        "952:do=rmg_set:angle:3",
        "952:do=rmg_set:level:2",
        "952:do=rmg_set:overwrite:1",
        "952:do=rmg_set:name:m3_auto_rmg_ui",
        "953:do=rmg_set:seed:4242",
        "958:shot=m3-rmg-dialog",
        "960:do=rmg_dialog:ok",
        "960:expect=rmg_phase:announce",
        "962:shot=m3-rmg-progress",
        "985:expect=rmg_phase:done",
        "986:expect=rmg_seed:4242",
        "988:shot=m3-rmg-result",
        "990:do=rmg_dialog:open_map",
        "992:expect=rmg_phase:idle",
        "1030:expect=title:m3_auto_rmg_ui.bzm",
        // Tools > Export lists (T3-T6, D-13): each list lands in the user's logs
        // folder in the MFC's line format (the writers' bytes are in the panels'
        // tests; here the files exist and hold what the shipped data lists).
        "1040:do=export_lists:graphs",
        "1042:expect=export_file:graphs",
        "1044:expect=export_lines:graphs:100",
        "1046:do=export_lists:contexts",
        "1048:expect=export_file:contexts",
        "1050:expect=export_lines:contexts:10",
        "1052:do=export_lists:patches",
        "1054:expect=export_file:patches",
        "1056:expect=export_lines:patches:5",
        "1058:do=export_lists:maps",
        "1060:expect=export_file:maps",
        "1062:expect=export_lines:maps:20",
        "1064:expect=status:created",
        // 05-09 (D-06..D-12): the Containers Composer. A shipped container opens (the
        // MFC's twelve columns and seven patch columns, shot), Check! finds nothing
        // wrong with it; a new container takes a map from the user's maps folder
        // through the copy-in (the YES/NO popup, shot) instead of the MFC's refusal,
        // its patch gets a setting and a direction cleared (undo and redo walk it),
        // and Save As writes it under the user RMG root, where Open reads it back.
        "1066:do=rmgc_window",
        "1068:expect=rmgc_listed:100",
        "1070:do=rmgc_open:common\\road_cross_asph_we_grunt_winter",
        "1072:expect=rmgc_patches:4",
        "1074:expect=rmgc_dirty:0",
        "1076:expect=rmgc_name:road_cross_asph_we_grunt_winter",
        "1078:shot=m3-containers-shipped",
        "1080:do=rmgc_check",
        "1082:expect=rmgc_errors:0",
        "1084:do=rmgc_new",
        "1086:expect=rmgc_patches:0",
        "1088:do=rmgc_import:m3_auto_rmg",
        "1090:expect=rmgc_pending:1",
        "1092:shot=m3-containers-copyin",
        "1094:do=rmgc_import_yes",
        "1096:expect=rmgc_pending:0",
        "1098:expect=rmgc_patches:1",
        "1100:expect=rmgc_dirty:1",
        "1102:do=rmgc_patch_set:0:place:summer_france",
        "1104:expect=rmgc_place:0:summer_france",
        "1106:do=rmgc_patch_set:0:east:0",
        "1108:expect=rmgc_cell:0:east:0",
        "1110:expect=rmgc_cell:0:north:1",
        "1112:do=rmgc_check",
        "1114:expect=rmgc_errors:0",
        "1116:do=rmgc_undo",
        "1118:expect=rmgc_cell:0:east:1",
        "1120:do=rmgc_redo",
        "1122:expect=rmgc_cell:0:east:0",
        "1124:do=rmgc_saveas:m3auto\\mine",
        "1126:expect=rmgc_dirty:0",
        "1128:expect=rmgc_name:m3auto\\mine",
        "1130:do=rmgc_new",
        "1132:do=rmgc_open:m3auto\\mine",
        "1134:expect=rmgc_patches:1",
        "1136:expect=rmgc_place:0:summer_france",
        "1138:expect=rmgc_cell:0:east:0",
        "1140:shot=m3-containers-user",
        // The Graphs Composer: a shipped graph opens on the canvas (shot); a new one is
        // drawn with the canvas gestures (two nodes, one moved, Ctrl+drag links them),
        // the nodes take a container and the link a descriptor and a part count below
        // eight, Check! finds that one thing (shot), Fix all repairs it, and the graph
        // saves under the user RMG root and opens again.
        "1142:do=rmgg_window",
        "1144:do=rmgg_open:winter\\graph_escort2",
        "1146:expect=rmgg_nodes:12",
        "1148:expect=rmgg_links:5",
        "1150:shot=m3-graphs-shipped",
        "1152:do=rmgg_new",
        "1154:expect=rmgg_nodes:0",
        "1156:do=rmgg_drag:0:0:31:31",
        "1158:do=rmgg_drag:48:0:79:31",
        "1160:expect=rmgg_nodes:2",
        "1162:do=rmgg_drag:10:10:12:10",
        "1164:expect=rmgg_dirty:1",
        "1166:do=rmgg_ctrl_drag:10:10:60:10",
        "1168:expect=rmgg_links:1",
        "1170:do=rmgg_node:0:winter\\army_s",
        "1172:do=rmgg_node:1:winter\\army_s",
        "1174:expect=rmgg_node_container:0:army_s",
        "1176:do=rmgg_link:0:desc:terrain\\sets\\2\\roads3d\\road_grunt",
        "1178:do=rmgg_link:0:parts:6",
        "1180:expect=rmgg_link_parts:0:6",
        "1182:do=rmgg_check",
        "1184:expect=rmgg_errors:1",
        "1186:shot=m3-graphs-check",
        "1188:do=rmgg_fix_all",
        "1190:expect=rmgg_errors:0",
        "1192:expect=rmgg_link_parts:0:8",
        "1194:do=rmgg_saveas:m3auto\\graph_mine",
        "1196:expect=rmgg_dirty:0",
        "1198:do=rmgg_new",
        "1200:do=rmgg_open:m3auto\\graph_mine",
        "1202:expect=rmgg_nodes:2",
        "1204:expect=rmgg_links:1",
        "1206:expect=rmgg_link_parts:0:8",
        "1208:expect=rmgg_node_container:1:army_s",
        "1210:shot=m3-graphs-user",
        // The Fields Composer (05-10): a shipped field set opens and its three tabs
        // show (shots), a shell and tiles and an object are edited with their weights,
        // Check! is clean on that and finds a tile past the tileset when one is added
        // (shot), Fix all removes it, and the set saves under the user RMG root and
        // opens again with the heights as set.
        "1214:do=rmgf_window",
        "1216:do=rmgf_open:summer\\field07",
        "1218:expect=rmgf_shells:terrain:3",
        "1220:expect=rmgf_shells:objects:5",
        "1222:expect=rmgf_listed:20",
        "1224:do=rmgf_tab:terrain",
        "1226:do=rmgf_shell_pick:terrain:0",
        "1228:shot=m3-fields-terrain",
        "1230:do=rmgf_tab:objects",
        "1232:do=rmgf_filter:Buildings",
        "1234:expect=rmgf_avail:20",
        "1236:do=rmgf_shell_pick:objects:1",
        "1238:shot=m3-fields-objects",
        "1240:do=rmgf_tab:heights",
        "1242:do=rmgf_set:height:3",
        "1244:expect=rmgf_value:height:3.00",
        "1246:do=rmgf_set:pattern_min:6",
        "1248:expect=rmgf_value:pattern_max:6",
        "1250:shot=m3-fields-heights",
        "1252:do=rmgf_shell_add:terrain",
        "1254:expect=rmgf_shells:terrain:4",
        "1256:do=rmgf_tile_add:3:2",
        "1258:do=rmgf_tile_weight:3:0:5",
        "1260:do=rmgf_shell_set:terrain:3:width:4",
        "1262:do=rmgf_object_add:0:_Birch",
        "1264:expect=rmgf_dirty:1",
        "1266:do=rmgf_check",
        "1268:expect=rmgf_errors:0",
        "1270:do=rmgf_tile_add:3:4000",
        "1272:do=rmgf_check",
        "1274:expect=rmgf_errors:1",
        "1276:do=rmgf_tab:terrain",
        "1278:shot=m3-fields-check",
        "1280:do=rmgf_fix_all",
        "1282:expect=rmgf_errors:0",
        "1284:expect=rmgf_tiles:3:1",
        "1286:do=rmgf_saveas:m3auto\\field_mine",
        "1288:expect=rmgf_dirty:0",
        "1290:do=rmgf_new",
        "1292:expect=rmgf_shells:terrain:0",
        "1294:do=rmgf_open:m3auto\\field_mine",
        "1296:expect=rmgf_shells:terrain:4",
        "1298:expect=rmgf_tiles:3:1",
        "1300:expect=rmgf_value:height:3.00",
        "1302:expect=rmgf_value:pattern_max:6",
        "1304:shot=m3-fields-user",
        // The Templates Composer (05-10): a shipped template opens (shot), a weight, a
        // vso, the default field, a third player with its unit creation and its appear
        // point, the game type and the script are edited, Check! runs (shot), and the
        // template saves under the user RMG root - the Template entry with its
        // QuickLoadMapInfo beside it - and opens again as edited.
        "1310:do=rmgt_window",
        "1312:do=rmgt_open:winter\\template04",
        "1314:expect=rmgt_graphs:8",
        "1316:expect=rmgt_fields:4",
        "1318:expect=rmgt_vsos:1",
        "1320:expect=rmgt_players:2",
        "1322:expect=rmgt_listed:40",
        "1324:shot=m3-templates-shipped",
        "1326:do=rmgt_weight_set:graphs:0:5",
        "1328:expect=rmgt_weight:graphs:0:5",
        "1330:do=rmgt_vso_set:0:3:50",
        "1332:expect=rmgt_vso_is:0:3:50",
        "1334:do=rmgt_default_field:1",
        "1336:expect=rmgt_default:1",
        "1338:do=rmgt_player_add:1",
        "1340:expect=rmgt_players:3",
        "1342:expect=rmgt_sides:0112",
        "1344:do=rmgt_units_set:2:party=German",
        "1346:do=rmgt_units_set:2:relax=77",
        "1348:expect=rmgt_unit:2:relax=77",
        "1350:do=rmgt_appear_add:2:1024:2048",
        "1352:expect=rmgt_appear:2:1",
        "1354:do=rmgt_game_type:2:1",
        "1356:expect=rmgt_game_type_is:2:1",
        "1358:do=rmgt_popup:units",
        "1360:shot=m3-templates-units",
        "1362:do=rmgt_popup:diplomacy",
        "1364:shot=m3-templates-diplomacy",
        "1366:do=rmgt_popup:none",
        "1368:do=rmgt_check",
        "1370:shot=m3-templates-check",
        "1372:do=rmgt_saveas:m3auto\\template_mine",
        "1374:expect=rmgt_dirty:0",
        "1376:do=rmgt_new",
        "1378:expect=rmgt_graphs:0",
        "1380:do=rmgt_open:m3auto\\template_mine",
        "1382:expect=rmgt_graphs:8",
        "1384:expect=rmgt_players:3",
        "1386:expect=rmgt_weight:graphs:0:5",
        "1388:expect=rmgt_vso_is:0:3:50",
        "1390:expect=rmgt_default:1",
        "1392:expect=rmgt_unit:2:party=German",
        "1394:expect=rmgt_game_type_is:2:1",
        "1396:shot=m3-templates-user",
        // The app shell (05-11, D-34): Tools > Options holds the game's extra command line
        // and the default save format (the window opens with its fields, a command commits
        // each as OK would), the View menu hides and shows the docked panels, the status
        // bar and the floating windows and Reset layout puts it all back, Help shows the
        // keys and tools and About the product, and a map dropped on the window opens
        // through the unsaved-changes guard while a file that is not a map is ignored.
        "1402:do=options_show",
        "1404:expect=panel_visible:options:1",
        "1406:shot=m3-options",
        "1408:do=options_set_gameparams:-nosound+-windowed",
        "1409:expect=game_parameters:-nosound+-windowed",
        "1410:do=options_set_format:xml",
        "1411:expect=default_format:xml",
        "1412:do=options_set_format:bzm",
        "1413:expect=default_format:bzm",
        "1414:do=options_set_gameparams",
        "1415:expect=game_parameters",
        "1416:do=options_show:off",
        "1418:expect=panel_visible:options:0",
        "1420:do=view_panel:sounds:off",
        "1421:expect=panel_visible:sounds:0",
        "1422:do=view_panel:status_bar:off",
        "1423:expect=panel_visible:status_bar:0",
        "1424:do=view_panel:tools:off",
        "1425:expect=panel_visible:tools:0",
        "1426:do=view_panel:containers_composer:on",
        "1427:expect=panel_visible:containers_composer:1",
        "1430:shot=m3-view-hidden",
        "1432:do=view_panel:containers_composer:off",
        "1433:expect=panel_visible:containers_composer:0",
        "1434:do=layout_reset",
        "1438:expect=layout_default:1",
        "1439:expect=panel_visible:sounds:1",
        "1439:expect=panel_visible:status_bar:1",
        "1439:expect=panel_visible:tools:1",
        "1442:shot=m3-view-reset",
        "1444:do=help_keys",
        "1448:shot=m3-help-keys",
        "1450:do=help_keys:off",
        "1451:do=about_show",
        "1455:shot=m3-about",
        "1457:do=about_show:off",
        // The direction wheel takes the pointer over its whole dial (05-11): the placer's angle is
        // set to 90, a press on the LOWER half of the dial (the dial's rim is x 224-263, y 274-313
        // in this layout) turns it to about 270 - before, only the upper frame-high strip answered.
        "1457:tool=select",
        "1458:do=wheel_turn:90",
        "1459:expect=placer_angle:90",
        "1460:press=244x308",
        "1462:release=244x308",
        "1464:expect=placer_angle:270:20",
        "1466:expect=dirty:0",
        "1468:do=drop_file:Data/Maps/Multiplayer/arnheim.txt",
        "1470:expect=status:drop:",
        "1472:do=drop_file:Data/Maps/Multiplayer/arnheim.bzm",
        "1510:expect=title:arnheim",
        // The Place tool's ghost (PARITY O7, 05-11): with every floating window put away the map
        // is free to point at; the engine's own half-opaque visual of the chosen object follows the
        // pointer (the shots), turned by the wheel and by E (read back from the engine itself),
        // and it is gone with the tool.
        "1512:do=view_panel:containers_composer:off",
        "1512:do=view_panel:graphs_composer:off",
        "1512:do=view_panel:fields_composer:off",
        "1512:do=view_panel:templates_composer:off",
        "1512:do=view_panel:filters_composer:off",
        "1512:do=view_panel:check_map:off",
        "1512:do=view_panel:heights:off",
        "1512:do=view_panel:fields:off",
        "1512:do=view_panel:minimap:off",
        "1512:do=view_panel:properties_window:off",
        "1512:do=view_panel:groups:off",
        "1512:do=view_panel:start_commands:off",
        "1512:do=view_panel:script:off",
        "1512:do=view_panel:unit_creation:off",
        "1514:tool=place",
        "1515:do=placer_name:T-34",
        // The pointer goes to the map a frame before the click: ImGui's capture flags are
        // those of the frame before the events, and the last pointer step (the dial) left them on.
        "1516:drag=640x500",
        "1518:click=640x500",
        "1522:expect=place_ghost:1",
        "1524:do=wheel_turn:90",
        "1526:expect=place_ghost:1:90:1",
        "1528:shot=m3-place-ghost-90",
        "1530:key=e",
        "1532:expect=place_ghost:1:112:2",
        "1534:do=wheel_turn:270",
        "1536:expect=place_ghost:1:270:1",
        "1538:shot=m3-place-ghost-270",
        "1540:tool=select",
        "1542:expect=place_ghost:0",
        "1550:exit",
    };
    const auto_m3_run = b.addRunArtifact(exe);
    auto_m3_run.setCwd(b.path(stage_root));
    auto_m3_run.addArgs(&.{ "--hidden", "Data\\Maps\\Multiplayer\\coldwinter.bzm" });
    auto_m3_run.setEnvironmentVariable("BK_EDITOR_AUTO_DIR", auto_m3_dir);
    // The user root of this run (Platform/Paths.cpp honours XDG_DATA_HOME on macOS and
    // Linux, BK_USER_ROOT on Windows): the generated random map and the exported lists land
    // here and not in the person's own maps and logs folders.
    auto_m3_run.setEnvironmentVariable("XDG_DATA_HOME", b.pathFromRoot("zig-out/local-test/map-editor-m3-auto-user"));
    auto_m3_run.setEnvironmentVariable("BK_USER_ROOT", b.pathFromRoot("zig-out/local-test/map-editor-m3-auto-user"));
    auto_m3_run.setEnvironmentVariable("BK_EDITOR_AUTO", std.mem.join(b.allocator, ",", &auto_m3_entries) catch @panic("OOM"));
    auto_m3_run.has_side_effects = true;
    auto_m3_run.step.dependOn(&install_exe.step);
    // After the M2 scenario (which itself runs after M1's), so two engines
    // never start at once.
    auto_m3_run.step.dependOn(&cleanup_autoshots_m2.step);
    auto_m3_run.step.dependOn(craft_fixtures_step);
    const cleanup_autoshots_m3 = b.addRunArtifact(delete_matching);
    cleanup_autoshots_m3.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup_autoshots_m3.step.dependOn(&auto_m3_run.step);
    const auto_m3_step = b.step("map-editor-m3-auto", "Run BK_EDITOR_AUTO's M3 scenario (named commands and predicates) on a shipped map");
    auto_m3_step.dependOn(&cleanup_autoshots_m3.step);

    // Task 1's headless test-launch proof (D-01..D-09): the editor places a
    // unit and the real Game plays it, no person watching. It starts a second
    // engine process end to end. CI runs it on the two GPU runners (Windows,
    // macos-14) through map-editor-game-reads-it-m3, which depends on it, so
    // D-33's railroad crash has a regression gate (05-REVIEW WR-D05).
    const game_reads_it_run = b.addRunArtifact(exe);
    game_reads_it_run.setCwd(b.path(stage_root));
    game_reads_it_run.addArgs(&.{ "--game-reads-it", "Data\\Maps\\Multiplayer\\coldwinter.bzm", b.pathFromRoot("zig-out/local-test/map-editor-game-reads-it.log") });
    // What it reads (the staged Data and Game) and the second process it
    // starts are not file inputs of this step, so a cached pass would say
    // nothing about the installation now.
    game_reads_it_run.has_side_effects = true;
    game_reads_it_run.step.dependOn(&install_exe.step);
    const game_reads_it_step = b.step("map-editor-game-reads-it", "Test-launch a unit the editor placed and prove the real Game plays it (D-01..D-09)");
    game_reads_it_step.dependOn(&game_reads_it_run.step);

    // Phase 4's M2 game-reads-it scenario (04-04): the same shape - edit
    // through the core Editor on the real bridge, save the test copy, play it
    // with the real Game - but the assertions are on the game's own
    // BK_MAP_TRACE report of what it consumed (the camera anchor here; each
    // later M2 plan adds its edit to game_reads_m2.zig). After its M1 sibling, so
    // two games never start at once.
    const game_reads_it_m2_run = b.addRunArtifact(exe);
    game_reads_it_m2_run.setCwd(b.path(stage_root));
    game_reads_it_m2_run.addArgs(&.{ "--game-reads-it-m2", "Data\\Maps\\Multiplayer\\coldwinter.bzm", b.pathFromRoot("zig-out/local-test/map-editor-game-reads-it-m2.log") });
    game_reads_it_m2_run.has_side_effects = true;
    game_reads_it_m2_run.step.dependOn(&install_exe.step);
    game_reads_it_m2_run.step.dependOn(&game_reads_it_run.step);
    // The games' own screenshot dumps stay in the stage root they ran from;
    // swept even when the scenario's own sweep was skipped by an early return.
    const cleanup_autoshots_game_reads_m2 = b.addRunArtifact(delete_matching);
    cleanup_autoshots_game_reads_m2.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup_autoshots_game_reads_m2.step.dependOn(&game_reads_it_m2_run.step);
    const game_reads_it_m2_step = b.step("map-editor-game-reads-it-m2", "Test-launch M2 edits and prove from the game's own BK_MAP_TRACE report that it read them (D-22, D-25)");
    game_reads_it_m2_step.dependOn(&cleanup_autoshots_game_reads_m2.step);

    // Phase 5's M3 game-reads-it scenario (05-05, D-37's game tier): the real Game
    // loads a map whose railroads hold fewer than two control points - the record
    // that crashed CRailroadGraphConstructor - and an ordinary map after it, and
    // both exit 0 (game_reads_m3.zig). The fixture map is crafted by the bridge
    // test's `--craft` first. After its siblings, so two games never start at once.
    const game_reads_it_m3_run = b.addRunArtifact(exe);
    game_reads_it_m3_run.setCwd(b.path(stage_root));
    game_reads_it_m3_run.addArgs(&.{ "--game-reads-it-m3", b.pathFromRoot("zig-out/local-test/m3-short-railroad.bzm"), b.pathFromRoot("zig-out/local-test/map-editor-game-reads-it-m3.log") });
    // The authored leg writes its template, graph, container and field set under the
    // user RMG root and generates a map into the user maps folder: a scratch user root
    // (Platform/Paths.cpp honours XDG_DATA_HOME on macOS and Linux and BK_USER_ROOT on Windows).
    game_reads_it_m3_run.setEnvironmentVariable("XDG_DATA_HOME", b.pathFromRoot("zig-out/local-test/map-editor-game-reads-it-m3-user"));
    game_reads_it_m3_run.setEnvironmentVariable("BK_USER_ROOT", b.pathFromRoot("zig-out/local-test/map-editor-game-reads-it-m3-user"));
    game_reads_it_m3_run.has_side_effects = true;
    game_reads_it_m3_run.step.dependOn(&install_exe.step);
    game_reads_it_m3_run.step.dependOn(&game_reads_it_m2_run.step);
    game_reads_it_m3_run.step.dependOn(craft_fixtures_step);
    const cleanup_autoshots_game_reads_m3 = b.addRunArtifact(delete_matching);
    cleanup_autoshots_game_reads_m3.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup_autoshots_game_reads_m3.step.dependOn(&game_reads_it_m3_run.step);
    const game_reads_it_m3_step = b.step("map-editor-game-reads-it-m3", "Load a map whose railroads hold fewer than two control points in the real Game and prove it exits cleanly (05-05, D-33)");
    game_reads_it_m3_step.dependOn(&cleanup_autoshots_game_reads_m3.step);

    // The engine tier of the core: c_bridge_test.zig, linked exactly as
    // MapEditor is and staged beside it, because on Windows the engine's roots
    // are the running executable's directory (SDL_GetBasePath). A Run step
    // runs an artifact's installed copy once it has been installed.
    const engine_test_module = mapEditorModule(b, "Sources/editor/app/c_bridge_test.zig", target, optimize, toolchain, sdl_module, editor_imgui_module, core_module, kit_module, engine);
    const engine_test = b.addTest(.{ .name = "map-editor-engine-test", .root_module = engine_test_module });
    // .console: a CI/local test tool, never packaged - its output is read
    // straight off the console it always had.
    configureMapEditorExecutable(engine_test, target, .console);
    const install_engine_test = b.addInstallArtifact(engine_test, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_engine_test.step.dependOn(install_game_step);
    const engine_test_run = b.addRunArtifact(engine_test);
    engine_test_run.setCwd(b.path(stage_root));
    // What it reads - the staged Data and engine - is not a file input of the
    // step, so a cached pass would say nothing about the installation now.
    engine_test_run.has_side_effects = true;
    engine_test_run.step.dependOn(&install_engine_test.step);
    const engine_test_step = b.step("test-map-editor-engine", "Drive the real engine through the editor core's commands and check it agrees");
    engine_test_step.dependOn(&install_engine_test.step);
    if (test_mode == .run) engine_test_step.dependOn(&engine_test_run.step);

    // Returned so the package steps (package-game, package-game-editors) can
    // add --map-editor with this exact artifact's emitted binary: passing the
    // Compile step rather than a hand-built path keeps the package's stage
    // command dependent on this build, not on whatever happened to be on disk
    // from a previous run.
    return .{ .exe = exe, .view_test_step = view_test_step };
}

/// The SDL and editor-kit modules an editor app is built against, for the
/// app's target: MapEditor and ResourceEditor share them.
const EditorAppKit = struct {
    sdl: *std.Build.Module,
    kit: *std.Build.Module,
};

fn editorAppKit(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_imgui_module: *std.Build.Module,
    sdl_include: std.Build.LazyPath,
) EditorAppKit {
    // SDL is not the overlay spike's sdl3 module: that links libc, and on MSVC
    // Zig's libc is its static release CRT, which the Windows job measured
    // colliding with the engine's debug DLL CRT (duplicate _cexit, _wctype,
    // __pctype_func and _invalid_parameter_noinfo: libucrt.lib against
    // ucrtd.lib). The app takes the headers and library of the SDL the engine
    // links instead.
    //
    // Translated as vendor/zig-sdl3 translates it, not by @cImport: an
    // @cImport in a compilation without libc has no libc headers on MSVC
    // ("libc headers not available"), and Zig 0.16's translate-c rejects the
    // `ui64` suffix of MSVC's SIZE_MAX, which SDL_stdinc.h uses. The
    // translation step may use libc headers; the module it makes must not
    // link libc, or the collision above comes back.
    const sdl_header = b.addWriteFiles().add("sdl3.h", "#include <SDL3/SDL.h>\n");
    const sdl_translate = b.addTranslateC(.{ .root_source_file = sdl_header, .target = target, .optimize = optimize });
    sdl_translate.addIncludePath(sdl_include);
    if (target.result.os.tag == .windows) sdl_translate.defineCMacro("SIZE_MAX", "18446744073709551615ULL");
    const sdl_c = sdl_translate.createModule();
    sdl_c.link_libc = false;
    const sdl_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/app/sdl3.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "sdl_c", .module = sdl_c }},
    });
    // The editor kit for the app's target. The host-only kit module
    // (test-editor-kit) is built separately for the test tier.
    const kit_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/kit/root.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl_module },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
        },
    });
    // bridge.h, for kit/host.zig's @cImport.
    kit_module.addIncludePath(b.path("Sources/src/EditorBridge"));
    addMsvcIncludePaths(b, kit_module, toolchain);
    addMsvcLibraryPaths(b, kit_module, toolchain);
    return .{ .sdl = sdl_module, .kit = kit_module };
}

/// M001 S05: ResourceEditor, built and installed the way addMapEditor builds
/// MapEditor - the same engine libraries, kit, CRT, entry and rpath, beside
/// Game in the installation - but on the resource core and the BkRes* half
/// of the bridge instead of the map's. Returns the executable so the package
/// steps stage this exact binary.
fn addResourceEditor(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_imgui_module: *std.Build.Module,
    comparator_lib: *std.Build.Step.Compile,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) *std.Build.Step.Compile {
    const engine: MapEditorEngine = .{
        .editor_bridge = editor_bridge,
        .map_file = map_file,
        .formats = formats,
        .randommapgen = randommapgen,
        .misc = misc,
        .main_lib = main_lib,
        .lualib = lualib,
        .zlib = zlib,
        .platform_runtime = platform_runtime,
        .sdl_dynamic = sdl_dynamic,
    };
    const app_kit = editorAppKit(b, target, optimize, toolchain, editor_imgui_module, sdl_include);
    // The resource core for the app's target; test-resource-core builds its
    // own for the host only, against the host kit.
    const resource_core_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/resource_core/root.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "editor_kit", .module = app_kit.kit }},
    });
    const module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/resource_app/main.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = app_kit.sdl },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
            .{ .name = "editor_kit", .module = app_kit.kit },
            .{ .name = "resource_core", .module = resource_core_module },
        },
    });
    // resource_bridge.h (and the bridge.h it includes), for the app's @cImport.
    module.addIncludePath(b.path("Sources/src/EditorBridge"));
    linkEditorEngine(b, module, target, optimize, toolchain, engine);
    // The particle and effect exporters read the Scene module's structs, which the
    // data-only comparator library compiles in.
    module.linkLibrary(comparator_lib);
    const exe = b.addExecutable(.{ .name = "ResourceEditor", .root_module = module });
    // .windows like MapEditor: no console on a double-click; the automated
    // modes attach to the parent's (crt.attachParentConsole).
    configureMapEditorExecutable(exe, target, .windows);

    const stage_suffix = stage_root["zig-out/".len..];
    // Beside Game and MapEditor: the engine's roots are the installation.
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);
    const install_step = b.step("install-resource-editor", "Install ResourceEditor into the game installation");
    install_step.dependOn(&install_exe.step);

    // Launched from zig-out like map-editor-host-check, for the same reason:
    // the editor finds its installation beside its own executable, whatever
    // the working directory.
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("zig-out"));
    // The tracked picture the docks half shows in the thumbnail list.
    run.addArgs(&.{ "--check", "wpn", b.pathFromRoot("zig-out/local-test/resource_editor/resource-editor-check.tga"), b.pathFromRoot("tools/zig/fixtures/resource_editor/spt/sprite-1frame.tga") });
    // Reads the staged installation, not a file input of this step.
    run.has_side_effects = true;
    run.step.dependOn(&install_exe.step);
    const check_step = b.step("resource-editor-host-check", "Start ResourceEditor hidden on a new project and check ImGui draws over the engine's frame");
    check_step.dependOn(&install_exe.step);
    if (test_mode == .run) check_step.dependOn(&run.step);

    // The batch mode's command line on the 21 tracked project fixtures,
    // copied under zig-out/local-test: -os re-saves each byte for byte and
    // the engine's reader reopens it, an export batch reports its project
    // and the missing gamma.cfg, the shipped Data folder is refused.
    const batch_run = b.addRunArtifact(exe);
    batch_run.setCwd(b.path("zig-out"));
    batch_run.addArgs(&.{ "--batch-check", b.pathFromRoot("tools/zig/fixtures/resource_editor"), b.pathFromRoot("zig-out/local-test/resource_editor/batch") });
    batch_run.has_side_effects = true;
    batch_run.step.dependOn(&install_exe.step);
    const batch_step = b.step("resource-editor-batch", "Run ResourceEditor --batch over copies of the 21 project fixtures and read the results back with the engine's reader");
    batch_step.dependOn(&install_exe.step);
    if (test_mode == .run) batch_step.dependOn(&batch_run.step);

    // The project half of the smoke: a copy of the tracked .unt opened,
    // edited, saved, undone, saved and compared byte for byte (scenario.zig).
    const smoke_run = b.addRunArtifact(exe);
    smoke_run.setCwd(b.path("zig-out"));
    smoke_run.addArgs(&.{ "--smoke-edit", b.pathFromRoot("tools/zig/fixtures/resource_editor/unt/project.unt"), b.pathFromRoot("zig-out/local-test/resource_editor/smoke") });
    smoke_run.has_side_effects = true;
    smoke_run.step.dependOn(&install_exe.step);
    const smoke_step = b.step("resource-editor-smoke", "Run ResourceEditor's scripted new/save/reopen and edit/undo/save byte-compare on tracked project fixtures");
    smoke_step.dependOn(&install_exe.step);
    // The kind-level new/save/reopen first (main.zig --smoke), then the edit one.
    const smoke_kind_run = b.addRunArtifact(exe);
    smoke_kind_run.setCwd(b.path("zig-out"));
    smoke_kind_run.addArgs(&.{ "--smoke", "unt", b.pathFromRoot("zig-out/local-test/resource_editor/smoke-new/smoke.unt") });
    smoke_kind_run.has_side_effects = true;
    smoke_kind_run.step.dependOn(&install_exe.step);
    smoke_run.step.dependOn(&smoke_kind_run.step);
    if (test_mode == .run) smoke_step.dependOn(&smoke_run.step);

    // BK_EDITOR_AUTO's schedules over the resource command registry, one step per editor so each stays
    // inside a foreground command's 10 minutes: the core frames (new, open, edit, undo, save, export, pack,
    // import, the exported mod played in the real Game), then each editor's blocks on copies of the tracked
    // fixtures, with captured frames measured by code. Every step runs after the smoke, so two engines never
    // start at once, and has its own scratch folder; the aggregate chains them in order.
    const delete_matching_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/delete_matching_files.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const delete_matching = b.addExecutable(.{ .name = "delete-matching-files", .root_module = delete_matching_module });
    const auto_step = b.step("resource-editor-auto", "Run BK_EDITOR_AUTO's ResourceEditor scenario over the resource command registry: every per-editor resource-editor-auto-* step, in order");
    auto_step.dependOn(&install_exe.step);
    // The aggregate's chain, so two engines never start at once.
    var chain: ?*std.Build.Step = null;
    inline for (resource_auto_steps) |auto| {
        const alone = addResourceAutoRun(b, exe, delete_matching, stage_root, &install_exe.step, &smoke_run.step, auto, null);
        const step_name = "resource-editor-auto-" ++ auto.name;
        const per_step = b.step(step_name, auto.about);
        per_step.dependOn(&install_exe.step);
        if (test_mode == .run) per_step.dependOn(alone);
        // The aggregate's own run of the same scenario, ordered after the one before it; a step run alone
        // does not pull the others in.
        const chained = addResourceAutoRun(b, exe, delete_matching, stage_root, &install_exe.step, &smoke_run.step, auto, chain);
        chain = chained;
        if (test_mode == .run) auto_step.dependOn(chained);
    }
    return exe;
}

/// One per-editor step of resource-editor-auto: the editor's schedule (a preview_on or mod_dir prefix where an
/// earlier block would have set it, an exit after the last frame) in its own scratch folder, then the sweep of
/// the game's autoshot dumps. `after` orders it behind the previous step of the aggregate.
fn addResourceAutoRun(
    b: *std.Build,
    exe: *std.Build.Step.Compile,
    delete_matching: *std.Build.Step.Compile,
    stage_root: []const u8,
    install_step: *std.Build.Step,
    smoke_step: *std.Build.Step,
    comptime auto: ResourceAutoStep,
    after: ?*std.Build.Step,
) *std.Build.Step {
    const auto_dir = b.pathFromRoot("zig-out/local-test/resource_editor/auto-" ++ auto.name);
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path(stage_root));
    run.addArgs(&.{ "--auto", b.pathFromRoot("tools/zig/fixtures/resource_editor"), auto_dir });
    run.setEnvironmentVariable("BK_EDITOR_AUTO", auto.prefix ++ auto.schedule ++ std.fmt.comptimePrint("{d}:exit", .{auto.exit_frame}));
    run.has_side_effects = true;
    run.step.dependOn(install_step);
    run.step.dependOn(smoke_step);
    if (after) |previous| run.step.dependOn(previous);
    // The game's own screenshot dump (BK_AUTO_UI's shot) lands in the stage
    // root it ran from: swept up like map-editor-auto does.
    const cleanup = b.addRunArtifact(delete_matching);
    cleanup.addArgs(&.{ stage_root, "autoshot_", ".rgba" });
    cleanup.step.dependOn(&run.step);
    return &cleanup.step;
}

const ResourceAutoStep = struct {
    name: []const u8,
    about: []const u8,
    /// Entries at frame 1 that an earlier editor's block would have set up.
    prefix: []const u8 = "",
    schedule: []const u8,
    /// One past the schedule's last frame.
    exit_frame: u32,
};

/// The sprite block's do=preview_on stayed on for every later block of the old single run.
const resource_auto_preview_on = "1:do=preview_on,";

/// The per-editor steps in run order.
const resource_auto_steps = [_]ResourceAutoStep{
    .{ .name = "core", .about = "Run BK_EDITOR_AUTO's core frames: new, open, edit, undo, save, export, pack, import and the exported mod in the Game", .schedule = resource_auto_core, .exit_frame = 37 },
    .{ .name = "wpn", .about = "Run BK_EDITOR_AUTO's S06 stats sub-editors: Weapon, Mine, Trench and Squad (one step, each is short)", .schedule = resource_auto_wpn, .exit_frame = 132 },
    .{ .name = "spt", .about = "Run BK_EDITOR_AUTO's Sprite (.spt) scenario", .schedule = resource_auto_spt, .exit_frame = 168 },
    .{ .name = "unt", .about = "Run BK_EDITOR_AUTO's Infantry (.unt) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_unt, .exit_frame = 187 },
    .{ .name = "msh", .about = "Run BK_EDITOR_AUTO's Unit (.msh) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_msh, .exit_frame = 232 },
    .{ .name = "obt", .about = "Run BK_EDITOR_AUTO's Object (.obt) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_obt, .exit_frame = 286 },
    .{ .name = "fnc", .about = "Run BK_EDITOR_AUTO's Fence (.fnc) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_fnc, .exit_frame = 331 },
    .{ .name = "bld", .about = "Run BK_EDITOR_AUTO's Building (.bld) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_bld, .exit_frame = 399 },
    .{ .name = "bdg", .about = "Run BK_EDITOR_AUTO's Bridge (.bdg) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_bdg, .exit_frame = 459 },
    .{ .name = "pcp", .about = "Run BK_EDITOR_AUTO's Particle (.pcp) scenario, the Function window's pointer path included", .prefix = resource_auto_preview_on, .schedule = resource_auto_pcp, .exit_frame = 539 },
    .{ .name = "eff", .about = "Run BK_EDITOR_AUTO's Effect (.eff) scenario", .prefix = resource_auto_preview_on ++ "2:do=mod_dir:{mods}/reseditor_auto12,3:do=copy:{fix}/pcp/project.pcp>{dir}/particle-2key/project.pcp,4:open={dir}/particle-2key/project.pcp,5:expect=kind:pcp,6:do=export,6:expect=exported,", .schedule = resource_auto_eff, .exit_frame = 558 },
    .{ .name = "til", .about = "Run BK_EDITOR_AUTO's Terrain (.til) scenario", .schedule = resource_auto_til, .exit_frame = 40 },
    .{ .name = "3rd", .about = "Run BK_EDITOR_AUTO's 3D Road (.3rd) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_3rd, .exit_frame = 60 },
    .{ .name = "3rv", .about = "Run BK_EDITOR_AUTO's 3D River (.3rv) scenario", .prefix = resource_auto_preview_on, .schedule = resource_auto_3rv, .exit_frame = 60 },
};

/// The old single schedule's order. Nothing but the per-step constants' concatenation may stand here: the
/// comptime check below keeps the table above equal to it, so a block cannot be dropped or reordered.
const resource_auto_all = resource_auto_core ++ resource_auto_wpn ++ resource_auto_spt ++ resource_auto_unt ++
    resource_auto_msh ++ resource_auto_obt ++ resource_auto_fnc ++ resource_auto_bld ++ resource_auto_bdg ++
    resource_auto_pcp ++ resource_auto_eff ++ resource_auto_til ++ resource_auto_3rd ++ resource_auto_3rv;

comptime {
    @setEvalBranchQuota(2_000_000);
    var joined: []const u8 = "";
    for (resource_auto_steps) |step| joined = joined ++ step.schedule;
    if (!std.mem.eql(u8, joined, resource_auto_all)) @compileError("resource_auto_steps no longer concatenate to the whole schedule");
}

// Each per-editor schedule below is a block of the old single string (scenario.zig): one entry per line,
// frames ascending, so a later slice appends a block rather than editing one long string. {dir}, {fix}
// and {mods} are the scratch folder, the fixtures folder and the installation's mods folder.

/// The core frames: new, open, edit, undo, save, export, pack, import and the exported mod in the real Game.
const resource_auto_core =
    // A new project of another kind first: it draws, and is untitled.
    "1:do=new:wpn," ++
    "2:expect=kind:wpn," ++
    "3:expect=untitled," ++
    "4:shot=new," ++
    "5:expect=shot_lit:new," ++
    // A copy of the tracked .unt opened: its tree and inspector change the frame.
    "6:do=copy:{fix}/unt/project.unt>{dir}/work/project.unt," ++
    "7:open={dir}/work/project.unt," ++
    "8:expect=kind:unt," ++
    "9:expect=nodes_min:2," ++
    "10:expect=dirty:false," ++
    "11:shot=opened," ++
    "12:differ=new/opened@0.05," ++
    // Edit, save, undo, save: one property, one undo step, the file follows.
    "13:do=set_prop:Armor=9," ++
    "14:expect=prop:Armor=9," ++
    "15:expect=dirty:true," ++
    "16:shot=edited," ++
    "17:expect=shot_lit:edited," ++
    "18:save," ++
    "19:expect=dirty:false," ++
    "20:do=undo," ++
    "21:expect=prop:Armor=4," ++
    "22:expect=dirty:true," ++
    "23:save," ++
    "24:expect=dirty:false," ++
    // The infantry exporter (S07) writes this project into the mod folder; a
    // tracked file stands in for further data so Compress to PAK, which the
    // engine's own PAK reader reads back, and Run Blitzkrieg have a mod.
    "25:do=mod_dir:{mods}/reseditor_auto," ++
    "26:do=export," ++
    "26:expect=exported," ++
    "27:do=copy:{fix}/unt/mesh-2x2x2.obj>{mods}/reseditor_auto/data/x/m.obj," ++
    "28:do=pack:{dir}/auto.pak," ++
    "29:expect=file:{dir}/auto.pak," ++
    "30:do=import:unt," ++
    "31:expect=untitled," ++
    "32:expect=dirty:true," ++
    "33:shot=imported," ++
    "34:expect=shot_lit:imported," ++
    // The exported mod played in the real Game.
    "35:do=run_game," ++
    "36:waitgame=240,";

/// S06's four stats sub-editors (Weapon, Mine, Trench, Squad) share one mod folder and one run.
const resource_auto_wpn =
    // S06: the four stats sub-editors, one block each on a copy of the tracked
    // project, a mod folder of their own (the Game has left the first one).
    // Per kind: open, the kind's tool, undo, redo, save, export. Squad, trench
    // and mine also shoot the frame (the preview) and differ it from the last.
    "40:do=mod_dir:{mods}/reseditor_auto_s06," ++
    // Weapon (WeaponFrm): a shoot type inserted into the tree. MFC draws no
    // preview for it (D015), so no shot.
    "41:do=copy:{fix}/wpn/project.wpn>{dir}/wpn/project.wpn," ++
    "42:open={dir}/wpn/project.wpn," ++
    "43:expect=kind:wpn," ++
    "44:expect=dirty:false," ++
    "45:do=tree:add_shoot_type," ++
    "46:expect=dirty:true," ++
    "47:do=undo," ++
    "48:expect=dirty:false," ++
    "49:do=redo," ++
    "50:expect=dirty:true," ++
    "51:save," ++
    "52:do=export," ++
    "53:expect=exported," ++
    // The weapon frame stands in for the unit one the mine is differed from when run alone.
    "54:shot=weapon," ++
    // Mine (MineFrm): the weight, then the preview of the compose.
    "60:do=copy:{fix}/mcp/project.mcp>{dir}/mcp/project.mcp," ++
    "61:do=copy:{fix}/mcp/1.tga>{dir}/mcp/1.tga," ++
    "62:do=copy:{fix}/mcp/1s.tga>{dir}/mcp/1s.tga," ++
    "63:open={dir}/mcp/project.mcp," ++
    "64:expect=kind:mcp," ++
    "65:shot=mine," ++
    "66:expect=shot_lit:mine," ++
    "67:differ=weapon/mine@0.02," ++
    "68:do=set_prop:Weight=11," ++
    "69:expect=prop:Weight=11," ++
    "70:do=undo," ++
    "71:expect=prop:Weight=10," ++
    "72:do=redo," ++
    "73:expect=prop:Weight=11," ++
    "74:save," ++
    "75:do=export," ++
    "76:expect=exported," ++
    // Trench (TrenchFrm): a source added to a side, then the preview.
    "80:do=copy:{fix}/trc/project.trc>{dir}/trc/project.trc," ++
    "81:do=copy:{fix}/trc/1.tga>{dir}/trc/1.tga," ++
    "82:do=copy:{fix}/trc/1w.tga>{dir}/trc/1w.tga," ++
    "83:do=copy:{fix}/trc/1a.tga>{dir}/trc/1a.tga," ++
    "84:open={dir}/trc/project.trc," ++
    "85:expect=kind:trc," ++
    "86:shot=trench," ++
    "87:expect=shot_lit:trench," ++
    "88:differ=mine/trench@0.05," ++
    "89:do=tree:add_source," ++
    "90:expect=dirty:true," ++
    "91:do=undo," ++
    "92:expect=dirty:false," ++
    "93:do=redo," ++
    "94:expect=dirty:true," ++
    "95:save," ++
    "96:do=export," ++
    "97:expect=exported," ++
    // Squad (SquadFrm): a member dragged, the zero point, the direction arrow,
    // each one undo step, then the formation overlay's frame.
    "100:do=copy:{fix}/scp/project.scp>{dir}/scp/project.scp," ++
    "101:do=copy:{fix}/scp/sprite-1frame.tga>{dir}/scp/sprite-1frame.tga," ++
    "102:open={dir}/scp/project.scp," ++
    "103:expect=kind:scp," ++
    "104:do=squad_drag:0/25/-15," ++
    "105:expect=slot:0=moved," ++
    "106:do=undo," ++
    "107:expect=slot:0=home," ++
    "108:do=redo," ++
    "109:expect=slot:0=moved," ++
    "110:do=squad_zero:5/5," ++
    "111:do=squad_dir:1.0," ++
    "112:expect=direction:1.0," ++
    "113:do=squad_arrow:5/15," ++
    "114:expect=squad_dir:0," ++
    "115:do=squad_arrow:15/5," ++
    "116:expect=squad_dir:-1.5707964," ++
    "117:do=undo," ++
    "118:expect=squad_dir:0," ++
    "119:do=undo," ++
    "120:expect=squad_dir:1.0," ++
    "121:do=undo," ++
    "122:do=undo," ++
    "123:do=redo," ++
    "124:do=redo," ++
    "125:expect=direction:1.0," ++
    "126:shot=squad," ++
    "127:expect=shot_lit:squad," ++
    "128:differ=trench/squad@0.05," ++
    "129:save," ++
    "130:do=export," ++
    "131:expect=exported,";

/// S07 Sprite: Run and Stop of the preview measured, thumbnails, export.
const resource_auto_spt =
    // S07 Sprite (SpriteFrm): the frame folder pointed at, a thumbnail
    // double-click, saved and exported (1.san + DDS), then Run and Stop of the
    // preview measured: the running frames differ, the stopped ones are equal.
    "139:do=preview_on," ++
    "140:do=mod_dir:{mods}/reseditor_auto_s07," ++
    "141:do=copy:{fix}/spt/project.spt>{dir}/spt/project.spt," ++
    "142:do=copy:{fix}/spt/sprite-1frame.tga>{dir}/spt/frames/sprite-1frame.tga," ++
    "142:do=copy:{fix}/mcp/art-16x16.tga>{dir}/spt/frames/art-16x16.tga," ++
    "143:open={dir}/spt/project.spt," ++
    "144:expect=kind:spt," ++
    "145:do=set_prop:Directory=frames\\," ++
    "146:do=frame:sprite-1frame," ++
    "146:do=frame:art-16x16," ++
    "147:expect=dirty:true," ++
    "148:save," ++
    "149:do=export," ++
    "150:expect=exported," ++
    "151:do=preview_run," ++
    "152:do=pause:100," ++
    "153:shot=sprite_a," ++
    "154:do=pause:150," ++
    "154:shot=sprite_b," ++
    "154:differ=sprite_a/sprite_b@0.001," ++
    "155:do=preview_stop," ++
    "156:do=pause:100," ++
    "157:shot=sprite_c," ++
    "158:do=pause:200," ++
    "158:shot=sprite_d," ++
    "158:expect=shot_same:sprite_c/sprite_d," ++
    "159:expect=shot_lit:sprite_d," ++
    "160:do=undo," ++
    "161:expect=dirty:true," ++
    "162:do=redo," ++
    "163:expect=dirty:false," ++
    "164:do=delete_frame," ++
    "165:expect=dirty:true," ++
    "166:do=undo," ++
    "167:expect=dirty:false,";

/// S07 Infantry (.unt): season directory, export, Run and Stop of the preview.
const resource_auto_unt =
    // S07 Infantry (AnimationFrm): a season directory set, exported (1.xml,
    // 1[b][w|a].san + DDS), Run and Stop of the preview, undo and redo.
    "170:do=mod_dir:{mods}/reseditor_auto_s07," ++
    "171:do=copy:{fix}/unt/project.unt>{dir}/unt/project.unt," ++
    "172:open={dir}/unt/project.unt," ++
    "173:expect=kind:unt," ++
    "174:do=set_prop:Directory=frames\\," ++
    "174:do=copy:{fix}/spt/sprite-1frame.tga>{dir}/unt/frames/sprite-1frame.tga," ++
    "174:do=frame:sprite-1frame," ++
    "175:save," ++
    "176:do=export," ++
    "177:expect=exported," ++
    "178:do=preview_run," ++
    "179:do=pause:150," ++
    "180:shot=infantry," ++
    "181:expect=shot_lit:infantry," ++
    "182:do=preview_stop," ++
    "183:do=undo," ++
    "184:expect=dirty:true," ++
    "185:do=redo," ++
    "186:expect=dirty:false,";

/// S08 Unit: Common value, the three model variants, locators, export and the real Game.
const resource_auto_msh =
    // S08 Unit (MeshFrm): a copy of the tracked fixture unit (its models and
    // pictures beside it) opened, a Common value edited, undone and redone, the
    // three model variants of the preview shot and differed, the locators shown
    // and one picked at its screen point (the tree selects its node), then
    // saved, exported (1.xml, the .mod copies) and played in the real Game.
    "190:do=mod_dir:{mods}/reseditor_auto_s08," ++
    "191:do=copy:{fix}/msh/project.msh>{dir}/msh/project.msh," ++
    "191:do=copy:{fix}/msh/1.mod>{dir}/msh/1.mod," ++
    "191:do=copy:{fix}/msh/2.mod>{dir}/msh/2.mod," ++
    "191:do=copy:{fix}/msh/3.mod>{dir}/msh/3.mod," ++
    "191:do=copy:{fix}/msh/1.tga>{dir}/msh/1.tga," ++
    "191:do=copy:{fix}/msh/1w.tga>{dir}/msh/1w.tga," ++
    "191:do=copy:{fix}/msh/1a.tga>{dir}/msh/1a.tga," ++
    "191:do=copy:{fix}/msh/2.tga>{dir}/msh/2.tga," ++
    "191:do=copy:{fix}/msh/2w.tga>{dir}/msh/2w.tga," ++
    "191:do=copy:{fix}/msh/2a.tga>{dir}/msh/2a.tga," ++
    "191:do=copy:{fix}/msh/icon.tga>{dir}/msh/icon.tga," ++
    "191:do=copy:{fix}/msh/name.txt>{dir}/msh/name.txt," ++
    "191:do=copy:{fix}/msh/desc.txt>{dir}/msh/desc.txt," ++
    "192:open={dir}/msh/project.msh," ++
    "193:expect=kind:msh," ++
    "194:expect=nodes_min:20," ++
    "195:expect=dirty:false," ++
    "196:do=set_prop:Health=120," ++
    "197:expect=prop:Health=120," ++
    "198:expect=dirty:true," ++
    "199:do=undo," ++
    "200:expect=prop:Health=100," ++
    "201:do=redo," ++
    "202:expect=prop:Health=120," ++
    "203:do=preview_run," ++
    "204:do=pause:200," ++
    "205:do=mesh_variant:0," ++
    "206:do=pause:150," ++
    "206:shot=unit_combat," ++
    "207:expect=shot_lit:unit_combat," ++
    "208:do=mesh_variant:1," ++
    "209:do=pause:150," ++
    "209:shot=unit_install," ++
    "210:expect=shot_lit:unit_install," ++
    "211:do=mesh_variant:2," ++
    "212:do=pause:150," ++
    "212:shot=unit_transportable," ++
    "213:expect=shot_lit:unit_transportable," ++
    "214:differ=unit_combat/unit_transportable@0.1," ++
    "214:differ=unit_combat/unit_install@0.005," ++
    "215:do=mesh_variant:0," ++
    "216:do=locators:1," ++
    "217:do=pause:150," ++
    "217:shot=unit_locators," ++
    "218:differ=unit_combat/unit_locators@0.001," ++
    "219:do=pick_locator:LMainGun," ++
    "220:expect=selected:LMainGun," ++
    "221:do=pick_locator:Turret," ++
    "222:expect=selected:Turret," ++
    "223:expect=dirty:true," ++
    "224:save," ++
    "225:expect=dirty:false," ++
    "226:do=export," ++
    "227:expect=exported," ++
    "228:expect=file:{mods}/reseditor_auto_s08/data/units/technics/msh/1.xml," ++
    "228:expect=file:{mods}/reseditor_auto_s08/data/units/technics/msh/1.mod," ++
    "229:do=preview_stop," ++
    "230:do=run_game," ++
    "231:waitgame=240,";

/// S09 Object: locked, transparency and one-way grid edits measured in the shots.
const resource_auto_obt =
    // S09 Object (ObjectFrm): a copy of the tracked fixture and its art opened, a locked tile, a
    // transparency tile and a one-way line drawn, the zero point moved, each checked in the stored
    // grid before and after, then undone and redone, saved and exported. The preview shots are
    // measured: a locked tile is 0xff0000, transparency value 3 is 0x606000 (GridFrm's colours).
    "240:do=mod_dir:{mods}/reseditor_auto_s09o," ++
    "241:do=copy:{fix}/obt/project.obt>{dir}/obt/project.obt," ++
    "241:do=copy:{fix}/obt/1.tga>{dir}/obt/1.tga," ++
    "241:do=copy:{fix}/obt/1s.tga>{dir}/obt/1s.tga," ++
    "241:do=copy:{fix}/obt/1w.tga>{dir}/obt/1w.tga," ++
    "241:do=copy:{fix}/obt/1ws.tga>{dir}/obt/1ws.tga," ++
    "241:do=copy:{fix}/obt/1a.tga>{dir}/obt/1a.tga," ++
    "241:do=copy:{fix}/obt/1as.tga>{dir}/obt/1as.tga," ++
    "242:open={dir}/obt/project.obt," ++
    "243:expect=kind:obt," ++
    "243:expect=nodes_min:2," ++
    "244:expect=dirty:false," ++
    "244:expect=grid_cell:5/5=0," ++
    "244:expect=trans_cell:6/4=0," ++
    "244:expect=lines:2," ++
    "246:shot=obt_base," ++
    "247:expect=shot_colour:obt_base/ff0000/max/1750," ++
    "247:expect=shot_colour:obt_base/606000/max/0," ++
    "248:do=grid_cell:5/5/1," ++
    "249:expect=grid_cell:5/5=1," ++
    "249:expect=dirty:true," ++
    "250:do=grid_trans:6/4/3," ++
    "251:expect=trans_cell:6/4=3," ++
    "252:do=trans_line:4/8/7/8," ++
    "253:expect=lines:3," ++
    "256:shot=obt_drawn," ++
    "257:expect=shot_colour:obt_drawn/ff0000/min/1850," ++
    "257:expect=shot_colour:obt_drawn/606000/min/150," ++
    "258:do=grid_zero:3/3," ++
    "259:expect=zero_tile:3/3," ++
    "260:do=undo," ++
    "261:expect=zero_tile:0/0," ++
    "262:do=undo," ++
    "263:expect=lines:2," ++
    "264:do=undo," ++
    "265:expect=trans_cell:6/4=0," ++
    "266:do=undo," ++
    "267:expect=grid_cell:5/5=0," ++
    "268:expect=dirty:false," ++
    "270:shot=obt_undone," ++
    "271:expect=shot_colour:obt_undone/ff0000/max/1750," ++
    "271:expect=shot_colour:obt_undone/606000/max/0," ++
    "271:differ=obt_drawn/obt_undone@0.01," ++
    "272:do=redo," ++
    "272:do=redo," ++
    "272:do=redo," ++
    "272:do=redo," ++
    "276:expect=grid_cell:5/5=1," ++
    "276:expect=trans_cell:6/4=3," ++
    "276:expect=lines:3," ++
    "276:expect=zero_tile:3/3," ++
    "277:save," ++
    "278:expect=dirty:false," ++
    "279:do=export," ++
    "280:expect=exported," ++
    "281:expect=file:{mods}/reseditor_auto_s09o/data/objects/obt/1.xml," ++
    "281:expect=file:{mods}/reseditor_auto_s09o/data/objects/obt/1_c.dds," ++
    "284:shot=obt_saved," ++
    "285:expect=shot_lit:obt_saved,";

/// S09 Fence: the first segment's grid and the sprite's tile.
const resource_auto_fnc =
    // S09 Fence (FenceFrm): the first segment's locked tile and transparency tile drawn, the sprite
    // centred on a tile, each checked before and after, undone, redone, saved and exported.
    "290:do=mod_dir:{mods}/reseditor_auto_s09f," ++
    "291:do=copy:{fix}/fnc/project.fnc>{dir}/fnc/project.fnc," ++
    "291:do=copy:{fix}/fnc/fences/art-16x16.tga>{dir}/fnc/fences/art-16x16.tga," ++
    "291:do=copy:{fix}/fnc/fences/art-16x16s.tga>{dir}/fnc/fences/art-16x16s.tga," ++
    "291:do=copy:{fix}/fnc/fences/ne-left.tga>{dir}/fnc/fences/ne-left.tga," ++
    "291:do=copy:{fix}/fnc/fences/ne-lefts.tga>{dir}/fnc/fences/ne-lefts.tga," ++
    "291:do=copy:{fix}/fnc/fences/nw.tga>{dir}/fnc/fences/nw.tga," ++
    "291:do=copy:{fix}/fnc/fences/nws.tga>{dir}/fnc/fences/nws.tga," ++
    "291:do=copy:{fix}/fnc/fences/sw.tga>{dir}/fnc/fences/sw.tga," ++
    "291:do=copy:{fix}/fnc/fences/sws.tga>{dir}/fnc/fences/sws.tga," ++
    "291:do=copy:{fix}/fnc/fences/se.tga>{dir}/fnc/fences/se.tga," ++
    "291:do=copy:{fix}/fnc/fences/ses.tga>{dir}/fnc/fences/ses.tga," ++
    "292:open={dir}/fnc/project.fnc," ++
    "293:expect=kind:fnc," ++
    "293:expect=nodes_min:2," ++
    "294:expect=dirty:false," ++
    "294:expect=grid_cell:19/17=0," ++
    "294:expect=trans_cell:18/19=0," ++
    "294:expect=sprite_tile:16/16," ++
    "296:shot=fnc_base," ++
    "297:expect=shot_colour:fnc_base/ff0000/max/0," ++
    "297:expect=shot_colour:fnc_base/808000/max/0," ++
    "298:do=grid_cell:19/17/1," ++
    "299:expect=grid_cell:19/17=1," ++
    "299:expect=dirty:true," ++
    "300:do=grid_trans:18/19/4," ++
    "301:expect=trans_cell:18/19=4," ++
    "302:do=fence_centre:20/20," ++
    "303:expect=sprite_tile:20/20," ++
    "306:shot=fnc_drawn," ++
    "307:expect=shot_colour:fnc_drawn/ff0000/min/200," ++
    "307:expect=shot_colour:fnc_drawn/808000/min/150," ++
    "308:do=undo," ++
    "309:expect=sprite_tile:16/16," ++
    "310:do=undo," ++
    "311:expect=trans_cell:18/19=0," ++
    "312:do=undo," ++
    "313:expect=grid_cell:19/17=0," ++
    "314:expect=dirty:false," ++
    "316:shot=fnc_undone," ++
    "317:expect=shot_colour:fnc_undone/ff0000/max/0," ++
    "317:expect=shot_colour:fnc_undone/808000/max/0," ++
    "317:differ=fnc_drawn/fnc_undone@0.01," ++
    "318:do=redo," ++
    "319:do=redo," ++
    "320:do=redo," ++
    "321:expect=grid_cell:19/17=1," ++
    "321:expect=trans_cell:18/19=4," ++
    "321:expect=sprite_tile:20/20," ++
    "322:save," ++
    "323:expect=dirty:false," ++
    "324:do=export," ++
    "325:expect=exported," ++
    "326:expect=file:{mods}/reseditor_auto_s09f/data/fences/fnc/1.xml," ++
    "326:expect=file:{mods}/reseditor_auto_s09f/data/fences/fnc/1_c.dds," ++
    "329:shot=fnc_saved," ++
    "330:expect=shot_lit:fnc_saved,";

/// S10 Building: grid, entrance, zero point and the point families.
const resource_auto_bld =
    // S10 Building (BuildFrm): a copy of the tracked fixture and its art opened, locked and
    // transparency tiles, the entrance and the zero point set, one point of each family placed
    // (the directed explosions generated), one turned, then undone, redone, saved and exported.
    // The shots are measured: a locked tile is 0xff0000, transparency value 3 is 0x606000, the
    // entrance 0x00ff00, the active fire point 0xff8000, the active directed explosion 0xff00ff,
    // the active point's cone edges and direction line 0xffff00.
    "340:do=mod_dir:{mods}/reseditor_auto_s10," ++
    "341:do=copy:{fix}/bld/project.bld>{dir}/bld/project.bld," ++
    "341:do=copy:{fix}/bld/1.tga>{dir}/bld/1.tga," ++
    "341:do=copy:{fix}/bld/1s.tga>{dir}/bld/1s.tga," ++
    "341:do=copy:{fix}/bld/1w.tga>{dir}/bld/1w.tga," ++
    "341:do=copy:{fix}/bld/1ws.tga>{dir}/bld/1ws.tga," ++
    "341:do=copy:{fix}/bld/2.tga>{dir}/bld/2.tga," ++
    "341:do=copy:{fix}/bld/2g.tga>{dir}/bld/2g.tga," ++
    "341:do=copy:{fix}/bld/2s.tga>{dir}/bld/2s.tga," ++
    "341:do=copy:{fix}/bld/2w.tga>{dir}/bld/2w.tga," ++
    "341:do=copy:{fix}/bld/2wg.tga>{dir}/bld/2wg.tga," ++
    "341:do=copy:{fix}/bld/2ws.tga>{dir}/bld/2ws.tga," ++
    "341:do=copy:{fix}/bld/3.tga>{dir}/bld/3.tga," ++
    "341:do=copy:{fix}/bld/3g.tga>{dir}/bld/3g.tga," ++
    "341:do=copy:{fix}/bld/3s.tga>{dir}/bld/3s.tga," ++
    "341:do=copy:{fix}/bld/3w.tga>{dir}/bld/3w.tga," ++
    "341:do=copy:{fix}/bld/3wg.tga>{dir}/bld/3wg.tga," ++
    "341:do=copy:{fix}/bld/3ws.tga>{dir}/bld/3ws.tga," ++
    "341:do=copy:{fix}/bld/art-16x16.tga>{dir}/bld/art-16x16.tga," ++
    "342:open={dir}/bld/project.bld," ++
    "343:expect=kind:bld," ++
    "343:expect=nodes_min:2," ++
    "344:expect=dirty:false," ++
    "346:shot=bld_base," ++
    "347:expect=shot_colour:bld_base/ff0000/max/20," ++
    "347:expect=shot_colour:bld_base/606000/max/0," ++
    "347:expect=shot_colour:bld_base/c0c0c0/max/12500," ++
    "348:do=grid_cell:28/28/1," ++
    "348:do=grid_cell:29/28/1," ++
    "349:do=grid_trans:30/28/3," ++
    "350:do=entrance:31/31," ++
    "351:do=grid_zero:30/30," ++
    "352:expect=grid_cell:28/28=1," ++
    "352:expect=trans_cell:30/28=3," ++
    "352:expect=entrance_tile:31/31," ++
    "352:expect=zero_tile:30/30," ++
    "352:expect=dirty:true," ++
    "355:shot=bld_tiles," ++
    "356:expect=shot_colour:bld_tiles/ff0000/min/380," ++
    "356:expect=shot_colour:bld_tiles/606000/min/150," ++
    "357:do=point:shoot/33/30," ++
    "358:expect=points:shoot=1," ++
    "359:do=point:fire/35/30," ++
    "360:expect=points:fire=1," ++
    "362:shot=bld_fire," ++
    "363:expect=shot_colour:bld_fire/ff8000/min/40," ++
    "364:do=point:smoke/33/33," ++
    "365:expect=points:smoke=1," ++
    "366:do=generate_points:smoke," ++
    "367:expect=points:smoke=2," ++
    "368:do=point_select:smoke/0," ++
    "370:shot=bld_dir," ++
    "371:expect=shot_colour:bld_dir/c0c0c0/min/20000," ++
    "372:do=point_select:shoot/0," ++
    "373:do=point_angle:0/90," ++
    "374:do=point_cone:0/40," ++
    "375:expect=point:shoot/0=90/40," ++
    "376:do=point_move:0/34/31," ++
    "378:shot=bld_points," ++
    "379:expect=shot_colour:bld_points/ffff00/min/25," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "380:do=undo," ++
    "382:expect=points:shoot=0," ++
    "382:expect=points:fire=0," ++
    "382:expect=points:smoke=0," ++
    "382:expect=grid_cell:28/28=0," ++
    "382:expect=dirty:false," ++
    "384:shot=bld_undone," ++
    "385:expect=shot_colour:bld_undone/ff0000/max/20," ++
    "385:expect=shot_colour:bld_undone/606000/max/0," ++
    "385:expect=shot_colour:bld_undone/ff8000/max/0," ++
    "385:expect=shot_colour:bld_undone/c0c0c0/max/12500," ++
    "385:expect=shot_colour:bld_undone/ffff00/max/0," ++
    "385:differ=bld_tiles/bld_undone@0.01," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "386:do=redo," ++
    "390:expect=points:shoot=1," ++
    "390:expect=points:fire=1," ++
    "390:expect=points:smoke=2," ++
    "390:expect=point:shoot/0=90/40," ++
    "391:save," ++
    "392:expect=dirty:false," ++
    "393:do=export," ++
    "394:expect=exported," ++
    "397:shot=bld_saved," ++
    "398:expect=shot_lit:bld_saved,";

/// S11 Bridge: span marks, grid and the fire and smoke points.
const resource_auto_bdg =
    // S11 Bridge (BridgeFrm): a copy of the tracked fixture and its art opened, two locked tiles of the
    // active span part, the four span marks, a fire and a smoke point placed, all undone and redone, then
    // saved and exported. The shots are measured: the red bridge line and the locked tiles are 0xff0000,
    // the span-mark crosses 0x00ffff, the active fire point 0xff8000, the active smoke point 0xc0c0c0.
    "410:do=mod_dir:{mods}/reseditor_auto_s11," ++
    "411:do=copy:{fix}/bdg/1-begin-back.tga>{dir}/bdg/1-begin-back.tga," ++
    "411:do=copy:{fix}/bdg/1-begin-backs.tga>{dir}/bdg/1-begin-backs.tga," ++
    "411:do=copy:{fix}/bdg/1-begin-front.tga>{dir}/bdg/1-begin-front.tga," ++
    "411:do=copy:{fix}/bdg/1-begin-fronts.tga>{dir}/bdg/1-begin-fronts.tga," ++
    "411:do=copy:{fix}/bdg/1-begin-slab.tga>{dir}/bdg/1-begin-slab.tga," ++
    "411:do=copy:{fix}/bdg/1-begin-slabs.tga>{dir}/bdg/1-begin-slabs.tga," ++
    "411:do=copy:{fix}/bdg/1-center-back.tga>{dir}/bdg/1-center-back.tga," ++
    "411:do=copy:{fix}/bdg/1-center-backs.tga>{dir}/bdg/1-center-backs.tga," ++
    "411:do=copy:{fix}/bdg/1-center-front.tga>{dir}/bdg/1-center-front.tga," ++
    "411:do=copy:{fix}/bdg/1-center-fronts.tga>{dir}/bdg/1-center-fronts.tga," ++
    "411:do=copy:{fix}/bdg/1-center-slab.tga>{dir}/bdg/1-center-slab.tga," ++
    "411:do=copy:{fix}/bdg/1-center-slabs.tga>{dir}/bdg/1-center-slabs.tga," ++
    "411:do=copy:{fix}/bdg/1-end-back.tga>{dir}/bdg/1-end-back.tga," ++
    "411:do=copy:{fix}/bdg/1-end-backs.tga>{dir}/bdg/1-end-backs.tga," ++
    "411:do=copy:{fix}/bdg/1-end-front.tga>{dir}/bdg/1-end-front.tga," ++
    "411:do=copy:{fix}/bdg/1-end-fronts.tga>{dir}/bdg/1-end-fronts.tga," ++
    "411:do=copy:{fix}/bdg/1-end-slab.tga>{dir}/bdg/1-end-slab.tga," ++
    "411:do=copy:{fix}/bdg/1-end-slabs.tga>{dir}/bdg/1-end-slabs.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-back.tga>{dir}/bdg/2-begin-back.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-backs.tga>{dir}/bdg/2-begin-backs.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-front.tga>{dir}/bdg/2-begin-front.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-fronts.tga>{dir}/bdg/2-begin-fronts.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-slab.tga>{dir}/bdg/2-begin-slab.tga," ++
    "411:do=copy:{fix}/bdg/2-begin-slabs.tga>{dir}/bdg/2-begin-slabs.tga," ++
    "411:do=copy:{fix}/bdg/2-center-back.tga>{dir}/bdg/2-center-back.tga," ++
    "411:do=copy:{fix}/bdg/2-center-backs.tga>{dir}/bdg/2-center-backs.tga," ++
    "411:do=copy:{fix}/bdg/2-center-front.tga>{dir}/bdg/2-center-front.tga," ++
    "411:do=copy:{fix}/bdg/2-center-fronts.tga>{dir}/bdg/2-center-fronts.tga," ++
    "411:do=copy:{fix}/bdg/2-center-slab.tga>{dir}/bdg/2-center-slab.tga," ++
    "411:do=copy:{fix}/bdg/2-center-slabs.tga>{dir}/bdg/2-center-slabs.tga," ++
    "411:do=copy:{fix}/bdg/2-end-back.tga>{dir}/bdg/2-end-back.tga," ++
    "411:do=copy:{fix}/bdg/2-end-backs.tga>{dir}/bdg/2-end-backs.tga," ++
    "411:do=copy:{fix}/bdg/2-end-front.tga>{dir}/bdg/2-end-front.tga," ++
    "411:do=copy:{fix}/bdg/2-end-fronts.tga>{dir}/bdg/2-end-fronts.tga," ++
    "411:do=copy:{fix}/bdg/2-end-slab.tga>{dir}/bdg/2-end-slab.tga," ++
    "411:do=copy:{fix}/bdg/2-end-slabs.tga>{dir}/bdg/2-end-slabs.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-back.tga>{dir}/bdg/3-begin-back.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-backs.tga>{dir}/bdg/3-begin-backs.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-front.tga>{dir}/bdg/3-begin-front.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-fronts.tga>{dir}/bdg/3-begin-fronts.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-slab.tga>{dir}/bdg/3-begin-slab.tga," ++
    "411:do=copy:{fix}/bdg/3-begin-slabs.tga>{dir}/bdg/3-begin-slabs.tga," ++
    "411:do=copy:{fix}/bdg/3-center-back.tga>{dir}/bdg/3-center-back.tga," ++
    "411:do=copy:{fix}/bdg/3-center-backs.tga>{dir}/bdg/3-center-backs.tga," ++
    "411:do=copy:{fix}/bdg/3-center-front.tga>{dir}/bdg/3-center-front.tga," ++
    "411:do=copy:{fix}/bdg/3-center-fronts.tga>{dir}/bdg/3-center-fronts.tga," ++
    "411:do=copy:{fix}/bdg/3-center-slab.tga>{dir}/bdg/3-center-slab.tga," ++
    "411:do=copy:{fix}/bdg/3-center-slabs.tga>{dir}/bdg/3-center-slabs.tga," ++
    "411:do=copy:{fix}/bdg/3-end-back.tga>{dir}/bdg/3-end-back.tga," ++
    "411:do=copy:{fix}/bdg/3-end-backs.tga>{dir}/bdg/3-end-backs.tga," ++
    "411:do=copy:{fix}/bdg/3-end-front.tga>{dir}/bdg/3-end-front.tga," ++
    "411:do=copy:{fix}/bdg/3-end-fronts.tga>{dir}/bdg/3-end-fronts.tga," ++
    "411:do=copy:{fix}/bdg/3-end-slab.tga>{dir}/bdg/3-end-slab.tga," ++
    "411:do=copy:{fix}/bdg/3-end-slabs.tga>{dir}/bdg/3-end-slabs.tga," ++
    "411:do=copy:{fix}/bdg/art-16x16.tga>{dir}/bdg/art-16x16.tga," ++
    "411:do=copy:{fix}/bdg/project.bdg>{dir}/bdg/project.bdg," ++
    "412:open={dir}/bdg/project.bdg," ++
    "413:expect=kind:bdg," ++
    "413:expect=nodes_min:2," ++
    "414:expect=dirty:false," ++
    "416:shot=bdg_base," ++
    "417:expect=shot_colour:bdg_base/ff0000/max/300," ++
    "417:expect=shot_colour:bdg_base/00ffff/min/150," ++
    "417:expect=shot_colour:bdg_base/ff8000/max/0," ++
    "417:expect=span_mark:begin=home," ++
    "417:expect=span_mark:front=home," ++
    "418:do=grid_cell:10/10/1," ++
    "418:do=grid_cell:11/10/1," ++
    "419:expect=grid_cell:10/10=1," ++
    "419:expect=dirty:true," ++
    "421:shot=bdg_tiles," ++
    "422:expect=shot_colour:bdg_tiles/ff0000/min/550," ++
    "423:do=span_mark:begin/17/17," ++
    "423:do=span_mark:end/20/17," ++
    "424:do=span_mark:front/18/19," ++
    "424:do=span_mark:back/18/15," ++
    "425:expect=span_mark:begin=moved," ++
    "425:expect=span_mark:end=moved," ++
    "425:expect=span_mark:front=moved," ++
    "425:expect=span_mark:back=moved," ++
    "427:shot=bdg_marks," ++
    "428:expect=shot_colour:bdg_marks/00ffff/min/300," ++
    "429:do=point:fire/13/10," ++
    "430:expect=points:fire=1," ++
    "431:shot=bdg_fire," ++
    "432:expect=shot_colour:bdg_fire/ff8000/min/40," ++
    "433:do=point:smoke/12/13," ++
    "434:expect=points:smoke=1," ++
    "436:shot=bdg_points," ++
    "437:differ=bdg_fire/bdg_points@0.0001," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "440:do=undo," ++
    "442:expect=points:fire=0," ++
    "442:expect=points:smoke=0," ++
    "442:expect=grid_cell:10/10=0," ++
    "442:expect=span_mark:begin=home," ++
    "442:expect=span_mark:front=home," ++
    "442:expect=dirty:false," ++
    "444:shot=bdg_undone," ++
    "445:expect=shot_colour:bdg_undone/ff0000/max/300," ++
    "445:expect=shot_colour:bdg_undone/00ffff/min/150," ++
    "445:expect=shot_colour:bdg_undone/ff8000/max/0," ++
    "445:differ=bdg_tiles/bdg_undone@0.01," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "446:do=redo," ++
    "450:expect=points:fire=1," ++
    "450:expect=points:smoke=1," ++
    "450:expect=grid_cell:11/10=1," ++
    "450:expect=span_mark:end=moved," ++
    "451:save," ++
    "452:expect=dirty:false," ++
    "453:do=export," ++
    "454:expect=exported," ++
    "455:expect=file:{mods}/reseditor_auto_s11/data/bridges/bdg/1.xml," ++
    "455:expect=file:{mods}/reseditor_auto_s11/data/bridges/bdg/1_c.dds," ++
    "457:shot=bdg_saved," ++
    "458:expect=shot_lit:bdg_saved,";

/// S12 Particle (with S13's source toggle and the Function window's pointer path).
const resource_auto_pcp =
    // S12 Particle (ParticleFrm) and Effect (EffectFrm): a copy of the tracked .pcp opened, its Opacity
    // curve edited in the Function window's editor (add, move, delete, Reset all, each undone and redone with
    // the keys read back through the bridge; the zoom steps), saved and exported, then Run, Stop and Camera
    // of the preview measured (the running frames differ, the stopped ones are equal, the horizontal camera
    // draws another frame) and the curve edited again. A shipped particle imports; the .eff imports not
    // (MFC has no reverse path); an .eff exports next to its source and its Run names the missing source.
    "480:do=mod_dir:{mods}/reseditor_auto12," ++
    "481:do=copy:{fix}/pcp/project.pcp>{dir}/particle-2key/project.pcp," ++
    "482:open={dir}/particle-2key/project.pcp," ++
    "483:expect=kind:pcp," ++
    "483:expect=nodes_min:5," ++
    "484:do=curve:Opacity," ++
    "484:expect=keys:1," ++
    "485:do=keyframe:add/0.5/200," ++
    "485:expect=keys:2," ++
    "485:expect=key:1=0.5/200," ++
    "485:expect=dirty:true," ++
    "486:do=keyframe:add/0.8/50," ++
    "486:expect=keys:3," ++
    "486:expect=key:2=0.8/50," ++
    "487:do=undo," ++
    "487:expect=keys:2," ++
    "488:do=redo," ++
    "488:expect=keys:3," ++
    "488:expect=key:2=0.8/50," ++
    "489:do=keyframe:move/1/0.4/120," ++
    "489:expect=keys:3," ++
    "489:expect=key:1=0.4/120," ++
    "490:do=undo," ++
    "490:expect=key:1=0.5/200," ++
    "491:do=redo," ++
    "491:expect=key:1=0.4/120," ++
    "492:do=keyframe:delete/2," ++
    "492:expect=keys:2," ++
    "493:do=undo," ++
    "493:expect=keys:3," ++
    "494:do=redo," ++
    "494:expect=keys:2," ++
    "495:do=keyframe:add/0.9/30," ++
    "495:expect=keys:3," ++
    "496:do=keyframe:reset," ++
    "496:expect=keys:1," ++
    "497:do=undo," ++
    "497:expect=keys:3," ++
    "498:do=redo," ++
    "498:expect=keys:1," ++
    "499:do=undo," ++
    "499:expect=keys:3," ++
    "500:do=keyframe:zoomy_out," ++
    "500:expect=zoom:29/20," ++
    "500:do=keyframe:zoomy_in," ++
    "500:expect=zoom:29/25," ++
    "500:do=keyframe:zoomy_in," ++
    "500:expect=zoom:29/50," ++
    "500:expect=keys:3," ++
    "500:expect=dirty:true," ++
    "501:save," ++
    "501:expect=dirty:false," ++
    "502:do=export," ++
    "502:expect=exported," ++
    "502:expect=file:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "503:shot=pcp_idle," ++
    "504:do=curve:Life," ++
    "504:do=keyframe:zoomx_in," ++
    "504:expect=zoom:29/25," ++
    "504:do=keyframe:zoomx_out," ++
    "504:do=keyframe:zoomx_out," ++
    "504:expect=zoom:29/25," ++
    "504:do=curve:Opacity," ++
    "505:do=particle_info," ++
    "505:expect=particle_info:present," ++
    // S13 T02: the Particle source toggle. The mode the bridge reads equals the flag each export writes, in
    // both directions, and undo and redo flip it back and forth; the project ends simple, as it began.
    "506:expect=source_mode:simple," ++
    "506:do=export," ++
    "506:expect=export_simple:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "507:do=source_mode:complex," ++
    "507:expect=source_mode:complex," ++
    "507:do=export," ++
    "507:expect=export_complex:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "508:do=undo," ++
    "508:expect=source_mode:simple," ++
    "508:do=export," ++
    "508:expect=export_simple:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "509:do=redo," ++
    "509:expect=source_mode:complex," ++
    "509:do=export," ++
    "509:expect=export_complex:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "509:do=source_mode:simple," ++
    "509:expect=source_mode:simple," ++
    "509:do=export," ++
    "509:expect=export_simple:{mods}/reseditor_auto12/data/effects/particles/particle-2key.xml," ++
    "510:do=preview_run," ++
    "512:do=pause:300," ++
    "512:shot=pcp_run_a," ++
    "513:do=pause:400," ++
    "513:shot=pcp_run_b," ++
    "513:differ=pcp_run_a/pcp_run_b@0.05," ++
    "514:do=preview_stop," ++
    "515:do=pause:100," ++
    "516:shot=pcp_stop_c," ++
    "517:do=pause:300," ++
    "517:shot=pcp_stop_d," ++
    "517:expect=shot_same:pcp_stop_c/pcp_stop_d," ++
    "517:expect=shot_lit:pcp_stop_d," ++
    "518:do=camera," ++
    "518:expect=camera:horizontal," ++
    "519:do=pause:100," ++
    "519:shot=pcp_cam_h," ++
    "519:differ=pcp_stop_d/pcp_cam_h@0.02," ++
    "520:do=camera," ++
    "520:expect=camera:default," ++
    "521:do=pause:100," ++
    "521:shot=pcp_cam_d," ++
    "521:differ=pcp_cam_h/pcp_cam_d@0.02," ++
    "522:do=keyframe:move/0/0/255," ++
    "522:expect=key:0=0/255," ++
    "523:do=preview_run," ++
    "524:do=pause:300," ++
    "524:shot=pcp_edit," ++
    "524:expect=shot_lit:pcp_edit," ++
    "524:differ=pcp_idle/pcp_edit@0.05," ++
    "525:do=preview_stop," ++
    // S13 T04: the same Opacity curve edited through the displayed Function window with real pointer and key
    // events (Ctrl+F opens it): a click on empty graph space adds a key, a drag over four frames moves it, a click
    // then the Delete key removes it. Each gesture reads the stored keys back through the bridge, then undoes and
    // redoes them; the widget's key handle is measured in the captured frame at its drawn place.
    "525:do=function_open," ++
    "525:expect=keys:3," ++
    "526:do=curve_click:0.6/150," ++
    "526:expect=keys:4," ++
    "527:shot=pcp_fn_add," ++
    "527:expect=shot_curve_handle:pcp_fn_add/2," ++
    "528:do=curve_drag:2/0.65/90," ++
    "528:expect=keys:4," ++
    "529:shot=pcp_fn_drag," ++
    "529:expect=shot_curve_handle:pcp_fn_drag/2," ++
    "530:do=curve_delete:2," ++
    "530:expect=keys:3," ++
    "531:shot=pcp_fn_delete," ++
    "531:expect=shot_lit:pcp_fn_delete," ++
    "532:do=function_close," ++
    "533:do=import_file:pcp/{mods}/../Data/Effects/Particles/flame.xml," ++
    "534:expect=kind:pcp," ++
    "534:expect=nodes_min:5," ++
    "534:expect=untitled," ++
    "534:do=copy:{fix}/pcp/project.pcp>{dir}/imported/seed.pcp," ++
    "535:saveas={dir}/imported/project.pcp," ++
    "536:do=export," ++
    "536:expect=exported," ++
    "536:expect=file:{mods}/reseditor_auto12/data/effects/particles/imported.xml," ++
    "537:do=import_refused:eff/{mods}/../Data/Effects/Particles/flame.xml,";

/// S12 Effect (with S13's whole-number position and the Direction dock).
const resource_auto_eff =
    "539:do=copy:{fix}/eff/project.eff>{dir}/eff-1/project.eff," ++
    "540:open={dir}/eff-1/project.eff," ++
    "541:expect=kind:eff," ++
    "541:expect=nodes_min:5," ++
    "542:do=export," ++
    "542:expect=exported," ++
    "543:do=preview_refused:particle-2key," ++
    // S13 T03: the Effect editor. A child's X position is a whole number: the edit reads back, is one undo
    // step and exports; the Direction dock's needle is view state (45 degrees on open, turned to 90 and back,
    // never dirty, never undone).
    "544:expect=effect_angle:0," ++
    "544:do=set_prop:X_position=120," ++
    "544:expect=prop:X_position=120," ++
    "544:expect=dirty:true," ++
    "544:do=effect_direction:90," ++
    "544:expect=effect_angle:90," ++
    "544:do=undo," ++
    "544:expect=prop:X_position=0," ++
    "544:expect=effect_angle:90," ++
    "544:do=redo," ++
    "544:expect=prop:X_position=120," ++
    "544:do=set_prop:X_position=7.9," ++
    "544:expect=prop:X_position=7," ++
    "544:do=effect_direction:0," ++
    "544:expect=effect_angle:0," ++
    "544:do=export," ++
    "544:expect=exported," ++
    "545:expect=file:{mods}/reseditor_auto12/data/effects/effects/eff-1.xml,";


/// S13 Terrain (.til, TileSetFrm): a copy of the tracked fixture opened, a tile added by the thumbnail
/// double-click and undone and redone with the item count asserted, the T08 terrains import, then the
/// export (tileset xml and its DDS atlas). The tileset has no preview (its GameWnd is hidden in MFC).
const resource_auto_til =
    "1:do=mod_dir:{mods}/reseditor_auto13_til," ++
    "2:do=copy:{fix}/til/project.til>{dir}/til/project.til," ++
    "2:do=copy:{fix}/til/art-16x16.tga>{dir}/til/art-16x16.tga," ++
    "3:open={dir}/til/project.til," ++
    "4:expect=kind:til," ++
    "4:expect=dirty:false," ++
    "4:expect=nodes_min:20," ++
    "5:do=tile_add:Added.tga," ++
    "6:expect=nodes:22," ++
    "6:expect=dirty:true," ++
    "7:do=undo," ++
    "8:expect=nodes:21," ++
    "9:do=redo," ++
    "10:expect=nodes:22," ++
    "11:do=copy:{fix}/til/import/terrains.xml>{dir}/til/import/terrains.xml," ++
    "11:do=copy:{fix}/til/import/terrains.tga>{dir}/til/import/terrains.tga," ++
    "12:do=tile_import:terrains/{dir}/til/import/terrains.xml," ++
    "13:expect=nodes:26," ++
    "13:expect=dirty:true," ++
    "14:do=export," ++
    "15:expect=exported," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/1.xml," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/1_c.dds," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/1_h.dds," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/1_l.dds," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/crosset.xml," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/terrain/sets/til/crosset_c.dds," ++
    "15:expect=file:{mods}/reseditor_auto13_til/data/mod.xml,";

/// S13 3D Road (.3rd, 3DRoadFrm): a property edit undone and redone, the preview on the maps\road3d terrain
/// (it shows without playing), the wireframe on and off with the frames measured, the export and the import
/// of a shipped road description (a single file).
const resource_auto_3rd =
    "1:do=mod_dir:{mods}/reseditor_auto13_3rd," ++
    "2:do=copy:{fix}/3rd/project.3rd>{dir}/3rd/project.3rd," ++
    "2:do=copy:{fix}/3rd/art-16x16.tga>{dir}/3rd/art-16x16.tga," ++
    "3:open={dir}/3rd/project.3rd," ++
    "4:expect=kind:3rd," ++
    "4:expect=dirty:false," ++
    "5:do=set_prop:Passability_coefficient=0.75," ++
    "6:expect=prop:Passability_coefficient=0.75," ++
    "6:expect=dirty:true," ++
    "7:do=undo," ++
    "8:do=redo," ++
    "9:expect=prop:Passability_coefficient=0.75," ++
    "10:do=export," ++
    "10:expect=exported," ++
    "11:do=preview_run," ++
    "13:do=pause:200," ++
    "14:shot=road_solid," ++
    "14:expect=shot_lit:road_solid," ++
    "15:do=wireframe:on," ++
    "16:do=pause:200," ++
    "17:shot=road_wire," ++
    "17:expect=shot_lit:road_wire," ++
    "17:differ=road_solid/road_wire@0.005," ++
    "18:do=wireframe:off," ++
    "19:do=pause:200," ++
    "20:shot=road_solid_again," ++
    "20:differ=road_wire/road_solid_again@0.005," ++
    "21:do=import_file:3rd/{mods}/../Data/Terrain/sets/1/Roads3D/rail_road_grass.xml," ++
    "22:expect=kind:3rd," ++
    "22:saveas={dir}/3rd/imported.3rd," ++
    "23:do=export," ++
    "23:expect=exported,";

/// S13 3D River (.3rv, 3DRiverFrm): the preview on the maps\river3d terrain, Run twice a pause apart (the
/// water animates), Stop, the wireframe, the export and the import of a shipped river description.
const resource_auto_3rv =
    "1:do=mod_dir:{mods}/reseditor_auto13_3rv," ++
    "2:do=copy:{fix}/3rv/project.3rv>{dir}/3rv/project.3rv," ++
    "2:do=copy:{fix}/3rv/art-16x16.tga>{dir}/3rv/art-16x16.tga," ++
    "3:open={dir}/3rv/project.3rv," ++
    "4:expect=kind:3rv," ++
    "4:expect=dirty:false," ++
    "5:do=export," ++
    "5:expect=exported," ++
    "6:do=preview_run," ++
    "8:do=pause:200," ++
    "9:shot=river_a," ++
    "9:expect=shot_lit:river_a," ++
    "10:do=pause:1500," ++
    "11:shot=river_b," ++
    "11:differ=river_a/river_b@0.0005," ++
    "12:do=preview_stop," ++
    "13:do=pause:100," ++
    "14:shot=river_c," ++
    "15:do=pause:300," ++
    "15:shot=river_d," ++
    "15:expect=shot_same:river_c/river_d," ++
    "16:do=wireframe:on," ++
    "17:do=pause:200," ++
    "18:shot=river_wire," ++
    "18:differ=river_d/river_wire@0.005," ++
    "19:do=wireframe:off," ++
    "20:do=import_file:3rv/{mods}/../Data/Terrain/sets/1/Rivers/water.xml," ++
    "21:expect=kind:3rv," ++
    "22:saveas={dir}/3rv/imported.3rv," ++
    "23:do=export," ++
    "23:expect=exported,";


/// A module of MapEditor's, with everything its executables link. The union
/// of two recipes: the engine half is addEditorBridgeTest's (the same static
/// libraries, imports and CRT), the ImGui half the overlay spike's (the
/// editor_imgui module, the MSVC library paths).
fn mapEditorModule(
    b: *std.Build,
    root_source: []const u8,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    sdl_module: *std.Build.Module,
    editor_imgui_module: *std.Build.Module,
    core_module: *std.Build.Module,
    kit_module: *std.Build.Module,
    engine: MapEditorEngine,
) *std.Build.Module {
    const module = b.createModule(.{
        .root_source_file = b.path(root_source),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl_module },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
            .{ .name = "editor_core", .module = core_module },
            .{ .name = "editor_kit", .module = kit_module },
        },
    });
    // bridge.h, for c_bridge.zig's @cImport.
    module.addIncludePath(b.path("Sources/src/EditorBridge"));
    // The M2 test script (04-10): game_reads_m2.zig embeds it, so the scenario
    // needs no path to the source tree at run time.
    module.addAnonymousImport("m2_script_lua", .{ .root_source_file = b.path("tools/zig/fixtures/m2_script.lua") });
    linkEditorEngine(b, module, target, optimize, toolchain, engine);
    return module;
}

/// What an editor executable links to host the engine, MapEditor's and
/// ResourceEditor's alike: the CRT the engine's statics want, the Windows
/// import libraries, the engine static libraries and SDL.
fn linkEditorEngine(
    b: *std.Build,
    module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    engine: MapEditorEngine,
) void {
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    // The engine's statics are built against the debug CRT in Debug, so the
    // executable links the one they want, as editor-bridge-test does; no
    // link_libc, which would have Zig bring a second CRT.
    linkMsvcRuntime(module, optimize);
    if (target.result.os.tag == .windows) {
        // addEditorBridgeTest's import list: the engine the game hosts.
        linkComSupport(module, optimize);
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        module.linkSystemLibrary("shlwapi", .{});
        module.linkSystemLibrary("advapi32", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("gdi32", .{});
        module.linkSystemLibrary("shell32", .{});
        // ImGui's default IME hook (imgui.cpp Platform_SetImeDataFn_DefaultImpl).
        module.linkSystemLibrary("imm32", .{});
    }
    module.linkLibrary(engine.editor_bridge);
    module.linkLibrary(engine.map_file);
    module.linkLibrary(engine.main_lib);
    module.linkLibrary(engine.randommapgen);
    module.linkLibrary(engine.formats);
    module.linkLibrary(engine.misc);
    module.linkLibrary(engine.lualib);
    module.linkLibrary(engine.zlib);
    module.linkLibrary(engine.platform_runtime);
    linkSdlImport(module, target, engine.sdl_dynamic);
}

/// Entry, symbols and rpath of a MapEditor executable, the test included.
fn configureMapEditorExecutable(exe: *std.Build.Step.Compile, target: std.Build.ResolvedTarget, subsystem: std.Target.SubSystem) void {
    if (target.result.os.tag == .windows) {
        // MapEditor ships `.windows` (no console window on a normal
        // double-click); map-editor-engine-test stays `.console` (a CI/local
        // tool, never packaged - Sources/editor/app/crt.zig's
        // attachParentConsole is what keeps MapEditor's own automated modes
        // printing under `.windows`). Either way the entry stays the CRT's
        // own, below.
        exe.subsystem = subsystem;
        // The CRT's entry, so the CRT is initialised and the engine statics'
        // constructors run; the root exports the C main it calls (crt.zig).
        exe.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    // Engine modules resolve RTTI and the coalesced host globals (the host's
    // g_pGlobalSingleton copy wins) from the executable, as they do from Game.
    if (target.result.os.tag == .macos or target.result.os.tag == .linux) exe.rdynamic = true;
    // Loader-relative: it runs from the installation, not the build cache.
    switch (target.result.os.tag) {
        .macos => exe.root_module.addRPathSpecial("@executable_path"),
        .linux => exe.root_module.addRPathSpecial("$ORIGIN"),
        else => {},
    }
}

fn addRandomMissionsTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
    sweep: []const u8,
) void {
    addEngineHostedTool(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main_lib, lualib, zlib, platform_runtime, sdl_dynamic, sdl_include, stage_root, install_game_step, test_mode, "random-missions-test", "tools/zig/random_missions_test.cpp", "test-random-missions", "Generate every random mission a chapter can offer and open it in the engine", &.{sweep});
}

// 05-08 (D-04/D-40.5): the Create Random Map determinism harness - the editor's own
// generation path, a fixed seed, the .bzm bytes compared. The data-only tier, like the
// random missions.
fn addRmgDeterminismTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) void {
    addEngineHostedTool(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main_lib, lualib, zlib, platform_runtime, sdl_dynamic, sdl_include, stage_root, install_game_step, test_mode, "rmg-determinism-test", "tools/zig/rmg_determinism_test.cpp", "test-rmg-determinism", "Generate a random map twice from a fixed seed through the editor and compare the files byte for byte", &.{});
}

// 05-09 (D-07/D-40.4): the composer round trip - every shipped container and graph read
// through the editor's composer records, written back under a scratch name in a scratch
// user RMG root, read again and compared, the bytes of a second write the same. The
// data-only tier, like the random missions.
fn addComposerRoundtripTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) void {
    addEngineHostedTool(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main_lib, lualib, zlib, platform_runtime, sdl_dynamic, sdl_include, stage_root, install_game_step, test_mode, "composer-roundtrip-test", "tools/zig/composer_roundtrip_test.cpp", "test-rmg-composer-roundtrip", "Read, write and re-read every shipped RMG container and graph through the composers' records and compare them and their bytes", &.{});
}

// M001 S01 T05: the ResourceEditor preview-scene spike - a GPU-hosted
// capture harness that proves BkEditorStart -> camera -> BkEditorCaptureFrame
// writes a usable TGA; the three per-kind captures (mesh / sprite / particle)
// and the runbook live beside the harness so S04 inherits a measured camera.
// Skips honestly where there is no GPU, like every other engine-hosted tier.
fn addPreviewSceneSpike(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) void {
    addEngineHostedTool(b, target, optimize, toolchain, editor_bridge, map_file, formats, randommapgen, misc, main_lib, lualib, zlib, platform_runtime, sdl_dynamic, sdl_include, stage_root, install_game_step, test_mode, "preview-scene-spike", "tools/zig/preview_scene_spike.cpp", "preview-scene-spike", "ResourceEditor preview-scene spike: capture mesh/sprite/particle frames and measure non-black-non-magenta pixels (skips with exit 0 on a GPU-less runner)", &.{});
}

// Both engine-hosted C++ tools of the editor's data-only tier (the random missions and
// the Create Random Map determinism harness) are linked, staged and run the same way:
// beside Game in the installation, on the engine the editor bridge starts, writing only
// under zig-out/local-test. `tool` is the executable's name, `source` its one
// translation unit, `step_name` and `step_description` the build step, and
// `extra_args` what follows the installation and scratch arguments.
fn addEngineHostedTool(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    // install-game stages every shared library the engine needs - StreamIO,
    // StreamIOOptionsAbi, PlatformRuntime, SDL3 and the rest - so the run step
    // installs none of them itself. The map file tier has to; it runs from the
    // build cache and there is nothing staged for it.
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
    tool: []const u8,
    source: []const u8,
    step_name: []const u8,
    step_description: []const u8,
    extra_args: []const []const u8,
) void {
    // The recipe of gfxgpu-factory-test, which is the C++ executable this
    // repository already runs on Linux CI. See the note in addMapFileTest.
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Formats"));
    module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{source},
        .flags = cppflagsForOptimize(optimize),
    });
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    linkMsvcRuntime(module, optimize);
    // This executable hosts the same engine the game does, so on Windows it
    // needs the same imports the game executable links. Discovering them one
    // missing symbol at a time costs a CI round each: COM through _com_util and
    // _variant_t (Platform/LegacyVariant.h via Initialization.cpp) brought
    // VariantClear, _com_issue_error, CoCreateGuid and SysAllocString; the
    // version information in Misc brought GetFileVersionInfoSizeA,
    // GetFileVersionInfoA and VerQueryValueA. The list is addGame's, minus the
    // splash-screen resources, which a test has no window to show.
    if (target.result.os.tag == .windows) {
        linkComSupport(module, optimize);
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        module.linkSystemLibrary("shlwapi", .{});
        module.linkSystemLibrary("advapi32", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("gdi32", .{});
        module.linkSystemLibrary("shell32", .{});
    }
    module.linkLibrary(editor_bridge);
    module.linkLibrary(map_file);
    module.linkLibrary(main_lib);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    // Main brings Lua (the Script class) and zlib with it, the way addGame does.
    module.linkLibrary(lualib);
    module.linkLibrary(zlib);
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);

    const exe = b.addExecutable(.{ .name = tool, .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    // Engine modules resolve RTTI and the host globals from the executable,
    // as they do from Game and MapEditor.
    if (target.result.os.tag == .linux) exe.rdynamic = true;
    // Loader-relative, because this binary runs from the installation and not
    // from the build cache its link-time rpath points at.
    switch (target.result.os.tag) {
        .macos => exe.root_module.addRPathSpecial("@executable_path"),
        .linux => exe.root_module.addRPathSpecial("$ORIGIN"),
        else => {},
    }

    // Staged beside Game rather than added to the shipped file list: every
    // engine module derives its roots from the running executable's location,
    // so this has to live in the installation it starts - but a test binary has
    // no business in a release layout or a package, so it is installed on its
    // own and stage.zig never hears about it.
    const stage_suffix = stage_root["zig-out/".len..];
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);

    const run = b.addRunArtifact(exe);
    // Run from the installation, and tell it so: cwd is what the engine's
    // relative data names resolve against.
    run.setCwd(b.path(stage_root));
    run.addArg(".");
    // Where the test may write. Shipped Data is read-only for every tier: a run
    // that is killed halfway must not leave a map behind in the installation.
    run.addArg(b.pathFromRoot("zig-out/local-test"));
    for (extra_args) |arg| run.addArg(arg);
    run.step.dependOn(&install_exe.step);
    const step = b.step(step_name, step_description);
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

fn addSdlEventTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    platform_runtime: *std.Build.Step.Compile,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = false });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFiles(.{
        // SDLApplication forwards clipboard calls to System.cpp.
        .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/System.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/platform_event_test.cpp" },
        .flags = &.{"-std=c++17"},
    });
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            // The CI runner links against the SDK sysroot (--sysroot), which
            // has to be on the search path for objc and c++ to resolve.
            addMacosSysrootPaths(b, module, target);
            module.linkSystemLibrary("c++", .{});
            // SDLApplication::SetAppIcon, as in the game executable.
            module.linkSystemLibrary("objc", .{});
        },
        else => {},
    }
    const test_exe = b.addExecutable(.{ .name = "platform-event-test", .root_module = module });
    test_exe.subsystem = .console;
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
    test_run.step.dependOn(&platform_runtime.step);
    test_run.step.dependOn(&b.addInstallArtifact(platform_runtime, .{}).step);
    test_run.step.dependOn(&sdl_dynamic.step);
    test_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    const sdl_runtime_dir = if (target.result.os.tag == .windows) "zig-out/bin" else "zig-out/lib";
    test_run.addPathDir(b.path(sdl_runtime_dir).getPath(b));
    if (target.result.os.tag != .windows) test_run.setEnvironmentVariable("LD_LIBRARY_PATH", b.path("zig-out/lib").getPath(b));
    const test_step = b.step("test-platform-events", "Run SDL event translation tests");
    test_step.dependOn(&test_exe.step);
    if (test_mode == .run) test_step.dependOn(&test_run.step);
}

// The portable ResourceModel library: the tree items, factory, project XML and
// variant. Every standalone ResourceModel test executable compiles this list, so
// a new item source is added here once.
const resource_model_sources = [_][]const u8{
    "Sources/src/ResourceModel/variant.cpp",
    "Sources/src/ResourceModel/tree_item.cpp",
    "Sources/src/ResourceModel/key_frame_tree_item.cpp",
    "Sources/src/ResourceModel/factory.cpp",
    "Sources/src/ResourceModel/future_blob.cpp",
    "Sources/src/ResourceModel/xml.cpp",
    "Sources/src/ResourceModel/project.cpp",
    "Sources/src/ResourceModel/mfc_value.cpp",
    "Sources/src/ResourceModel/editor_env.cpp",
    "Sources/src/ResourceModel/localization.cpp",
    "Sources/src/ResourceModel/localization_item.cpp",
    "Sources/src/ResourceModel/combos.cpp",
    "Sources/src/ResourceModel/items/stats_item.cpp",
    "Sources/src/ResourceModel/items/ai_tiles.cpp",
    "Sources/src/ResourceModel/items/weapon/weapon.cpp",
    "Sources/src/ResourceModel/items/mine/mine.cpp",
    "Sources/src/ResourceModel/items/trench/trench.cpp",
    "Sources/src/ResourceModel/items/squad/squad.cpp",
    "Sources/src/ResourceModel/items/sprite/sprite.cpp",
    "Sources/src/ResourceModel/items/infantry/infantry.cpp",
    "Sources/src/ResourceModel/items/mesh/mesh.cpp",
    "Sources/src/ResourceModel/items/object/object.cpp",
    "Sources/src/ResourceModel/items/fence/fence.cpp",
    "Sources/src/ResourceModel/items/building/building.cpp",
    "Sources/src/ResourceModel/items/bridge/bridge.cpp",
    "Sources/src/ResourceModel/items/particle/particle.cpp",
    "Sources/src/ResourceModel/items/effect/effect.cpp",
    "Sources/src/ResourceModel/items/tileset/tileset.cpp",
    "Sources/src/ResourceModel/items/road3d/road3d.cpp",
    "Sources/src/ResourceModel/items/river3d/river3d.cpp",
    "Sources/src/ResourceModel/items/mission/mission.cpp",
    "Sources/src/ResourceModel/items/chapter/chapter.cpp",
    "Sources/src/ResourceModel/items/campaign/campaign.cpp",
    "Sources/src/ResourceModel/items/medal/medal.cpp",
    "Sources/src/ResourceModel/items/gui/gui.cpp",
};

// Scaffold-shape smoke for Sources/src/ResourceModel/. Compiles the new
// portable library and the one-fixture round-trip harness as a standalone
// C++17 translation unit set (no engine, no MFC, no SDL). This is the T01
// verification surface - it proves the files compile standalone and the wpn
// fixture round-trips byte-identically. T06 adds the sweep over every repo
// fixture as test-resource-model.
fn addResourceModelScaffoldTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    const flags: []const []const u8 = if (target.result.os.tag == .windows)
        &(cppflags_debug.* ++ .{"-std=c++17"})
    else
        &.{"-std=c++17"};
    module.addCSourceFiles(.{
        .files = &(resource_model_sources ++ .{"tools/zig/resource_model_scaffold_test.cpp"}),
        .flags = flags,
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            addMacosSysrootPaths(b, module, target);
            module.linkSystemLibrary("c++", .{});
        },
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "resource-model-scaffold-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    // Fixtures live at repo-root-relative paths; the test defaults to the wpn
    // fixture and the CI run step passes no args so that default lands.
    run.setCwd(b.path("."));
    // The fixture is a file input, not a step input; the test is cheap and
    // the sweep is where the fixture matrix lives (T06).
    run.has_side_effects = true;
    const step = b.step("resource-model-scaffold-test", "Load and save the wpn fixture through Sources/src/ResourceModel and prove the bytes round-trip");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

// S03 fidelity test (reopened slice, D-04/D-07): every port item class against
// the MFC inventory tools/zig/mfc_item_inventory.py generates from
// Sources/src/editor, load/save of every TestProjects and fixture project
// (byte-identical, content-equal, typed, edit reaches the file) and an
// insert of every item type. Known gaps are listed in
// tools/zig/fixtures/resource_editor/resource-model-xfail.txt; T02-T05 empty it.
fn addResourceModelFidelityTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    const flags: []const []const u8 = if (target.result.os.tag == .windows)
        &(cppflags_debug.* ++ .{"-std=c++17"})
    else
        &.{"-std=c++17"};
    module.addCSourceFiles(.{
        .files = &(resource_model_sources ++ .{"tools/zig/resource_model_test.cpp"}),
        .flags = flags,
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            addMacosSysrootPaths(b, module, target);
            module.linkSystemLibrary("c++", .{});
        },
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "resource-model-fidelity-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    // The inventory, the xfail list and the projects are repo-root-relative;
    // the copies and fidelity.log go to zig-out/local-test/resource_model.
    run.setCwd(b.path("."));
    run.has_side_effects = true;
    const step = b.step("test-resource-model-fidelity", "Port item classes vs the generated MFC inventory, load/save of every TestProjects and fixture project, insert of every item type");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

// T04 test: reference, combo and localization lists of Sources/src/ResourceModel
// over tools/zig/fixtures/resource_editor/references_root.
fn addResourceModelReferencesTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    const flags: []const []const u8 = if (target.result.os.tag == .windows)
        &(cppflags_debug.* ++ .{"-std=c++17"})
    else
        &.{"-std=c++17"};
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/ResourceModel/references.cpp",
            "Sources/src/ResourceModel/combos.cpp",
            "Sources/src/ResourceModel/editor_env.cpp",
            "Sources/src/ResourceModel/localization.cpp",
            "tools/zig/resource_model_references_test.cpp",
        },
        .flags = flags,
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            addMacosSysrootPaths(b, module, target);
            module.linkSystemLibrary("c++", .{});
        },
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "resource-model-references-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    // Fixtures live at repo-root-relative paths; the test defaults to the wpn
    // fixture and the CI run step passes no args so that default lands.
    run.setCwd(b.path("."));
    // The fixture is a file input, not a step input; the test is cheap and
    // the sweep is where the fixture matrix lives (T06).
    run.has_side_effects = true;
    const step = b.step("test-resource-model-references", "Enumerate every EReferenceType list, the AI-class and player-sides combos and the localization read over the tracked references_root fixture");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

// S09 T01: the AI-tile grid math of the Object, Fence, Building and Bridge editors,
// headless like the other test-resource-model members.
fn addResourceModelGridProjectionTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    const flags: []const []const u8 = if (target.result.os.tag == .windows)
        &(cppflags_debug.* ++ .{"-std=c++17"})
    else
        &.{"-std=c++17"};
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/ResourceModel/grid_projection.cpp",
            "Sources/src/ResourceModel/items/ai_tiles.cpp",
            "Sources/src/ResourceModel/mfc_value.cpp",
            "Sources/src/ResourceModel/variant.cpp",
            "Sources/src/ResourceModel/xml.cpp",
            "tools/zig/resource_grid_projection_test.cpp",
        },
        .flags = flags,
    });
    switch (target.result.os.tag) {
        .windows => {
            addMsvcIncludePaths(b, module, toolchain);
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, .Debug);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => {
            addMacosSysrootPaths(b, module, target);
            module.linkSystemLibrary("c++", .{});
        },
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "resource-grid-projection-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    const run = b.addRunArtifact(exe);
    run.has_side_effects = true;
    const step = b.step("test-resource-grid-projection", "S09 T01: GridProjection tile and screen math, tile-list and grid helpers and one-way line tiles, headless");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

// S03 T06 (D-11): the comparator over exported game data. Stats files are
// read by the engine's own StreamIO tree and each struct's operator&, so this
// is an engine-hosted executable like test-resource-bridge, but data-only: it
// loads StreamIO and nothing that needs a window or a GPU, so it never skips.
// It proves the comparator on the shipped Data (copied to zig-out/local-test),
// plants a one-ulp float, a dropped and an unknown field, and reports the
// golden comparison as pending until win-home has produced the goldens.
fn addResourceModelComparatorTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    editor_bridge: *std.Build.Step.Compile,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    main_lib: *std.Build.Step.Compile,
    lualib: *std.Build.Step.Compile,
    zlib: *std.Build.Step.Compile,
    platform_runtime: *std.Build.Step.Compile,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    stage_root: []const u8,
    install_game_step: *std.Build.Step,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    module.addIncludePath(b.path("Sources/src/Common"));
    module.addIncludePath(b.path("Sources/src/Main"));
    module.addIncludePath(b.path("Sources/src/Image"));
    module.addIncludePath(b.path("Sources/src/GFX"));
    // The Scene sources compiled in below take their StdAfx.h from Scene,
    // which names the StreamIO headers bare, as the Scene module's own build
    // resolves them.
    module.addIncludePath(b.path("Sources/src/StreamIO"));
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        // The particle structs live in the Scene module, which a data-only
        // host does not load; their readers are compiled in instead. xml.cpp
        // comes with the EditorBridge archive. The DXT gate decodes with NDxt,
        // which lives in the Image module the host does not load either.
        .files = &.{
            "Sources/src/ResourceModel/comparator.cpp",
            "Sources/src/ResourceModel/dxt_gate.cpp",
            "Sources/src/Image/DxtCodec.cpp",
            "Sources/src/Scene/ParticleSourceData.cpp",
            "Sources/src/Scene/SmokinParticleSourceData.cpp",
            "Sources/src/Scene/Track.cpp",
            "tools/zig/resource_model_comparator_test.cpp",
        },
        .flags = cppflagsForOptimize(optimize),
    });
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    addMacosSysrootPaths(b, module, target);
    linkMsvcRuntime(module, optimize);
    if (target.result.os.tag == .windows) {
        linkComSupport(module, optimize);
        module.linkSystemLibrary("version", .{});
        module.linkSystemLibrary("winmm", .{});
        module.linkSystemLibrary("odbc32", .{});
        module.linkSystemLibrary("odbccp32", .{});
        module.linkSystemLibrary("shlwapi", .{});
        module.linkSystemLibrary("advapi32", .{});
        module.linkSystemLibrary("user32", .{});
        module.linkSystemLibrary("gdi32", .{});
        module.linkSystemLibrary("shell32", .{});
    }
    module.linkLibrary(editor_bridge);
    module.linkLibrary(map_file);
    module.linkLibrary(main_lib);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    module.linkLibrary(lualib);
    module.linkLibrary(zlib);
    module.linkLibrary(platform_runtime);
    linkSdlImport(module, target, sdl_dynamic);

    const exe = b.addExecutable(.{ .name = "resource-model-comparator-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };
    // rdynamic and a loader-relative rpath, as for every executable that loads
    // engine modules (AGENTS.md).
    if (target.result.os.tag == .linux) exe.rdynamic = true;
    switch (target.result.os.tag) {
        .macos => exe.root_module.addRPathSpecial("@executable_path"),
        .linux => exe.root_module.addRPathSpecial("$ORIGIN"),
        else => {},
    }
    const stage_suffix = stage_root["zig-out/".len..];
    const install_exe = b.addInstallArtifact(exe, .{ .dest_dir = .{ .override = .{ .custom = stage_suffix } } });
    install_exe.step.dependOn(install_game_step);

    const run = b.addRunArtifact(exe);
    run.setCwd(b.path(stage_root));
    run.addArg(".");
    run.addArg(b.pathFromRoot("zig-out/local-test"));
    // The tracked Data and fixtures, read in place and copied before any edit.
    run.addArg(b.pathFromRoot("Data"));
    run.addArg(b.pathFromRoot("tools/zig/fixtures/resource_editor"));
    run.has_side_effects = true;
    run.step.dependOn(&install_exe.step);
    const step = b.step("test-resource-model-comparator", "D-11 comparator: shipped stats read by the engine's own readers equal themselves, planted float/dropped/unknown/byte/DXT changes fail, goldens reported pending");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);
}

// The S03 aggregate: the slice's "test-resource-model is green" is this one
// invocation. The scaffold, references and fidelity tiers need no engine; the
// comparator (D-11) hosts the engine's StreamIO data-only, without a window
// or a GPU, so it runs wherever the engine builds.
fn addResourceModelAggregateStep(b: *std.Build) void {
    const step = b.step("test-resource-model", "Aggregate ResourceModel sweep: scaffold round trip, references/combos/localization lists, MFC fidelity, and the D-11 comparator over exported game data");
    const scaffold = &(b.top_level_steps.get("resource-model-scaffold-test") orelse @panic("resource-model-scaffold-test is defined by addResourceModelScaffoldTest")).step;
    const references = &(b.top_level_steps.get("test-resource-model-references") orelse @panic("test-resource-model-references is defined by addResourceModelReferencesTest")).step;
    const comparator = &(b.top_level_steps.get("test-resource-model-comparator") orelse @panic("test-resource-model-comparator is defined by addResourceModelComparatorTest")).step;
    const fidelity = &(b.top_level_steps.get("test-resource-model-fidelity") orelse @panic("test-resource-model-fidelity is defined by addResourceModelFidelityTest")).step;
    step.dependOn(scaffold);
    step.dependOn(references);
    step.dependOn(&(b.top_level_steps.get("test-resource-grid-projection") orelse @panic("test-resource-grid-projection is defined by addResourceModelGridProjectionTest")).step);
    step.dependOn(comparator);
    step.dependOn(fidelity);
}

// S03 T06 top-level aggregate: the S16 full-sweep gate pulls this in. Every
// resource-side tier that needs no window or GPU: the ResourceModel sweep (its
// comparator hosts the engine data-only), the
// project-XML round trip, the DXT tolerance measurement and the fixture
// generator's own tests. The engine-hosted resource tiers (test-resource-bridge,
// preview-scene-spike) stay separate steps, like the Map Editor's.
fn addResourcesAllAggregateStep(b: *std.Build) void {
    const step = b.step("test-resources-all", "Aggregate every resource-side tier that needs no window or GPU: test-resource-model, test-resource-xml-roundtrip, test-dxt-tolerance and test-resource-editor-fixtures");
    for ([_][]const u8{ "test-resource-model", "test-resource-xml-roundtrip", "test-dxt-tolerance", "test-resource-editor-fixtures" }) |name| {
        const member = b.top_level_steps.get(name) orelse std.debug.panic("{s} must be registered before test-resources-all", .{name});
        step.dependOn(&member.step);
    }
}

fn linkSdlRuntime(
    module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    module.addIncludePath(sdl_include);
    linkSdlImport(module, target, sdl_dynamic);
}

fn linkSdlImport(
    module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    sdl_dynamic: *std.Build.Step.Compile,
) void {
    switch (target.result.os.tag) {
        .windows => module.addObjectFile(sdl_dynamic.getEmittedImplib()),
        else => module.linkLibrary(sdl_dynamic),
    }
}

// The winter ("w") and Africa ("a") unit textures Data lacks, derived from the
// summer ones by tools/zig/season_textures.zig. A build output, not a source:
// generated into the cache (one stored .pak of 1428 files, ~70 MB, about 2 s;
// one file rather than 1428 because the package's zip has little of its
// 65,535-entry limit left), staged beside Data as SeasonData by stage-game and
// mounted over Data by the engine (Sources/src/StreamIO/SeasonData.h). Nothing
// is written into Data, which -Dcopy-data=false stages as a link into this
// repository.
fn addSeasonData(b: *std.Build, tool: *std.Build.Step.Compile) std.Build.LazyPath {
    const run = b.addRunArtifact(tool);
    run.setName("generate SeasonData");
    run.addDirectoryArg(b.path("Data"));
    run.addArg("--out");
    const out = run.addOutputDirectoryArg("SeasonData");
    run.addArgs(&.{ "--pak", "SeasonTextures.pak" });
    // A directory argument is hashed by its path only, so what the tool reads
    // goes into the cache key explicitly, and the output is regenerated
    // exactly when that changes.
    const inputs = seasonDataInputs(b) catch |err| std.debug.panic("SeasonData inputs: {s}", .{@errorName(err)});
    for (inputs.sources) |source| run.addFileInput(b.path(source));
    run.addFileInput(b.addWriteFiles().add("season-data-inputs.txt", inputs.listing));
    return out;
}

const SeasonDataInputs = struct {
    /// The summer textures, whose bytes decide the generated files'.
    sources: []const []const u8,
    /// Every mesh and season texture name the plan looks at, one per line: a
    /// folder that gains a hand-painted 1w, or a new unit, changes it.
    listing: []const u8,
};

// Found at configure time, the way shaderSourceFiles finds the shaders:
// walking Data/Units takes a few milliseconds.
fn seasonDataInputs(b: *std.Build) !SeasonDataInputs {
    const io = b.graph.io;
    var dir = b.build_root.handle.openDir(io, "Data/Units", .{ .iterate = true }) catch |err| switch (err) {
        // CI's sparse checkouts for the jobs that never stage the game (the
        // Linux, MinGW and Intel macOS ones) leave out Data/Units. Staging
        // without it fails in the generation step itself, which reads it.
        error.FileNotFound => return .{ .sources = &.{}, .listing = "" },
        else => return err,
    };
    defer dir.close(io);
    var walker = try dir.walk(b.allocator);
    defer walker.deinit();
    var names: std.ArrayList([]const u8) = .empty;
    var sources: std.ArrayList([]const u8) = .empty;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const lower = try std.ascii.allocLowerString(b.allocator, entry.basename);
        if (!season_textures_plan.isPlanName(lower)) continue;
        // Forward slashes, so the listing is the same on every host.
        const relative = try std.mem.replaceOwned(u8, b.allocator, entry.path, "\\", "/");
        const path = b.fmt("Data/Units/{s}", .{relative});
        try names.append(b.allocator, path);
        if (season_textures_plan.isSummerSource(lower)) try sources.append(b.allocator, path);
    }
    const Sort = struct {
        fn less(_: void, left: []const u8, right: []const u8) bool {
            return std.mem.order(u8, left, right) == .lt;
        }
    };
    // The walk's order is the filesystem's; the key must not be.
    std.mem.sort([]const u8, names.items, {}, Sort.less);
    std.mem.sort([]const u8, sources.items, {}, Sort.less);
    return .{ .sources = sources.items, .listing = try std.mem.join(b.allocator, "\n", names.items) };
}

// Every file the shader driver reads: the manifest plus the .hlsl sources beside
// it, including the shared headers the entry points include.
fn shaderSourceFiles(b: *std.Build) ![]const []const u8 {
    const directory = "Sources/src/GFXGPU/shaders";
    var sources: std.ArrayList([]const u8) = .empty;
    try sources.append(b.allocator, b.fmt("{s}/manifest.json", .{directory}));
    var dir = try std.Io.Dir.cwd().openDir(b.graph.io, directory, .{ .iterate = true });
    defer dir.close(b.graph.io);
    var iterator = dir.iterate();
    while (try iterator.next(b.graph.io)) |entry| {
        if (entry.kind != .file or !std.mem.endsWith(u8, entry.name, ".hlsl")) continue;
        try sources.append(b.allocator, b.fmt("{s}/{s}", .{ directory, entry.name }));
    }
    return sources.items;
}

fn linkMsvcRuntime(module: *std.Build.Module, optimize: std.builtin.OptimizeMode) void {
    if (module.resolved_target) |module_target| linkCxxRuntime(module, module_target);
    if (!build_target_msvc) {
        // A Windows module that is not MSVC is MinGW, and Zig supplies both the
        // CRT and the C++ standard library for it. Call sites lean on this helper
        // for a module to compile at all, so returning bare here left them
        // without even <stdio.h>. Non-Windows modules keep their previous
        // behaviour of being configured by their own call sites.
        if (module.resolved_target) |module_target| {
            if (module_target.result.os.tag == .windows) {
                module.link_libc = true;
                module.link_libcpp = true;
            }
        }
        return;
    }
    if (module.resolved_target.?.result.os.tag != .windows) {
        module.link_libc = true;
        return;
    }
    switch (optimize) {
        .Debug => {
            module.linkSystemLibrary("ucrtd", .{});
            module.linkSystemLibrary("msvcrtd", .{});
            module.linkSystemLibrary("msvcprtd", .{});
            module.linkSystemLibrary("vcruntimed", .{});
        },
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => {
            module.linkSystemLibrary("ucrt", .{});
            module.linkSystemLibrary("msvcrt", .{});
            module.linkSystemLibrary("msvcprt", .{});
            module.linkSystemLibrary("vcruntime", .{});
        },
    }
    module.linkSystemLibrary("oldnames", .{});
    module.linkSystemLibrary("kernel32", .{});
    module.linkSystemLibrary("ntdll", .{});
}

fn linkComSupport(module: *std.Build.Module, optimize: std.builtin.OptimizeMode) void {
    switch (optimize) {
        .Debug => module.linkSystemLibrary("comsuppwd", .{}),
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => module.linkSystemLibrary("comsuppw", .{}),
    }
    module.linkSystemLibrary("oleaut32", .{});
    module.linkSystemLibrary("ole32", .{});
    module.linkSystemLibrary("uuid", .{});
}
