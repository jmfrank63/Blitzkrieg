const std = @import("std");
const build_support = @import("tools/zig/build_support.zig");

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
const cppflags_debug_trap = &(cppflags_debug.* ++ .{"-fsanitize-trap=undefined"});

const cppflags_release = &.{
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
};

const cppflags_sfx_release = &.{
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
    "Sources/src/Platform/Clock.cpp",
    "Sources/src/Platform/Debug.cpp",
    "Sources/src/Platform/DynamicLibrary.cpp",
    "Sources/src/Platform/LegacyVariant.cpp",
    "Sources/src/Platform/Paths.cpp",
    "Sources/src/Platform/Sync.cpp",
    "Sources/src/Platform/System.cpp",
    "Sources/src/Platform/SocketWin32.cpp",
    "Sources/src/Platform/SocketPosix.cpp",
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
    "Sources/src/Game/SysKeys.cpp",
    "Sources/src/Game/WinFrame.cpp",
    "Sources/src/Platform/SDLApplication.cpp",
};

const cppflags_game_debug = &.{
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
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .x86_64,
            .os_tag = .windows,
            .abi = .msvc,
        },
    });
    const platform = build_support.classify(target.result) catch @panic("unsupported target; supported triples are x86_64-windows-msvc, x86_64-linux-gnu, and aarch64-macos");
    // Runtime eligibility follows the host OS and CPU. Windows can execute
    // an MSVC-targeted binary even when the Zig host itself reports the GNU
    // Windows ABI (the common Scoop Zig installation does); ABI selection is
    // still enforced by the target and linker configuration below.
    const native_target = target.result.os.tag == b.graph.host.result.os.tag and
        target.result.cpu.arch == b.graph.host.result.cpu.arch;
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
    const optimize = b.standardOptimizeOption(.{});
    const library_arch = build_support.libraryArch(platform);
    const toolchain = ToolchainIncludes{
        .msvc_include = b.option([]const u8, "msvc-include", "MSVC C/C++ include directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\include",
        .windows_sdk_include = b.option([]const u8, "windows-sdk-include", "Windows SDK include version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.26100.0",
        .msvc_lib = b.option([]const u8, "msvc-lib", "MSVC library directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\lib",
        .windows_sdk_lib = b.option([]const u8, "windows-sdk-lib", "Windows SDK library version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0",
        .library_arch = library_arch,
    };
    const platform_headers_module = b.createModule(.{ .target = target, .optimize = .Debug });
    platform_headers_module.addCSourceFiles(.{ .files = &.{"tools/zig/platform_headers_test.cpp"}, .flags = &.{} });
    platform_headers_module.addIncludePath(b.path("Sources/src"));
    if (platform == .windows_x64) addMsvcIncludePaths(b, platform_headers_module, toolchain);
    const platform_headers_object = b.addObject(.{ .name = "platform-headers-test", .root_module = platform_headers_module });
    const platform_headers_step = b.step("test-platform-headers", "Validate portable compiler and legacy value types");
    platform_headers_step.dependOn(&platform_headers_object.step);
    if (test_mode == .run) {
        const platform_headers_test = b.addExecutable(.{ .name = "platform-headers-test-run", .root_module = platform_headers_module });
        if (platform == .windows_x64) {
            addMsvcLibraryPaths(b, platform_headers_module, toolchain);
            linkMsvcRuntime(platform_headers_module, optimize);
        }
        const platform_headers_run = b.addRunArtifact(platform_headers_test);
        platform_headers_step.dependOn(&platform_headers_run.step);
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
        .flags = &(cppflags_debug.* ++ .{"-DBLITZKRIEG_PLATFORM_SYNC_ONLY"}),
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
            "Sources/src/Platform/Debug.cpp",
        },
        .flags = cppflags_debug,
    });
    const platform_debug_test = b.addExecutable(.{ .name = "platform-debug-test", .root_module = platform_debug_module });
    platform_debug_test.subsystem = .console;
    if (platform == .windows_x64) platform_debug_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_debug_run = b.addRunArtifact(platform_debug_test);
    const platform_debug_step = b.step("test-platform-debug", "Run portable diagnostic and debugger facade tests");
    platform_debug_step.dependOn(&platform_debug_test.step);
    if (test_mode == .run) platform_debug_step.dependOn(&platform_debug_run.step);

    const sdl_c_dep = b.dependency("sdl", .{
        .target = target,
        .optimize = optimize,
        .preferred_linkage = .static,
        .install_build_config_h = true,
    });
    const sdl_c = sdl_c_dep.artifact("SDL3");
    const platform_test_module_module = b.createModule(.{ .target = target, .optimize = .ReleaseFast });
    platform_test_module_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_test_module.cpp"), .flags = cppflags_release });
    if (platform == .windows_x64) {
        addMsvcIncludePaths(b, platform_test_module_module, toolchain);
        addMsvcLibraryPaths(b, platform_test_module_module, toolchain);
        linkMsvcRuntime(platform_test_module_module, .ReleaseFast);
    }
    const platform_test_module = b.addLibrary(.{ .name = "platform-test-module", .linkage = .dynamic, .root_module = platform_test_module_module });
    const sdl_dynamic_dep = b.dependency("sdl", .{
        .target = target,
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
    }
    platform_dynamic_module.addCSourceFiles(.{
        .files = &.{
            "tools/zig/platform_dynamic_library_test.cpp",
            "Sources/src/Platform/DynamicLibrary.cpp",
        },
        .flags = cppflags_release,
    });
    platform_dynamic_module.linkLibrary(sdl_dynamic);
    const platform_dynamic_test = b.addExecutable(.{ .name = "platform-dynamic-library-test", .root_module = platform_dynamic_module });
    platform_dynamic_test.subsystem = .console;
    if (platform == .windows_x64) platform_dynamic_test.entry = .{ .symbol_name = "mainCRTStartup" };
    const platform_dynamic_run = b.addRunArtifact(platform_dynamic_test);
    platform_dynamic_run.addArtifactArg(platform_test_module);
    const platform_dynamic_step = b.step("test-platform-dynamic-library", "Run portable dynamic library ownership tests");
    platform_dynamic_step.dependOn(&platform_dynamic_test.step);
    platform_dynamic_step.dependOn(&platform_test_module.step);
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
        .flags = cppflags_release,
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
    legacy_variant_module.link_libc = platform != .windows_x64;
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
    addSdlApplicationTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addSdlEventTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addInputCodesTest(b, target, test_mode, toolchain);
    addPlatformInputTest(b, target, test_mode, toolchain);
    addPlatformClipboardTest(b, target, test_mode, toolchain);
    addPlatformAudioTest(b, target, test_mode, toolchain);
    addInputAudioGateTest(b, target, test_mode, toolchain);
    addPlatformSocketTypesTest(b, target, test_mode, toolchain);
    addPlatformNetworkTest(b, target, test_mode, toolchain);
    addNetworkSystemGateTest(b, target, test_mode, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    addRuntimeHeadersTest(b, target, test_mode, toolchain);

    const sdl3_dep = b.dependency("sdl3", .{
        .target = target,
        .optimize = optimize,
        .c_sdl_preferred_linkage = .dynamic,
        .c_sdl_install_build_config_h = true,
    });
    const sdl3 = sdl3_dep.module("sdl3");
    const gfx_gpu_zig = addGfxGpuZig(b, target, optimize, sdl3);
    addGameBootstrapSmoke(b, target, optimize, toolchain, gfx_gpu_zig, sdl_c, sdl_dynamic_dep.path("include"), test_mode);
    if (platform != .windows_x64) {
        addPortableStreamioFilesTest(b, target, optimize, test_mode);
        addPortableModuleTest(b, target, test_mode);
        b.default_step = platform_foundation;
        return;
    }
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
    const shadercross_cli = sdl3_build.shadercross.cli(b, null, true, false) orelse return;
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
    shader_driver_run.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_driver_run.addPathDir(path);
    shader_driver_run.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_driver_run.addArtifactArg(shadercross_cli);
    shader_driver_run.addArg("zig-out/shaders");

    const gfx_gpu_shaders_step = b.step("gfxgpu-shaders", "Compile deterministic GfxGpu shader blobs and manifest");
    gfx_gpu_shaders_step.dependOn(&shader_driver_run.step);
    gfx_gpu_shaders_step.dependOn(&shader_parser_tests_run.step);

    const shader_determinism_a = b.addRunArtifact(shader_driver);
    shader_determinism_a.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_determinism_a.addPathDir(path);
    shader_determinism_a.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_determinism_a.addArtifactArg(shadercross_cli);
    shader_determinism_a.addArg("zig-out/shaders-determinism-a");
    const shader_determinism_b = b.addRunArtifact(shader_driver);
    shader_determinism_b.step.dependOn(shadercross_build_step);
    if (dxc_runtime_path) |path| shader_determinism_b.addPathDir(path);
    shader_determinism_b.addArg("Sources/src/GFXGPU/shaders/manifest.json");
    shader_determinism_b.addArtifactArg(shadercross_cli);
    shader_determinism_b.addArg("zig-out/shaders-determinism-b");

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
    const stage_root = b.fmt("zig-out/game/{s}", .{platform_policy.package_root});
    const package_root = b.fmt("zig-out/packages/{s}", .{platform_policy.package_root});
    const stage_game_name = platform_policy.executable_name;
    const stage_runtime_files = switch (target.result.os.tag) {
        .windows => &[_][]const u8{ "Game.exe", "StreamIO.dll", "StreamIOOptionsAbi.dll", "Anim.dll", "GFXGPU.dll", "SDL3.dll", "Image.dll", "Input.dll", "Net.dll", "SFX.dll", "UI.dll", "Scene.dll", "AILogic.dll", "GameTT.dll" },
        .linux => &[_][]const u8{ "Game", "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so", "libGFXGPU.so", "libSDL3.so", "libImage.so", "libInput.so", "libNet.so", "libSFX.so", "libUI.so", "libScene.so", "libAILogic.so", "libGameTT.so" },
        .macos => &[_][]const u8{ "Game", "libStreamIO.dylib", "libStreamIOOptionsAbi.dylib", "libAnim.dylib", "libGFXGPU.dylib", "libSDL3.dylib", "libImage.dylib", "libInput.dylib", "libNet.dylib", "libSFX.dylib", "libUI.dylib", "libScene.dylib", "libAILogic.dylib", "libGameTT.dylib" },
        else => &[_][]const u8{stage_game_name},
    };
    const stage_debug_files = if (target.result.os.tag == .windows)
        &[_][]const u8{ "Game.pdb", "StreamIO.pdb", "StreamIOOptionsAbi.pdb", "Anim.pdb", "GFXGPU.pdb", "Image.pdb", "Input.pdb", "Net.pdb", "SFX.pdb", "UI.pdb", "Scene.pdb", "AILogic.pdb", "GameTT.pdb" }
    else
        &[_][]const u8{};
    const gfx_gpu_abi_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    gfx_gpu_abi_test_module.addCSourceFiles(.{
        .files = &.{"tools/zig/gfxgpu_abi_test.cpp"},
        .flags = cppflagsForOptimize(optimize),
    });
    gfx_gpu_abi_test_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    addMsvcIncludePaths(b, gfx_gpu_abi_test_module, toolchain);
    addMsvcLibraryPaths(b, gfx_gpu_abi_test_module, toolchain);
    gfx_gpu_abi_test_module.linkLibrary(gfx_gpu_zig);
    gfx_gpu_abi_test_module.linkLibrary(sdl_c);
    linkMsvcRuntime(gfx_gpu_abi_test_module, optimize);
    const gfx_gpu_abi_test = b.addExecutable(.{
        .name = "gfxgpu-abi-test",
        .root_module = gfx_gpu_abi_test_module,
    });
    gfx_gpu_abi_test.subsystem = .console;
    gfx_gpu_abi_test.entry = .{ .symbol_name = "main" };
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
    gfx_gpu_smoke_run.step.dependOn(&gfx_gpu_smoke_install.step);
    gfx_gpu_smoke_run.setCwd(b.path("zig-out/bin"));
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
    const options_bridge = addOptionsBridge(b, target, optimize, toolchain, sdl_c);
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
    const platform_module_test_module = b.createModule(.{ .target = target, .optimize = .Debug });
    platform_module_test_module.addCSourceFile(.{ .file = b.path("tools/zig/platform_module_test.cpp"), .flags = cppflagsForOptimize(.Debug) });
    addMsvcIncludePaths(b, platform_module_test_module, toolchain);
    addMsvcLibraryPaths(b, platform_module_test_module, toolchain);
    linkMsvcRuntime(platform_module_test_module, .Debug);
    const platform_module_test = b.addExecutable(.{ .name = "platform-module-test", .root_module = platform_module_test_module });
    platform_module_test.subsystem = .console;
    platform_module_test.entry = .{ .symbol_name = "main" };
    const platform_module_test_run = b.addRunArtifact(platform_module_test);
    platform_module_test_run.setCwd(b.path("."));
    const platform_module_test_step = b.step("test-platform-modules", "Run portable runtime module tests");
    platform_module_test_step.dependOn(&platform_module_test_run.step);
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
    const streamio_zig = addStreamIOZig(b, target, optimize, toolchain, options_bridge, streamio_fast);
    const copy_data = b.option(bool, "copy-data", "Copy Data into install layout (the default)") orelse true;
    const startup_trace = b.option(bool, "startup-trace", "Emit Windows startup checkpoint markers to the debugger") orelse false;
    ubsan_trap = b.option(bool, "ubsan-trap", "Compile UBSan checks as traps so debuggers break at the faulting line (Debug only)") orelse false;

    const zlib = addZlib(b, target, optimize, toolchain);
    const libpng = addLibpng(b, target, optimize, toolchain, zlib);
    const misc = addMisc(b, target, optimize, toolchain, sdl_dynamic, sdl_dynamic_dep.path("include"));
    const image = addImage(b, target, optimize, toolchain, zlib, libpng, misc, sdl_dynamic);
    const lualib = addLuaLib(b, target, optimize, toolchain);
    const net = addNet(b, target, optimize, toolchain, misc, sdl_dynamic);
    const buildversion = addBuildVersion(b, target, optimize, toolchain, misc, sdl_dynamic);
    const betakeygen = addBetaKeyGen(b, target, optimize, toolchain, zlib, misc, sdl_dynamic);
    const input = addInput(b, target, optimize, toolchain, misc, sdl_dynamic);
    const formats = addFormats(b, target, optimize, toolchain);
    const scene = addLegacyProjectDll(b, target, optimize, toolchain, "Scene", "Sources/src/Scene/Scene.vcxproj", "Sources/src/Scene/Scene.def", &.{ "Sources/src/Scene", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/libtheora-1.2.0/include" }, &.{ misc, formats }, sdl_dynamic);
    const anim = addAnim(b, target, optimize, toolchain, misc, formats, sdl_dynamic);
    const common = addCommon(b, target, optimize, toolchain);
    const ui = addUI(b, target, optimize, toolchain, misc, common, lualib, sdl_dynamic);
    const fontgen = addFontGen(b, target, optimize, toolchain, image, common, formats, misc, sdl_dynamic);
    const sfx = addSFX(b, target, optimize, toolchain, misc, common, sdl_dynamic);
    const gfx_legacy = addGFX(b, target, optimize, toolchain, misc, formats, sdl_dynamic);
    const gfx_gpu = addGFXGPU(b, target, optimize, toolchain, misc, formats, gfx_gpu_zig, sdl_dynamic, sdl_dynamic_dep.path("include"));
    const gfx = if (std.mem.eql(u8, renderer, "sdl_gpu")) gfx_gpu else gfx_legacy;
    const randommapgen = addRandomMapGen(b, target, optimize, toolchain);
    const ailogic = addLegacyProjectDll(b, target, optimize, toolchain, "AILogic", "Sources/src/AILogic/AILogic.vcxproj", "Sources/src/AILogic/AILogic.def", &.{ "Sources/src/AILogic", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/GameTT", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/vorbis-1.3.7/include" }, &.{ misc, lualib, formats, randommapgen, zlib }, sdl_dynamic);
    const gamett = addLegacyProjectDll(b, target, optimize, toolchain, "GameTT", "Sources/src/GameTT/GameTT.vcxproj", "Sources/src/GameTT/GameTT.def", &.{ "Sources/src/GameTT", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/src/AILogic" }, &.{ misc, formats, common, randommapgen }, sdl_dynamic);
    // Compile the game version directly into GameTT.dll so the title screen
    // shows the version string without relying on the Win32 version resource
    // API (which Zig's resinator does not produce correctly for runtime reads).
    gamett.root_module.addCMacro("BLITZKRIEG_VERSION", b.fmt("\"{d}.{d}.{d}\"", .{ game_version.major, game_version.minor, game_version.patch }));
    const main = addMain(b, target, optimize, toolchain);
    if (startup_trace) main.root_module.addCMacro("BK_STARTUP_TRACE", "1");
    const game = addGame(b, target, optimize, toolchain, main, misc, lualib, zlib, randommapgen, formats, blitz64, startup_trace, renderer, platform, sdl_dynamic, sdl_dynamic_dep.path("include"));
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
    b.installArtifact(buildversion);
    b.installArtifact(betakeygen);
    b.installArtifact(input);
    b.installArtifact(formats);
    b.installArtifact(anim);
    b.installArtifact(common);
    b.installArtifact(ui);
    b.installArtifact(fontgen);
    b.installArtifact(sfx);
    b.installArtifact(gfx);
    b.installArtifact(randommapgen);
    b.installArtifact(main);
    b.installArtifact(options_bridge);
    b.installArtifact(ailogic);
    b.installArtifact(gamett);
    b.installArtifact(streamio_zig);
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

    const buildversion_step = b.step("buildversion", "Build the BuildVersion console utility");
    buildversion_step.dependOn(&b.addInstallArtifact(buildversion, .{}).step);

    const betakeygen_step = b.step("betakeygen", "Build the BetaKeyGen console utility");
    betakeygen_step.dependOn(&b.addInstallArtifact(betakeygen, .{}).step);

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

    const fontgen_step = b.step("fontgen", "Build the FontGen console utility");
    fontgen_step.dependOn(&b.addInstallArtifact(fontgen, .{}).step);

    const sfx_step = b.step("sfx", "Build the SFX dynamic library");
    sfx_step.dependOn(&b.addInstallArtifact(sfx, .{}).step);

    const gfx_step = b.step("gfx", "Build the GFX dynamic library");
    gfx_step.dependOn(&b.addInstallArtifact(gfx, .{}).step);

    const gfx_legacy_step = b.step("gfx-legacy", "Build the legacy DirectX GFX dynamic library");
    gfx_legacy_step.dependOn(&b.addInstallArtifact(gfx_legacy, .{}).step);

    const gfx_gpu_step = b.step("gfx-sdl-gpu", "Build the SDL GPU GFX adapter dynamic library");
    gfx_gpu_step.dependOn(&b.addInstallArtifact(gfx_gpu, .{}).step);

    const gfx_gpu_factory_test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    gfx_gpu_factory_test_module.addCSourceFiles(.{
        .files = &.{ "tools/zig/gfxgpu_factory_test.cpp", "Sources/src/GFXGPU/GraphicsEngineGpu.cpp", "Sources/src/GFXGPU/TextureGpu.cpp", "Sources/src/GFXGPU/GeometryBufferGpu.cpp", "Sources/src/GFXGPU/MeshGpu.cpp" },
        .flags = cppflagsForOptimize(optimize),
    });
    addProjectIncludePaths(b, gfx_gpu_factory_test_module);
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFX"));
    gfx_gpu_factory_test_module.addIncludePath(b.path("Sources/src/GFXGPU"));
    addMsvcIncludePaths(b, gfx_gpu_factory_test_module, toolchain);
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
    game_all_step.dependOn(&b.addInstallArtifact(game, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(options_bridge, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(scene, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(ailogic, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(gamett, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(anim, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(gfx, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(image, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(input, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(net, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(sfx, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(ui, .{}).step);

    const stage_tool = b.addExecutable(.{
        .name = "stage-game",
        .root_module = stage_module,
    });

    const install_game_cmd = b.addRunArtifact(stage_tool);
    install_game_cmd.addArg(".");
    install_game_cmd.addArg(stage_root);
    addStageLayoutArgs(install_game_cmd, stage_game_name, stage_runtime_files, stage_debug_files, target.result.os.tag == .windows);
    if (!copy_data) install_game_cmd.addArg("--link-data");
    install_game_cmd.step.dependOn(gfx_gpu_shaders_step);

    const install_game_step = b.step("install-game", "Create runnable game install layout with binaries and Data");
    install_game_cmd.step.dependOn(game_all_step);
    install_game_step.dependOn(&install_game_cmd.step);

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

    const stage_package_game_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_cmd.addArg(".");
    stage_package_game_cmd.addArg(b.fmt("{s}/game", .{stage_root}));
    addStageLayoutArgs(stage_package_game_cmd, stage_game_name, stage_runtime_files, stage_debug_files, target.result.os.tag == .windows);

    const package_tool = b.addExecutable(.{
        .name = "package",
        .root_module = package_module,
    });
    const package_tool_run = b.addRunArtifact(package_tool);
    package_tool_run.addArg(b.fmt("{s}/game", .{stage_root}));
    package_tool_run.addArg(b.fmt("{s}/Blitzkrieg-game.zip", .{package_root}));
    package_tool_run.step.dependOn(&stage_package_game_cmd.step);

    const package_game_step = b.step("package-game", "Create game-only installation zip package");
    package_game_step.dependOn(game_all_step);
    package_game_step.dependOn(gfx_gpu_shaders_step);
    package_game_step.dependOn(&stage_package_game_cmd.step);
    package_game_step.dependOn(&package_tool_run.step);

    const stage_package_game_editors_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_editors_cmd.addArg(".");
    stage_package_game_editors_cmd.addArg(b.fmt("{s}/game", .{stage_root}));
    addStageLayoutArgs(stage_package_game_editors_cmd, stage_game_name, stage_runtime_files, stage_debug_files, target.result.os.tag == .windows);
    stage_package_game_editors_cmd.addArg("--include-editors");
    stage_package_game_editors_cmd.addArg("--editors-only");
    stage_package_game_editors_cmd.step.dependOn(&package_tool_run.step);

    const package_tool_editors = b.addRunArtifact(package_tool);
    package_tool_editors.step.dependOn(&stage_package_game_editors_cmd.step);
    package_tool_editors.addArg(b.fmt("{s}/game", .{stage_root}));
    package_tool_editors.addArg(b.fmt("{s}/Blitzkrieg-game-with-editors.zip", .{package_root}));

    const package_game_editors_step = b.step("package-game-editors", "Create installation zip package with editor tools");
    package_game_editors_step.dependOn(game_all_step);
    package_game_editors_step.dependOn(&package_tool_editors.step);

    const package_step = b.step("package", "Create both game-only and with-editors installation zip packages");
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
    abi_test.subsystem = .console;
    abi_test.entry = .{ .symbol_name = "main" };
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
    } else file_utils_module.link_libc = true;
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
    } else paths_module.link_libc = true;
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

fn addStageLayoutArgs(run: anytype, game_name: []const u8, runtime_files: []const []const u8, debug_files: []const []const u8, editors_supported: bool) void {
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
    sdl_c: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    var flags: std.ArrayListUnmanaged([]const u8) = .empty;
    flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
    flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    module.addCSourceFiles(.{
        .files = &.{ "Sources/src/StreamIOZig/options_bridge.cpp", "Sources/src/Platform/DynamicLibrary.cpp" },
        .flags = flags.items,
    });
    addMsvcIncludePaths(b, module, toolchain);
    addMsvcLibraryPaths(b, module, toolchain);
    linkMsvcRuntime(module, optimize);
    module.linkLibrary(sdl_c);
    if (target.result.os.tag == .windows) module.linkSystemLibrary("comsuppw", .{});
    return b.addLibrary(.{ .name = "StreamIOOptionsAbi", .linkage = .dynamic, .root_module = module });
}

fn addStreamIOZig(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    options_bridge: *std.Build.Step.Compile,
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
            "Sources/src/Platform/Clock.cpp",
            "Sources/src/Platform/Debug.cpp",
        },
        .flags = flags.items,
    });
    addMsvcIncludePaths(b, streamio_module, toolchain);
    addMsvcLibraryPaths(b, streamio_module, toolchain);
    streamio_module.linkLibrary(options_bridge);
    linkMsvcRuntime(streamio_module, optimize);
    // x86 exports carry stdcall decorations (_name@N) that do not exist on
    // x86_64, so the def file is per-arch.
    const def_path = if (target.result.cpu.arch == .x86)
        "Sources/src/StreamIOZig/StreamIO.def"
    else
        "Sources/src/StreamIOZig/StreamIO.x64.def";
    return b.addLibrary(.{
        .name = "StreamIO",
        .linkage = .dynamic,
        .root_module = streamio_module,
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
        if (std.mem.endsWith(u8, source, ".cpp") or std.mem.endsWith(u8, source, ".c")) {
            const placed = b.fmt("Sources/src/{s}/{s}", .{ name, source });
            if (std.mem.indexOf(u8, source, "xiph") != null) {
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
    return b.addLibrary(.{
        .name = name,
        .linkage = .dynamic,
        .root_module = module,
        .win32_module_definition = if (target.result.os.tag == .windows) b.path(definition) else null,
    });
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
    addMsvcIncludePaths(b, game_module, toolchain);
    addMsvcLibraryPaths(b, game_module, toolchain);
    game_module.addIncludePath(b.path("Sources/src/Game"));
    game_module.addIncludePath(sdl_include);
    game_module.addIncludePath(b.path("Sources/src/Main"));
    game_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    if (startup_trace) game_module.addCMacro("BK_STARTUP_TRACE", "1");
    game_module.addCSourceFiles(.{
        .files = game_sources,
        .flags = cppflagsGameForOptimize(optimize),
    });
    game_module.linkLibrary(main);
    game_module.linkLibrary(misc);
    linkSdlImport(game_module, target, sdl_dynamic);
    game_module.linkLibrary(lualib);
    game_module.linkLibrary(zlib);
    game_module.linkLibrary(randommapgen);
    game_module.linkLibrary(formats);
    game_module.linkLibrary(blitz64);
    linkMsvcRuntime(game_module, optimize);
    game_module.linkSystemLibrary("version", .{});
    game_module.linkSystemLibrary("winmm", .{});
    game_module.linkSystemLibrary("odbc32", .{});
    game_module.linkSystemLibrary("odbccp32", .{});
    if (std.mem.eql(u8, renderer, "legacy")) {
        game_module.linkSystemLibrary("d3d9", .{});
    }
    game_module.linkSystemLibrary("shlwapi", .{});
    game_module.linkSystemLibrary("advapi32", .{});
    game_module.linkSystemLibrary("user32", .{});
    game_module.linkSystemLibrary("gdi32", .{});
    linkComSupport(game_module, optimize);
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

    const game = b.addExecutable(.{
        .name = "Game",
        .root_module = game_module,
    });
    game.subsystem = switch (build_support.subsystem(platform, true)) {
        .windows => .windows,
        .console => .console,
    };
    game.entry = switch (build_support.entryPoint(platform, true)) {
        .win_main_crt_startup => .{ .symbol_name = "WinMainCRTStartup" },
        .main_crt_startup => .{ .symbol_name = "mainCRTStartup" },
        .main => .{ .symbol_name = "main" },
    };
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
    sdl_dynamic: *std.Build.Step.Compile,
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
    var misc_flags: std.ArrayListUnmanaged([]const u8) = .empty;
    misc_flags.appendSlice(b.allocator, cppflagsForOptimize(optimize)) catch @panic("OOM");
    misc_flags.append(b.allocator, "-std=c++17") catch @panic("OOM");
    misc_module.addCSourceFiles(.{
        .files = misc_sources,
        .flags = misc_flags.items,
    });
    linkSdlRuntime(misc_module, target, sdl_dynamic, sdl_include);

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
    linkSdlImport(image_module, target, sdl_dynamic);
    image_module.linkLibrary(libpng);
    image_module.linkLibrary(zlib);
    linkMsvcRuntime(image_module, optimize);
    image_module.linkSystemLibrary("user32", .{});

    return b.addLibrary(.{
        .name = "Image",
        .linkage = .dynamic,
        .root_module = image_module,
        .win32_module_definition = b.path("Sources/src/Image/Image.def"),
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
    net_module.addCSourceFiles(.{
        .files = net_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    net_module.linkLibrary(misc);
    linkSdlImport(net_module, target, sdl_dynamic);
    linkMsvcRuntime(net_module, optimize);
    net_module.linkSystemLibrary("ws2_32", .{});
    net_module.linkSystemLibrary("odbc32", .{});
    net_module.linkSystemLibrary("odbccp32", .{});

    return b.addLibrary(.{
        .name = "Net",
        .linkage = .dynamic,
        .root_module = net_module,
        .win32_module_definition = b.path("Sources/src/Net/net.def"),
    });
}

fn addBuildVersion(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
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
    linkSdlImport(buildversion_module, target, sdl_dynamic);
    linkMsvcRuntime(buildversion_module, optimize);
    buildversion_module.linkSystemLibrary("odbc32", .{});
    buildversion_module.linkSystemLibrary("odbccp32", .{});

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
    linkSdlImport(betakeygen_module, target, sdl_dynamic);
    betakeygen_module.linkLibrary(zlib);
    linkMsvcRuntime(betakeygen_module, optimize);
    betakeygen_module.linkSystemLibrary("odbc32", .{});
    betakeygen_module.linkSystemLibrary("odbccp32", .{});

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
    input_module.addCSourceFiles(.{
        .files = input_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    input_module.linkLibrary(misc);
    linkSdlImport(input_module, target, sdl_dynamic);
    linkMsvcRuntime(input_module, optimize);
    input_module.linkSystemLibrary("winmm", .{});
    input_module.linkSystemLibrary("dinput8", .{});
    input_module.linkSystemLibrary("dxguid", .{});
    input_module.linkSystemLibrary("user32", .{});
    input_module.linkSystemLibrary("odbc32", .{});
    input_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(input_module, optimize);

    return b.addLibrary(.{
        .name = "Input",
        .linkage = .dynamic,
        .root_module = input_module,
        .win32_module_definition = b.path("Sources/src/Input/Input.def"),
    });
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
    linkSdlImport(anim_module, target, sdl_dynamic);
    anim_module.linkLibrary(formats);
    linkMsvcRuntime(anim_module, optimize);
    anim_module.linkSystemLibrary("odbc32", .{});
    anim_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(anim_module, optimize);

    return b.addLibrary(.{
        .name = "Anim",
        .linkage = .dynamic,
        .root_module = anim_module,
        .win32_module_definition = b.path("Sources/src/Anim/Animation.def"),
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
    ui_module.addCSourceFiles(.{
        .files = ui_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    ui_module.linkLibrary(misc);
    linkSdlImport(ui_module, target, sdl_dynamic);
    ui_module.linkLibrary(common);
    ui_module.linkLibrary(lualib);
    linkMsvcRuntime(ui_module, optimize);
    ui_module.linkSystemLibrary("odbc32", .{});
    ui_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(ui_module, optimize);

    return b.addLibrary(.{
        .name = "UI",
        .linkage = .dynamic,
        .root_module = ui_module,
        .win32_module_definition = b.path("Sources/src/UI/UI.def"),
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
    linkSdlImport(fontgen_module, target, sdl_dynamic);
    linkMsvcRuntime(fontgen_module, optimize);
    fontgen_module.linkSystemLibrary("user32", .{});
    fontgen_module.linkSystemLibrary("gdi32", .{});
    fontgen_module.linkSystemLibrary("odbc32", .{});
    fontgen_module.linkSystemLibrary("odbccp32", .{});

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
    linkSdlImport(sfx_module, target, sdl_dynamic);
    sfx_module.linkLibrary(common);
    linkMsvcRuntime(sfx_module, optimize);
    sfx_module.linkSystemLibrary("winmm", .{});
    sfx_module.linkSystemLibrary("odbc32", .{});
    sfx_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(sfx_module, optimize);

    return b.addLibrary(.{
        .name = "SFX",
        .linkage = .dynamic,
        .root_module = sfx_module,
        .win32_module_definition = b.path("Sources/src/SFX/Sound.def"),
    });
}

fn addGFX(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
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
    linkSdlImport(gfx_module, target, sdl_dynamic);
    gfx_module.linkLibrary(formats);
    linkMsvcRuntime(gfx_module, optimize);
    gfx_module.linkSystemLibrary("d3d9", .{});
    gfx_module.linkSystemLibrary("dxguid", .{});
    gfx_module.linkSystemLibrary("user32", .{});
    gfx_module.linkSystemLibrary("gdi32", .{});
    gfx_module.linkSystemLibrary("odbc32", .{});
    gfx_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(gfx_module, optimize);

    return b.addLibrary(.{
        .name = "GFX",
        .linkage = .dynamic,
        .root_module = gfx_module,
        .win32_module_definition = b.path("Sources/src/GFX/GFX.def"),
    });
}

fn addGFXGPU(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
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
    gfx_gpu_module.linkLibrary(formats);
    gfx_gpu_module.linkLibrary(gfx_gpu_zig);
    linkSdlRuntime(gfx_gpu_module, target, sdl_dynamic, sdl_include);
    linkMsvcRuntime(gfx_gpu_module, optimize);
    gfx_gpu_module.linkSystemLibrary("user32", .{});
    gfx_gpu_module.linkSystemLibrary("gdi32", .{});

    return b.addLibrary(.{
        .name = "GFXGPU",
        .linkage = .dynamic,
        .root_module = gfx_gpu_module,
        .win32_module_definition = b.path("Sources/src/GFXGPU/GFXGPU.def"),
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

    return b.addLibrary(.{
        .name = "GfxGpuZig",
        .linkage = .static,
        .root_module = gfx_gpu_module,
    });
}

fn cflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_release,
    };
}

fn cppflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => if (ubsan_trap) cppflags_debug_trap else cppflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_release,
    };
}

fn cppflagsBetaForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_beta_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_beta_release,
    };
}

fn cflagsSfxForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cflags_sfx_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_sfx_release,
    };
}

fn cppflagsSfxForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_sfx_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_sfx_release,
    };
}

fn cppflagsGameForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_game_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_game_release,
    };
}

fn addProjectIncludePaths(b: *std.Build, module: *std.Build.Module) void {
    module.addIncludePath(b.path("Sources/src"));
    module.addIncludePath(b.path("Sources/src/Misc"));
    module.addIncludePath(b.path("Sources/src/Formats"));
}

const ToolchainIncludes = struct {
    msvc_include: []const u8,
    windows_sdk_include: []const u8,
    msvc_lib: []const u8,
    windows_sdk_lib: []const u8,
    library_arch: []const u8,
};

fn addMsvcIncludePaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    module.addSystemIncludePath(.{ .cwd_relative = toolchain.msvc_include });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\ucrt", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\shared", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\um", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\winrt", .{toolchain.windows_sdk_include}) });
}

fn addMsvcLibraryPaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\{s}", .{ toolchain.msvc_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\ucrt\\{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\um\\{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
}

fn addLinuxCxxIncludePaths(b: *std.Build, module: *std.Build.Module, target: std.Build.ResolvedTarget) void {
    if (b.graph.host.result.os.tag != .linux) return;
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
    const arch = switch (target.result.cpu.arch) {
        .x86_64 => "x86_64",
        .aarch64 => "aarch64",
        else => return,
    };
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/include/c++/{s}", .{version}) });
    module.addSystemIncludePath(.{ .cwd_relative = "/usr/include" });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/include/{s}-linux-gnu", .{arch}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/include/{s}-linux-gnu/c++/{s}", .{ arch, version }) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/include/c++/{s}/backward", .{version}) });
    var gcc_versions = std.Io.Dir.openDirAbsolute(b.graph.io, b.fmt("/usr/lib/gcc/{s}-linux-gnu", .{arch}), .{ .iterate = true }) catch return;
    defer std.Io.Dir.close(gcc_versions, b.graph.io);
    var gcc_iterator = gcc_versions.iterate();
    while (gcc_iterator.next(b.graph.io) catch null) |entry| {
        if (entry.kind == .directory) {
            module.addSystemIncludePath(.{ .cwd_relative = b.fmt("/usr/lib/gcc/{s}-linux-gnu/{s}/include", .{ arch, entry.name }) });
            break;
        }
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

fn addSdlApplicationTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    test_mode: build_support.TestMode,
    toolchain: ToolchainIncludes,
    sdl_dynamic: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
) void {
    const module = b.createModule(.{
        .target = target,
        .optimize = .Debug,
        .link_libc = true,
    });
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{
            "Sources/src/Platform/SDLApplication.cpp",
            "tools/zig/platform_window_test.cpp",
        },
        .flags = &.{"-std=c++17"},
    });
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
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "main" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
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
        if (target.result.os.tag == .linux) addLinuxCxxIncludePaths(b, module, target);
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
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    gfx_gpu_zig: *std.Build.Step.Compile,
    sdl_c: *std.Build.Step.Compile,
    sdl_include: std.Build.LazyPath,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    module.addIncludePath(sdl_include);
    module.addIncludePath(b.path("Sources/src/GFXGPU"));
    module.addCSourceFiles(.{ .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "tools/zig/game_bootstrap_smoke.cpp" }, .flags = &.{"-std=c++17"} });
    module.linkLibrary(gfx_gpu_zig);
    module.linkLibrary(sdl_c);
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
    const exe = b.addExecutable(.{ .name = "game-bootstrap-smoke", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "main" };
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
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
) void {
    const module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = true });
    module.addIncludePath(sdl_include);
    module.addCSourceFiles(.{
        .files = &.{ "Sources/src/Platform/SDLApplication.cpp", "tools/zig/platform_event_test.cpp" },
        .flags = &.{"-std=c++17"},
    });
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
    if (target.result.os.tag == .windows) test_exe.entry = .{ .symbol_name = "main" };
    const test_run = b.addRunArtifact(test_exe);
    test_run.setCwd(b.path("."));
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
        else => module.addObjectFile(sdl_dynamic.getEmittedBin()),
    }
}

fn linkMsvcRuntime(module: *std.Build.Module, optimize: std.builtin.OptimizeMode) void {
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
