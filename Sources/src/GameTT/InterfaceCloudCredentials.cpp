#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceCloudCredentials.h"
#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "../UI/UIMessages.h"
#include "../Main/CloudSyncFacade.h"

static const NInput::SRegisterCommandEntry commands[] =
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum
{
	// Seven label+edit rows; which are visible and how they are labelled
	// depends on the protocol. Vendor and endpoint/URL and the credential
	// pair share rows across protocols; bucket and region are S3-only.
	E_EDIT_VENDOR								= 2001,
	E_EDIT_ENDPOINT							= 2002,
	E_EDIT_BUCKET								= 2003,
	E_EDIT_REGION								= 2004,
	E_EDIT_ACCESS								= 2005,
	E_EDIT_SECRET								= 2006,
	E_EDIT_RCLONE								= 2007,

	E_LABEL_BASE								= 3000,		// label id = E_LABEL_BASE + (edit id - 2000)

	E_STATIC_DISCOVERY					= 3101,
	E_STATIC_STATUS							= 3102,

	E_BUTTON_PROVIDER						= 10020,
	E_BUTTON_TEST								= 10021,
	E_BUTTON_CLEAR_SECRET				= 10022,

	E_BUTTON_OK									= 10002,
	E_BUTTON_CANCEL							= 10001,
};
static const wchar_t MASK_CHAR = L'*';

