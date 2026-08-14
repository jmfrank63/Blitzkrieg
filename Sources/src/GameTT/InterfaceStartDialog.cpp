#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceStartDialog.h"
#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "../Main/ScenarioTracker.h"
#include "../UI/UIMessages.h"
#include "OptionEntryWrapper.h"
#include "../StreamIO/ProfilePaths.h"
#include <fstream>
#include <filesystem>
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

	E_PROFILE_LIST												= 2100,
	E_BUTTON_NEW													= 10010,
	E_BUTTON_RENAME												= 10011,
	E_BUTTON_DELETE												= 10012,
};
// Profile names are printable ASCII (NProfile::Sanitize guarantees it), so
// per-character widening is exact.
static std::wstring WideFromProfileName( const std::string &szName )
{
	std::wstring szWide;
	for ( std::string::size_type i = 0; i < szName.size(); ++i )
		szWide += wchar_t( (unsigned char)szName[i] );
	return szWide;
}
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
				if ( !szWindowText.empty() )
				{
					// The player name IS the profile: a new name switches to
					// (or creates) profiles\<name>\ with its own saves,
					// screenshots and settings.
					SwitchToProfile( NProfile::Sanitize( std::string( (const char*)bstr_t( szWindowText.c_str() ) ) ), szWindowText );
				}
				pButtonOK->EnableWindow( false ); // to disable second return
				CloseInterface();
			}
		}
		break;

	case E_PROFILE_LIST:
		{
			const int nRow = pProfileList ? pProfileList->GetSelectionItem() : -1;
			if ( nRow >= 0 )
			{
				const int nName = pProfileList->GetItem( nRow )->GetUserData();
				if ( nName >= 0 && nName < profileNames.size() )
				{
					SetEditBoxText( WideFromProfileName( profileNames[nName] ) );
					pButtonOK->EnableWindow( true );
				}
			}
		}

		return true;
	case E_BUTTON_NEW:
		{
			// The typed name IS the new profile: create it and switch to it
			// right away, keeping the screen open with the list refreshed.
			// A name that already exists (the active profile included)
			// changes nothing - no error box, the field keeps its text.
			const std::wstring szTyped = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );
			if ( szTyped.empty() )
				return true;
			const std::string szTarget = NProfile::Sanitize( std::string( (const char*)bstr_t( szTyped.c_str() ) ) );
			if ( szTarget.empty() )
				return true;
			for ( int i = 0; i < profileNames.size(); ++i )
				if ( NProfile::NameEquals( profileNames[i], szTarget ) )
					return true;
			SwitchToProfile( szTarget, szTyped );
			// SwitchToProfile flushed the config, which leaves the "default"
			// input bind section behind (see the rename path); this screen
			// stays open, so its section goes back or input dies.
			pInput->SetBindSection( "intermission" );
			FillProfileList( szTarget );
			SetEditBoxText( WideFromProfileName( szTarget ) );
			pButtonOK->EnableWindow( true );
		}

		return true;
	case E_BUTTON_RENAME:
		{
			const int nRow = pProfileList ? pProfileList->GetSelectionItem() : -1;
			if ( nRow < 0 )
				return true;
			const int nName = pProfileList->GetItem( nRow )->GetUserData();
			if ( nName < 0 || nName >= profileNames.size() )
				return true;
			const std::string szSource = profileNames[nName];
			const std::wstring szTyped = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );
			if ( szTyped.empty() )
				return true;
			const std::string szTarget = NProfile::Sanitize( std::string( (const char*)bstr_t( szTyped.c_str() ) ) );
			// Renaming to the unchanged name touches nothing - not even a
			// config flush, whose side effects (the input binder resets the
			// bind section to "default" even on a write) would kill the
			// screen's input. A case-only change is a real rename and passes.
			if ( szTarget == szSource )
				return true;
			// The message key must be picked before the attempt:
			// NProfile::Rename reports a collision and a filesystem failure
			// through the same string.
			bool bCollision = false;
			for ( int i = 0; i < profileNames.size(); ++i )
				if ( NProfile::NameEquals( profileNames[i], szTarget ) && !NProfile::NameEquals( profileNames[i], szSource ) )
					bCollision = true;
			const std::string szActive = GetGlobalVar( "Profile.Name", "" );
			const bool bActive = NProfile::NameEquals( szSource, szActive );
			if ( bActive )
			{
				// The directory about to move must hold the latest settings.
				// The flush leaves the "default" input bind section behind
				// (CInputBinder::SerializeConfig resets it even on a write);
				// this screen lives in the main menu flow, so its section is
				// put back or every real click and key after this goes dead.
				GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
				pInput->SetBindSection( "intermission" );
			}
			std::string szError;
			if ( !NProfile::Rename( szSource, szTarget, &szError ) )
			{
				ReportProfileError( bCollision ? "Textes\\UI\\Intermission\\MainMenu\\PlayerProfile\\err_exists"
																			 : "Textes\\UI\\Intermission\\MainMenu\\PlayerProfile\\err_fs", szError );
				return true;
			}
			if ( bActive )
			{
				SetGlobalVar( "Profile.Name", szTarget.c_str() );
				std::ofstream active( "profiles/active.cfg", std::ios::trunc );
				if ( active )
					active << szTarget;
				GetSingleton<IOptionSystem>()->Set( "GamePlay.PlayerName", variant_t( szTyped.c_str() ) );
			}
			FillProfileList( szTarget );
			SetEditBoxText( WideFromProfileName( szTarget ) );
			pButtonOK->EnableWindow( true );
		}

		return true;
	case E_BUTTON_DELETE:
		{
			const int nRow = pProfileList ? pProfileList->GetSelectionItem() : -1;
			if ( nRow < 0 )
				return true;
			const int nName = pProfileList->GetItem( nRow )->GetUserData();
			if ( nName < 0 || nName >= profileNames.size() )
				return true;
			// The answer comes back through the global var when this screen
			// regains focus (see OnGetFocus) - the standard message box flow.
			szPendingDelete = profileNames[nName];
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX,
				NStr::Format( "%s;%s;1;PlayerProfile.ConfirmDelete",
					"Textes\\UI\\Intermission\\MainMenu\\PlayerProfile\\delete",
					"Textes\\UI\\Intermission\\MainMenu\\PlayerProfile\\confirm_delete" ) );
		}

		return true;
	default:
		return false;
	}
	return true;
}
void CInterfacePlayerProfile::SwitchToProfile( const std::string &szNewProfile, const std::wstring &szTypedName )
{
	IOptionSystem * pOptionsSystem = GetSingleton<IOptionSystem>();
	const std::string szOldProfile = GetGlobalVar( "Profile.Name", "" );
	bool bLoadedExistingProfile = false;
	if ( !szOldProfile.empty() && szNewProfile != szOldProfile )
	{
		IMainLoop *pML = GetSingleton<IMainLoop>();
		// Leave the old profile with its settings flushed and
		// its own player name intact.
		pML->SerializeConfig( false, 0xffffffff );
		SetGlobalVar( "Profile.Name", szNewProfile.c_str() );
		std::error_code pathError;
		std::filesystem::create_directories( "profiles/" + szNewProfile + "/saves", pathError );
		std::filesystem::create_directories( "profiles/" + szNewProfile + "/screenshots", pathError );
		std::ofstream active( "profiles/active.cfg", std::ios::trunc );
		if ( active )
			active << szNewProfile;
		if ( std::filesystem::exists( "profiles/" + szNewProfile + "/config.cfg", pathError ) )
		{
			// An existing profile brings its own settings - monitor
			// included; the screen regaining focus applies mode (and
			// display) changes live via the ChangeResolution diff.
			pML->SerializeConfig( true, 0xffffffff );
			pOptionsSystem->Init();
			bLoadedExistingProfile = true;
		}
		// A brand-new profile is seeded with the settings in
		// effect right now by the flush below.
	}
	pOptionsSystem->Set( "GamePlay.PlayerName", variant_t( szTypedName.c_str() ) );

	/*{
		IOptionSystem * pOptions =  GetSingleton<IOptionSystem>();
		const SOptionDesc * pDesc = pOptions->GetDesc( "Multiplayer.PlayerName" );
		variant_t varPlayerName;
		pOptions->Get( "Multiplayer.PlayerName", &varPlayerName );
		const std::wstring szNameFromOptions = (wchar_t*)(bstr_t)varPlayerName;
		const std::wstring szDefault = (wchar_t*)(bstr_t)pDesc->defaultValue;
		if ( szNameFromOptions == szDefault )
			pOptionsSystem->Set( "Multiplayer.PlayerName", variant_t( szTypedName.c_str() ) );
	}*/

	GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
	// The difficulty/blood rows still show the previous
	// profile's selections; applying them would overwrite what
	// the loaded profile chose for itself.
	if ( !bLoadedExistingProfile )
		pOptions->Apply();
}
void CInterfacePlayerProfile::DeleteProfile( const std::string &szName )
{
	const std::string szActive = GetGlobalVar( "Profile.Name", "" );
	std::string szError;
	if ( !NProfile::Delete( szName, &szError ) )
	{
		ReportProfileError( "Textes\\UI\\Intermission\\MainMenu\\PlayerProfile\\err_fs", szError );
		return;
	}
	if ( !NProfile::NameEquals( szName, szActive ) )
	{
		FillProfileList( szActive );
		return;
	}
	// The active profile is gone; the screen may not close without one.
	const std::vector<std::string> remaining = NProfile::List();
	const std::string szTarget = remaining.empty() ? std::string( "Player" ) : remaining[0];
	const std::wstring szWideTarget = WideFromProfileName( szTarget );
	if ( !NProfile::NameEquals( szTarget, szActive ) )
		SwitchToProfile( szTarget, szWideTarget );
	else
	{
		// The deleted profile was the last one and carried the fallback name
		// already: SwitchToProfile would see old == new and change nothing, so
		// the fresh directories and active.cfg are made here, seeded with the
		// settings in effect by the flush.
		std::error_code pathError;
		std::filesystem::create_directories( "profiles/" + szTarget + "/saves", pathError );
		std::filesystem::create_directories( "profiles/" + szTarget + "/screenshots", pathError );
		std::ofstream active( "profiles/active.cfg", std::ios::trunc );
		if ( active )
			active << szTarget;
		GetSingleton<IOptionSystem>()->Set( "GamePlay.PlayerName", variant_t( szWideTarget.c_str() ) );
		GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
	}
	// Both branches ran SerializeConfig with the screen staying open; the
	// binder left the "default" bind section behind (see the rename path).
	pInput->SetBindSection( "intermission" );
	FillProfileList( szTarget );
	SetEditBoxText( szWideTarget );
	pButtonOK->EnableWindow( true );
}
void CInterfacePlayerProfile::FillProfileList( const std::string &szSelect )
{
	if ( pProfileList == 0 )
		return;
	for ( int i = pProfileList->GetNumberOfItems() - 1; i >= 0; i-- )
		pProfileList->RemoveItem( i );
	profileNames = NProfile::List();
	int nSelect = -1;
	for ( int i = 0; i < profileNames.size(); ++i )
	{
		pProfileList->AddItem();
		IUIListRow *pRow = pProfileList->GetItem( i );
		pRow->SetUserData( i );
		const std::wstring szName = WideFromProfileName( profileNames[i] );
		pRow->GetElement( 0 )->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szName.c_str() ) ) );
		if ( NProfile::NameEquals( profileNames[i], szSelect ) )
			nSelect = i;
	}
	if ( nSelect >= 0 )
		pProfileList->SetSelectionItem( nSelect );
	pProfileList->InitialUpdate();
}
void CInterfacePlayerProfile::SetEditBoxText( const std::wstring &szText )
{
	pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
	pEdit->SetSel( 0, -1 );
	pEdit->SetCursor( szText.length() );
}
void CInterfacePlayerProfile::ReportProfileError( const char *pszTextKey, const std::string &szDetail )
{
	// Localized line first when the key resolves, the raw filesystem message
	// either way - a failure must never be silent.
	IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>();
	if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetDialog( pszTextKey ) )
	{
		if ( pText->GetString() != 0 )
			pConsole->Write( CONSOLE_STREAM_CHAT, NPlatform::WideFromWordString( pText->GetString() ).c_str(), 0xffff0000 );
	}
	if ( !szDetail.empty() )
		pConsole->WriteASCII( CONSOLE_STREAM_CHAT, szDetail.c_str(), 0xffff0000 );
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
		if ( !szPendingDelete.empty() )
		{
			// Focus returning with a pending name means the confirm box just
			// closed; the global var says which button did it.
			const std::string szDoomed = szPendingDelete;
			szPendingDelete.clear();
			if ( GetGlobalVar( "PlayerProfile.ConfirmDelete", 0 ) )
			{
				RemoveGlobalVar( "PlayerProfile.ConfirmDelete" );
				DeleteProfile( szDoomed );
			}
		}
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

	// The edit box shows the ACTIVE PROFILE name, not the raw
	// GamePlay.PlayerName option: the option is a wide string that survived a
	// wchar->char truncation round trip in older builds, so it can hold
	// mojibake the profile system itself would never accept. Profile.Name is
	// always the sanitized ASCII directory name and defaults to "Player", so
	// this is what the player actually owns and what OK would switch against.
	const std::string szProfile = NProfile::Sanitize( GetGlobalVar( "Profile.Name", "" ) );
	std::wstring szName;
	for ( std::string::size_type i = 0; i < szProfile.size(); ++i )
		szName += wchar_t( (unsigned char)szProfile[i] );

	pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szName.c_str() ) ) );
	
	pButtonOK = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_OK ) );
	pButtonCancel = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_CANCEL ) );
	
	pButtonOK->EnableWindow( bEnableOk );
	pButtonCancel->EnableWindow( true );

	pProfileList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_PROFILE_LIST ) );
	szPendingDelete.clear();
	FillProfileList( szProfile );
	
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
