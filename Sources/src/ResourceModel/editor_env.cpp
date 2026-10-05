#include "editor_env.h"

namespace NResourceModel
{

const std::string szTGAFilter = "TGA Files (*.tga)|*.tga||";
const std::string szTextFilter = "Text files (*.txt)|*.txt||";
const std::string szLuaFilter = "LUA files (*.lua)|*.lua||";
const std::string szMusicFilter = "Music files (*.ogg)|*.ogg||";
const std::string szMovieFilter = "Bik movie files (*.bik)|*.bik||";
const std::string szXMLFilter = "XML files (*.xml)|*.xml||";
const std::string szMapFilter = "Map files (*.xml, *.bzm)|*.xml;*.bzm||";
const std::string szSoundFilter = "Sound Files (*.wav, *.ogg)|*.wav;*.ogg||";
const std::string szMODFilter = "MOD Files (*.mod)|*.mod||";
const std::string szSANFilter = "Structure animation files (*.san)|*.san||";
const std::string szDDSFilter = "DDS compressed textures (*.dds)|*.dds||";

namespace
{

struct SEnv
{
	std::string szEditorDataDir;
	std::string szProjectFileName;
	bool bHasProject = false;
};

SEnv &Env()
{
	static SEnv env;
	return env;
}

}

std::string GetEditorDataDir() { return Env().szEditorDataDir; }
void SetEditorDataDir( const std::string &szDir ) { Env().szEditorDataDir = szDir; }

bool GetActiveProjectFileName( std::string &szFileName )
{
	if ( !Env().bHasProject )
		return false;
	szFileName = Env().szProjectFileName;
	return true;
}

void SetActiveProjectFileName( const std::string &szFileName )
{
	Env().szProjectFileName = szFileName;
	Env().bHasProject = true;
}

void ClearActiveProject()
{
	Env().szProjectFileName.clear();
	Env().bHasProject = false;
}

std::string GetDirectory( const std::string &szFileName )
{
	const size_t nPos = szFileName.find_last_of( "\\/" );
	return nPos == std::string::npos ? std::string() : szFileName.substr( 0, nPos + 1 );
}

}
