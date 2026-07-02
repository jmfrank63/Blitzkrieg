# Microsoft Developer Studio Project File - Name="Scene" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Scene - Win32 Profiler
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Scene.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Scene.mak" CFG="Scene - Win32 Profiler"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Scene - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Scene - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Scene - Win32 BetaRelease" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Scene - Win32 FastDebug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Scene - Win32 Profiler" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/A7/Scene", QLOAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Scene - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GR /GX /O2 /D "_FINALRELEASE" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /Yu"StdAfx.h" /FD /Zm150 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386
# SUBTRACT LINK32 /debug
# Begin Special Build Tool
OutDir=.\Release
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(OutDir)\*.dll c:\a7\*.*
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Scene - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GR /GX /ZI /Od /D "_WINDOWS" /D "_DO_ASSERT_SLOW" /D "_DO_CHECKED_CAST" /D "WIN32" /D "_DEBUG" /D "_STL_RANGE_CHECK" /Yu"StdAfx.h" /FD /GZ /Zm200 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
OutDir=.\Debug
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(OutDir)\*.dll c:\a7\*.*
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Scene - Win32 BetaRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BetaRelease"
# PROP BASE Intermediate_Dir "BetaRelease"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "BetaRelease"
# PROP Intermediate_Dir "BetaRelease"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GR /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Yu"StdAfx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GR /GX /Zd /Ot /Og /Op /Ob2 /D "_DO_ASSERT_SLOW" /D "_BETARELEASE" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /Yu"StdAfx.h" /FD /Zm150 /c
# SUBTRACT CPP /Ox /Oi
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /pdb:none /map /debug /machine:I386 /def:".\Scene.def" /FIXED:NO
# Begin Special Build Tool
OutDir=.\BetaRelease
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(OutDir)\*.dll c:\a7\*.*
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Scene - Win32 FastDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Scene___Win32_FastDebug"
# PROP BASE Intermediate_Dir "Scene___Win32_FastDebug"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "FastDebug"
# PROP Intermediate_Dir "FastDebug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GR /GX /ZI /Od /D "_WINDOWS" /D "_DO_ASSERT_SLOW" /D "WIN32" /D "_DEBUG" /D "_STL_FAST_DEBUG" /Yu"StdAfx.h" /FD /GZ /c
# ADD CPP /nologo /MD /W3 /GR /GX /Zi /Od /D "_DO_ASSERT_SLOW" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "_DO_CHECKED_CAST" /Yu"StdAfx.h" /FD /GZ /Zm150 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
OutDir=.\FastDebug
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(OutDir)\*.dll c:\a7\*.*
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Scene - Win32 Profiler"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Scene___Win32_Profiler"
# PROP BASE Intermediate_Dir "Scene___Win32_Profiler"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Profiler"
# PROP Intermediate_Dir "Profiler"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GR /GX /Zi /Od /D "_DO_ASSERT_SLOW" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "_DO_CHECKED_CAST" /Yu"StdAfx.h" /FD /GZ /Zm150 /c
# ADD CPP /nologo /MD /W3 /GR /GX /Zi /O2 /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "_FINALRELEASE" /D "_PROFILER" /Yu"StdAfx.h" /FD /Zm150 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comsupp.lib /nologo /subsystem:windows /dll /incremental:no /debug /machine:I386 /def:".\Scene.def" /pdbtype:sept /fixed:no
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
OutDir=.\Profiler
SOURCE="$(InputPath)"
PostBuild_Cmds=copy $(OutDir)\*.dll c:\a7\*.*
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "Scene - Win32 Release"
# Name "Scene - Win32 Debug"
# Name "Scene - Win32 BetaRelease"
# Name "Scene - Win32 FastDebug"
# Name "Scene - Win32 Profiler"
# Begin Group "Common"

# PROP Default_Filter "h;hpp;hxx;hm;inl;cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\cursor\attack.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\aviation.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\board.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\build_antitank.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\build_bridge.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\build_entrenchment.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\build_wire_fence.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\cancel.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\capture_artillery.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\clear_mines.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\deploy_artillery.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\entrench_self.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\fill_ru.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\follow.cur
# End Source File
# Begin Source File

