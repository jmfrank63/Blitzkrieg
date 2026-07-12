const std = @import("std");

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
    "Sources/src/Game/main.cpp",
    "Sources/src/Game/SysKeys.cpp",
    "Sources/src/Game/WinFrame.cpp",
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
            .cpu_arch = .x86,
            .os_tag = .windows,
            .abi = .msvc,
        },
    });
    const optimize = b.standardOptimizeOption(.{});
    const blitz64 = addBlitz64(b, target, optimize);
    const library_arch = switch (target.result.cpu.arch) {
        .x86 => "x86",
        .x86_64 => "x64",
        else => @panic("The Windows runtime build supports only x86 and x86_64 targets"),
    };
    const toolchain = ToolchainIncludes{
        .msvc_include = b.option([]const u8, "msvc-include", "MSVC C/C++ include directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\include",
        .windows_sdk_include = b.option([]const u8, "windows-sdk-include", "Windows SDK include version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.26100.0",
        .msvc_lib = b.option([]const u8, "msvc-lib", "MSVC library directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\lib",
        .windows_sdk_lib = b.option([]const u8, "windows-sdk-lib", "Windows SDK library version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0",
        .library_arch = library_arch,
    };
    const streamio_zig = addStreamIOZig(b, target, optimize, toolchain);
    const install_dir = b.option([]const u8, "install-dir", "Relative install layout path (default: zig-out/game)") orelse "zig-out/game";
    const copy_data = b.option(bool, "copy-data", "Copy Data into install layout instead of creating a junction") orelse false;
    const package_dir = b.option([]const u8, "package-dir", "Relative output directory for zip installers (default: zig-out/packages)") orelse "zig-out/packages";

    const zlib = addZlib(b, target, optimize, toolchain);
    const libpng = addLibpng(b, target, optimize, toolchain, zlib);
    const misc = addMisc(b, target, optimize, toolchain);
    const image = addImage(b, target, optimize, toolchain, zlib, libpng, misc);
    const lualib = addLuaLib(b, target, optimize, toolchain);
    const net = addNet(b, target, optimize, toolchain, misc);
    const buildversion = addBuildVersion(b, target, optimize, toolchain, misc);
    const betakeygen = addBetaKeyGen(b, target, optimize, toolchain, zlib, misc);
    const input = addInput(b, target, optimize, toolchain, misc);
    const formats = addFormats(b, target, optimize, toolchain);
    const scene = addLegacyProjectDll(b, target, optimize, toolchain, "Scene", "Sources/src/Scene/Scene.vcxproj", "Sources/src/Scene/Scene.def", &.{ "Sources/src/Scene", "Sources/src/Common", "Sources/src/StreamIO", "Sources/src/GFX", "Sources/src/Input", "Sources/src/Anim", "Sources/src/Image", "Sources/src/SFX", "Sources/src/UI", "Sources/src/Main", "Sources/sdk/xiph/ogg-1.3.5/include", "Sources/sdk/xiph/libtheora-1.2.0/include" }, &.{ misc, formats });
    const anim = addAnim(b, target, optimize, toolchain, misc, formats);
    const common = addCommon(b, target, optimize, toolchain);
    const ui = addUI(b, target, optimize, toolchain, misc, common, lualib);
    const fontgen = addFontGen(b, target, optimize, toolchain, image, common, formats, misc);
    const sfx = addSFX(b, target, optimize, toolchain, misc, common);
    const gfx = addGFX(b, target, optimize, toolchain, misc, formats);
    const randommapgen = addRandomMapGen(b, target, optimize, toolchain);
    const main = addMain(b, target, optimize, toolchain);
    const game = addGame(b, target, optimize, toolchain, main, misc, lualib, zlib, randommapgen, formats, blitz64);
    const package_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/package.zig"),
        .target = target,
        .optimize = .ReleaseFast,
    });
    const stage_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/stage.zig"),
        .target = target,
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
    b.installArtifact(streamio_zig);
    b.installArtifact(game);

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

    const randommapgen_step = b.step("randommapgen", "Build the RandomMapGen static library");
    randommapgen_step.dependOn(&b.addInstallArtifact(randommapgen, .{}).step);

    const main_step = b.step("main", "Build the Main static library");
    main_step.dependOn(&b.addInstallArtifact(main, .{}).step);

    const streamio_step = b.step("streamio", "Build the Zig StreamIO dynamic library");
    streamio_step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);

    const scene_step = b.step("scene", "Build the Scene x64 dynamic library");
    scene_step.dependOn(&b.addInstallArtifact(scene, .{}).step);

    const game_step = b.step("game", "Build the Game executable");
    game_step.dependOn(&b.addInstallArtifact(game, .{}).step);

    const game_all_step = b.step("game-all", "Build and install the playable game runtime set");
    game_all_step.dependOn(&b.addInstallArtifact(game, .{}).step);
    game_all_step.dependOn(&b.addInstallArtifact(streamio_zig, .{}).step);
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
    install_game_cmd.addArg(install_dir);
    if (target.result.cpu.arch == .x86_64) {
        install_game_cmd.addArg("--exclude-x86-runtime");
    }
    if (copy_data) {
        install_game_cmd.addArg("--copy-data");
    }

    const install_game_step = b.step("install-game", "Create runnable game install layout with binaries and Data");
    install_game_step.dependOn(game_all_step);
    install_game_step.dependOn(&install_game_cmd.step);

    const run_game_cmd = b.addSystemCommand(&.{"Game.exe"});
    run_game_cmd.setCwd(b.path(install_dir));
    run_game_cmd.step.dependOn(install_game_step);
    if (b.args) |args| {
        run_game_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Build, install, and run Game.exe from install layout");
    run_step.dependOn(install_game_step);
    run_step.dependOn(&run_game_cmd.step);

    const verify_x64_cmd = b.addSystemCommand(&.{
        "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        "tools/zig/verify_x64_runtime.ps1", install_dir,
    });
    verify_x64_cmd.step.dependOn(install_game_step);
    const verify_x64_step = b.step("verify-x64-runtime", "Validate the staged Windows x64 runtime");
    verify_x64_step.dependOn(&verify_x64_cmd.step);

    const stage_package_game_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_cmd.addArg(".");
    stage_package_game_cmd.addArg("zig-out/package-staging/game");
    stage_package_game_cmd.addArg("--copy-data");

    const package_tool = b.addExecutable(.{
        .name = "package",
        .root_module = package_module,
    });
    const package_tool_run = b.addRunArtifact(package_tool);
    package_tool_run.addArg("zig-out/package-staging/game");
    package_tool_run.addArg(b.fmt("{s}/Blitzkrieg-game.zip", .{package_dir}));

    const package_game_step = b.step("package-game", "Create game-only installation zip package");
    package_game_step.dependOn(game_all_step);
    package_game_step.dependOn(&stage_package_game_cmd.step);
    package_game_step.dependOn(&package_tool_run.step);

    const stage_package_game_editors_cmd = b.addRunArtifact(stage_tool);
    stage_package_game_editors_cmd.addArg(".");
    stage_package_game_editors_cmd.addArg("zig-out/package-staging/game");
    stage_package_game_editors_cmd.addArg("--include-editors");
    stage_package_game_editors_cmd.addArg("--editors-only");
    stage_package_game_editors_cmd.step.dependOn(&package_tool_run.step);

    const package_tool_editors = b.addRunArtifact(package_tool);
    package_tool_editors.step.dependOn(&stage_package_game_editors_cmd.step);
    package_tool_editors.addArg("zig-out/package-staging/game");
    package_tool_editors.addArg(b.fmt("{s}/Blitzkrieg-game-with-editors.zip", .{package_dir}));

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
    const test_step = b.step("test", "Run Zig unit tests and the Blitz64 ABI smoke test");
    test_step.dependOn(&run_blitz64_unit_tests.step);
    test_step.dependOn(&run_abi_test.step);
    if (target.result.cpu.arch == .x86_64) test_step.dependOn(&verify_x64_cmd.step);

    b.default_step = game_all_step;
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

fn addStreamIOZig(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const streamio_module = b.createModule(.{
        .root_source_file = b.path("Sources/src/StreamIOZig/streamio.zig"),
        .target = target,
        .optimize = optimize,
    });
    streamio_module.addCSourceFile(.{
        .file = b.path("Sources/src/StreamIOZig/legacy_bridge.cpp"),
        .flags = &.{"-std=c++17"},
    });
    addMsvcLibraryPaths(b, streamio_module, toolchain);
    linkMsvcRuntime(streamio_module, optimize);
    return b.addLibrary(.{
        .name = "StreamIO",
        .linkage = .dynamic,
        .root_module = streamio_module,
        .win32_module_definition = b.path("Sources/src/StreamIOZig/StreamIO.def"),
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
) *std.Build.Step.Compile {
    const contents = std.Io.Dir.cwd().readFileAlloc(b.graph.io, project, b.allocator, .limited(8 * 1024 * 1024)) catch |err| @panic(@errorName(err));
    var files: std.ArrayListUnmanaged([]const u8) = .empty;
    var offset: usize = 0;
    const marker = "<ClCompile Include=\"";
    while (std.mem.indexOfPos(u8, contents, offset, marker)) |start| {
        const path_start = start + marker.len;
        const path_end = std.mem.indexOfPos(u8, contents, path_start, "\"") orelse break;
        const source = contents[path_start..path_end];
        if (std.mem.endsWith(u8, source, ".cpp") or std.mem.endsWith(u8, source, ".c")) {
            files.append(b.allocator, b.fmt("Sources/src/{s}/{s}", .{ name, source })) catch @panic("OOM");
        }
        offset = path_end + 1;
    }
    const module = b.createModule(.{ .target = target, .optimize = optimize });
    addProjectIncludePaths(b, module);
    addMsvcIncludePaths(b, module, toolchain);
    addMsvcLibraryPaths(b, module, toolchain);
    for (includes) |include| module.addIncludePath(b.path(include));
    module.addCSourceFiles(.{ .files = files.items, .flags = cppflagsForOptimize(optimize) });
    for (libraries) |library| module.linkLibrary(library);
    linkMsvcRuntime(module, optimize);
    module.linkSystemLibrary("winmm", .{});
    module.linkSystemLibrary("odbc32", .{});
    module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(module, optimize);
    return b.addLibrary(.{ .name = name, .linkage = .dynamic, .root_module = module, .win32_module_definition = b.path(definition) });
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
) *std.Build.Step.Compile {
    const game_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, game_module);
    addMsvcIncludePaths(b, game_module, toolchain);
    addMsvcLibraryPaths(b, game_module, toolchain);
    game_module.addIncludePath(b.path("Sources/src/Game"));
    game_module.addIncludePath(b.path("Sources/src/Main"));
    game_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    game_module.addCSourceFiles(.{
        .files = game_sources,
        .flags = cppflagsGameForOptimize(optimize),
    });
    game_module.linkLibrary(main);
    game_module.linkLibrary(misc);
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
    game_module.linkSystemLibrary("d3d9", .{});
    game_module.linkSystemLibrary("shlwapi", .{});
    game_module.linkSystemLibrary("advapi32", .{});
    game_module.linkSystemLibrary("user32", .{});
    game_module.linkSystemLibrary("gdi32", .{});
    linkComSupport(game_module, optimize);

    const game = b.addExecutable(.{
        .name = "Game",
        .root_module = game_module,
    });
    game.subsystem = .windows;
    game.entry = .{ .symbol_name = "WinMainCRTStartup" };
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
) *std.Build.Step.Compile {
    const misc_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, misc_module);
    addMsvcIncludePaths(b, misc_module, toolchain);
    misc_module.addIncludePath(b.path("Sources/src/Misc"));
    misc_module.addIncludePath(b.path("Sources/src/zlib"));
    misc_module.addCSourceFiles(.{
        .files = misc_sources,
        .flags = cppflagsForOptimize(optimize),
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

fn cflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_release,
    };
}

fn cppflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_debug,
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
