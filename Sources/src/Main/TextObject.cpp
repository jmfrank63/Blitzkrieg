#include "StdAfx.h"

#include "TextObject.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( IText );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTextString::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szString );
	saver.Add( 2, &bChanged );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextString::SwapData( ISharedResource *pResource )
{
	CTextString *pRes = dynamic_cast<CTextString*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CTextString", return );
	//
	std::swap( szString, pRes->szString );
	bChanged = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDialog::SwapData( ISharedResource *pResource )
{
	CTextDialog *pRes = dynamic_cast<CTextDialog*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CTextDialog", return );
	//
	std::swap( szString, pRes->szString );
	bChanged = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTextDialog::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	// read and check UNICODE signature (0xfffe)
	WORD wSignature = 0;
	pStream->Read( &wSignature, 2 );
	NI_ASSERT_TF( wSignature == 0xfeff, NStr::Format("Text \"%s\" is not a UNICODE text!", szStreamName.c_str()), return false );
	// read string
	const int nSize = pStream->GetSize() - 2;
	szString.resize( nSize / 2 );
	const int nCheck = pStream->Read( &(szString[0]), nSize );
	NI_ASSERT_SLOW_TF( nCheck == nSize, NStr::Format("Readed size (%d) doesn't match requested (%d)", nCheck, nSize), return false );
	// cut all CRLF symbols from the end
	int nPos = szString.find_last_not_of( L'\n' );
	while ( nPos + 1 < szString.size() ) 
	{
		if ( nPos == std::string::npos )
		{
			if ( szString.find_first_of( L'\n' ) == 0 )
				szString.clear();
			break;
		}
		else
		{
			szString.erase( nPos, std::string::npos );
			nPos = szString.find_last_not_of( L'\n' );
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextDialog::SetText( const WORD *pszText ) 
{ 
	szString = pszText == 0 ? L"" : reinterpret_cast<const wchar_t*>(pszText);
	bChanged = true; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
