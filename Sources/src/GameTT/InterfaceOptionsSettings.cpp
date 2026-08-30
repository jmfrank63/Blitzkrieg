#include "StdAfx.h"

#include "../Platform/LegacyText.h"
#include "InterfaceOptionsSettings.h"
#include "../StreamIO/OptionSystem.h"
#include "CommonId.h"
#include "OptionEntryWrapper.h"
#include "MainMenu.h"
#include "../Main/iMainCommands.h"
#include <mmsystem.h>
#include "../Common/Actions.h"
#include "../Main/ScenarioTracker.h"
#include "../Main/CloudSyncFacade.h"
#include "CloudJson.h"
#include <algorithm>
static const NInput::SRegisterCommandEntry commonCommands[] = 
{
	{ "cancel_load"	,	IMC_CANCEL					},
	{ "load_mission", IMC_OK							},
	{ "key_up",				MESSAGE_KEY_UP			},
	{ "key_down",			MESSAGE_KEY_DOWN		},
	{ "key_left",			MESSAGE_KEY_LEFT		},
	{ "key_right",		MESSAGE_KEY_RIGHT		},
	{ "key_tab",			MESSAGE_KEY_TAB			},
	{ 0							,	0										}
};
enum EButtonsInOptionsSettings
{
	// Six division buttons and six lists exist in Data/UI/OptionsSettings.xml
	// (ids 10007..10012 and 1000..1005). The END constants are unused by the
	// code but must document that layout truthfully.
	_E_BUTTON_CHANGE_DIVISION_BEGIN					= 10007,
	_E_BUTTON_CHANGE_DIVISION_END						= 10012,


	_E_LIST_BEGIN						= 1000,
	_E_LIST_END							= 1005,


	E_BUTTON_DEFAULT				= 10003,
	E_BUTTON_OK							= 10002,			// the V button; same event id as IMC_OK
	E_BUTTON_CANCEL					= 10001,			// the X button; same event id as IMC_CANCEL

	E_STATIC_CAPTION				= 20000,
	// Gold static in the lower left with the active profile name - the id is
	// a convention, other screens showing the profile label reuse it.
	E_STATIC_PROFILE				= 21000,

