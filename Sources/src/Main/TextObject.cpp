#include "StdAfx.h"

#include "TextObject.h"
BASIC_REGISTER_CLASS( IText );
int CTextString::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szString );
	saver.Add( 2, &bChanged );
	return 0;
}
void CTextString::SwapData( ISharedResource *pResource )
{
	CTextString *pRes = dynamic_cast<CTextString*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CTextString", return );
	std::swap( szString, pRes->szString );
	bChanged = true;
}
void CTextDialog::SwapData( ISharedResource *pResource )
{
	CTextDialog *pRes = dynamic_cast<CTextDialog*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CTextDialog", return );
	std::swap( szString, pRes->szString );
	bChanged = true;
}
bool CTextDialog::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	WORD wSignature = 0;
	pStream->Read( &wSignature, 2 );
	NI_ASSERT_TF( wSignature == 0xfeff, NStr::Format("Text \"%s\" is not a UNICODE text!", szStreamName.c_str()), return false );
	const int nSize = pStream->GetSize() - 2;
	szString.resize( nSize / 2 );
	const int nCheck = pStream->Read( &(szString[0]), nSize );
	NI_ASSERT_SLOW_TF( nCheck == nSize, NStr::Format("Readed size (%d) doesn't match requested (%d)", nCheck, nSize), return false );
	int nPos = szString.find_last_not_of( u'\n' );
	while ( nPos + 1 < szString.size() ) 
	{
		if ( nPos == std::string::npos )
		{
			if ( szString.find_first_of( u'\n' ) == 0 )
				szString.clear();
			break;
		}
		else
		{
			szString.erase( nPos, std::string::npos );
			nPos = szString.find_last_not_of( u'\n' );
		}
	}

	// The stock data promises an exit to Windows; this build does not run
	// there. Substituted on every (re)load, so the patch survives resource
	// reloads, and only for this one text.
#if defined(__APPLE__) || defined(__linux__)
	if ( szStreamName.find( "exittowindows" ) != std::string::npos )
	{
		const std::u16string::size_type nWinPos = szString.find( u"Windows" );
		if ( nWinPos != std::u16string::npos )
#if defined(__APPLE__)
			szString.replace( nWinPos, 7, u"MacOS" );
#else
			szString.replace( nWinPos, 7, u"Linux" );
#endif
	}
#endif
	return true;
}
void CTextDialog::SetText( const WORD *pszText ) 
{ 
	szString = pszText == 0 ? u"" : reinterpret_cast<const char16_t*>(pszText);
	bChanged = true; 
}
