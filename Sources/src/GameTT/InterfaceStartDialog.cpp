#include "StdAfx.h"

#include "InterfaceStartDialog.h"
#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "..\Main\ScenarioTracker.h"
#include "..\UI\UIMessages.h"
#include "OptionEntryWrapper.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum 
{
	E_EDITBOX															= 2000,

	E_BUTTON_OK														= 10002,
	E_BUTTON_CANCEL												= 10001,

	E_LIST																= 1001,
};
bool CInterfacePlayerProfile::ProcessMessage( const SGameMessage &msg )
{
	if ( pOptions && pOptions->ProcessMessage( msg ) ) return true;

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
	case 7779:
	case UI_NOTIFY_EDIT_BOX_TEXT_CHANGED:
		{
			const std::wstring szName = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );
			pButtonOK->EnableWindow( !szName.empty() );
		}

		return true;
	case 7777:
	case IMC_OK:
		if ( !bFinished )
		{
			if ( pButtonOK->IsWindowEnabled() )
			{
				bFinished = true;
				const std::wstring szWindowText = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );
				IOptionSystem * pOptionsSystem = GetSingleton<IOptionSystem>();
				if ( !szWindowText.empty() )
				{
					pOptionsSystem->Set( "GamePlay.PlayerName", variant_t( szWindowText.c_str() ) );

					/*{
						IOptionSystem * pOptions =  GetSingleton<IOptionSystem>();
						const SOptionDesc * pDesc = pOptions->GetDesc( "Multiplayer.PlayerName" );
						variant_t varPlayerName;
						pOptions->Get( "Multiplayer.PlayerName", &varPlayerName );
						const std::wstring szNameFromOptions = (wchar_t*)(bstr_t)varPlayerName;
						const std::wstring szDefault = (wchar_t*)(bstr_t)pDesc->defaultValue;
						if ( szNameFromOptions == szDefault )
							pOptionsSystem->Set( "Multiplayer.PlayerName", variant_t( szWindowText.c_str() ) );
					}*/
					
					GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
					pOptions->Apply();
				}
				pButtonOK->EnableWindow( false ); // to disable second return
				CloseInterface();
			}
		}
		break;
		
	default:
		return false;
	}
	return true;
}
bool CInterfacePlayerProfile::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
void CInterfacePlayerProfile::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );
	if ( bFocus ) 
	{
		pEdit->SetFocus( true );
		pEdit->SetSel( 0, -1 );
		pEdit->SetCursor( MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) ).length() );
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
	}
}
bool CInterfacePlayerProfile::Init()
{
	CInterfaceScreenBase::Init();
	msgs.Init( pInput, commands );

	return true;
}
void CInterfacePlayerProfile::StartInterface()
{
	bFinished = false;
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\PlayerProfile" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( E_EDITBOX ) );

	IScenarioTracker * pTracker = GetSingleton<IScenarioTracker>();
	bEnableCancel = true;
	bool bEnableOk = true;

	IOptionSystem * pOptionsSystem = GetSingleton<IOptionSystem>();
	variant_t var;
	pOptionsSystem->Get( "GamePlay.PlayerName", &var );
	const wchar_t *pszStoredName = static_cast<const wchar_t*>( bstr_t(var) );
	std::wstring szName = pszStoredName ? pszStoredName : L"";
	if ( szName.empty() )
		szName = L"Player";

	pEdit->SetWindowText( 0, reinterpret_cast<const WORD*>( szName.c_str() ) );
	
	pButtonOK = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_OK ) );
	pButtonCancel = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_CANCEL ) );
	
	pButtonOK->EnableWindow( bEnableOk );
	pButtonCancel->EnableWindow( true );
	
	OptionDescs desc;
	
	IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();
	
	const SOptionDesc * pDesc = pOptionSystem->GetDesc( "GamePlay.Difficulty" );
	if ( pDesc )
		desc.push_back( *pDesc );
	
	pDesc = pOptionSystem->GetDesc( "GFX.Blood" );
	if ( pDesc )
		desc.push_back( *pDesc );
	
	pOptions = new COptionsListWrapper( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) ),
		desc, 100 );
	
	pScene->AddUIScreen( pUIScreen );

	IInput * pInput = GetSingleton<IInput>();
	pInput->PumpMessages( true );
	SGameMessage msg;
	while( pInput->GetMessage( &msg ) )
	{
	}
}
