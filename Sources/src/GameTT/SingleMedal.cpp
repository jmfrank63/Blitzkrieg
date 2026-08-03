#include "StdAfx.h"

#include "SingleMedal.h"

#include "../Main/GameStats.h"
#include "CommonId.h"
#include "eTypes.h"
enum ECommands
{
	IMC_SHOW_ENCYCLOPEDIA		= 10003,
};
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_CANCEL		},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
void CICSingleMedal::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	szName = pszConfig;
}
void CICSingleMedal::PostCreate( IMainLoop *pML, CInterfaceSingleMedal *pISM )
{
	pISM->Create( szName.c_str() );
	pML->PushInterface( pISM );
}
CInterfaceSingleMedal::~CInterfaceSingleMedal()
{
}
bool CInterfaceSingleMedal::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceSingleMedal::Create( const char *pszName )
{
	szMedalName = pszName;
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\Popup\\PlayerRank" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	
	IUIContainer *pDialog = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 100 ) );
	pDialog = checked_cast<IUIContainer *> ( pDialog->GetChildByID( 101 ) );
	IUIElement *pPicture = pUIScreen->GetChildByID( 20002 );
	NI_ASSERT_T( pPicture != 0, "Invalid SingleMedal picture control" );
	
	const SMedalStats *pMedalStats = NGDB::GetGameStats<SMedalStats>( szMedalName.c_str(), IObjectsDB::MEDAL );
	NI_ASSERT_TF( pMedalStats != 0, "Invalid medal stats in SingleMedal interface", return );

	
	IUIElement *pText = pDialog->GetChildByID( 20001 );
	NI_ASSERT_T( pText != 0, "Invalid PlayerGainLevel interface static rank text control" );
	
	CPtr<IText> p1 = GetSingleton<ITextManager>()->GetDialog( pMedalStats->szDescriptionText.c_str() );
	if ( p1 != 0 )
		pText->SetWindowText( 0, p1->GetString() );
	
	IUIElement *pCaption = pUIScreen->GetChildByID( 20000 );
	IText * pTextCaption = GetSingleton<ITextManager>()->GetString( pMedalStats->szHeaderText.c_str() );
	pCaption->SetWindowText( 0, pTextCaption->GetString() );

	
	const CVec2 vMedalSize ( pMedalStats->mapImageRect.x1,  pMedalStats->mapImageRect.y1 );
	CVec2 vStaticSize;
	CVec2 vStaticPos;
	pPicture->GetWindowPlacement( &vStaticPos, &vStaticSize, 0 );
	CVec2 vMedalPos = vStaticPos + ( vStaticSize - vMedalSize ) / 2;
	pPicture->SetWindowPlacement( &vMedalPos, &vMedalSize );
	/*		
	CVec2 size;
	size.x = pMedalStats->mapImageRect.x1;
	size.y = pMedalStats->mapImageRect.y1;
	pPicture->SetWindowPlacement( 0, &size );
	*/
	
	CTRect<float> rc( 0.0f, 0.0f, pMedalStats->mapImageRect.x2, pMedalStats->mapImageRect.y2 );
	pPicture->SetWindowMap( rc );
	
	IGFXTexture *pTexture = pTM->GetTexture( pMedalStats->szTexture.c_str() );
	pPicture->SetWindowTexture( pTexture );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceSingleMedal::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface();
			return true;
			
		case IMC_SHOW_ENCYCLOPEDIA:
			const SMedalStats *pMedalStats = NGDB::GetGameStats<SMedalStats>( szMedalName.c_str(), IObjectsDB::MEDAL );
			NI_ASSERT_TF( pMedalStats != 0, "Invalid medal stats in SingleMedal interface", return true );
			std::string szTemp = NStr::Format( "%d;", E_MEDAL );
			szTemp += pMedalStats->szParentName.c_str();
			FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
			return true;
	}
	
	return false;
}
