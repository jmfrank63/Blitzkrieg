#include "StdAfx.h"

#include "UIMapInfo.h"
#include "MultiplayerCommandManager.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Main\TextSystem.h"

static const std::string szMultiplayer = "Multiplayer\\";
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SUIMapInfo::LoadMapInfo( const char *szMapName )
{
	std::string _szPath = szMultiplayer;
	_szPath += szMapName;
	return LoadMapByPath( _szPath.c_str() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const wchar_t * SUIMapInfo::GetVisualName( const std::string szPath )
{
	static std::wstring szMapName;

	std::string szMaps = "maps\\";
	std::string szTmp = szMaps;
	szTmp += szPath;
	std::replace( szTmp.begin(), szTmp.end(), '/', '\\' );

	IText * pMapName = GetSingleton<ITextManager>()->GetDialog( szTmp.c_str() );

	if ( pMapName )
	{
		szMapName = MakeWideStringFromWordString( pMapName->GetString() );
		return szMapName.c_str();
	}
	else
	{
		if ( szPath.size() < szMultiplayer.size() ) 
			return 0;
		szMapName = NStr::ToUnicode( szPath.c_str() + szMultiplayer.size() );
		return szMapName.c_str();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const wchar_t * SUIMapInfo::GetVisualName()
{
	return GetVisualName( szPath );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SUIMapInfo::LoadMapByPath( const char *_szPath )
{
	szPath = _szPath;
	std::string szMaps = "maps\\";

	const bool bRes = LoadLatestDataResource( szMaps + szPath, ".bzm", 
		RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, RMGC_QUICK_LOAD_MAP_INFO_NAME, 
		mapInfo
	);
	
	return bRes && mapInfo.playerParties.size() != 0 ;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
