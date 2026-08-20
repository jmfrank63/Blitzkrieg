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
	E_BUTTON_CANCEL							= 10001,
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
bool CInterfaceCloudBackups::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case UI_NOTIFY_EDIT_BOX_ESCAPE:
	case IMC_CANCEL:		// also the X button - its element id IS this event id
		CloseInterface();
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
		}
		else if ( eState == NCloudSync::STATE_FAILED )
		{
			const std::string szError = NCloudSync::Error( nListHandle );
			NStr::DebugTrace( "cloud backups: listing failed: %s\n", szError.c_str() );
			const std::string szKey = std::string( "Textes\\UI\\CloudSync\\" ) + CloudOutcomeTextKey( szError );
			SetStatus( szKey.c_str(), L"" );
			NCloudSync::Release( nListHandle );
			nListHandle = -1;
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
	CInterfaceScreenBase::Done();
}
void CInterfaceCloudBackups::StartInterface()
{
	nListHandle = -1;
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

	IInput *pInputSingleton = GetSingleton<IInput>();
	pInputSingleton->PumpMessages( true );
	SGameMessage msg;
	while ( pInputSingleton->GetMessage( &msg ) )
	{
	}
}
