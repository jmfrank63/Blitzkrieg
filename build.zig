const std = @import("std");
const build_support = @import("tools/zig/build_support.zig");
const package_policy = @import("tools/zig/verify_runtime.zig");

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
    const default_target: std.Target.Query = switch (b.graph.host.result.os.tag) {
        .linux => .{
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .gnu,
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

    const sdl_c_dep = b.dependency("sdl", .{
        .target = dependency_target,
        .optimize = optimize,
        .preferred_linkage = .static,
        .install_build_config_h = true,
    });
    const sdl_c = sdl_c_dep.artifact("SDL3");
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
    addGameFrameTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addGameSystemKeysTest(b, target, test_mode, toolchain);
    addGameMouseCaptureTest(b, target, test_mode, toolchain);
    addGameLoopTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addSdlApplicationTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"), platform_runtime);
    addSdlEventTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"), platform_runtime);
    addInputCodesTest(b, target, test_mode, toolchain);
    addPlatformInputTest(b, target, test_mode, toolchain);
    addInputStateFixtureTest(b, target, test_mode, toolchain);
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
    const ailogic = addLegacyProjectDll(b, target, optimize, toolchain, "AILogic", "Sources/src/AILogic/AILogic.vcxproj", "Sources/src/AILogic/AILogic.def", &.{ "Sources/src/AILogic", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/GameTT", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/vorbis-1.3.7/include" }, &.{ misc, lualib, formats, randommapgen, zlib }, platform_runtime, sdl_dynamic);
    const gamett = addLegacyProjectDll(b, target, optimize, toolchain, "GameTT", "Sources/src/GameTT/GameTT.vcxproj", "Sources/src/GameTT/GameTT.def", &.{ "Sources/src/GameTT", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/AILogic" }, &.{ misc, formats, common, randommapgen }, platform_runtime, sdl_dynamic);
    // Compile the game version directly into GameTT.dll so the title screen
    // shows the version string without relying on the Win32 version resource
    // API (which Zig's resinator does not produce correctly for runtime reads).
    gamett.root_module.addCMacro("BLITZKRIEG_VERSION", b.fmt("\"{d}.{d}.{d}\"", .{ game_version.major, game_version.minor, game_version.patch }));
    const main = addMain(b, target, optimize, toolchain);
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
        .flags = cppflagsForTarget(target, optimize),
    });
    addProjectIncludePaths(b, gfx_gpu_factory_test_module);
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFX"));
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    addMsvcIncludePaths(b, gfx_gpu_factory_test_module, toolchain);
    addLinuxCxxIncludePaths(b, gfx_gpu_factory_test_module);
    addMsvcLibraryPaths(b, gfx_gpu_factory_test_module, toolchain);
    linkMsvcRuntime(gfx_gpu_factory_test_module, optimize);
    gfx_gpu_factory_test_module.linkLibrary(gfx_gpu_zig);
    gfx_gpu_factory_test_module.linkLibrary(sdl_c);
    gfx_gpu_factory_test_module.linkLibrary(formats);
    gfx_gpu_factory_test_module.linkSystemLibrary("user32", .{});
    const gfx_gpu_factory_test = b.addExecutable(.{
        .name = "gfxgpu-factory-test",
        .root_module = gfx_gpu_factory_test_module,
    });
    gfx_gpu_factory_test.subsystem = .console;
    gfx_gpu_factory_test.entry = .{ .symbol_name = "main" };
    const gfx_gpu_factory_test_run = b.addRunArtifact(gfx_gpu_factory_test);
    gfx_gpu_factory_test_run.step.dependOn(&b.addInstallArtifact(gfx_gpu, .{}).step);
    gfx_gpu_factory_test_run.setCwd(b.path("."));
    gfx_gpu_factory_test_run.addArg("zig-out/bin/GFXGPU.dll");
    const gfx_gpu_factory_test_step = b.step("gfxgpu-factory-test", "Load the SDL GPU GFX DLL and create its IGFX object");
    gfx_gpu_factory_test_step.dependOn(&gfx_gpu_factory_test_run.step);

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

    const install_game_cmd = b.addRunArtifact(stage_tool);
    install_game_cmd.addArg(".");
    install_game_cmd.addArg(stage_root);
    addStageLayoutArgs(install_game_cmd, stage_game_name, stage_runtime_files, stage_debug_files, stage_metadata_files, target.result.os.tag == .windows);
    // Staging copies the third-party notice out of a plain path, the way it
    // copies the shader blobs, so an edited licence text has to be part of the
    // cache key or the staged and packaged copies keep the superseded notice.
    install_game_cmd.addFileInput(b.path(package_policy.third_party_notices_source));
    if (!copy_data) install_game_cmd.addArg("--link-data");
    if (!use_prebuilt_shaders) {
        install_game_cmd.step.dependOn(gfx_gpu_shaders_step);
        // Staging copies the compiled shader blobs out of a plain path, so the same
        // sources have to be part of its cache key or an edited shader never reaches
        // the install layout. It cannot simply always run: it deletes and re-copies
        // the whole 2.7 GB Data tree.
        for (shader_sources) |source| install_game_cmd.addFileInput(b.path(source));
    }

    const install_game_step = b.step("install-game", "Create runnable game install layout with binaries and Data");
    install_game_cmd.step.dependOn(game_all_step);
    install_game_step.dependOn(&install_game_cmd.step);

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
    const stage_package_game_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_cmd.addArg(".");
    stage_package_game_cmd.addArg(package_stage_root);
    addStageLayoutArgs(stage_package_game_cmd, stage_game_name, stage_runtime_files, stage_debug_files, stage_metadata_files, target.result.os.tag == .windows);
    stage_package_game_cmd.addFileInput(b.path(package_policy.third_party_notices_source));

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

    const stage_package_game_editors_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_editors_cmd.addArg(".");
    stage_package_game_editors_cmd.addArg(package_stage_root);
    addStageLayoutArgs(stage_package_game_editors_cmd, stage_game_name, stage_runtime_files, stage_debug_files, stage_metadata_files, target.result.os.tag == .windows);
    stage_package_game_editors_cmd.addArg("--include-editors");
    stage_package_game_editors_cmd.addArg("--editors-only");
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
    test_step.dependOn(&run_blitz64_unit_tests.step);
    test_step.dependOn(&run_streamio_unit_tests.step);
    test_step.dependOn(&run_abi_test.step);
    if (target.result.cpu.arch == .x86_64) test_step.dependOn(&verify_x64_cmd.step);

    b.default_step = game_all_step;
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
    randommapgen_module.addCSourceFiles(.{
        .files = randommapgen_sources,
        .flags = cppflagsForOptimize(optimize),
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
    if (target.result.os.tag != .windows) return &.{"-std=c++17"};
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
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Game"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/Game/GameFrame.cpp", "Sources/src/Game/MouseCapture.cpp", "tools/zig/game_frame_test.cpp" }, .flags = &.{ "-std=c++17" } });
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
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/Game"));
    module.addIncludePath(b.path("Sources/src/Platform"));
    module.addIncludePath(b.path("Sources/src/GFXGPU"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/Game/SysKeys.cpp", "Sources/src/Game/GameFrame.cpp", "Sources/src/Game/MouseCapture.cpp", "tools/zig/game_loop_test.cpp" }, .flags = &.{ "-std=c++17" } });
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
        .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "Sources/src/Platform/Debug.cpp", "Sources/src/PlatformABI/PlatformClient.cpp", "tools/zig/platform_event_test.cpp" },
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
        .macos => module.linkSystemLibrary("c++", .{}),
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
