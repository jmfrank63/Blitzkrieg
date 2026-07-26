
#include "stdafx.h"

#include "PlayersInterface.h"

#include "..\Main\GameStats.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\PlayerSkill.h"
#include "etypes.h"
#include "CommonId.h"
#include "CutScenesHelper.h"
#include "time.h"
#include "InterfaceAfterMissionPopups.h"
#include "Campaign.h"
#include "MainMenu.h"
#include "MultiplayerCommandManager.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{ "inter_ok"				, IMC_CANCEL		},
	{ 0									,	0							}
};
enum 
{
	MC_PLAYER_CHAPTER_STATS			= 10005,
	MC_ECYCLOPEDIA							= 10006,

	E_SKILL_BAR_CURRENT_SMALLER	= 1001,
	E_SKILL_BAR_CURRENT_GREATER	= 1002,
	E_SKILL_BAR_FORMER_GREATER  = 1003,
	E_SKILL_BAR_FORMER_SMALLER	= 1004,
	E_TEXT_SKILL								= 1005,

	E_RANK_VALUE								= 20002,
	E_RANK_LIST									= 1001,
	
	E_MEDAL_STATIC_BEGIN				= 20010,
	E_MEDAL_STATIC_END					= 20015,

	E_DIFFICULTY								= 20010,
	E_DIFFICULTY_VAL						= 20011,

