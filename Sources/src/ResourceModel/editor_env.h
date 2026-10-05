#pragma once
// What the MFC item defaults read from the running editor: the browse-dialog
// filters of Sources/src/editor/common.cpp, CEditorApp::GetEditorDataDir()
// and the active frame's project file. The model holds them as settings the
// editor fills in; until it does, the data directory is empty and there is no
// active project, which is the MFC state before a project is opened.

#include <string>

namespace NResourceModel
{

extern const std::string szTGAFilter;
extern const std::string szTextFilter;
extern const std::string szLuaFilter;
extern const std::string szMusicFilter;
extern const std::string szMovieFilter;
extern const std::string szXMLFilter;
extern const std::string szMapFilter;
extern const std::string szSoundFilter;
extern const std::string szMODFilter;
extern const std::string szSANFilter;
extern const std::string szDDSFilter;

// theApp.GetEditorDataDir(): the editor's data directory, ending in a separator.
std::string GetEditorDataDir();
void SetEditorDataDir( const std::string &szDir );

// The root of GetSingleton<IDataStorage>(), the game's Data directory, ending
// in a separator; what MFC opens partys.xml from. Empty until the editor sets it.
std::string GetGameDataDir();
void SetGameDataDir( const std::string &szDir );

// g_frameManager.GetActiveFrame()->GetProjectFileName(); false with no project.
bool GetActiveProjectFileName( std::string &szFileName );
void SetActiveProjectFileName( const std::string &szFileName );
void ClearActiveProject();

// g_frameManager.GetFrame( CFrameManager::E_*_FRAME ): MFC creates every
// sub-editor frame when the editor starts, so the "no frame" early returns of
// the Mission, Chapter, Campaign and Medal InitDefaultValues never fire in the
// running editor. The port keeps those guards and answers true.
bool HasEditorFrame();
// g_frameManager.GetFrame( E_*_FRAME )->GetProjectFileName(): the file of the
// project open in that sub-editor, empty before one is opened. The port opens
// one project at a time, so that is the active project.
std::string GetFrameProjectFileName();

// frames.cpp GetDirectory: the part of a path up to and including the last
// separator, or empty. MFC splits on '\'; the port also accepts '/'.
std::string GetDirectory( const std::string &szFileName );

}
