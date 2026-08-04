#include "StdAfx.h"

#include "encyclopedia.h"

#include "../Main/GameStats.h"
#include "../Main/RPGStats.h"
#include "MultiplayerCommandManager.h"
#include "etypes.h"
#include "CommonId.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_CANCEL		},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
void CICEncyclopedia::Configure( const char *pszConfig )
{
	if ( pszConfig == 0 )
		return;

	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ';' );
	NI_ASSERT_T( szStrings.size() == 2, "Invalid number of parameters for encyclopedia interface" );
	nType = NStr::ToInt( szStrings[0] );
	szName = szStrings[1];
}
void CICEncyclopedia::PostCreate( IMainLoop *pML, CInterfaceEncyclopedia *pEI )
{
	pEI->Create( nType, szName.c_str() );
	pML->PushInterface( pEI );
}
CInterfaceEncyclopedia::~CInterfaceEncyclopedia()
{
}
void CInterfaceEncyclopedia::LoadMedalInfo( const SMedalStats *pMedalStats, std::string *pszTextureFileName, std::wstring *pszTitle, std::wstring *pDesc )
{
	ITextManager *pTextM = GetSingleton<ITextManager>();
	*pszTextureFileName = pMedalStats->szTexture;
	CPtr<IText> p1 = pTextM->GetDialog( pMedalStats->szHeaderText.c_str() );
	if ( p1 )
		pszTitle->assign( MakeWideStringFromWordString( p1->GetString() ) );
	p1 = pTextM->GetDialog( pMedalStats->szDescriptionText.c_str() );
	if ( p1 )
		pDesc->assign( MakeWideStringFromWordString( p1->GetString() ) );
}
void CInterfaceEncyclopedia::LoadUnitInfo( const SUnitBaseRPGStats *pUnitStats, std::string *pszTextureFileName, std::wstring *pszTitle, std::wstring *pDesc, std::wstring *pStatistics )
{
	IObjectsDB *pDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pObjectDesc = pDB->GetDesc( pUnitStats->szParentName.c_str() );
	
	CPtr<IText> p1 = GetSingleton<ITextManager>()->GetDialog( (pObjectDesc->szPath + "\\name").c_str() );
	if ( p1 )
		pszTitle->assign( MakeWideStringFromWordString( p1->GetString() ) );
	
	p1 = GetSingleton<ITextManager>()->GetDialog( (pObjectDesc->szPath + "\\desc").c_str() );
	if ( p1 )
		pDesc->assign( MakeWideStringFromWordString( p1->GetString() ) );

	p1 = GetSingleton<ITextManager>()->GetDialog( (pObjectDesc->szPath + "\\stats").c_str() );
	if ( p1 )
		pStatistics->assign( MakeWideStringFromWordString( p1->GetString() ) );

	*pszTextureFileName = pObjectDesc->szPath + "\\icon512";
}
bool CInterfaceEncyclopedia::Init()
{
	CInterfaceInterMission::Init();

	return true;
}
void CInterfaceEncyclopedia::Done()
{
	CInterfaceInterMission::Done();
	GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	SetGlobalVar( "EncyclopediaShown", 1 );
}
void CInterfaceEncyclopedia::Create( int nType, const char *pszName )
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\encyclopedia" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	
	std::string szTextureFileName;
	std::wstring szTitle, szDesc, szStatistics;
	
	IUIElement *pPicture = checked_cast<IUIElement *> ( pUIScreen->GetChildByID( 100 ) );
	NI_ASSERT_T( pPicture != 0, "Invalid encyclopedia picture control" );
	SetTutorialNumber( nType );
	
	switch ( nType )
	{
		case E_MEDAL:
		{
			const SMedalStats *pMedalStats = NGDB::GetGameStats<SMedalStats>( pszName, IObjectsDB::MEDAL );
			NI_ASSERT_TF( pMedalStats != 0, "Invalid medal stats in encyclopedia", return );
			LoadMedalInfo( pMedalStats, &szTextureFileName, &szTitle, &szDesc );

			CVec2 size, pos;
			pPicture->GetWindowPlacement( &pos, 0, 0 );
			size.x = pMedalStats->mapImageRect.x1;
			size.y = pMedalStats->mapImageRect.y1;

			pos.x = 277 - size.x / 2;
			pos.y = 380 - size.y / 2;
			pPicture->SetWindowPlacement( &pos, &size );
			
			CTRect<float> rc( 0.0f, 0.0f, pMedalStats->mapImageRect.x2, pMedalStats->mapImageRect.y2 );
			pPicture->SetWindowMap( rc );
			break;
		}
		case E_UNIT:
		{
			const SUnitBaseRPGStats *pUnitStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( pszName );
			NI_ASSERT_TF( pUnitStats != 0, "Invalid unit RPG stats in encyclopedia", return );
			LoadUnitInfo( pUnitStats, &szTextureFileName, &szTitle, &szDesc, &szStatistics );

			CVec2 size( 512, 512 );
			pPicture->SetWindowPlacement( 0, &size );
			break;
		}
		default:
			NI_ASSERT_T( 0, "Unknown encyclopedia type" );
	}


	IGFXTexture *pTexture = pTM->GetTexture( szTextureFileName.c_str() );
	pPicture->SetWindowTexture( pTexture );
	
	IUIElement *pHeader = pUIScreen->GetChildByID( 20000 );
	NI_ASSERT_T( pHeader != 0, "Invalid encyclopedia header control" );
	pHeader->SetWindowText( 0, reinterpret_cast<const WORD*>( szTitle.c_str() ) );

	IUIElement *pDesc = checked_cast<IUIElement *> ( pUIScreen->GetChildByID( 2000 ) );
	NI_ASSERT_T( pDesc != 0, "Invalid encyclopedia text description control" );
	pDesc->SetWindowText( 0, reinterpret_cast<const WORD*>( szDesc.c_str() ) );

/*
	IUIElement *pStatistics = checked_cast<IUIElement *> ( pUIScreen->GetChildByID( 3000 ) );
	NI_ASSERT_T( pStatistics != 0, "Invalid encyclopedia text statistics control" );
	pStatistics->SetWindowText( 0, szStatistics.c_str() );
*/

	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceEncyclopedia::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface( true );
			return true;
	}
	
	return false;
}
