#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceCloudCredentials.h"
#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "../UI/UIMessages.h"
#include "../Main/CloudSyncFacade.h"
#include "CloudJson.h"
#include "../Platform/System.h"
#include "../StreamIO/OptionSystem.h"

static const NInput::SRegisterCommandEntry commands[] =
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum
{
	// Seven label+edit row slots: a window over the form's field list, not
	// the field list itself. Slot ids are fixed; what a slot shows depends
	// on the scroll position.
	E_ROW_COUNT									= 7,
	E_EDIT_BASE									= 2001,		// edit id = E_EDIT_BASE + slot
	E_LABEL_BASE								= 3001,		// label id = E_LABEL_BASE + slot
	E_CYCLE_BASE								= 4001,		// example-cycle button per slot

	E_BUTTON_SCROLL_UP					= 4100,
	E_BUTTON_SCROLL_DOWN				= 4101,

	E_STATIC_DISCOVERY					= 3101,
	E_STATIC_STATUS							= 3102,

	E_BUTTON_BACKEND						= 10020,	// "Service: <id>" label; the retry while there is no catalogue
	E_BUTTON_TEST								= 10021,
	E_BUTTON_CLEAR_SECRET				= 10022,
	E_BUTTON_ADVANCED						= 10023,

	E_BUTTON_OK									= 10002,
	E_BUTTON_CANCEL							= 10001,
};
static const wchar_t MASK_CHAR = L'*';

