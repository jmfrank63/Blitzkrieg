#include "StdAfx.h"
#include "ARText.h"

bool CARText::Load( const std::vector<BYTE> &rData )
{
	if ( ( rData.size() < 4 ) || ( rData[0] != 0xff ) || ( rData[1] != 0xfe ) )
	{
		return false;
	}
	
	wszText.resize( ( rData.size() - 2 ) / sizeof( wchar_t ) );
	memcpy( &( wszText[0] ), &( rData[0] ) + 2, wszText.size() * sizeof( wchar_t ) );
	return true;
}

void CARText::SetCodePage( int nCodePage )
{
	int nBufferLength = ::WideCharToMultiByte( nCodePage, 0, wszText.c_str(), wszText.length(), 0, 0, 0, 0 );
	LPTSTR lptStr = szText.GetBuffer( nBufferLength );
	::WideCharToMultiByte( nCodePage, 0, wszText.c_str(), wszText.length(), lptStr, nBufferLength, 0, 0 );
	szText.ReleaseBuffer();
}
