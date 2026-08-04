#ifndef __UICONSTS_H__
#define __UICONSTS_H__
#pragma ONCE
#include "../Main/GameStats.h"
#include "MultiplayerCommandManager.h"
class CUIConsts  
{
public:
	static std::string GetPartyNameByNumber( const int nCampaign );
	
	static const WORD * GetLocalPartyName( const char * pszPartyKey );
	static const WORD * GetGamesListTitle( const enum EMultiplayerConnectionType eType );

	static std::string ConstructOptionKey( const char * pszOptionName, const char * pszSelectionName );
	static std::string CreateTexturePathFromMapPath( const char * pszMapPath ) ;
	static void CreateDescription( const struct SChapterStats::SMission *pStats, std::wstring *pDescription, const bool bNeedBonuses );

	static const WORD *GetMapTypeString( const int /*SQuickLoadMapInfo::EMultiplayerMapType*/ nGameType );
	static std::string GetCampaignNameAddition();

};
#endif // __UICONSTS_H__