	E_DARK											= 1003,
};
int CPlayersInterface::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CInterfaceInterMission*>( this ) );
	saver.Add( 2, &pPopups );
	saver.Add( 3, &bAfterMission );
	saver.Add( 4, &bDisableGetFocus );
	return 0;
}
void CPlayersInterface::OnGetFocus( bool bFocus )
{
	CInterfaceInterMission::OnGetFocus( bFocus );
	if ( bFocus )
		commandMsgs.Init( pInput, commands );
	
	if ( bDisableGetFocus && bFocus )
	{
		bDisableGetFocus = false;
		return;
	}

	if ( pPopups )
	{
		pPopups->OnGetFocus( bFocus );
		
		if ( pPopups->IsNeedFinish() )
		{
			GetSingleton<IMainLoop>()->Command( pPopups->GetFinishCommandID(), pPopups->GetFinishCommandParams() );
			IUIElement *pEl = pUIScreen->GetChildByID( E_DARK );
			if ( pEl )
				pEl->ShowWindow( pPopups->IsNeedBlack() ? UI_SW_SHOW : UI_SW_HIDE );
		}
	}
}
bool CPlayersInterface::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	bDisableGetFocus = false;
	return true;
}
void CPlayersInterface::Create( const bool _bAfterMission )
{
	bAfterMission = _bAfterMission;
	CInterfaceInterMission::StartInterface();
	
	const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
	IMainLoop * pML = GetSingleton<IMainLoop>();
	
	if ( szChapterName == "tutorial" )
	{
		const int nTutorialMode = GetGlobalVar( "TutorialMode", 0 );
		if ( nTutorialMode == 0 )
		{
			pML->Command( MISSION_COMMAND_MAIN_MENU, "5" );
			pML->Command( MISSION_COMMAND_CUSTOM_MISSION, 0 );
		}
		else
		{
			pML->Command( MISSION_COMMAND_MAIN_MENU, "1" );
			pML->Command( MISSION_COMMAND_TUTORIAL_LIST, 0 );
		}
		return;
	}
	if ( GetGlobalVar("FinishingCampaign", 0) != 0 ) 
	{
		
	}

	if ( 0 != GetGlobalVar( "History.Playing", 0 ) )
	{
		SetGlobalVar( "CurtainsClosed", 1 );

		pML->RestoreScenarioTracker();
		RemoveGlobalVar( "History.Playing" );
		RemoveGlobalVar( "MultiplayerGame" );

		pML->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
		pML->Command( MISSION_COMMAND_MAIN_MENU, "6" );
		pML->Command( MISSION_COMMAND_REPLAY_LIST, 0 );
		return;
	}
	if ( bAfterMission )
		pPopups = new CAfterMissionPopups;


	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\PlayerStats" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	std::string szTemp;
	
	IUIDialog *pDialog = checked_cast<IUIDialog *> ( pUIScreen->GetChildByID( 100 ) );
	IScenarioTracker * pScenarioTracker = GetSingleton<IScenarioTracker>();

	IPlayerScenarioInfo *pPlayerInfo = pScenarioTracker->GetUserPlayer();
	
	for (  int nMedalSlot = 0; nMedalSlot < NUM_MEDAL_SLOTS; ++nMedalSlot )
	{
		const std::string &szMedalName = pPlayerInfo->GetMedalInSlot( nMedalSlot );
		if ( !szMedalName.empty() )
		{
			CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
			CPtr<IUIStatic> pMedalStatic = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MEDAL_STATIC_BEGIN + nMedalSlot ) );

			const SMedalStats *pStats = NGDB::GetGameStats<SMedalStats>( szMedalName.c_str(), IObjectsDB::MEDAL );

			IGFXTexture *pTexture = pTM->GetTexture( pStats->szTexture.c_str() );
			pMedalStatic->SetWindowTexture( pTexture );

			const CVec2 vMedalSize ( pStats->mapImageRect.x1,  pStats->mapImageRect.y1 );
			CVec2 vStaticSize;
			CVec2 vStaticPos;
			pMedalStatic->GetWindowPlacement( &vStaticPos, &vStaticSize, 0 );
			CVec2 vMedalPos = vStaticPos + ( vStaticSize - vMedalSize ) / 2;
			pMedalStatic->SetWindowPlacement( &vMedalPos, &vMedalSize );
			
			CTRect<float> rc( 0.0f, 0.0f, pStats->mapImageRect.x2, pStats->mapImageRect.y2 );
			pMedalStatic->SetWindowMap( rc );
			CVec2 vPos( pStats->vPicturePos.x, pStats->vPicturePos.y );
			pMedalStatic->SetWindowID( 2 * nMedalSlot + MEDAL );
			
			CPtr<IText> p1 = pTextM->GetString( pStats->szHeaderText.c_str() );
			pMedalStatic->SetHelpContext( 0, p1->GetString() );
			pMedalStatic->ShowWindow( UI_SW_SHOW );
		}
	}
	pDialog->ShowWindow( UI_SW_SHOW );
	
	const SPlayerRank &rRank = pPlayerInfo->GetRankInfo();
	IUIStatic * pRankValue = checked_cast<IUIStatic*>( pUIScreen->GetChildByID( E_RANK_VALUE ) );
	IText * pTextCurrentRank = pTextM->GetString( rRank.szCurrentRank.c_str() );
	NI_ASSERT_T( pTextCurrentRank!= 0, NStr::Format( "cannot find localized string by key \"%s\"", rRank.szCurrentRank.c_str() ) );
	if ( pTextCurrentRank )
		pRankValue->SetWindowText( 0, pTextCurrentRank->GetString() );
	IUIListControl * pRankList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_RANK_LIST ) );
	IText *pToolTip = pTextM->GetDialog( "Textes\\UI\\Intermission\\PlayerStats\\tt_next_rank" );
	pRankList->AddItem();
	IUIListRow * pRow = pRankList->GetItem( 0 );
	SetValues( pRow, rRank.fValue, rRank.fFormerValue, 100 );
	pRow->GetElement( 0 )->SetHelpContext( 0, pToolTip->GetString() );
		
	IUIStatic * pElementName = checked_cast<IUIStatic*>( pRow->GetElement( 0 ) );
	IText * pTextNextRank = pTextM->GetString( "Textes\\UI\\Intermission\\PlayersInterface\\next_rank" );
	NI_ASSERT_T( pTextNextRank!= 0, NStr::Format( "cannot find localized string by key %s", "Textes\\UI\\Intermission\\PlayersInterface\\next_rank" ) );
	if ( pTextNextRank )
		pElementName->SetWindowText( 0, pTextNextRank->GetString() );
	pRankList->InitialUpdate();

	IUIStatic *pCaption = checked_cast<IUIStatic *> ( pUIScreen->GetChildByID( 20000 ) );
	if ( pTextCurrentRank && pPlayerInfo )
	{
		std::wstring wszCaption = MakeWideStringFromWordString( pTextCurrentRank->GetString() );
		wszCaption += L" ";
		wszCaption += pPlayerInfo->GetName();
		pCaption->SetWindowText( 0, reinterpret_cast<const WORD*>( wszCaption.c_str() ) );
	}
	
	IUIListControl * pListControl = checked_cast<IUIListControl *>( pUIScreen->GetChildByID( 1000 ) );
	for ( int nStat = 0; nStat < _EPST_COUNT; ++nStat )
	{
		pListControl->AddItem();
		IUIListRow * pRow = pListControl->GetItem( nStat );
		const SPlayerSkill & rSkill = pPlayerInfo->GetSkill( nStat );
		SetValues( pRow, rSkill.fValue, rSkill.fFormerValue, 100 );
		IUIStatic * pElementName = checked_cast<IUIStatic*>( pRow->GetElement( 0 ) );
		IText * pText = pTextM->GetString( rSkill.szSkillName.c_str() );
		NI_ASSERT_T( pText!= 0, NStr::Format( "cannot find localized string by key %s", rSkill.szSkillName.c_str() ) );
		if ( pText )
			pElementName->SetWindowText( 0, pText->GetString() );
		const std::string szToolTip = rSkill.szSkillName + ".tooltip";
		pText = pTextM->GetString( szToolTip.c_str() );
		if ( pText )
			pElementName->SetHelpContext( 0, pText->GetString() );
	}
	pListControl->InitialUpdate();
	
	IUIElement *pDifficulty = pUIScreen->GetChildByID( E_DIFFICULTY );
	IText *pDifficutlyName = pTextM->GetDialog( "Textes\\Options\\GamePlay.Difficulty.name" );
	if ( pDifficulty && pDifficutlyName )
		pDifficulty->SetWindowText( 0, pDifficutlyName->GetString() );
	
	IUIElement *pDifficultyVal = pUIScreen->GetChildByID( E_DIFFICULTY_VAL );
	std::string szKey = "Textes\\Options\\GamePlay.Difficulty";
	szKey += "\\";
	szKey += GetSingleton<IScenarioTracker>()->GetMinimumDifficulty();
	szKey += ".name";
	IText * pDifficultyValName = GetSingleton<ITextManager>()->GetDialog( szKey.c_str() );
	if ( pDifficultyVal && pDifficultyValName )
		pDifficultyVal->SetWindowText( 0, pDifficultyValName->GetString() );
		
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );

	if ( GetGlobalVar("FinishingCampaign", 0) != 0 ) 
	{
		std::string szSaveName = NStr::ToAscii( GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetName() );
		szSaveName += " - ";
		
		std::string szCampaignStatsName = GetGlobalVar( "Campaign.Current.Name", "" );
		int nPos = szCampaignStatsName.rfind("\\");
		if ( nPos == std::string::npos )
			nPos = szCampaignStatsName.rfind("/");
		if ( nPos != std::string::npos )
		{
			szCampaignStatsName = szCampaignStatsName.substr( nPos == std::string::npos ? 0 : nPos + 1  );
			NStr::ToUpper( szCampaignStatsName );
			szSaveName += szCampaignStatsName;
		}
		
		szSaveName += " - End Campaign.sav";
		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_SAVE, NStr::Format( "%s;1", szSaveName.c_str() ) );
	}

	if ( bAfterMission )
	{
		const int nCampaign = GetGlobalVar( "Campaign.Current", -1 );
		if ( nCampaign >= 0 )
		{
			CInterfaceCampaign::PlayCampaignMusic();
		}
		else
		{
			CInterfaceMainMenu::PlayIntermissionSound();
		}
	}
}
void CPlayersInterface::SetValues( IUIListRow * pRow, const float fCurrentVal, const float fFormerVal, const int nMultiply )
{
	IUIDialog * pElementDialog = checked_cast<IUIDialog*>( pRow->GetElement( 1 ) );

	IUINumberIndicator *pCurrent = 0, *pFormer = 0;
	if ( fCurrentVal >= fFormerVal )
	{
		pCurrent = checked_cast<IUINumberIndicator*>( pElementDialog->GetChildByID( E_SKILL_BAR_CURRENT_GREATER ) );
		pFormer = checked_cast<IUINumberIndicator*>( pElementDialog->GetChildByID( E_SKILL_BAR_FORMER_SMALLER ) );
		pCurrent->ShowWindow( UI_SW_SHOW );
		pFormer->ShowWindow( UI_SW_SHOW );
	}
	else
	{
		pCurrent = checked_cast<IUINumberIndicator*>( pElementDialog->GetChildByID( E_SKILL_BAR_CURRENT_SMALLER ) );
		pFormer = checked_cast<IUINumberIndicator*>( pElementDialog->GetChildByID( E_SKILL_BAR_FORMER_GREATER ) );
		pFormer->ShowWindow( UI_SW_SHOW );
		pCurrent->ShowWindow( UI_SW_SHOW );
	}

	if ( pCurrent )
		pCurrent->SetValue( fCurrentVal );
	if ( pFormer )
		pFormer->SetValue( fFormerVal );

	IUIStatic *pText = checked_cast<IUIStatic*>( pElementDialog->GetChildByID( E_TEXT_SKILL ) );
	const int nValue =  fCurrentVal * nMultiply;
	const int nDiff = ( fCurrentVal - fFormerVal ) * nMultiply;

	const std::wstring wszSkill = NStr::ToUnicode( NStr::Format("%i%%(%i)", nValue, nDiff) );
	pText->SetWindowText( 0, reinterpret_cast<const WORD*>( wszSkill.c_str() ) );
	pText->ShowWindow( UI_SW_SHOW );
}		
bool CPlayersInterface::ProcessMessage( const SGameMessage &msg )
{ 
	if ( msg.nEventID == TUTORIAL_WINDOW_ID || msg.nEventID == 	TUTORIAL_BUTTON_ID )
	{
		bDisableGetFocus = true;
	}
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
	case IMC_CANCEL:

		if ( GetGlobalVar("FinishingCampaign", 0) != 0 ) 
		{
			const std::string szCampaignStatsName = GetGlobalVar( "Campaign.Current.Name", "" );
				const SCampaignStats *pStats = szCampaignStatsName.empty() ? 0 : NGDB::GetGameStats<SCampaignStats>( szCampaignStatsName.c_str(), IObjectsDB::CAMPAIGN );
				if ( pStats ) 
				{
					NCutScenes::AddCutScene( pStats->szOutroMovie );
					// flush the profile immediately (see UIState.cpp: crashes would
					// otherwise lose the unlocked cutscene)
					GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
					const std::string szConfig = NStr::Format( "%s;%d;7", pStats->szOutroMovie.c_str(), MISSION_COMMAND_MAIN_MENU );
					GetSingleton<ISFX>()->StopStream( GetGlobalVar( "Sound.TimeToFade", 5000 ) );
					FinishInterface( MISSION_COMMAND_VIDEO, szConfig.c_str() );
				}
				else
					FinishInterface( MISSION_COMMAND_MAIN_MENU, 0 );
		}
		else if ( bAfterMission )
		{
			OnGetFocus( true );
		}
		else 
		{
			CloseInterface( true );
		}
		return true;

	case MC_PLAYER_CHAPTER_STATS:
		bDisableGetFocus = true;
		FinishInterface( MISSION_COMMAND_STATS, "0"/*STATS_COMPLEXITY_TOTAL*/ );
		return true;

	case MC_MEDAL_CLICKED:
		{
			bDisableGetFocus = true;
			IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
			std::string szTemp = NStr::Format( "%d;", E_MEDAL );
			szTemp += pPlayerInfo->GetMedalInSlot( msg.nParam );
			FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
		}
		return true;
	}

	return false; 
}