// ---- small string helpers -------------------------------------------------
// The credentials document is UTF-8 JSON; edit boxes hold wide text. The
// values here are endpoints, key ids and paths - ASCII in practice - but the
// conversion is done honestly so a non-ASCII path survives the round trip.
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
// Extract "key": <string|true|false|null> from a flat JSON document. The
// facade's documents nest one protocol object, but every key name appears at
// most once per document, so a scan by key is unambiguous.
static bool JsonFind( const std::string &szDoc, const char *pszKey, std::string *pszValue, bool *pbIsString )
{
	const std::string szNeedle = std::string( "\"" ) + pszKey + "\"";
	std::string::size_type nPos = szDoc.find( szNeedle );
	if ( nPos == std::string::npos )
		return false;
	nPos += szNeedle.size();
	while ( nPos < szDoc.size() && ( szDoc[nPos] == ':' || szDoc[nPos] == ' ' || szDoc[nPos] == '\t' ) )
		++nPos;
	if ( nPos >= szDoc.size() )
		return false;
	if ( szDoc[nPos] == '"' )
	{
		*pbIsString = true;
		std::string szValue;
		for ( ++nPos; nPos < szDoc.size() && szDoc[nPos] != '"'; ++nPos )
		{
			if ( szDoc[nPos] == '\\' && nPos + 1 < szDoc.size() )
			{
				++nPos;
				switch ( szDoc[nPos] )
				{
					case 'n': szValue += '\n'; break;
					case 't': szValue += '\t'; break;
					case 'u':
						// Rare in these documents; keep the scan aligned and
						// substitute the code point when it is Latin-1.
						if ( nPos + 4 < szDoc.size() )
						{
							const int nCode = (int)strtol( szDoc.substr( nPos + 1, 4 ).c_str(), 0, 16 );
							if ( nCode > 0 && nCode < 256 )
								szValue += char( nCode );
							nPos += 4;
						}
						break;
					default: szValue += szDoc[nPos]; break;
				}
			}
			else
				szValue += szDoc[nPos];
		}
		*pszValue = szValue;
		return true;
	}
	*pbIsString = false;
	std::string szBare;
	while ( nPos < szDoc.size() && ( isalnum( (unsigned char)szDoc[nPos] ) || szDoc[nPos] == '.' || szDoc[nPos] == '-' ) )
		szBare += szDoc[nPos++];
	*pszValue = szBare;
	return true;
}
static std::string JsonString( const std::string &szDoc, const char *pszKey )
{
	std::string szValue;
	bool bIsString = false;
	if ( !JsonFind( szDoc, pszKey, &szValue, &bIsString ) || !bIsString )
		return "";
	return szValue;
}
static bool JsonBool( const std::string &szDoc, const char *pszKey )
{
	std::string szValue;
	bool bIsString = false;
	if ( !JsonFind( szDoc, pszKey, &szValue, &bIsString ) || bIsString )
		return false;
	return szValue == "true";
}
// Which text under textes\ui\cloudsync\ a classified failure maps to - the
// same mapping the main menu indicator uses, fed by the same leading-tag
// contract on failure texts.
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
void CInterfaceCloudCredentials::SetRow( int nRow, const char *pszLabelKey, const std::wstring &szValue, bool bVisible )
{
	IUIElement *pLabel = pUIScreen->GetChildByID( E_LABEL_BASE + nRow );
	IUIElement *pEdit = pUIScreen->GetChildByID( 2000 + nRow );
	if ( pLabel == 0 || pEdit == 0 )
		return;
	if ( pszLabelKey != 0 )
	{
		const std::string szKey = std::string( "Textes\\UI\\CloudCredentials\\" ) + pszLabelKey;
		const std::wstring szLabel = TextOrFallback( szKey.c_str(), WideFromUtf8( pszLabelKey ).c_str() );
		pLabel->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szLabel.c_str() ) ) );
	}
	SetEdit( 2000 + nRow, szValue );
	pLabel->ShowWindow( bVisible ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
	pEdit->ShowWindow( bVisible ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE );
}
void CInterfaceCloudCredentials::ApplyProtocol()
{
	const bool bS3 = szProtocol != "webdav";
	// Labels swap with the protocol; values in the shared rows survive the
	// switch so a typo in the provider choice does not eat the endpoint.
	SetRow( 1, "label_vendor", GetEdit( E_EDIT_VENDOR ), true );
	SetRow( 2, bS3 ? "label_endpoint" : "label_url", GetEdit( E_EDIT_ENDPOINT ), true );
	SetRow( 3, "label_bucket", GetEdit( E_EDIT_BUCKET ), bS3 );
	SetRow( 4, "label_region", GetEdit( E_EDIT_REGION ), bS3 );
	SetRow( 5, bS3 ? "label_access_key" : "label_user", GetEdit( E_EDIT_ACCESS ), true );
	SetRow( 6, bS3 ? "label_secret" : "label_password", std::wstring( bStoredSecret && !bSecretTouched ? 8 : szSecretReal.size(), MASK_CHAR ), true );
	SetRow( 7, "label_rclone", GetEdit( E_EDIT_RCLONE ), true );

	if ( IUIElement *pProvider = pUIScreen->GetChildByID( E_BUTTON_PROVIDER ) )
	{
		const std::wstring szText = TextOrFallback(
			bS3 ? "Textes\\UI\\CloudCredentials\\provider_s3" : "Textes\\UI\\CloudCredentials\\provider_webdav",
			bS3 ? L"Storage: S3" : L"Storage: WebDAV" );
		pProvider->SetWindowText( -1, NPlatform::WordStringData( NPlatform::WordStringFromWide( szText.c_str() ) ) );
	}
}
void CInterfaceCloudCredentials::PopulateFromCredentials()
{
	char pszDoc[16384];
	bStoredSecret = false;
	bSecretTouched = false;
	bGenericStored = false;
	szSecretReal.clear();
	if ( NCloudSync::LoadCredentials( pszDoc, sizeof( pszDoc ) ) )
	{
		const std::string szDoc = pszDoc;
		const std::string szDocProtocol = JsonString( szDoc, "protocol" );
		// No "protocol" key means the generic schema: partial prefill only,
		// and SaveCredentials refuses rather than writing the blanks back.
		bGenericStored = szDocProtocol.empty();
		if ( !szDocProtocol.empty() )
			szProtocol = szDocProtocol;
		bStoredSecret = JsonBool( szDoc, "has_secret" );
		if ( szProtocol == "webdav" )
		{
			SetEdit( E_EDIT_VENDOR, WideFromUtf8( JsonString( szDoc, "vendor" ) ) );
			SetEdit( E_EDIT_ENDPOINT, WideFromUtf8( JsonString( szDoc, "url" ) ) );
			SetEdit( E_EDIT_ACCESS, WideFromUtf8( JsonString( szDoc, "user" ) ) );
		}
		else
		{
			SetEdit( E_EDIT_VENDOR, WideFromUtf8( JsonString( szDoc, "s3_provider" ) ) );
			SetEdit( E_EDIT_ENDPOINT, WideFromUtf8( JsonString( szDoc, "endpoint" ) ) );
			SetEdit( E_EDIT_BUCKET, WideFromUtf8( JsonString( szDoc, "bucket" ) ) );
			SetEdit( E_EDIT_REGION, WideFromUtf8( JsonString( szDoc, "region" ) ) );
			SetEdit( E_EDIT_ACCESS, WideFromUtf8( JsonString( szDoc, "access_key" ) ) );
		}
		SetEdit( E_EDIT_RCLONE, WideFromUtf8( JsonString( szDoc, "rclone_path" ) ) );
	}
	else
	{
		// Present but unreadable here: a legacy document always fits this
		// buffer, so this is a generic one (an OAuth token can exceed it).
		// Guard the save rather than treating it as absent.
		bGenericStored = NCloudSync::CredentialsPresent();
	}
	ApplyProtocol();
}
void CInterfaceCloudCredentials::RefreshDiscoveryLine()
{
	const std::string szDoc = NCloudSync::DiscoveryStatus();
	std::wstring szLine;
	if ( JsonBool( szDoc, "found" ) )
	{
		szLine = TextOrFallback( "Textes\\UI\\CloudCredentials\\discovery_found", L"rclone" );
		const std::string szVersion = JsonString( szDoc, "version" );
		const std::string szPath = JsonString( szDoc, "path" );
		if ( !szVersion.empty() )
			szLine += L" " + WideFromUtf8( szVersion );
		if ( !szPath.empty() )
			szLine += L" - " + WideFromUtf8( szPath );
	}
	else
	{
		const std::string szReason = JsonString( szDoc, "reason" );
		const std::string szKey = std::string( "Textes\\UI\\CloudCredentials\\discovery_" ) +
			( szReason.empty() ? "not_found" : szReason );
		szLine = TextOrFallback( szKey.c_str(), L"rclone was not found" );
		const std::string szPath = JsonString( szDoc, "path" );
		if ( !szPath.empty() )
			szLine += L" - " + WideFromUtf8( szPath );
	}
	SetEdit( E_STATIC_DISCOVERY, szLine );
}
void CInterfaceCloudCredentials::OnSecretEdited()
{
	// The box shows only mask characters; the real value lives in
	// szSecretReal. Leading and trailing runs of the mask character map onto
	// the kept prefix and suffix of the real secret, whatever sits between
	// them was just typed. (A secret whose own characters are typed as '*'
	// at the edges is misread as kept - the one corner this trades away.)
	const std::wstring szShown = GetEdit( E_EDIT_SECRET );
	std::wstring::size_type nLead = 0;
	while ( nLead < szShown.size() && szShown[nLead] == MASK_CHAR )
		++nLead;
	std::wstring::size_type nTrail = 0;
	while ( nTrail < szShown.size() - nLead && szShown[szShown.size() - 1 - nTrail] == MASK_CHAR )
		++nTrail;
	if ( nLead > szSecretReal.size() )
		nLead = szSecretReal.size();
	if ( nTrail > szSecretReal.size() - nLead )
		nTrail = szSecretReal.size() - nLead;
	const std::wstring szMiddle = szShown.substr( nLead, szShown.size() - nLead - nTrail );
	szSecretReal = szSecretReal.substr( 0, nLead ) + szMiddle + szSecretReal.substr( szSecretReal.size() - nTrail );
	bSecretTouched = true;
	SetEdit( E_EDIT_SECRET, std::wstring( szSecretReal.size(), MASK_CHAR ) );
	if ( IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( E_EDIT_SECRET ) ) )
		pEdit->SetCursor( (int)( nLead + szMiddle.size() ) );
}
bool CInterfaceCloudCredentials::SaveCredentials()
{
	if ( bGenericStored )
	{
		// An accept from the half-blank prefill would blank the S3 remote
		// root and route every sync at the account root.
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed",
			L"stored in a newer format this dialog cannot edit yet" );
		return false;
	}
	const bool bS3 = szProtocol != "webdav";
	std::string szJson = "{";
	szJson += bS3 ? "\"protocol\":\"s3\",\"s3\":{" : "\"protocol\":\"webdav\",\"webdav\":{";
	if ( bS3 )
	{
		szJson += "\"s3_provider\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_VENDOR ) ) ) + "\",";
		szJson += "\"endpoint\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_ENDPOINT ) ) ) + "\",";
		szJson += "\"bucket\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_BUCKET ) ) ) + "\",";
		szJson += "\"region\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_REGION ) ) ) + "\",";
		szJson += "\"access_key\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_ACCESS ) ) ) + "\"";
		if ( bSecretTouched && !szSecretReal.empty() )
			szJson += ",\"secret\":\"" + JsonEscape( Utf8FromWide( szSecretReal ) ) + "\"";
	}
	else
	{
		szJson += "\"url\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_ENDPOINT ) ) ) + "\",";
		szJson += "\"vendor\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_VENDOR ) ) ) + "\",";
		szJson += "\"user\":\"" + JsonEscape( Utf8FromWide( GetEdit( E_EDIT_ACCESS ) ) ) + "\"";
		if ( bSecretTouched && !szSecretReal.empty() )
			szJson += ",\"pass\":\"" + JsonEscape( Utf8FromWide( szSecretReal ) ) + "\"";
	}
	szJson += "},";
	const std::string szRclone = Utf8FromWide( GetEdit( E_EDIT_RCLONE ) );
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
	if ( bSecretTouched && !szSecretReal.empty() )
		bStoredSecret = true;
	// A secret typed and saved is now the stored one; the box goes back to
	// meaning "keep what is stored".
	bSecretTouched = false;
	szSecretReal.clear();
	SetEdit( E_EDIT_SECRET, std::wstring( bStoredSecret ? 8 : 0, MASK_CHAR ) );
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
	nTestHandle = NCloudSync::TestConnection();
	if ( nTestHandle < 0 )
	{
		SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
		return;
	}
	SetStatus( "Textes\\UI\\CloudCredentials\\testing", L"" );
}
bool CInterfaceCloudCredentials::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
		break;
	case UI_NOTIFY_EDIT_BOX_TEXT_CHANGED:
		if ( msg.nParam == E_EDIT_SECRET && !bFinished )
			OnSecretEdited();
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
			if ( SaveCredentials() )
			{
				bFinished = true;
				CloseInterface();
			}
		}
		return true;
	case E_BUTTON_PROVIDER:
		szProtocol = ( szProtocol == "webdav" ) ? "s3" : "webdav";
		ApplyProtocol();
		return true;
	case E_BUTTON_TEST:
		BeginConnectionTest();
		return true;
	case E_BUTTON_CLEAR_SECRET:
		// The deliberate act, distinct from saving with an empty box - an
		// empty box preserves the stored secret.
		if ( NCloudSync::ClearCredentialsSecret() )
		{
			bStoredSecret = false;
			bSecretTouched = false;
			szSecretReal.clear();
			SetEdit( E_EDIT_SECRET, L"" );
			SetStatus( "Textes\\UI\\CloudCredentials\\secret_cleared", L"" );
		}
		else
			SetStatus( "Textes\\UI\\CloudCredentials\\save_failed", WideFromUtf8( NCloudSync::LastError() ) );
		return true;
	default:
		return false;
	}
	return true;
}
bool CInterfaceCloudCredentials::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	// The connection test is observed, never awaited: Poll is a mutex and a
	// struct copy. The classified outcome leads the failure text, and maps
	// onto the same per-outcome messages the menu indicator uses.
	if ( nTestHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nTestHandle );
		if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
		{
			if ( eState == NCloudSync::STATE_DONE )
			{
				NStr::DebugTrace( "cloud credentials: connection test ok\n" );
				SetStatus( "Textes\\UI\\CloudCredentials\\test_ok", L"" );
			}
			else
			{
				const std::string szError = NCloudSync::Error( nTestHandle );
				NStr::DebugTrace( "cloud credentials: connection test failed: %s\n", szError.c_str() );
				const std::string szKey = std::string( "Textes\\UI\\CloudSync\\" ) + CloudOutcomeTextKey( szError );
				SetStatus( szKey.c_str(), L"" );
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
	return true;
}
void CInterfaceCloudCredentials::Done()
{
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
	szProtocol = "s3";
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

	PopulateFromCredentials();
	RefreshDiscoveryLine();
	SetStatus( 0, L"" );

	pScene->AddUIScreen( pUIScreen );

	IInput *pInputSingleton = GetSingleton<IInput>();
	pInputSingleton->PumpMessages( true );
	SGameMessage msg;
	while ( pInputSingleton->GetMessage( &msg ) )
	{
	}
}