	// The cloud screen buttons, visible only while the Cloud tab is active,
	// sharing the sixth tab slot. Endpoints, keys and the secret are not
	// options - the option store truncates long strings and must never
	// carry a secret - so the Cloud tab opens dedicated screens for them.
	E_BUTTON_CLOUD_CREDENTIALS	= 10013,
	E_BUTTON_CLOUD_BACKUPS			= 10014,
};
bool CInterfaceOptionsSettings::OpenCurtains()
{
	if ( !GetGlobalVar( "AreWeInMission", 0 ) )
		OpenCurtainsForced();
	return true;
}
void CInterfaceOptionsSettings::SuspendAILogic( bool bSuspend )
{
	if ( GetGlobalVar( "AreWeInMission", 0 ) && GetGlobalVar( "MultiplayerGame", 0 ) == 1 )
	{
	}
	else
	{
		CInterfaceInterMission::SuspendAILogic( bSuspend );
	}
}
bool CInterfaceOptionsSettings::Init()
{
	CInterfaceInterMission::Init();
	SetBindSection( "loadmission" );
	commandMsgs.Init( pInput, commonCommands );
	pWheelScroll = pInput->CreateSlider( "mouse_wheel" );
	if ( GetGlobalVar( "AreWeInMission", 0 ) && GetGlobalVar( "MultiplayerGame", 0 ) == 1 )
		SuspendAILogic( false );
	return true;
}
bool CInterfaceOptionsSettings::StepLocal( bool bAppActive )
{
	const bool bResult = CInterfaceInterMission::StepLocal( bAppActive );
	// A Provider change - a row click, or the left/right keys - reshapes the
	// Cloud tab: the timing rows and the two buttons follow the value. Read
	// per frame rather than per message so every path that commits the
	// instant-apply row is caught, including clicks the list handles itself.
	if ( nCloudDivision >= 0 && nActive == nCloudDivision )
	{
		const std::string szProvider = ReadCloudProvider();
		if ( szProvider != szCloudProvider )
		{
			szCloudProvider = szProvider;
			BuildCloudList();
		}
	}
	if ( nCatalogueHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nCatalogueHandle );
		if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
		{
			NCloudSync::Release( nCatalogueHandle );
			nCatalogueHandle = -1;
			if ( eState == NCloudSync::STATE_DONE )
				LoadCloudDestinations();
		}
	}
	// The UI screen already routes the wheel to whatever list the cursor is
	// inside of; this covers the rest of the screen, so the wheel scrolls the
	// option list wherever it is turned. Only outside the list rect - inside
	// it the screen's own routing delivers the same delta and forwarding it
	// again would scroll twice per notch.
	if ( bAppActive && pWheelScroll && pUIScreen && nActive >= 0 )
	{
		const float fDelta = pWheelScroll->GetDelta();
		if ( fDelta != 0.0f )
		{
			IUIElement *pList = pUIScreen->GetChildByID( _E_LIST_BEGIN + nActive );
			if ( pList && pList->IsVisible() )
			{
				CTRect<float> rcList;
				pList->GetWindowPlacement( 0, 0, &rcList );
				if ( !rcList.IsInside( pCursor->GetPos() ) )
					pList->OnMouseWheel( CVec2( rcList.x1 + rcList.Width() * 0.5f, rcList.y1 + rcList.Height() * 0.5f ), E_MOUSE_FREE, fDelta );
			}
		}
	}
	return bResult;
}
void CInterfaceOptionsSettings::Done()
{
	if ( nCatalogueHandle >= 0 )
	{
		NCloudSync::Release( nCatalogueHandle );
		nCatalogueHandle = -1;
	}
	CInterfaceInterMission::Done();
}
// Cloud.Provider is the switch: "Off" - and the pre-row "ON"/"OFF" values a
// profile written before it may still carry - mean cloud sync is off.
bool CInterfaceOptionsSettings::IsCloudProviderOff( const std::string &szValue )
{
	return szValue.empty() ||
		NStr::CompareAsciiNoCase( szValue.c_str(), "Off" ) == 0 ||
		NStr::CompareAsciiNoCase( szValue.c_str(), "On" ) == 0;
}
std::string CInterfaceOptionsSettings::ReadCloudProvider() const
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( "Cloud.Provider", &var ) )
		return std::string();
	return std::string( (const char*)bstr_t( var ) );
}
// The Cloud tab's list. Provider first; the timing rows and the settings
// backup only when a provider is chosen - with Provider Off they would be
// four switches that do nothing. Rebuilt whenever the Provider value changes
// and when the catalogue fetch lands, so the row's list is always the full
// destination list the facade offers.
void CInterfaceOptionsSettings::BuildCloudList()
{
	if ( nCloudDivision < 0 || pUIScreen == 0 )
		return;
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( _E_LIST_BEGIN + nCloudDivision ) );
	if ( pList == 0 )
		return;
	const bool bOff = IsCloudProviderOff( szCloudProvider );

	OptionDescs descs;
	for ( OptionDescs::const_iterator it = cloudDescs.begin(); it != cloudDescs.end(); ++it )
	{
		// Cloud.Enabled is gone from defconf; a profile config written before
		// the Provider row may still carry it, and it must not come back as a
		// stray row.
		if ( NStr::CompareAsciiNoCase( it->szName.c_str(), "Cloud.Enabled" ) == 0 )
			continue;
		const bool bProviderRow = NStr::CompareAsciiNoCase( it->szName.c_str(), "Cloud.Provider" ) == 0;
		if ( bProviderRow || !bOff )
			descs.push_back( *it );
	}

	// The Provider row's list: Off, then the destinations sorted by id, and
	// always the row's own value - COptionSelection resolves an absent value
	// to entry 0, which would turn cloud sync off on the next OK. The saved
	// credentials' backend rides along for the same reason.
	OptionDropOverrides overrides;
	std::vector<SOptionDropListValue> &values = overrides["Cloud.Provider"];
	SOptionDropListValue off;
	off.szProgName = "Off";
	values.push_back( off );
	std::vector<std::string> names = cloudDestinations;
	if ( !bOff )
		names.push_back( szCloudProvider );
	char szBackend[256];
	const int nBackend = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	if ( nBackend > 0 && nBackend < (int)sizeof szBackend )
		names.push_back( szBackend );
	std::sort( names.begin(), names.end() );
	for ( size_t i = 0; i < names.size(); ++i )
	{
		if ( IsCloudProviderOff( names[i] ) || ( i > 0 && names[i] == names[i - 1] ) )
			continue;
		SOptionDropListValue value;
		value.szProgName = names[i];
		values.push_back( value );
	}

	while ( pList->GetNumberOfItems() )
		pList->RemoveItem( 0 );
	optionsLists[nCloudDivision] = new COptionsListWrapper( pList, descs, 100, overrides );
	if ( pList->GetNumberOfItems() > 0 )
		pList->SetSelectionItem( 0 );
	RefreshCloudButtons();
}
// The catalogue fetch, on opening the Cloud tab: a deliberate player action,
// the same rule under which the credentials dialog spawns the daemon. A
// cached list rebuilds at once; a fetch is polled in StepLocal. No catalogue
// is not an error here - the row offers Off and the values it must keep, and
// the full list arrives when the fetch does.
void CInterfaceOptionsSettings::BeginCloudCatalogue()
{
	if ( nCatalogueHandle >= 0 || !cloudDestinations.empty() )
		return;
	const int nResult = NCloudSync::EnsureCatalogue();
	if ( nResult == NCloudSync::CATALOGUE_CACHED )
		LoadCloudDestinations();
	else if ( nResult >= 0 )
		nCatalogueHandle = nResult;
}
// The facade's filtered destination list, with the saved backend riding
// along even when the running rclone's catalogue lacks it.
void CInterfaceOptionsSettings::LoadCloudDestinations()
{
	char szBackend[256];
	const int nBackend = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	const char *pszConfigured = ( nBackend > 0 && nBackend < (int)sizeof szBackend ) ? szBackend : "";
	std::string szListDoc;
	if ( !ReadSizedDocument( [pszConfigured]( char *psz, unsigned int nCap ) { return NCloudSync::CatalogueDestinations( pszConfigured, psz, nCap ); }, &szListDoc ) )
		return;
	SJsonValue list;
	if ( !JsonParse( szListDoc, &list ) )
		return;
	cloudDestinations.clear();
	if ( const SJsonValue *pNames = list.Get( "destinations" ) )
		for ( size_t i = 0; i < pNames->children.size(); ++i )
			if ( pNames->children[i].eType == SJsonValue::T_STRING )
				cloudDestinations.push_back( pNames->children[i].szValue );
	BuildCloudList();
}
// The cloud screens belong to the Cloud tab, and only to a chosen provider:
// Config... sets up the service the row names, Backups... browses what that
// service holds.
void CInterfaceOptionsSettings::RefreshCloudButtons()
{
	const bool bShow = nActive == nCloudDivision && nCloudDivision >= 0 && !IsCloudProviderOff( szCloudProvider );
	const int nShow = bShow ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE;
	if ( IUIElement *pCredentials = pUIScreen->GetChildByID( E_BUTTON_CLOUD_CREDENTIALS ) )
		pCredentials->ShowWindow( nShow );
	if ( IUIElement *pBackups = pUIScreen->GetChildByID( E_BUTTON_CLOUD_BACKUPS ) )
		pBackups->ShowWindow( nShow );
}
void CInterfaceOptionsSettings::Create()
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	const bool bInMission = GetGlobalVar( "AreWeInMission", 0 );
	if ( bInMission )
		pUIScreen->Load( "ui\\MissionOptionsSettings" );
	else
		pUIScreen->Load( "ui\\OptionsSettings" );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	if ( !bInMission )
		StoreScreen();
	pScene->AddUIScreen( pUIScreen );
	
	std::unordered_map< std::string, OptionDescs > sections;
	std::vector< std::string > sectionOrder;
	
	IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();

	const int nOptionFlag = GetGlobalVar("MultiplayerGame", 0) == 1 ? 
													OPTION_FLAG_IN_MP_MISSION  : 
													(bInMission ? OPTION_FLAG_IN_MISSION : OPTION_FLAG_MAIN_OPTIONS);
	
	for ( CPtr<IOptionSystemIterator> pIter = pOptionSystem->CreateIterator( nOptionFlag );
				!pIter->IsEnd(); pIter->Next() )
	{
		const SOptionDesc * pDesc = pIter->GetDesc();
		NI_ASSERT_T( pDesc != 0, "IOptionSystemIterator::GetDesc returned null" );
		if ( pDesc == 0 )
			continue;

		NI_ASSERT_T( !pDesc->szName.empty(), "Option descriptor name is empty" );
		NI_ASSERT_T( !pDesc->szDivision.empty(), NStr::Format( "Option descriptor has empty division: %s", pDesc->szName.c_str() ) );
		if ( pDesc->szDivision.empty() )
			continue;

		const SOptionDesc desc = *pDesc;
		OptionDescs &sectionDescs = sections[desc.szDivision];
		// Divisions become the tab buttons in first-encounter order; walking
		// the unordered_map instead made the tab layout depend on the STL's
		// hash order, which differs between platforms.
		if ( sectionDescs.empty() )
			sectionOrder.push_back( desc.szDivision );
		sectionDescs.push_back( desc );
	}
	
	ITextManager * pTM = GetSingleton<ITextManager>();

	nMaxDivision = 0;
	nCloudDivision = -1;
	const std::string szKeyOption = "Textes\\Options\\";
	for ( int nSection = 0; nSection < sectionOrder.size(); ++nSection )
	{
		const std::string &szSection = sectionOrder[nSection];
		const std::string szKeyName = szKeyOption + szSection + ".name";
		const std::string szKeyTooltip = szKeyOption + szSection + ".tooltip";

		IText *pT = pTM->GetString( szKeyName.c_str() );

		NI_ASSERT_T( pT != 0, NStr::Format( "no local name for section %s", szKeyName.c_str() ) );

		// The screen has exactly six tab/list slots. A seventh division - a
		// mod declaring more option sections than the UI has tabs - must not
		// reach checked_cast on the missing child and take the settings
		// screen down with it; it is skipped, and the trace names it so the
		// mod author can find out why their tab never appeared.
		if ( pUIScreen->GetChildByID( _E_LIST_BEGIN + nMaxDivision ) == 0 ||
				 pUIScreen->GetChildByID( _E_BUTTON_CHANGE_DIVISION_BEGIN + nMaxDivision ) == 0 )
		{
			NStr::DebugTrace( "CInterfaceOptionsSettings: no tab slot for division %d (\"%s\") - skipped\n", nMaxDivision, szSection.c_str() );
			continue;
		}

		IUIListControl * pList = checked_cast<IUIListControl*>(pUIScreen->GetChildByID( _E_LIST_BEGIN + nMaxDivision ));
		IUIStatic * pCaption = checked_cast<IUIStatic*>( pList->GetChildByID( 10 ) );

		if ( szSection == "Cloud" )
		{
			nCloudDivision = nMaxDivision;
			cloudDescs = sections[szSection];
			szCloudProvider = ReadCloudProvider();
			optionsLists.push_back( CPtr<COptionsListWrapper>() );
			BuildCloudList();
		}
		else
			optionsLists.push_back( new COptionsListWrapper( pList, sections[szSection], 100 ) );
		IUIButton * pButton = checked_cast<IUIButton *>( pUIScreen->GetChildByID( _E_BUTTON_CHANGE_DIVISION_BEGIN + nMaxDivision ) );
		
		pButton->SetWindowText( -1, pT->GetString() );
		pCaption->SetWindowText( -1, pT->GetString() );

		pT = pTM->GetString( szKeyTooltip.c_str() );
		if ( pT )
			pButton->SetHelpContext( 0, pT->GetString() );
		
		pButton->ShowWindow( UI_SW_SHOW );
		++nMaxDivision;
	}
	
	nActive = -1;
	OnChangeDivision( 0 );

	// "Settings of <profile>" caption and the gold profile label in the lower
	// left (id convention E_STATIC_PROFILE, mirroring the main menu's version
	// label in the lower right). The profile name is printable ASCII by
	// contract (NProfile::Sanitize), so widening per character is exact.
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	if ( !szProfile.empty() )
	{
		std::wstring szWideProfile;
		for ( int i = 0; i < szProfile.size(); ++i )
			szWideProfile += wchar_t( (unsigned char)szProfile[i] );

		if ( IUIElement *pCaption = pUIScreen->GetChildByID( E_STATIC_CAPTION ) )
		{
			if ( IText *pText = pTM->GetString( "Textes\\UI\\OptionsSettings\\caption_settings_of" ) )
			{
				const std::wstring szCaption = std::wstring( NPlatform::WideFromWordString( pText->GetString() ) ) + L" " + szWideProfile;
				pCaption->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szCaption ) ) );
			}
		}
		if ( IUIElement *pProfileLabel = pUIScreen->GetChildByID( E_STATIC_PROFILE ) )
			pProfileLabel->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szWideProfile ) ) );
	}
}
void CInterfaceOptionsSettings::OnChangeDivision( const int nDivision )
{
	if ( nActive != -1 )
	{
		optionsLists[nActive]->ShowWindow( UI_SW_HIDE );
		
		IUIElement *pElement = pUIScreen->GetChildByID( nActive + _E_BUTTON_CHANGE_DIVISION_BEGIN );
		NI_ASSERT_T( pElement != 0, NStr::Format("There is no button with id %d") );
		pElement->EnableWindow( true );
	}
	nActive = nDivision;

	optionsLists[nActive]->ShowWindow( UI_SW_SHOW );
	optionsLists[nActive]->ShowWindow( UI_SW_MAXIMIZE );

	// Start keyboard navigation on the first row: the selection bar shows
	// which row up/down/left/right work on, and a list nobody has clicked
	// yet otherwise has no selection at all.
	if ( IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( nActive + _E_LIST_BEGIN ) ) )
	{
		if ( pList->GetSelectionItem() == -1 && pList->GetNumberOfItems() > 0 )
			pList->SetSelectionItem( 0 );
	}

	IUIElement *pElement = pUIScreen->GetChildByID( nActive + _E_BUTTON_CHANGE_DIVISION_BEGIN );
	NI_ASSERT_T( pElement != 0, NStr::Format("There is no button with id %d") );
	pElement->EnableWindow( false );

	RefreshCloudButtons();
	if ( nActive == nCloudDivision )
		BeginCloudCatalogue();
}
// Tab parks the cursor on the next button of the screen - division tabs,
// then V (ok), then X (cancel) - so the button highlights, tooltips and
// clicks through the very same path the mouse uses. The cursor position IS
// the keyboard "active button" state: as soon as the player moves the mouse
// somewhere else, Enter falls back to its usual ok meaning.
void CInterfaceOptionsSettings::CycleNavButton()
{
	std::vector<int> buttons;
	for ( int i = 0; i < nMaxDivision; ++i )
	{
		if ( i != nActive )				// the active division's tab is disabled
			buttons.push_back( _E_BUTTON_CHANGE_DIVISION_BEGIN + i );
	}
	buttons.push_back( E_BUTTON_OK );
	buttons.push_back( E_BUTTON_CANCEL );

	int nFrom = -1;
	const CVec2 vCursor = pCursor->GetPos();
	for ( int i = 0; i < buttons.size() && nFrom == -1; ++i )
	{
		IUIElement *pButton = pUIScreen->GetChildByID( buttons[i] );
		CTRect<float> rcButton;
		if ( pButton && pButton->IsVisible() )
		{
			pButton->GetWindowPlacement( 0, 0, &rcButton );
			if ( rcButton.IsInside( vCursor ) )
				nFrom = i;
		}
	}

	for ( int nStep = 1; nStep <= buttons.size(); ++nStep )
	{
		const int nNext = ( nFrom + nStep ) % buttons.size();
		IUIElement *pButton = pUIScreen->GetChildByID( buttons[nNext] );
		if ( pButton == 0 || !pButton->IsVisible() )
			continue;
		CTRect<float> rcButton;
		pButton->GetWindowPlacement( 0, 0, &rcButton );
		pCursor->SetPos( int( rcButton.x1 + rcButton.Width() * 0.5f ), int( rcButton.y1 + rcButton.Height() * 0.5f ) );
		nActiveNavButton = buttons[nNext];
		return;
	}
}
int CInterfaceOptionsSettings::GetArmedNavButton()
{
	if ( nActiveNavButton == -1 || pUIScreen == 0 )
		return -1;
	IUIElement *pButton = pUIScreen->GetChildByID( nActiveNavButton );
	if ( pButton == 0 || !pButton->IsVisible() )
		return -1;
	CTRect<float> rcButton;
	pButton->GetWindowPlacement( 0, 0, &rcButton );
	return rcButton.IsInside( pCursor->GetPos() ) ? nActiveNavButton : -1;
}
void CInterfaceOptionsSettings::Close()
{
	// Whatever changed on the Cloud tab, the menu indicator re-evaluates
	// "chosen but not set up" when it gets the screen back.
	SetGlobalVar( "CloudSync.Recheck", 1 );
	if ( GetGlobalVar( "AreWeInMission", 0 ) )
	{
		IMainLoop *pML = GetSingleton<IMainLoop>();
		CloseInterface();
		pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//������ �����
		pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_LOAD_FINISHED) );
	}
	else
		FinishInterface( MISSION_COMMAND_MAIN_MENU, NStr::Format( "%d", CInterfaceMainMenu::E_OPTIONS ) );
}
bool CInterfaceOptionsSettings::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	if ( msg.nEventID >= _E_BUTTON_CHANGE_DIVISION_BEGIN && msg.nEventID < nMaxDivision + _E_BUTTON_CHANGE_DIVISION_BEGIN )
	{
		OnChangeDivision( msg.nEventID - _E_BUTTON_CHANGE_DIVISION_BEGIN );
	}

	if ( nActive >= 0 &&
			 optionsLists.size() > nActive && 
			 optionsLists[nActive]->ProcessMessage( msg ) ) 
	{
		return true;
	}

	if ( CInterfaceInterMission::ProcessMessage( msg ) ) return true;

	switch( msg.nEventID )
	{

	case MESSAGE_KEY_LEFT:
	case MESSAGE_KEY_RIGHT:
		// Left/right edit the highlighted row's value the way its own controls
		// do - next/previous choice, or a slider key step.
		if ( nActive >= 0 && optionsLists.size() > nActive )
			return optionsLists[nActive]->ChangeSelectedOption( msg.nEventID == MESSAGE_KEY_RIGHT );
		return false;

	case MESSAGE_KEY_TAB:
		CycleNavButton();
		return true;

	case IMC_OK:
		{
			// Enter presses the button Tab parked the cursor on; without one it
			// keeps its usual meaning of ok.
			const int nArmed = GetArmedNavButton();
			if ( nArmed >= _E_BUTTON_CHANGE_DIVISION_BEGIN && nArmed < _E_BUTTON_CHANGE_DIVISION_BEGIN + nMaxDivision )
			{
				OnChangeDivision( nArmed - _E_BUTTON_CHANGE_DIVISION_BEGIN );
				return true;
			}
			if ( nArmed == E_BUTTON_CANCEL )
				return ProcessMessage( SGameMessage( IMC_CANCEL ) );

			for ( int i = 0; i < optionsLists.size(); ++i )
				optionsLists[i]->Apply();
			GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
			Close();
		}
		return true;

	case E_BUTTON_CLOUD_CREDENTIALS:
		// The settings screen stays below; the dialog pops back to it.
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CLOUD_CREDENTIALS, 0 );
		return true;

	case E_BUTTON_CLOUD_BACKUPS:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CLOUD_BACKUPS, 0 );
		return true;

	case IMC_CANCEL:
		for ( int i = 0; i < optionsLists.size(); ++i )
			optionsLists[i]->CancelChanges();
		Close();

		return true;
	case E_BUTTON_DEFAULT:
		for ( int i = 0; i < optionsLists.size(); ++i )
			optionsLists[i]->ToDefault();
		
		return true;
	}
	return false;
}