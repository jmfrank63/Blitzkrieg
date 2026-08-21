#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceCloudBackups.h"
#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "../UI/UIMessages.h"
#include "../Main/CloudSyncFacade.h"
#include <ctime>
#include <algorithm>

static const NInput::SRegisterCommandEntry commands[] =
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum
{
	E_LIST											= 2100,
	E_STATIC_STATUS							= 3102,
	E_STATIC_EXPLAIN						= 3103,
	E_BUTTON_CANCEL							= 10001,
	E_BUTTON_PRIMARY						= 10030,		// restore... / merge restore / full restore now
	E_BUTTON_SECONDARY					= 10031,		// undo (named for its state) / full restore... / back
};
// One parsed row of the facade's entry document.
struct SBackupRow
{
	std::string szID;
	std::string szHost;
	long long nTimestamp;
	unsigned long long nSize;
};
static bool RowOrder( const SBackupRow &a, const SBackupRow &b )
{
	// Grouped by host, newest first within the group.
	if ( a.szHost != b.szHost )
		return a.szHost < b.szHost;
	return a.nTimestamp > b.nTimestamp;
}
// The same flat-scan JSON helpers the credentials dialog uses; every key
// name is unique within one entry document.
static std::string EntryString( const std::string &szDoc, const char *pszKey )
{
	const std::string szNeedle = std::string( "\"" ) + pszKey + "\"";
	std::string::size_type nPos = szDoc.find( szNeedle );
	if ( nPos == std::string::npos )
		return "";
	nPos += szNeedle.size();
	while ( nPos < szDoc.size() && ( szDoc[nPos] == ':' || szDoc[nPos] == ' ' ) )
		++nPos;
	if ( nPos >= szDoc.size() )
		return "";
	if ( szDoc[nPos] == '"' )
	{
		std::string szValue;
		for ( ++nPos; nPos < szDoc.size() && szDoc[nPos] != '"'; ++nPos )
		{
			if ( szDoc[nPos] == '\\' && nPos + 1 < szDoc.size() )
				++nPos;
			szValue += szDoc[nPos];
		}
		return szValue;
	}
	std::string szBare;
	while ( nPos < szDoc.size() && ( isdigit( (unsigned char)szDoc[nPos] ) || szDoc[nPos] == '-' ) )
		szBare += szDoc[nPos++];
	return szBare;
}
static const char *CloudOutcomeTextKey( const std::string &szError )
{
	if ( szError == "Cancelled" )
		return "offline";
	static const char *pszOutcomes[] = { "needs_resync", "too_many_deletes", "name_too_long",
		"out_of_sync", "auth_failed", "remote_unreachable", "remote_missing",
		"daemon_gone", "timed_out", 0 };
	for ( int i = 0; pszOutcomes[i] != 0; ++i )
	{
		const int nLen = (int)strlen( pszOutcomes[i] );
		if ( szError.compare( 0, nLen, pszOutcomes[i] ) == 0 )
			return pszOutcomes[i];
	}
	return "failed";
}
static std::wstring TextOrFallback( const char *pszKey, const wchar_t *pszFallback )
{
	if ( IText *pText = GetSingleton<ITextManager>()->GetString( pszKey ) )
		if ( pText->GetString() != 0 )
			return std::wstring( NPlatform::WideFromWordString( pText->GetString() ) );
	return pszFallback;
}
static std::wstring WideFromNarrow( const std::string &szText )
{
	std::wstring szWide;
	for ( std::string::size_type i = 0; i < szText.size(); ++i )
		szWide += wchar_t( (unsigned char)szText[i] );
	return szWide;
}
void CInterfaceCloudBackups::SetStatus( const char *pszTextKey, const std::wstring &szSuffix )
{
	IUIElement *pStatus = pUIScreen->GetChildByID( E_STATIC_STATUS );
	if ( pStatus == 0 )
		return;
	std::wstring szLine;
	if ( pszTextKey != 0 )
		szLine = TextOrFallback( pszTextKey, L"" );
	if ( !szSuffix.empty() )
	{
		if ( !szLine.empty() )
			szLine += L" ";
		szLine += szSuffix;
	}
	pStatus->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szLine.c_str() ) ) );
}
void CInterfaceCloudBackups::FillList()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	if ( pList == 0 )
		return;

	std::vector<SBackupRow> rows;
	char pszDoc[2048];
	for ( unsigned int i = 0; NCloudSync::BackupEntry( nListHandle, i, pszDoc, sizeof( pszDoc ) ); ++i )
	{
		const std::string szDoc = pszDoc;
		SBackupRow row;
		row.szID = EntryString( szDoc, "id" );
		row.szHost = EntryString( szDoc, "host" );
		row.nTimestamp = atoll( EntryString( szDoc, "timestamp" ).c_str() );
		row.nSize = (unsigned long long)atoll( EntryString( szDoc, "size" ).c_str() );
		rows.push_back( row );
	}
	std::sort( rows.begin(), rows.end(), RowOrder );

	entryIDs.clear();
	for ( int i = pList->GetNumberOfItems() - 1; i >= 0; i-- )
		pList->RemoveItem( i );
	for ( int i = 0; i < rows.size(); ++i )
	{
		const SBackupRow &row = rows[i];
		entryIDs.push_back( row.szID );
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( i );
		pRow->SetUserData( i );

		std::wstring szWhen;
		if ( row.nTimestamp > 0 )
		{
			const time_t nTime = (time_t)row.nTimestamp;
			tm *pLocal = localtime( &nTime );
			char pszWhen[64] = "";
			if ( pLocal != 0 )
				strftime( pszWhen, sizeof( pszWhen ), "%Y-%m-%d %H:%M", pLocal );
			szWhen = WideFromNarrow( pszWhen );
		}
		else
			szWhen = L"-";		// a file in the backup root that is not one of ours

		const unsigned long long nKB = row.nSize == 0 ? 0 : ( row.nSize + 1023 ) / 1024;
		const std::wstring szSize = WideFromNarrow( NStr::Format( "%d KB", (int)nKB ) );

		pRow->GetElement( 0 )->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( WideFromNarrow( row.szHost ) ) ) );
		pRow->GetElement( 1 )->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szWhen ) ) );
		pRow->GetElement( 2 )->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szSize ) ) );
	}
	pList->InitialUpdate();

	if ( rows.empty() )
		SetStatus( "Textes\\UI\\CloudBackups\\empty", L"" );
	else
		SetStatus( 0, L"" );
}
void CInterfaceCloudBackups::SetExplain( const char *pszTextKey )
{
	IUIElement *pExplain = pUIScreen->GetChildByID( E_STATIC_EXPLAIN );
	if ( pExplain == 0 )
		return;
	const std::wstring szText = pszTextKey != 0 ? TextOrFallback( pszTextKey, L"" ) : L"";
	pExplain->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
}
// Labels, visibility and enablement follow the state machine and what
// restore_undo_available reports - three answers plus busy, each with its
// own name, because cancelling a restore that has not happened and
// reversing one that has are visibly different acts.
void CInterfaceCloudBackups::RefreshControls()
{
	IUIElement *pPrimary = pUIScreen->GetChildByID( E_BUTTON_PRIMARY );
	IUIElement *pSecondary = pUIScreen->GetChildByID( E_BUTTON_SECONDARY );
	if ( pPrimary == 0 || pSecondary == 0 )
		return;

	const bool bWorking = nActionHandle >= 0;
	const char *pszPrimary = 0;
	const char *pszSecondary = 0;
	bool bPrimaryOn = false;
	bool bSecondaryOn = false;

	switch ( eBrowseState )
	{
	case BS_BROWSE:
		{
			SetExplain( "Textes\\UI\\CloudBackups\\explain_browse" );
			pszPrimary = "button_restore";
			bPrimaryOn = !bWorking && nSelectedEntry >= 0 && nSelectedEntry < (int)entryIDs.size();
			const NCloudSync::EUndoAvailability eUndo = NCloudSync::UndoAvailability( GetGlobalVar( "Profile.Name", "" ) );
			if ( eUndo == NCloudSync::UNDO_CANCELLABLE )
			{
				pszSecondary = "button_cancel_pending";
				bSecondaryOn = !bWorking;
			}
			else if ( eUndo == NCloudSync::UNDO_REINSTATABLE )
			{
				pszSecondary = "button_undo_applied";
				bSecondaryOn = !bWorking;
			}
			else if ( eUndo == NCloudSync::UNDO_BUSY )
			{
				// A restore download holds the slot; an undo now would be
				// silently overwritten when it lands.
				pszSecondary = "button_undo_busy";
				bSecondaryOn = false;
			}
		}
		break;
	case BS_CONFIRM:
		SetExplain( "Textes\\UI\\CloudBackups\\explain_merge" );
		pszPrimary = "button_restore_merge";
		pszSecondary = "button_restore_full";
		bPrimaryOn = !bWorking;
		bSecondaryOn = !bWorking;
		break;
	case BS_CONFIRM_FULL:
		SetExplain( "Textes\\UI\\CloudBackups\\explain_full" );
		pszPrimary = "button_restore_full_go";
		pszSecondary = "button_back";
		bPrimaryOn = !bWorking;
		bSecondaryOn = !bWorking;
		break;
	}

	if ( pszPrimary != 0 )
	{
		const std::string szKey = std::string( "Textes\\UI\\CloudBackups\\" ) + pszPrimary;
		pPrimary->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide( TextOrFallback( szKey.c_str(), L"?" ).c_str() ) ) );
		pPrimary->EnableWindow( bPrimaryOn );
		pPrimary->ShowWindow( UI_SW_SHOW_DONT_MOVE_UP );
	}
	else
		pPrimary->ShowWindow( UI_SW_HIDE );
	if ( pszSecondary != 0 )
	{
		const std::string szKey = std::string( "Textes\\UI\\CloudBackups\\" ) + pszSecondary;
		pSecondary->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide( TextOrFallback( szKey.c_str(), L"?" ).c_str() ) ) );
		pSecondary->EnableWindow( bSecondaryOn );
		pSecondary->ShowWindow( UI_SW_SHOW_DONT_MOVE_UP );
	}
	else
		pSecondary->ShowWindow( UI_SW_HIDE );
}
void CInterfaceCloudBackups::BeginRestore( int eMode )
{
	if ( nActionHandle >= 0 || nSelectedEntry < 0 || nSelectedEntry >= (int)entryIDs.size() )
		return;
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	bActionIsUndo = false;
	nActionHandle = NCloudSync::RestoreBackup( szProfile.c_str(), entryIDs[nSelectedEntry].c_str(),
		(NCloudSync::ERestoreMode)eMode );
	if ( nActionHandle < 0 )
	{
		NStr::DebugTrace( "cloud backups: restore begin failed: %s\n", NCloudSync::LastError() );
		SetStatus( "Textes\\UI\\CloudBackups\\unavailable", L"" );
		return;
	}
	NStr::DebugTrace( "cloud backups: restore staged download begun (%s)\n",
		eMode == NCloudSync::RESTORE_FULL ? "full" : "merge" );
	SetStatus( "Textes\\UI\\CloudBackups\\restoring", L"" );
	RefreshControls();
}
void CInterfaceCloudBackups::BeginUndo()
{
	if ( nActionHandle >= 0 )
		return;
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	eUndoWas = NCloudSync::UndoAvailability( szProfile.c_str() );
	if ( eUndoWas != NCloudSync::UNDO_CANCELLABLE && eUndoWas != NCloudSync::UNDO_REINSTATABLE )
		return;
	bActionIsUndo = true;
	nActionHandle = NCloudSync::UndoRestore( szProfile.c_str() );
	if ( nActionHandle < 0 )
	{
		NStr::DebugTrace( "cloud backups: undo begin failed: %s\n", NCloudSync::LastError() );
		SetStatus( "Textes\\UI\\CloudBackups\\unavailable", L"" );
		return;
	}
	NStr::DebugTrace( "cloud backups: undo begun (%s)\n",
		eUndoWas == NCloudSync::UNDO_CANCELLABLE ? "cancel pending" : "reinstate previous" );
	RefreshControls();
}
bool CInterfaceCloudBackups::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case UI_NOTIFY_EDIT_BOX_ESCAPE:
	case IMC_CANCEL:		// also the X button - its element id IS this event id
		// Inside the confirmation, cancel means "back to the list", not
		// "leave the screen" - a mis-aimed escape must not skip the
		// explicit step.
		if ( eBrowseState != BS_BROWSE )
		{
			eBrowseState = BS_BROWSE;
			RefreshControls();
			return true;
		}
		CloseInterface();
		return true;
	case E_LIST:
		{
			IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
			const int nRow = pList != 0 ? pList->GetSelectionItem() : -1;
			nSelectedEntry = nRow >= 0 ? pList->GetItem( nRow )->GetUserData() : -1;
			RefreshControls();
		}
		return true;
	case E_BUTTON_PRIMARY:
		if ( eBrowseState == BS_BROWSE )
		{
			if ( nSelectedEntry >= 0 && nSelectedEntry < (int)entryIDs.size() && nActionHandle < 0 )
			{
				eBrowseState = BS_CONFIRM;
				RefreshControls();
			}
		}
		else if ( eBrowseState == BS_CONFIRM )
			BeginRestore( NCloudSync::RESTORE_MERGE );
		else
			BeginRestore( NCloudSync::RESTORE_FULL );
		return true;
	case E_BUTTON_SECONDARY:
		if ( eBrowseState == BS_BROWSE )
			BeginUndo();
		else if ( eBrowseState == BS_CONFIRM )
		{
			eBrowseState = BS_CONFIRM_FULL;
			RefreshControls();
		}
		else
		{
			eBrowseState = BS_CONFIRM;
			RefreshControls();
		}
		return true;
	default:
		return false;
	}
}
bool CInterfaceCloudBackups::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	if ( nListHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nListHandle );
		if ( eState == NCloudSync::STATE_DONE )
		{
			NStr::DebugTrace( "cloud backups: listing done\n" );
			FillList();
			NCloudSync::Release( nListHandle );
			nListHandle = -1;
			RefreshControls();
		}
		else if ( eState == NCloudSync::STATE_FAILED )
		{
			const std::string szError = NCloudSync::Error( nListHandle );
			NStr::DebugTrace( "cloud backups: listing failed: %s\n", szError.c_str() );
			const std::string szKey = std::string( "Textes\\UI\\CloudSync\\" ) + CloudOutcomeTextKey( szError );
			SetStatus( szKey.c_str(), L"" );
			NCloudSync::Release( nListHandle );
			nListHandle = -1;
			RefreshControls();
		}
	}
	if ( nActionHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nActionHandle );
		if ( eState == NCloudSync::STATE_DONE )
		{
			if ( bActionIsUndo )
			{
				// The two undo answers finish as visibly different things:
				// a pending stage is simply gone, an applied restore is
				// staged back for the next launch.
				NStr::DebugTrace( "cloud backups: undo done\n" );
				SetStatus( eUndoWas == NCloudSync::UNDO_CANCELLABLE
					? "Textes\\UI\\CloudBackups\\undone_pending"
					: "Textes\\UI\\CloudBackups\\undone_applied", L"" );
			}
			else
			{
				NStr::DebugTrace( "cloud backups: restore staged\n" );
				SetStatus( "Textes\\UI\\CloudBackups\\staged", L"" );
			}
			NCloudSync::Release( nActionHandle );
			nActionHandle = -1;
			eBrowseState = BS_BROWSE;
			RefreshControls();
		}
		else if ( eState == NCloudSync::STATE_FAILED )
		{
			const std::string szError = NCloudSync::Error( nActionHandle );
			NStr::DebugTrace( "cloud backups: %s failed: %s\n", bActionIsUndo ? "undo" : "restore", szError.c_str() );
			const std::string szKey = std::string( "Textes\\UI\\CloudSync\\" ) + CloudOutcomeTextKey( szError );
			SetStatus( szKey.c_str(), L"" );
			NCloudSync::Release( nActionHandle );
			nActionHandle = -1;
			eBrowseState = BS_BROWSE;
			RefreshControls();
		}
	}
	return true;
}
bool CInterfaceCloudBackups::Init()
{
	CInterfaceScreenBase::Init();
	msgs.Init( pInput, commands );
	return true;
}
void CInterfaceCloudBackups::Done()
{
	if ( nListHandle >= 0 )
	{
		NCloudSync::Cancel( nListHandle );
		NCloudSync::Release( nListHandle );
		nListHandle = -1;
	}
	// A restore download deliberately keeps running - it stages a file and
	// COMMITs last, so an unobserved finish is safe; only the handle goes.
	if ( nActionHandle >= 0 )
	{
		NCloudSync::Release( nActionHandle );
		nActionHandle = -1;
	}
	CInterfaceScreenBase::Done();
}
void CInterfaceCloudBackups::StartInterface()
{
	nListHandle = -1;
	nActionHandle = -1;
	nSelectedEntry = -1;
	eBrowseState = BS_BROWSE;
	bActionIsUndo = false;
	eUndoWas = NCloudSync::UNDO_NONE;
	entryIDs.clear();
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\CloudBackups" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	pScene->AddUIScreen( pUIScreen );

	// Fetch on open; the poll in StepLocal consumes the result. The screen
	// never blocks - a player looking at the loading line can leave.
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	nListHandle = NCloudSync::ListBackups( szProfile.c_str() );
	if ( nListHandle >= 0 )
		SetStatus( "Textes\\UI\\CloudBackups\\loading", L"" );
	else
	{
		char pszError[512];
		std::snprintf( pszError, sizeof( pszError ), "%s", NCloudSync::LastError() );
		SetStatus( "Textes\\UI\\CloudBackups\\unavailable", WideFromNarrow( pszError ) );
	}
	RefreshControls();

	IInput *pInputSingleton = GetSingleton<IInput>();
	pInputSingleton->PumpMessages( true );
	SGameMessage msg;
	while ( pInputSingleton->GetMessage( &msg ) )
	{
	}
}
