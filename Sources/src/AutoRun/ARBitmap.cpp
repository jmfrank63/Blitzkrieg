#include "StdAfx.h"
#include "FileUtils.h"
#include "ARBitmap.h"

bool CARBitmap::Load( const std::vector<BYTE> &rData, CDC *pDC )
{
	long nFileStart = 0;

	const BYTE *pData = static_cast<const BYTE*>( &( rData[0] ) );

	BITMAPFILEHEADER bitmapFileHeader;
	memcpy( static_cast<void*>( &bitmapFileHeader ), static_cast<const void*>( pData ), sizeof( BITMAPFILEHEADER ) );
	BITMAPINFOHEADER bitmapInfoHeader;
	memcpy( static_cast<void*>( &bitmapInfoHeader ), static_cast<const void*>( pData + sizeof( BITMAPFILEHEADER ) ), sizeof( BITMAPINFOHEADER ) );

	size.x = bitmapInfoHeader.biWidth;
	size.y = bitmapInfoHeader.biHeight;

	bitmap.CreateCompatibleBitmap( pDC, size.x, size.y );
	CDC memDC;
	int nRes = memDC.CreateCompatibleDC( pDC );
	CBitmap *pOldBitmap = memDC.SelectObject( &bitmap );
  ::SetDIBitsToDevice( memDC.GetSafeHdc(),
											 0, 0, bitmapInfoHeader.biWidth, bitmapInfoHeader.biHeight,
											 0, 0,
											 0, bitmapInfoHeader.biHeight,
											 static_cast<const void*>( pData + bitmapFileHeader.bfOffBits ),
											 (LPBITMAPINFO)( &bitmapInfoHeader ),
											 DIB_RGB_COLORS );
	memDC.SelectObject( pOldBitmap );
	return true;
}
