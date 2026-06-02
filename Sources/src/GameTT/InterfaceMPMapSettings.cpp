#include "StdAfx.h"

#include "InterfaceMPMapSettings.h"
#include "InterfaceStartDialog.h"
#include "CommonId.h"
#include "..\Main\ScenarioTracker.h"
#include "..\UI\UIMessages.h"
#include "OptionEntryWrapper.h"
extern SMultiplayerGameSettings configuration;	
extern bool bServerconfiguration ;
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum 
{
	E_BUTTON_OK														= 10002,
	E_BUTTON_CANCEL												= 10001,

	E_LIST																= 1001,
};
bool CInterfaceMPMapSettings::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
bool CInterfaceMPMapSettings::ProcessMessage( const SGameMessage &msg )
{
	if ( pOptions && pOptions->ProcessMessage( msg ) ) return true;
	if ( pMapSettingsWrapper && pMapSettingsWrapper->ProcessMessage( msg ) ) return true;

	switch( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );

		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );

		break;
	case 7778:
	case IMC_CANCEL:
		{
			CloseInterface();
		}

		return true;
	case 7777:
	case IMC_OK:
		if ( !bFinished )
		{
			bFinished = true;
			SetGlobalVar( "MultiplayerOptions.Changed", 1 );

			if ( pMapSettingsWrapper ) 
				pMapSettingsWrapper->Apply();
			
			if ( pOptions ) 
				pOptions->Apply();

			GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
			
			CloseInterface();
		}
		break;
		
	default:
		return false;
	}
	return true;
}
bool CInterfaceMPMapSettings::Init()
{
	CInterfaceScreenBase::Init();
	msgs.Init( pInput, commands );

	return true;
}
void CInterfaceMPMapSettings::Create( const bool _bDisableChages, const bool _bStagingRoom )
{
	if ( pOptions && _bDisableChages ) 
		pOptions->DisableChange();
	bDisableChanges = _bDisableChages;

	bFinished = false;
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	if ( _bDisableChages )
		pUIScreen->Load( "ui\\MPMapSettings_NoChanges" );
	else
		pUIScreen->Load( "ui\\MPMapSettings" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	pButtonOK = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_OK ) );
	pButtonCancel = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_CANCEL ) );

	const int nFlag = _bStagingRoom ? OPTION_FLAG_MULTIPLAYER_START : OPTION_FLAG_BEFORE_MULTIPLAYER_START;

	if ( bServerconfiguration )
	{
		pMapSettingsWrapper = new CMapSettingsWrapper( false, nFlag );
		pMapSettingsWrapper->Init( configuration );
		pMapSettingsWrapper->Init( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) ), 0 );
	}
	else
	{
		OptionDescs desc;

		IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();
		
		
		for ( CPtr<IOptionSystemIterator> pIter = pOptionSystem->CreateIterator( nFlag ); !pIter->IsEnd(); pIter->Next() )
		{
			desc.push_back( *pIter->GetDesc() );
		}

		pOptions = new COptionsListWrapper( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) ),
			desc, 100 );
	}
	pScene->AddUIScreen( pUIScreen );
}