SOURCE=.\GlobalsLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\cursor\hook_artillery.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\human_resupply.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\intermission.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\leave.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\move.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\move2grid.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\place_marker.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\ranging.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\repair.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\resupply.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\rotate.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\select_enemy.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\select_friend.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\select_neutral.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\self_actions.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\set_mines.cur
# End Source File
# Begin Source File

SOURCE=.\Specific.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"StdAfx.h"
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\stl_user_config.h
# End Source File
# Begin Source File

SOURCE=.\cursor\supress.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\swarm.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\unknown.cur
# End Source File
# Begin Source File

SOURCE=.\cursor\use_spyglasses.cur
# End Source File
# End Group
# Begin Group "Scene"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Scene.h
# End Source File
# Begin Source File

SOURCE=.\SceneDraw.cpp
# End Source File
# Begin Source File

SOURCE=.\SceneInternal.cpp
# End Source File
# Begin Source File

SOURCE=.\SceneInternal.h
# End Source File
# Begin Source File

SOURCE=.\SceneObjectFactory.cpp
# End Source File
# Begin Source File

SOURCE=.\SceneObjectFactory.h
# End Source File
# Begin Source File

SOURCE=.\ScenePick.cpp
# End Source File
# End Group
# Begin Group "Vis Objects"

# PROP Default_Filter ""
# Begin Group "Icons"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Icon.cpp
# End Source File
# Begin Source File

SOURCE=.\Icon.h
# End Source File
# Begin Source File

SOURCE=.\IconBar.cpp
# End Source File
# Begin Source File

SOURCE=.\IconBar.h
# End Source File
# Begin Source File

SOURCE=.\IconHPBar.cpp
# End Source File
# Begin Source File

SOURCE=.\IconHPBar.h
# End Source File
# Begin Source File

SOURCE=.\IconPic.cpp
# End Source File
# Begin Source File

SOURCE=.\IconPic.h
# End Source File
# Begin Source File

SOURCE=.\IconText.cpp
# End Source File
# Begin Source File

SOURCE=.\IconText.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\BoldLineVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\BoldLineVisObj.h
# End Source File
# Begin Source File

SOURCE=.\EffectVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\EffectVisObj.h
# End Source File
# Begin Source File

SOURCE=.\FixedObjList.h
# End Source File
# Begin Source File

SOURCE=.\FlashVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\FlashVisObj.h
# End Source File
# Begin Source File

SOURCE=.\MeshVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\MeshVisObj.h
# End Source File
# Begin Source File

SOURCE=.\ObjVisObj.h
# End Source File
# Begin Source File

SOURCE=.\SpriteVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\SpriteVisObj.h
# End Source File
# Begin Source File

SOURCE=.\SquadVisObj.cpp
# End Source File
# Begin Source File

SOURCE=.\SquadVisObj.h
# End Source File
# Begin Source File

SOURCE=.\VisObjBuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\VisObjBuilder.h
# End Source File
# End Group
# Begin Group "Statistics System"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\StatSystem.cpp
# End Source File
# Begin Source File

SOURCE=.\StatSystem.h
# End Source File
# End Group
# Begin Group "Camera"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Camera.cpp
# End Source File
# Begin Source File

SOURCE=.\Camera.h
# End Source File
# End Group
# Begin Group "Cursor"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Cursor.cpp
# End Source File
# Begin Source File

SOURCE=.\Cursor.h
# End Source File
# End Group
# Begin Group "Frame Selection"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FrameSelection.cpp
# End Source File
# Begin Source File

SOURCE=.\FrameSelection.h
# End Source File
# End Group
# Begin Group "Visitors"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\AnimVisitor.cpp
# End Source File
# Begin Source File

SOURCE=.\AnimVisitor.h
# End Source File
# Begin Source File

SOURCE=.\DrawVisitor.cpp
# End Source File
# Begin Source File

SOURCE=.\DrawVisitor.h
# End Source File
# End Group
# Begin Group "Effectors"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\MaterialEffector.cpp
# End Source File
# Begin Source File

