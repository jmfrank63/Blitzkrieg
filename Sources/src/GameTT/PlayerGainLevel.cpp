#include "StdAfx.h"

#include "PlayerGainLevel.h"

#include "..\Main\ScenarioTracker.h"
#include "..\Main\PlayerSkill.h"
#include "CommonId.h"
#include "..\Main\GameStats.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_CANCEL		},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
void CICPlayerGainLevel::PostCreate( IMainLoop *pML, CInterfacePlayerGainLevel *pISM )
{
	pML->PushInterface( pISM );
}
CInterfacePlayerGainLevel::~CInterfacePlayerGainLevel()
{
}
bool CInterfacePlayerGainLevel::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfacePlayerGainLevel::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\Popup\\PlayerRank" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	IUIContainer *pDialog = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 100 ) );
	pDialog = checked_cast<IUIContainer *> ( pDialog->GetChildByID( 101 ) );
	IUIElement *pRankText = pDialog->GetChildByID( 20001 );
	NI_ASSERT_T( pRankText != 0, "Invalid PlayerGainLevel interface static rank text control" );
	
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	const SPlayerRank &rank = pPlayer->GetRankInfo();
	CPtr<IText> p1 = GetSingleton<ITextManager>()->GetDialog( rank.szFullTextKey.c_str() );
	NI_ASSERT_T( p1 != 0, (std::string("Can not get dialog with player rank full text: ") + rank.szFullTextKey).c_str() );
	if ( p1 != 0 )
		pRankText->SetWindowText( 0, p1->GetString() );
	
	IUIElement *pCaption = pUIScreen->GetChildByID( 20000 );
	IText * pTextCaption = GetSingleton<ITextManager>()->GetString( rank.szCurrentRank.c_str() );
	pCaption->SetWindowText( 0, pTextCaption->GetString() );
	
	const std::string	szMedalName = rank.szRankPicture;
	
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	
	std::string szTextureFileName;
	std::wstring szTitle, szDesc;
	
	IUIElement *pPicture = pUIScreen->GetChildByID( 20002 );
	NI_ASSERT_T( pPicture != 0, "Invalid SingleMedal picture control" );
	
	const SMedalStats *pMedalStats = NGDB::GetGameStats<SMedalStats>( szMedalName.c_str(), IObjectsDB::MEDAL );
	NI_ASSERT_TF( pMedalStats != 0, "Invalid medal stats in SingleMedal interface", return );
	
	const CVec2 vMedalSize ( pMedalStats->mapImageRect.x1,  pMedalStats->mapImageRect.y1 );
	CVec2 vStaticSize;
	CVec2 vStaticPos;
	pPicture->GetWindowPlacement( &vStaticPos, &vStaticSize, 0 );
	CVec2 vMedalPos = vStaticPos + ( vStaticSize - vMedalSize ) / 2;
	pPicture->SetWindowPlacement( &vMedalPos, &vMedalSize );

	CTRect<float> rc( 0.0f, 0.0f, pMedalStats->mapImageRect.x2, pMedalStats->mapImageRect.y2 );
	pPicture->SetWindowMap( rc );
	
	IGFXTexture *pTexture = pTM->GetTexture( pMedalStats->szTexture.c_str() );
	pPicture->SetWindowTexture( pTexture );

	pPlayer->ClearLevelGain();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfacePlayerGainLevel::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface();
			return true;
	}
	
	return false;
}
