#include "stdafx.h"
#include "UIConsts.h"
#include "..\Main\TextSystem.h"
#include "MultiplayerCommandManager.h"
std::string CUIConsts::GetPartyNameByNumber( const int nCampaign )
{
	switch ( nCampaign )
	{
		case 0:
			return "german";
		case 1:
			return "ussr";
		case 2:
			return "allies";
		default:
			NI_ASSERT_T( 0, "Invalid campaign" );
			return "";
	}
}	
const WORD * CUIConsts::GetLocalPartyName( const char * pszPartyKey )
{
	ITextManager *pTextM = GetSingleton<ITextManager>();
	std::string szPath = "Textes\\Opponents\\";
	szPath += pszPartyKey;

	IText * pText = pTextM->GetDialog( szPath.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format( "unknown party %s", szPath.c_str()) );
	return pText->GetString();
}
const WORD * CUIConsts::GetGamesListTitle( const enum EMultiplayerConnectionType eType )
{
	ITextManager * pTM = GetSingleton<ITextManager>();
	IText * pText = 0;

	switch( eType )
	{
	case EMCT_LAN:
		pText = pTM->GetDialog( "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\caption_lan" );
		break;
	case EMCT_INTERNET:
		pText = pTM->GetDialog( "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\caption_internet" );
		break;
	case EMCT_GAMESPY:
		pText = pTM->GetDialog( "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\caption_gamespy" );
		break;
	}
	
	if ( pText ) 
		return pText->GetString();

	return 0;
}
std::string CUIConsts::ConstructOptionKey( const char *pszOptionName, const char *pszSelectionName )
{
	std::string szPath = "Textes\\Options\\";
	szPath += pszOptionName;
	szPath += "\\";
	szPath += pszSelectionName;
	return szPath;
}
std::string CUIConsts::CreateTexturePathFromMapPath( const char * pszMapPath )
{
	std::string szTexture = "Maps\\";
	szTexture += pszMapPath;
	return szTexture;
}
void CUIConsts::CreateDescription( const struct SChapterStats::SMission *pStats, std::wstring *pDescription, const bool bNeedBonuses )
{
	pDescription->clear();
	
	NI_ASSERT_T( pStats != 0, "ERROR in parameters. CUIConsts::CreateDescription(), pStats == 0. Plz tell about this bug to programmers" );
	if ( !pStats )
		return;
	const SMissionStats *pMissionStats = NGDB::GetGameStats<SMissionStats>( pStats->szMission.c_str(), IObjectsDB::MISSION );
	NI_ASSERT_T( pMissionStats != 0, NStr::Format( "Can not get mission stats for mission %s", pStats->szMission.c_str() ) );
	if ( !pMissionStats )
		return;
	ITextManager *pTM = GetSingleton<ITextManager>();
	
	IText *pText = pTM->GetString( "missiontext" );
	*pDescription += MakeWideStringFromWordString( pText->GetString() );
	*pDescription += L" ";
	pText = pTM->GetString( pStats->pMission->szHeaderText.c_str() );
	*pDescription += MakeWideStringFromWordString( pText->GetString() );
	*pDescription += L"\n";

	pText = pTM->GetString( "missiontypetext" );
	*pDescription += MakeWideStringFromWordString( pText->GetString() );
	*pDescription += L" ";
		
	if ( !pMissionStats->IsTemplate() )
	{
		pText = pTM->GetString( "scenariomission" );
		*pDescription += MakeWideStringFromWordString( pText->GetString() );
		*pDescription += L"\n";
	}
	else
	{
		pText = pTM->GetString( "templatemission" );
		*pDescription += MakeWideStringFromWordString( pText->GetString() );
		*pDescription += L"\n";

		pText = pTM->GetString( "missiondifficulty" );
		*pDescription += MakeWideStringFromWordString( pText->GetString() );
		*pDescription += L" ";
		std::string szKey = NStr::Format( "textes\\ui\\difficulty\\mission\\level%d", pStats->nMissionDifficulty );
		pText = pTM->GetDialog( szKey.c_str() );
		if ( pText )
			*pDescription += MakeWideStringFromWordString( pText->GetString() );
		*pDescription += L"\n";

		pText = pTM->GetString( "missionbonus" );
		*pDescription += MakeWideStringFromWordString( pText->GetString() );
		*pDescription += L" ";
		
		if ( !pStats->szMissionBonus.empty() )
		{
			const SGDBObjectDesc *pObjectDesc = GetSingleton<IObjectsDB>()->GetDesc( pStats->szMissionBonus.c_str() );
			NI_ASSERT_T( pObjectDesc != 0, (std::string( "Error: can not get DB stats by key ") + pStats->szMissionBonus).c_str() );
			if ( pObjectDesc != 0 )
			{
				pText = GetSingleton<ITextManager>()->GetDialog( (pObjectDesc->szPath + "\\name").c_str() );
				NI_ASSERT_T( pText != 0, (std::string( "Error: can not get text by key ") + pObjectDesc->szPath + "\\name").c_str() );
				if ( pText )
					*pDescription += MakeWideStringFromWordString( pText->GetString() );
			}
		}
		*pDescription += L"\n";
	}

	*pDescription += L"\n";
	pText = pTM->GetString( "missiondescriptiontext" );
	*pDescription += MakeWideStringFromWordString( pText->GetString() );
	*pDescription += L"\n";
	pText = pTM->GetString( pMissionStats->szDescriptionText.c_str() );
	*pDescription += MakeWideStringFromWordString( pText->GetString() );

	if ( pMissionStats->IsTemplate() && bNeedBonuses )
	{
		*pDescription += L"\n";
		*pDescription += L"\n";
		pText = pTM->GetString( "allbonuses" );
		if ( pText )
			*pDescription += MakeWideStringFromWordString( pText->GetString() );
		/*for ( int i=0; i<pStats->szAllBonuses.size(); i++ )
		{
			if ( !pStats->szAllBonuses.empty() )
			{
				const SGDBObjectDesc *pObjectDesc = GetSingleton<IObjectsDB>()->GetDesc( pStats->szAllBonuses[i].c_str() );
				NI_ASSERT_T( pObjectDesc != 0, (std::string( "Error: can not get DB stats by key ") + pStats->szAllBonuses[i]).c_str() );
				if ( pObjectDesc != 0 )
				{
					pText = GetSingleton<ITextManager>()->GetDialog( (pObjectDesc->szPath + "\\name").c_str() );
					NI_ASSERT_T( pText != 0, (std::string( "Error: can not get text by key ") + pObjectDesc->szPath + "\\name").c_str() );
					if ( pText )
					{
						*pDescription += pText->GetString();
						*pDescription += L"\n";
					}
				}
			}
		}*/
	}
}
const WORD * CUIConsts::GetMapTypeString( const int nGameType )
{
	ITextManager * pT = GetSingleton<ITextManager>();
	IText * pText = 0;
	switch( nGameType )
	{
	case CMapInfo::TYPE_FLAGCONTROL:
		pText = pT->GetString( "Textes\\GameTypes\\game_type_flagcontrol" );
		break;
	case CMapInfo::TYPE_SABOTAGE:
		pText = pT->GetString( "Textes\\GameTypes\\game_type_sabotage" );
		break;
	default:
		pText = pT->GetString( "Textes\\GameTypes\\game_type_unknown" );
		break;
	}
	if ( pText )
		return pText->GetString();
	return 0;
}
std::string CUIConsts::GetCampaignNameAddition()
{
	std::string szCampaignName = GetGlobalVar( "Campaign.Current.Name", "" );
	std::string szSaveName; 

	std::string szFirstLetter;
	std::string szWord;
	
	int nPos = szCampaignName.rfind("\\");
	if ( nPos == std::string::npos )
		nPos = szCampaignName.rfind("/");
	if ( nPos != std::string::npos )
	{
		szCampaignName = szCampaignName.substr( nPos == std::string::npos ? 0 : nPos + 1  );
		NStr::ToUpper( szCampaignName );
	}
	if ( !szCampaignName.empty() )
		szSaveName = szCampaignName;

	return szSaveName;
}