// ---- small string helpers -------------------------------------------------
// The documents crossing the facade are UTF-8 JSON; edit boxes hold wide
// text. Values are endpoints, key ids and paths - ASCII in practice - but
// the conversion is done honestly so a non-ASCII path survives.
static std::string Utf8FromWide( const std::wstring &szWide )
{
	std::string szOut;
	for ( std::wstring::size_type i = 0; i < szWide.size(); ++i )
	{
		const unsigned int c = (unsigned int)szWide[i];
		if ( c < 0x80 )
			szOut += char( c );
		else if ( c < 0x800 )
		{
			szOut += char( 0xC0 | ( c >> 6 ) );
			szOut += char( 0x80 | ( c & 0x3F ) );
		}
		else
		{
			szOut += char( 0xE0 | ( c >> 12 ) );
			szOut += char( 0x80 | ( ( c >> 6 ) & 0x3F ) );
			szOut += char( 0x80 | ( c & 0x3F ) );
		}
	}
	return szOut;
}
static std::wstring WideFromUtf8( const std::string &szUtf8 )
{
	std::wstring szOut;
	for ( std::string::size_type i = 0; i < szUtf8.size(); )
	{
		const unsigned char c = szUtf8[i];
		if ( c < 0x80 )
		{
			szOut += wchar_t( c );
			i += 1;
		}
		else if ( ( c & 0xE0 ) == 0xC0 && i + 1 < szUtf8.size() )
		{
			szOut += wchar_t( ( ( c & 0x1F ) << 6 ) | ( szUtf8[i + 1] & 0x3F ) );
			i += 2;
		}
		else if ( ( c & 0xF0 ) == 0xE0 && i + 2 < szUtf8.size() )
		{
			szOut += wchar_t( ( ( c & 0x0F ) << 12 ) | ( ( szUtf8[i + 1] & 0x3F ) << 6 ) | ( szUtf8[i + 2] & 0x3F ) );
			i += 3;
		}
		else
			i += 1;			// malformed byte - drop it rather than loop forever
	}
	return szOut;
}
static std::string JsonEscape( const std::string &szRaw )
{
	std::string szOut;
	for ( std::string::size_type i = 0; i < szRaw.size(); ++i )
	{
		const unsigned char c = szRaw[i];
		if ( c == '"' )
			szOut += "\\\"";
		else if ( c == '\\' )
			szOut += "\\\\";
		else if ( c < 0x20 )
			szOut += NStr::Format( "\\u%04x", (int)c );
		else
			szOut += char( c );
	}
	return szOut;
}
// ---- interactive-config state ----------------------------------------------
// Tracked between polls while a config job runs. File statics rather than
// members - the packet that owns this flow may not touch the header - reset
// on StartInterface; one credentials dialog exists at a time.
static bool s_bConfigQuestion = false;	// a machine question owns the rows
static std::string s_szLastCard;				// each card handled once, URL opened once
// ---- misc ------------------------------------------------------------------
// Which text under textes\ui\cloudsync\ a classified failure maps to - the
// same mapping the main menu indicator uses, fed by the same leading-tag
// contract on failure texts.
static const char *CloudOutcomeTextKey( const std::string &szError )
{
	if ( szError == "Cancelled" )
		return "offline";
	static const char *pszOutcomes[] = { "needs_resync", "too_many_deletes", "name_too_long",
		"out_of_sync", "auth_failed", "remote_unreachable", "remote_missing",
		"remote_unwritable", "daemon_gone", "timed_out", 0 };
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
// The Provider row's value: the backend this dialog sets up. The row is the
// selector now; the dialog no longer offers one of its own. Empty when the
// row is Off (the button that opens this dialog is hidden then, so only a
// harness opening it directly arrives here) or holds a pre-row ON/OFF.
static std::string ProviderRowValue()
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( "Cloud.Provider", &var ) )
		return std::string();
	const std::string szValue( (const char*)bstr_t( var ) );
	if ( NStr::CompareAsciiNoCase( szValue.c_str(), "Off" ) == 0 || NStr::CompareAsciiNoCase( szValue.c_str(), "On" ) == 0 )
		return std::string();
	return szValue;
}
// ---- the dialog -----------------------------------------------------------
std::wstring CInterfaceCloudCredentials::GetEdit( int nID )
{
	if ( IUIElement *pElement = pUIScreen->GetChildByID( nID ) )
		return MakeWideStringFromWordString( pElement->GetWindowText( 0 ) );
	return L"";
}
void CInterfaceCloudCredentials::SetEdit( int nID, const std::wstring &szText )
{
	if ( IUIElement *pElement = pUIScreen->GetChildByID( nID ) )
		pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
}
void CInterfaceCloudCredentials::SetStatus( const char *pszTextKey, const std::wstring &szSuffix )
{
	std::wstring szLine;
	if ( pszTextKey != 0 )
		szLine = TextOrFallback( pszTextKey, L"" );
	if ( !szSuffix.empty() )
	{
		if ( !szLine.empty() )
			szLine += L" ";
		szLine += szSuffix;
	}
	SetEdit( E_STATIC_STATUS, szLine );
}
CInterfaceCloudCredentials::SField *CInterfaceCloudCredentials::FieldAtSlot( int nSlot )
{
	const int nIndex = nScroll + nSlot;
	if ( nIndex < 0 || nIndex >= (int)visibleRows.size() )
		return 0;
	return &fields[visibleRows[nIndex]];
}
CInterfaceCloudCredentials::SField *CInterfaceCloudCredentials::FieldNamed( const char *pszName )
{
	for ( size_t i = 0; i < fields.size(); ++i )
		if ( fields[i].nRole == 0 && fields[i].szName == pszName )
			return &fields[i];
	return 0;
}
void CInterfaceCloudCredentials::LoadStored()
{
	szStoredBackend.clear();
	szStoredRoot.clear();
	szStoredRclone.clear();
	storedOptions.clear();
	storedSecretNames.clear();
	bLoadFailed = false;

	// The redacted document withholds every secret value, so it stays small;
	// 64K is generous. A document that does not fit (or does not parse) with
	// credentials present must not be silently overwritten by a save built
	// from a blank form.
	char pszDoc[65536];
	if ( !NCloudSync::LoadCredentials( pszDoc, sizeof( pszDoc ) ) )
	{
		bLoadFailed = NCloudSync::CredentialsPresent();
		return;
	}
	SJsonValue doc;
	if ( !JsonParse( pszDoc, &doc ) || doc.eType != SJsonValue::T_OBJECT )
	{
		bLoadFailed = true;
		return;
	}
	szStoredBackend = doc.Str( "backend" );
	szStoredRoot = doc.Str( "remote_root" );
	szStoredRclone = doc.Str( "rclone_path" );
	if ( const SJsonValue *pOptions = doc.Get( "options" ) )
		for ( size_t i = 0; i < pOptions->keys.size(); ++i )
			if ( pOptions->children[i].eType == SJsonValue::T_STRING )
				storedOptions.push_back( std::make_pair( pOptions->keys[i], pOptions->children[i].szValue ) );
	if ( const SJsonValue *pSecrets = doc.Get( "secret_options" ) )
		for ( size_t i = 0; i < pSecrets->children.size(); ++i )
			if ( pSecrets->children[i].eType == SJsonValue::T_STRING )
				storedSecretNames.push_back( pSecrets->children[i].szValue );
}
void CInterfaceCloudCredentials::BeginCatalogue()
{
	if ( nCatalogueHandle >= 0 )
		return;
	const int nResult = NCloudSync::EnsureCatalogue();
	if ( nResult == NCloudSync::CATALOGUE_CACHED )
	{
		OnCatalogueReady();
		return;
	}
	if ( nResult >= 0 )
	{
		// The fetch can spawn a daemon; it runs on the worker and this
		// screen polls it like any other job.
		nCatalogueHandle = nResult;
		SetStatus( "Textes\\UI\\CloudCredentials\\fetching_catalogue", L"" );
		if ( IUIElement *pChooser = pUIScreen->GetChildByID( E_BUTTON_BACKEND ) )
			pChooser->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide(
				TextOrFallback( "Textes\\UI\\CloudCredentials\\fetching_catalogue", L"Fetching provider list..." ).c_str() ) ) );
		return;
	}
	ShowCatalogueMissing( WideFromUtf8( NCloudSync::LastError() ) );
}
void CInterfaceCloudCredentials::ShowCatalogueMissing( const std::wstring &szReason )
{
	// A fresh install is legitimately here until the first fetch succeeds:
	// say so, show why the fetch failed when it did, and make the chooser
	// button the retry. An empty form would look like a finished dialog.
	bCatalogueReady = false;
	fields.clear();
	visibleRows.clear();
	nScroll = 0;
	LayoutRows();
	SetStatus( "Textes\\UI\\CloudCredentials\\catalogue_missing", szReason );
	if ( IUIElement *pChooser = pUIScreen->GetChildByID( E_BUTTON_BACKEND ) )
	{
		pChooser->EnableWindow( true );			// the retry
		pChooser->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide(
			TextOrFallback( "Textes\\UI\\CloudCredentials\\catalogue_retry", L"Fetch provider list" ).c_str() ) ) );
	}
}
void CInterfaceCloudCredentials::OnCatalogueReady()
{
	// The chooser offers the filtered destination list; the stored backend
	// rides along even when the filter or a newer rclone would drop it.
	destinations.clear();
	std::string szListDoc;
	{
		const char *pszConfigured = szStoredBackend.c_str();
		if ( !ReadSizedDocument( [pszConfigured]( char *psz, unsigned int nCap ) { return NCloudSync::CatalogueDestinations( pszConfigured, psz, nCap ); }, &szListDoc ) )
		{
			ShowCatalogueMissing( WideFromUtf8( NCloudSync::LastError() ) );
			return;
		}
	}
	SJsonValue list;
	if ( !JsonParse( szListDoc, &list ) )
	{
		ShowCatalogueMissing( L"" );
		return;
	}
	if ( const SJsonValue *pNames = list.Get( "destinations" ) )
		for ( size_t i = 0; i < pNames->children.size(); ++i )
			if ( pNames->children[i].eType == SJsonValue::T_STRING )
				destinations.push_back( pNames->children[i].szValue );
	if ( destinations.empty() )
	{
		ShowCatalogueMissing( L"" );
		return;
	}
	bCatalogueReady = true;
	// The backend is the Provider row's, fixed at open; the destination list
	// is kept only so RebuildForm can tell a backend this catalogue lacks
	// (the empty, cannot-save form) from a missing catalogue.
	if ( szBackend.empty() )
		szBackend = szStoredBackend.empty() ? destinations[0] : szStoredBackend;
	SetStatus( 0, L"" );
	RebuildForm( false );
}
void CInterfaceCloudCredentials::RebuildForm( bool bPreserveTyped )
{
	// What the player already typed, keyed by name (roles keep synthetic
	// keys so the root and the override survive a vendor rebuild too).
	std::vector<SField> old;
	if ( bPreserveTyped )
		old.swap( fields );
	fields.clear();

	// The selected provider is just the current value of the option named
	// "provider" - rclone's own convention, the same one the save path's
	// vendor cleanup keys on. The form is re-derived under it; nothing
	// typed crosses the ABI.
	std::string szProvider;
	for ( size_t i = 0; i < old.size(); ++i )
		if ( old[i].nRole == 0 && old[i].szName == "provider" )
			szProvider = Utf8FromWide( old[i].szValue );
	if ( !bPreserveTyped && szBackend == szStoredBackend )
		for ( size_t i = 0; i < storedOptions.size(); ++i )
			if ( storedOptions[i].first == "provider" )
				szProvider = storedOptions[i].second;

	std::string szFormDoc;
	{
		const char *pszBackend = szBackend.c_str();
		const char *pszProvider = szProvider.c_str();
		if ( !ReadSizedDocument( [pszBackend, pszProvider]( char *psz, unsigned int nCap ) { return NCloudSync::CatalogueForm( pszBackend, pszProvider, psz, nCap ); }, &szFormDoc ) )
		{
			// A populated catalogue that simply lacks this backend - the
			// stored one, after the bundled rclone was swapped for an older
			// build - must not collapse into the missing-catalogue state:
			// there the retry button re-derives this same form, and the
			// player is trapped with no way to step to a backend that
			// exists. Show an empty row set instead of that dead end; an
			// empty form refuses to save by the existing blank-form rule,
			// so the stored document cannot be overwritten from here. This
			// dialog no longer offers its own chooser - the way out is the
			// Provider row on the Cloud tab, one level up.
			if ( bCatalogueReady && !destinations.empty() )
			{
				fields.clear();
				visibleRows.clear();
				nScroll = 0;
				LayoutRows();
				SetStatus( 0, WideFromUtf8( NCloudSync::LastError() ) );
				return;
			}
			ShowCatalogueMissing( WideFromUtf8( NCloudSync::LastError() ) );
			return;
		}
	}
	SJsonValue form;
	if ( !JsonParse( szFormDoc, &form ) )
	{
		ShowCatalogueMissing( L"" );
		return;
	}
	const char *pszSections[2] = { "basic", "advanced" };
	for ( int nSection = 0; nSection < 2; ++nSection )
	{
		const SJsonValue *pSection = form.Get( pszSections[nSection] );
		if ( pSection == 0 )
			continue;
		for ( size_t i = 0; i < pSection->children.size(); ++i )
		{
			const SJsonValue &item = pSection->children[i];
			SField field;
			field.szName = item.Str( "name" );
			field.nRole = ( item.Str( "role" ) == "remote_root" ) ? 1 : 0;
			field.szLabel = WideFromUtf8( item.Str( "label" ) );
			field.szHelp = WideFromUtf8( item.Str( "help" ) );
			field.szWidget = item.Str( "widget" );
			field.bRequired = item.Bool( "required" );
			field.bAdvanced = ( nSection == 1 );
			field.bIsPassword = item.Bool( "is_password" );
			field.szPlaceholder = item.Str( "placeholder" );
			if ( const SJsonValue *pExamples = item.Get( "examples" ) )
				for ( size_t nExample = 0; nExample < pExamples->children.size(); ++nExample )
				{
					field.exampleValues.push_back( pExamples->children[nExample].Str( "value" ) );
					field.exampleHelp.push_back( WideFromUtf8( pExamples->children[nExample].Str( "help" ) ) );
				}
			if ( field.nRole == 1 )
				field.szLabel = TextOrFallback( "Textes\\UI\\CloudCredentials\\label_remote_root", field.szLabel.c_str() );
			fields.push_back( field );
		}
	}
	// The rclone override is ours, not the catalogue's: a local path, kept
	// even across a backend switch - it names a binary, not a credential.
	{
		SField field;
		field.nRole = 2;
		field.szLabel = TextOrFallback( "Textes\\UI\\CloudCredentials\\label_rclone", L"rclone path" );
		field.szWidget = "text";
		field.szValue = WideFromUtf8( szStoredRclone );
		fields.push_back( field );
	}

	// Values: preserved-typed on a rebuild, stored on a fresh build of the
	// stored backend, empty otherwise. A field surviving a rebuild does not
	// mean its value did - a closed field keeps a value only while it is
	// still among the newly filtered examples; preserving on existence
	// alone would resubmit a value the new vendor never offers.
	for ( size_t i = 0; i < fields.size(); ++i )
	{
		SField &field = fields[i];
		if ( bPreserveTyped )
		{
			for ( size_t nOld = 0; nOld < old.size(); ++nOld )
			{
				const SField &previous = old[nOld];
				if ( previous.nRole != field.nRole )
					continue;
				if ( field.nRole == 0 && previous.szName != field.szName )
					continue;
				bool bKeep = ( field.szWidget != "droplist_closed" );
				if ( !bKeep )
					for ( size_t nExample = 0; nExample < field.exampleValues.size(); ++nExample )
						if ( WideFromUtf8( field.exampleValues[nExample] ) == previous.szValue )
						{
							bKeep = true;
							break;
						}
				if ( bKeep )
				{
					field.szValue = previous.szValue;
					field.bTouched = previous.bTouched;
					field.bStoredSecret = previous.bStoredSecret;
				}
				else
				{
					// The new vendor does not offer what was here: a
					// deliberate drop, not an unfilled box. Left untouched,
					// SaveCredentials()'s same-backend fallback would read
					// this blank as never having reached the field and
					// resurrect the dropped value from storedOptions -
					// exactly the resubmit the comment above rules out.
					field.bTouched = true;
				}
				break;
			}
		}
		else if ( szBackend == szStoredBackend )
		{
			if ( field.nRole == 1 )
				field.szValue = WideFromUtf8( szStoredRoot );
			else if ( field.nRole == 0 )
			{
				for ( size_t nStored = 0; nStored < storedOptions.size(); ++nStored )
					if ( storedOptions[nStored].first == field.szName )
						field.szValue = WideFromUtf8( storedOptions[nStored].second );
				for ( size_t nSecret = 0; nSecret < storedSecretNames.size(); ++nSecret )
					if ( storedSecretNames[nSecret] == field.szName )
						field.bStoredSecret = true;
			}
		}
	}

	nScroll = 0;
	LayoutRows();
}
void CInterfaceCloudCredentials::LayoutRows()
{
	visibleRows.clear();
	int nAdvancedCount = 0;
	for ( size_t i = 0; i < fields.size(); ++i )
	{
		if ( fields[i].bAdvanced )
			++nAdvancedCount;
		if ( !fields[i].bAdvanced || bShowAdvanced )
			visibleRows.push_back( (int)i );
	}
	const int nMaxScroll = (int)visibleRows.size() > E_ROW_COUNT ? (int)visibleRows.size() - E_ROW_COUNT : 0;
	if ( nScroll > nMaxScroll )
		nScroll = nMaxScroll;
	if ( nScroll < 0 )
		nScroll = 0;

	for ( int nSlot = 0; nSlot < E_ROW_COUNT; ++nSlot )
	{
		IUIElement *pLabel = pUIScreen->GetChildByID( E_LABEL_BASE + nSlot );
		IUIElement *pEdit = pUIScreen->GetChildByID( E_EDIT_BASE + nSlot );
		IUIElement *pCycle = pUIScreen->GetChildByID( E_CYCLE_BASE + nSlot );
		const SField *pField = FieldAtSlot( nSlot );
		const bool bVisible = ( pField != 0 );
		if ( pLabel != 0 )
		{
			if ( pField != 0 )
			{
				std::wstring szLabel = pField->szLabel;
				if ( pField->bRequired )
					szLabel += L" *";
				pLabel->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szLabel.c_str() ) ) );
			}
			pLabel->ShowWindow( bVisible ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
		}
		if ( pEdit != 0 )
		{
			if ( pField != 0 )
			{
				std::wstring szShown = pField->szValue;
				if ( pField->IsMasked() )
					szShown = std::wstring( pField->bStoredSecret && !pField->bTouched ? 8 : pField->szValue.size(), MASK_CHAR );
				SetEdit( E_EDIT_BASE + nSlot, szShown );
			}
			pEdit->ShowWindow( bVisible ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
		}
		if ( pCycle != 0 )
		{
			const bool bCycle = pField != 0 && !pField->exampleValues.empty() && !pField->IsMasked();
			pCycle->ShowWindow( bCycle ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
		}
	}

	if ( IUIElement *pUp = pUIScreen->GetChildByID( E_BUTTON_SCROLL_UP ) )
		pUp->ShowWindow( nMaxScroll > 0 ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
	if ( IUIElement *pDown = pUIScreen->GetChildByID( E_BUTTON_SCROLL_DOWN ) )
		pDown->ShowWindow( nMaxScroll > 0 ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );

	if ( IUIElement *pAdvanced = pUIScreen->GetChildByID( E_BUTTON_ADVANCED ) )
	{
		if ( nAdvancedCount == 0 || !bCatalogueReady )
			pAdvanced->ShowWindow( UI_SW_HIDE );
		else
		{
			const std::wstring szBase = TextOrFallback(
				bShowAdvanced ? "Textes\\UI\\CloudCredentials\\advanced_shown" : "Textes\\UI\\CloudCredentials\\advanced_hidden",
				bShowAdvanced ? L"Advanced: shown" : L"Advanced: hidden" );
			const std::wstring szText = szBase + L" (" + WideFromUtf8( NStr::Format( "%d", nAdvancedCount ) ) + L")";
			pAdvanced->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
			pAdvanced->ShowWindow( UI_SW_SHOW_DONT_MOVE_UP );
		}
	}
	if ( bCatalogueReady )
	{
		if ( IUIElement *pChooser = pUIScreen->GetChildByID( E_BUTTON_BACKEND ) )
		{
			const std::wstring szText = TextOrFallback( "Textes\\UI\\CloudCredentials\\label_service", L"Service:" ) +
				L" " + WideFromUtf8( szBackend );
			pChooser->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
			pChooser->EnableWindow( false );		// a label now: the row chose
		}
	}
}
void CInterfaceCloudCredentials::RefreshDiscoveryLine()
{
	char pszDoc[2048];
	pszDoc[0] = 0;
	const char *pszStatus = NCloudSync::DiscoveryStatus();
	strncpy( pszDoc, pszStatus != 0 ? pszStatus : "", sizeof( pszDoc ) - 1 );
	pszDoc[sizeof( pszDoc ) - 1] = 0;
	SJsonValue doc;
	std::wstring szLine;
	if ( JsonParse( std::string( pszDoc ), &doc ) && doc.Bool( "found" ) )
	{
		szLine = TextOrFallback( "Textes\\UI\\CloudCredentials\\discovery_found", L"rclone" );
		if ( !doc.Str( "version" ).empty() )
			szLine += L" " + WideFromUtf8( doc.Str( "version" ) );
		if ( !doc.Str( "path" ).empty() )
			szLine += L" - " + WideFromUtf8( doc.Str( "path" ) );
	}
	else
	{
		const std::string szReason = doc.Str( "reason" );
		const std::string szKey = std::string( "Textes\\UI\\CloudCredentials\\discovery_" ) +
			( szReason.empty() ? "not_found" : szReason );
		szLine = TextOrFallback( szKey.c_str(), L"rclone was not found" );
		if ( !doc.Str( "path" ).empty() )
			szLine += L" - " + WideFromUtf8( doc.Str( "path" ) );
	}
	SetEdit( E_STATIC_DISCOVERY, szLine );
}
void CInterfaceCloudCredentials::OnSecretEdited( SField *pField, int nEditID )
{
	// The box shows only mask characters; the real value lives in the field.
	// Leading and trailing runs of the mask character map onto the kept
	// prefix and suffix of the real secret; whatever sits between them was
	// just typed. The first edit of an untouched stored secret starts from
	// empty - the placeholder masks stand for a value this dialog never had.
	const std::wstring szShown = GetEdit( nEditID );
	std::wstring szReal = pField->bTouched || !pField->bStoredSecret ? pField->szValue : L"";
	std::wstring::size_type nLead = 0;
	while ( nLead < szShown.size() && szShown[nLead] == MASK_CHAR )
		++nLead;
	std::wstring::size_type nTrail = 0;
	while ( nTrail < szShown.size() - nLead && szShown[szShown.size() - 1 - nTrail] == MASK_CHAR )
		++nTrail;
	if ( nLead > szReal.size() )
		nLead = szReal.size();
	if ( nTrail > szReal.size() - nLead )
		nTrail = szReal.size() - nLead;
	const std::wstring szMiddle = szShown.substr( nLead, szShown.size() - nLead - nTrail );
	pField->szValue = szReal.substr( 0, nLead ) + szMiddle + szReal.substr( szReal.size() - nTrail );
	pField->bTouched = true;
	SetEdit( nEditID, std::wstring( pField->szValue.size(), MASK_CHAR ) );
	if ( IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( nEditID ) ) )
		pEdit->SetCursor( (int)( nLead + szMiddle.size() ) );
}
void CInterfaceCloudCredentials::OnRowEdited( int nSlot )
{
	SField *pField = FieldAtSlot( nSlot );
	if ( pField == 0 )
		return;
	if ( pField->IsMasked() )
	{
		OnSecretEdited( pField, E_EDIT_BASE + nSlot );
		return;
	}
	const std::wstring szValue = GetEdit( E_EDIT_BASE + nSlot );
	if ( szValue == pField->szValue )
		return;
	pField->szValue = szValue;
	pField->bTouched = true;
	// Selecting a vendor is not an ordinary edit: it changes which fields
	// exist, so the form is re-derived - preserving still-applicable typed
	// values, never a value the new vendor does not offer. The rebuild
	// rewrites every visible box, which resets the caret - and this fires
	// per keystroke, so without restoring it each following character lands
	// at the front ("Minio" arrives as "oinM"). The provider row survives
	// its own rebuild at the same slot (row 0, scroll reset), so the caret
	// goes back where the keystroke left it, the way OnSecretEdited already
	// does for the masked boxes.
	if ( pField->nRole == 0 && pField->szName == "provider" )
	{
		int nCursor = (int)szValue.size();
		if ( IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( E_EDIT_BASE + nSlot ) ) )
			nCursor = pEdit->GetCursor();
		RebuildForm( true );
		if ( IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( E_EDIT_BASE + nSlot ) ) )
			pEdit->SetCursor( nCursor );
	}
}
void CInterfaceCloudCredentials::CycleExample( int nSlot )
{
	SField *pField = FieldAtSlot( nSlot );
	if ( pField == 0 || pField->exampleValues.empty() )
		return;
	size_t nNext = 0;
	for ( size_t i = 0; i < pField->exampleValues.size(); ++i )
		if ( WideFromUtf8( pField->exampleValues[i] ) == pField->szValue )
		{
			nNext = ( i + 1 ) % pField->exampleValues.size();
			break;
		}
	pField->szValue = WideFromUtf8( pField->exampleValues[nNext] );
	pField->bTouched = true;
	SetEdit( E_EDIT_BASE + nSlot, pField->szValue );
	// The example's help is the only per-value documentation the catalogue
	// carries; the status line is where this dialog shows it.
	SetStatus( 0, pField->exampleHelp[nNext] );
	if ( pField->nRole == 0 && pField->szName == "provider" )
		RebuildForm( true );
}
bool CInterfaceCloudCredentials::SaveCredentials()
{
	if ( bLoadFailed )
	{
		// Credentials exist but could not be read; a save built from this
		// form would overwrite them with blanks.
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed",
			TextOrFallback( "Textes\\UI\\CloudCredentials\\creds_unreadable", L"the stored credentials could not be read" ) );
		return false;
	}
	if ( !bCatalogueReady || fields.empty() )
	{
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed",
			TextOrFallback( "Textes\\UI\\CloudCredentials\\catalogue_missing", L"no provider list yet" ) );
		return false;
	}
	// Whatever this backend already has on record, keyed by option name -
	// the fallback an untouched, blank non-secret box defers to below, the
	// same protection a blank masked box already has through bStoredSecret.
	// A box the player actually typed or cycled through (bTouched) always
	// wins, including a deliberate blank: this only covers a box that
	// never carried anything this session, on the very backend already
	// saved - never a switch to a different one, which is meant to start
	// clean.
	const bool bSameBackend = ( szBackend == szStoredBackend );
	auto StoredOptionValue = [this]( const std::string &szName ) -> std::string
	{
		for ( size_t i = 0; i < storedOptions.size(); ++i )
			if ( storedOptions[i].first == szName )
				return storedOptions[i].second;
		return std::string();
	};
	// An untouched form on the backend already saved has nothing to say: the
	// document on disk is the authority, this form at best mirrors it, and
	// writing from it could only lose information - a view that came up
	// blank is exactly how the document got wiped. So succeed without
	// writing: the caller's next step reads that document anyway (the
	// connection probe, or closing the dialog), and nothing is lost either
	// way. An untouched cross-backend save still writes - that is the
	// player's deliberate switch, and first-time OAuth setup needs the
	// {backend, options:{}} document ConfigBegin() works from.
	if ( bSameBackend && NCloudSync::CredentialsPresent() )
	{
		bool bAnyTouched = false;
		for ( size_t i = 0; i < fields.size() && !bAnyTouched; ++i )
			if ( fields[i].bTouched )
				bAnyTouched = true;
		if ( !bAnyTouched )
			return true;		// saved is where it already was
	}
	// Required means must-fill: the model already folded catalogue defaults
	// and other vendors' requirements out of bRequired, so blank is simply
	// blank - and a masked field with a stored secret is not blank, and
	// neither is an untouched non-secret field the record still has a
	// value for. Named by its label, before any network call and before
	// the document is built.
	for ( size_t i = 0; i < fields.size(); ++i )
	{
		const SField &field = fields[i];
		if ( !field.bRequired || !field.szValue.empty() )
			continue;
		if ( field.IsMasked() && field.bStoredSecret )
			continue;
		if ( !field.IsMasked() && !field.bTouched && bSameBackend && !StoredOptionValue( field.szName ).empty() )
			continue;
		SetStatus( "Textes\\UI\\CloudCredentials\\required_missing", field.szLabel );
		return false;
	}

	std::string szOptions;
	std::string szSecretNames;
	std::string szPasswordNames;
	std::string szRoot;
	std::string szRclone;
	for ( size_t i = 0; i < fields.size(); ++i )
	{
		const SField &field = fields[i];
		const std::string szValue = Utf8FromWide( field.szValue );
		if ( field.nRole == 1 )
		{
			szRoot = szValue;
			if ( szRoot.empty() && !field.bTouched && bSameBackend )
				szRoot = szStoredRoot;
			continue;
		}
		if ( field.nRole == 2 )
		{
			szRclone = szValue;
			continue;
		}
		if ( field.IsMasked() )
		{
			// A typed secret is sent; an untouched stored one is named so
			// the merge preserves it; empty-and-nothing-stored is nothing.
			// Clearing is the dedicated button, never a side effect here.
			const bool bSend = field.bTouched && !szValue.empty();
			if ( !bSend && !field.bStoredSecret )
				continue;
			if ( bSend )
			{
				if ( !szOptions.empty() )
					szOptions += ",";
				szOptions += "\"" + JsonEscape( field.szName ) + "\":\"" + JsonEscape( szValue ) + "\"";
			}
			if ( !szSecretNames.empty() )
				szSecretNames += ",";
			szSecretNames += "\"" + JsonEscape( field.szName ) + "\"";
			if ( field.bIsPassword )
			{
				if ( !szPasswordNames.empty() )
					szPasswordNames += ",";
				szPasswordNames += "\"" + JsonEscape( field.szName ) + "\"";
			}
			continue;
		}
		// Only what the player set: an empty value is unset, and a value
		// equal to the catalogue default must follow upstream rather than
		// being pinned in the credentials file - unless the box is blank
		// only because this backend's own prefill never reached it
		// (untouched, same backend as stored), in which case the value
		// already on record carries forward.
		std::string szSend = szValue;
		if ( szSend.empty() && !field.bTouched && bSameBackend )
			szSend = StoredOptionValue( field.szName );
		if ( szSend.empty() || szSend == field.szPlaceholder )
			continue;
		if ( !szOptions.empty() )
			szOptions += ",";
		szOptions += "\"" + JsonEscape( field.szName ) + "\":\"" + JsonEscape( szSend ) + "\"";
	}

	std::string szJson = "{\"backend\":\"" + JsonEscape( szBackend ) + "\",";
	szJson += "\"remote_root\":\"" + JsonEscape( szRoot ) + "\",";
	szJson += "\"options\":{" + szOptions + "},";
	szJson += "\"secret_options\":[" + szSecretNames + "],";
	szJson += "\"password_options\":[" + szPasswordNames + "],";
	if ( szRclone.empty() )
		szJson += "\"rclone_path\":null}";
	else
		szJson += "\"rclone_path\":\"" + JsonEscape( szRclone ) + "\"}";

	// Never through the option system: values here are longer than the
	// option store carries, and the secret must not exist there at all.
	if ( !NCloudSync::SaveCredentials( szJson.c_str() ) )
	{
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
		return false;
	}
	// Typed secrets are now the stored ones; their boxes go back to meaning
	// "keep what is stored". The stored snapshot follows the save so a later
	// backend switch and return prefills what was just written.
	for ( size_t i = 0; i < fields.size(); ++i )
	{
		SField &field = fields[i];
		if ( field.IsMasked() && field.bTouched && !field.szValue.empty() )
			field.bStoredSecret = true;
		if ( field.IsMasked() )
		{
			field.bTouched = false;
			field.szValue.clear();
		}
	}
	LoadStored();
	LayoutRows();
	SetStatus( "Textes\\UI\\CloudCredentials\\saved", L"" );
	// creds_save invalidated the discovery cache; show what the saved path
	// resolves to right now rather than after the next failed sync.
	RefreshDiscoveryLine();
	return true;
}
void CInterfaceCloudCredentials::BeginConnectionTest()
{
	if ( nTestHandle >= 0 )
		return;			// one probe at a time; the running one reports soon
	// The probe reads the saved document, so what is typed must be saved
	// first - which is also what re-runs discovery on a changed rclone path.
	if ( !SaveCredentials() )
		return;
	// The interactive config machine, not a bare probe: for most backends
	// it asks nothing and ends in the same connection test, and for an
	// OAuth backend it asks its questions here - rendered by these rows -
	// and sends the player through the browser when consent is needed.
	nTestHandle = NCloudSync::ConfigBegin();
	if ( nTestHandle < 0 )
	{
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
		return;
	}
	SetStatus( "Textes\\UI\\CloudCredentials\\testing", L"" );
}
bool CInterfaceCloudCredentials::ProcessMessage( const SGameMessage &msg )
{
	// While the config machine's question owns the rows, the form-shaping
	// buttons are inert - a backend switch or an advanced toggle would
	// rebuild the form over the question. OK answers it, Cancel leaves.
	if ( s_bConfigQuestion &&
			( msg.nEventID == E_BUTTON_BACKEND || msg.nEventID == E_BUTTON_ADVANCED ||
				msg.nEventID == E_BUTTON_TEST || msg.nEventID == E_BUTTON_CLEAR_SECRET ) )
		return true;
	switch ( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
		break;
	case UI_NOTIFY_EDIT_BOX_TEXT_CHANGED:
		if ( !bFinished && msg.nParam >= E_EDIT_BASE && msg.nParam < E_EDIT_BASE + E_ROW_COUNT )
			OnRowEdited( msg.nParam - E_EDIT_BASE );
		return true;
	case UI_NOTIFY_EDIT_BOX_ESCAPE:
	case IMC_CANCEL:
		if ( !bFinished )
		{
			bFinished = true;
			CloseInterface();
		}
		return true;
	case UI_NOTIFY_EDIT_BOX_RETURN:
	case IMC_OK:
		if ( !bFinished )
		{
			if ( s_bConfigQuestion )
			{
				// OK submits the machine's answer; the flow continues and
				// StepLocal renders whatever comes next.
				if ( !fields.empty() && nTestHandle >= 0 &&
						NCloudSync::ConfigAnswer( nTestHandle, Utf8FromWide( fields[0].szValue ).c_str() ) == 0 )
				{
					s_bConfigQuestion = false;
					SetStatus( "Textes\\UI\\CloudCredentials\\testing", L"" );
				}
				else
					SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
				return true;
			}
			if ( SaveCredentials() )
			{
				bFinished = true;
				CloseInterface();
			}
		}
		return true;
	case E_BUTTON_BACKEND:
		if ( !bCatalogueReady )
		{
			BeginCatalogue();		// the chooser doubles as the retry
			return true;
		}
		return true;
	case E_BUTTON_ADVANCED:
		bShowAdvanced = !bShowAdvanced;
		LayoutRows();
		return true;
	case E_BUTTON_SCROLL_UP:
		--nScroll;
		LayoutRows();
		return true;
	case E_BUTTON_SCROLL_DOWN:
		++nScroll;
		LayoutRows();
		return true;
	case E_BUTTON_TEST:
		BeginConnectionTest();
		return true;
	case E_BUTTON_CLEAR_SECRET:
		// The deliberate act, distinct from saving with an empty box - an
		// empty box preserves the stored secrets. Clears every withheld
		// field of the stored credentials at once.
		if ( NCloudSync::ClearCredentialsSecret() )
		{
			LoadStored();
			for ( size_t i = 0; i < fields.size(); ++i )
				if ( fields[i].IsMasked() )
				{
					fields[i].bStoredSecret = false;
					fields[i].bTouched = false;
					fields[i].szValue.clear();
				}
			LayoutRows();
			SetStatus( "Textes\\UI\\CloudCredentials\\secret_cleared", L"" );
		}
		else
			SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
		return true;
	default:
		if ( !bFinished && msg.nEventID >= E_CYCLE_BASE && msg.nEventID < E_CYCLE_BASE + E_ROW_COUNT )
		{
			CycleExample( msg.nEventID - E_CYCLE_BASE );
			return true;
		}
		return false;
	}
	return true;
}
bool CInterfaceCloudCredentials::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	// The wheel scrolls the row window wherever it is turned; the row area
	// has no list control of its own, so there is no double delivery.
	if ( bAppActive && pWheelScroll != 0 && !bFinished )
	{
		const float fDelta = pWheelScroll->GetDelta();
		if ( fDelta != 0.0f )
		{
			nScroll += fDelta > 0.0f ? -1 : 1;
			LayoutRows();
		}
	}
	// Both jobs are observed, never awaited: Poll is a mutex and a struct
	// copy. The catalogue fetch feeds the form; the connection test feeds
	// the status line through the same classified-outcome mapping the menu
	// indicator uses.
	if ( nCatalogueHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nCatalogueHandle );
		if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
		{
			const bool bReady = ( eState == NCloudSync::STATE_DONE );
			std::wstring szReason;
			if ( !bReady )
				szReason = WideFromUtf8( NCloudSync::Error( nCatalogueHandle ) );
			NCloudSync::Release( nCatalogueHandle );
			nCatalogueHandle = -1;
			if ( bReady )
				OnCatalogueReady();
			else
				ShowCatalogueMissing( szReason );
		}
	}
	if ( nTestHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nTestHandle );
		if ( eState == NCloudSync::STATE_AWAITING_INPUT )
		{
			const int nHandle = nTestHandle;
			std::string szCard;
			if ( ReadSizedDocument( [nHandle]( char *pszOut, unsigned int nCap )
					{ return NCloudSync::ConfigQuestion( nHandle, pszOut, nCap ); }, &szCard )
					&& !szCard.empty() && szCard != s_szLastCard )
			{
				s_szLastCard = szCard;
				SJsonValue card;
				if ( JsonParse( szCard, &card ) )
				{
					if ( card.Str( "role" ) == "consent" )
					{
						// The consent URL carries a state secret: it goes to
						// the platform browser and nowhere else - above all
						// not into a trace. The visible waiting state matters
						// on macOS, where the game runs in its own Space and
						// the player comes back to what would otherwise look
						// like a hang.
						NPlatform::OpenUrl( card.Str( "url" ).c_str() );
						SetStatus( 0, TextOrFallback( "Textes\\UI\\CloudCredentials\\consent_waiting",
							L"Finish signing in in your browser, then come back - waiting here." ) );
					}
					else
					{
						// A machine question takes over the rows: one field,
						// rendered by the same code as any form field. OK
						// submits it; the real form returns when the flow
						// settles.
						SField field;
						field.szName = card.Str( "name" );
						field.nRole = 0;
						field.szLabel = WideFromUtf8( card.Str( "label" ) );
						field.szHelp = WideFromUtf8( card.Str( "help" ) );
						field.szWidget = card.Str( "widget" );
						field.bRequired = card.Bool( "required" );
						field.bIsPassword = card.Bool( "is_password" );
						field.szPlaceholder = card.Str( "placeholder" );
						if ( const SJsonValue *pExamples = card.Get( "examples" ) )
							for ( size_t i = 0; i < pExamples->children.size(); ++i )
							{
								field.exampleValues.push_back( pExamples->children[i].Str( "value" ) );
								field.exampleHelp.push_back( WideFromUtf8( pExamples->children[i].Str( "help" ) ) );
							}
						field.szValue = WideFromUtf8( card.Str( "placeholder" ) );
						fields.clear();
						fields.push_back( field );
						bShowAdvanced = false;
						nScroll = 0;
						s_bConfigQuestion = true;
						LayoutRows();
						const std::string szWhy = card.Str( "error" );
						if ( !szWhy.empty() )
							SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( szWhy ) );
						else
							SetStatus( 0, field.szHelp );
					}
				}
			}
		}
		if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
		{
			// However the flow ended, the machine's takeover ends with it:
			// the real form comes back, prefilled from what was saved. The
			// flow that just settled is itself a writer of the stored
			// document, not only a reader of it - an OAuth backend's worker
			// reads the token back and writes it mid-flow - so the snapshot
			// LoadStored() last took predates what settled onto disk just
			// now. Re-read before the rebuild: skip this and the rebuilt
			// token field carries bStoredSecret == false, so a player who
			// touches any other field (the folder, say) and presses OK takes
			// the touched save path, whose masked-field branch omits an
			// untouched field with no stored flag - the token this flow just
			// acquired is silently dropped and the next sync fails auth.
			if ( s_bConfigQuestion || !s_szLastCard.empty() )
			{
				s_bConfigQuestion = false;
				s_szLastCard.clear();
				LoadStored();
				RebuildForm( false );
			}
			if ( eState == NCloudSync::STATE_DONE )
			{
				NStr::DebugTrace( "cloud credentials: connection test ok\n" );
				SetStatus( "Textes\\UI\\CloudCredentials\\test_ok", L"" );
			}
			else
			{
				const std::string szError = NCloudSync::Error( nTestHandle );
				NStr::DebugTrace( "cloud credentials: connection test failed: %s\n", szError.c_str() );
				const char *pszOutcome = CloudOutcomeTextKey( szError );
				const std::string szKey = std::string( "Textes\\UI\\CloudSync\\" ) + pszOutcome;
				// The unwritable verdict can name a probe file the service
				// refused to delete; that detail asks the player to act, so
				// it rides the status line after the mapped text.
				std::wstring szDetail;
				if ( strcmp( pszOutcome, "remote_unwritable" ) == 0 )
				{
					const size_t nColon = szError.find( ": " );
					if ( nColon != std::string::npos )
						szDetail = WideFromUtf8( szError.substr( nColon + 2 ) );
				}
				SetStatus( szKey.c_str(), szDetail );
			}
			NCloudSync::Release( nTestHandle );
			nTestHandle = -1;
		}
	}
	return true;
}
void CInterfaceCloudCredentials::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );
	if ( bFocus )
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
}
bool CInterfaceCloudCredentials::Init()
{
	CInterfaceScreenBase::Init();
	msgs.Init( pInput, commands );
	pWheelScroll = pInput->CreateSlider( "mouse_wheel" );
	return true;
}
void CInterfaceCloudCredentials::Done()
{
	// A fetch the player is no longer watching is abandoned, not awaited.
	if ( nCatalogueHandle >= 0 )
	{
		NCloudSync::Cancel( nCatalogueHandle );
		NCloudSync::Release( nCatalogueHandle );
		nCatalogueHandle = -1;
	}
	if ( nTestHandle >= 0 )
	{
		NCloudSync::Cancel( nTestHandle );
		NCloudSync::Release( nTestHandle );
		nTestHandle = -1;
	}
	CInterfaceScreenBase::Done();
}
void CInterfaceCloudCredentials::StartInterface()
{
	bFinished = false;
	nTestHandle = -1;
	nCatalogueHandle = -1;
	bCatalogueReady = false;
	bShowAdvanced = false;
	nScroll = 0;
	fields.clear();
	visibleRows.clear();
	destinations.clear();
	szBackend.clear();
	s_bConfigQuestion = false;
	s_szLastCard.clear();
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\CloudCredentials" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	pButtonOK = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_OK ) );
	pButtonCancel = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_CANCEL ) );
	if ( pButtonOK )
		pButtonOK->EnableWindow( true );
	if ( pButtonCancel )
		pButtonCancel->EnableWindow( true );

	LoadStored();
	szBackend = ProviderRowValue();
	if ( szBackend.empty() )
		szBackend = szStoredBackend;
	LayoutRows();
	RefreshDiscoveryLine();
	SetStatus( 0, L"" );
	BeginCatalogue();

	pScene->AddUIScreen( pUIScreen );

	IInput *pInputSingleton = GetSingleton<IInput>();
	pInputSingleton->PumpMessages( true );
	SGameMessage msg;
	while ( pInputSingleton->GetMessage( &msg ) )
	{
	}
}