SOURCE=.\MaterialEffector.h
# End Source File
# Begin Source File

SOURCE=.\MatrixEffector.cpp
# End Source File
# Begin Source File

SOURCE=.\MatrixEffector.h
# End Source File
# End Group
# Begin Group "SoundScene"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CellsConglomerateContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\CellsConglomerateContainer.h
# End Source File
# Begin Source File

SOURCE=.\SceneSound.cpp
# End Source File
# Begin Source File

SOURCE=.\SoundScene.cpp
# End Source File
# Begin Source File

SOURCE=.\SoundScene.h
# End Source File
# Begin Source File

SOURCE=.\SoundSceneSerialize.cpp
# End Source File
# End Group
# Begin Group "Resource"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\Scene.rc
# End Source File
# End Group
# Begin Group "VideoPlayer"

# PROP Default_Filter ""
# End Group
# Begin Group "Particles"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\KeyBasedParticleSource.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyBasedParticleSource.h
# End Source File
# Begin Source File

SOURCE=.\ParticleManager.cpp
# End Source File
# Begin Source File

SOURCE=.\ParticleManager.h
# End Source File
# Begin Source File

SOURCE=.\ParticleSourceData.cpp
# End Source File
# Begin Source File

SOURCE=.\ParticleSourceData.h
# End Source File
# Begin Source File

SOURCE=.\PFX.h
# End Source File
# Begin Source File

SOURCE=.\SmokinParticleSource.cpp
# End Source File
# Begin Source File

SOURCE=.\SmokinParticleSource.h
# End Source File
# Begin Source File

SOURCE=.\SmokinParticleSourceData.cpp
# End Source File
# Begin Source File

SOURCE=.\SmokinParticleSourceData.h
# End Source File
# Begin Source File

SOURCE=.\Track.cpp
# End Source File
# Begin Source File

SOURCE=.\Track.h
# End Source File
# End Group
# Begin Group "FastSinCos"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FastSinCos.cpp
# End Source File
# Begin Source File

SOURCE=.\FastSinCos.h
# End Source File
# End Group
# Begin Group "Depth Optimizer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DepthOptimizer.h
# End Source File
# End Group
# Begin Group "Transition"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Transition.cpp
# End Source File
# Begin Source File

SOURCE=.\Transition.h
# End Source File
# End Group
# Begin Group "Terrain"

# PROP Default_Filter ""
# Begin Group "Builder"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Builders.h
# End Source File
# Begin Source File

SOURCE=.\MeshBuilders.cpp
# End Source File
# Begin Source File

SOURCE=.\RiverBuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\TerrainBuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\TerrainBuilder.h
# End Source File
# Begin Source File

SOURCE=.\TerrainEditor.cpp
# End Source File
# End Group
# Begin Group "Vector objects"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\TerrainRoad.cpp
# End Source File
# Begin Source File

SOURCE=.\TerrainRoad.h
# End Source File
# Begin Source File

SOURCE=.\TerrainWater.cpp
# End Source File
# Begin Source File

SOURCE=.\TerrainWater.h
# End Source File
# Begin Source File

SOURCE=.\VectorObject.cpp
# End Source File
# Begin Source File

SOURCE=.\VectorObject.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\TerraDraw.cpp
# End Source File
# Begin Source File

SOURCE=.\Terrain.h
# End Source File
# Begin Source File

SOURCE=.\TerrainInternal.cpp
# End Source File
# Begin Source File

SOURCE=.\TerrainInternal.h
# End Source File
# Begin Source File

SOURCE=.\TerraSound.cpp
# End Source File
# End Group
# Begin Group "Gamma Effect"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\GammaEffect.cpp
# End Source File
# Begin Source File

SOURCE=.\GammaEffect.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Scene.def

!IF  "$(CFG)" == "Scene - Win32 Release"

!ELSEIF  "$(CFG)" == "Scene - Win32 Debug"

!ELSEIF  "$(CFG)" == "Scene - Win32 BetaRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Scene - Win32 FastDebug"

!ELSEIF  "$(CFG)" == "Scene - Win32 Profiler"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Target
# End Project
