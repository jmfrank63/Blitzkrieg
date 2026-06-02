#include "StdAfx.h"

#include "ImageBMP.h"
enum EBitmapCompression
{
 BC_RGB       = 0,
 BC_RLE8      = 1,
 BC_RLE4      = 2,
 BC_BITFIELDS = 3
};
inline long GetShort( int nOffset, BYTE *pBuff )
{
	return *reinterpret_cast<short*>( pBuff + nOffset );
}
inline long GetWord( int nOffset, BYTE *pBuff )
{
	return *reinterpret_cast<WORD*>( pBuff + nOffset );
}
inline long GetLong( int nOffset, BYTE *pBuff )
{
	return *reinterpret_cast<long*>( pBuff + nOffset );
}
inline long GetDWord( int nOffset, BYTE *pBuff )
{
	return *reinterpret_cast<DWORD*>( pBuff + nOffset );
}
bool NImage::RecognizeFormatBMP( IDataStream *pStream )
{
	char signature[2];
	int nCounter = pStream->Read( signature, 2 );
	pStream->Seek( -nCounter, STREAM_SEEK_CUR );
	if ( nCounter != 2 )
		return false;
	return (signature[0] == 'B') && (signature[1] == 'M');
}
CImage* NImage::LoadImageBMP( IDataStream *pStream )
{
	int i;
	long nFileStart = pStream->GetPos();
	BYTE header[54];
	pStream->Read( header, 54 );
	DWORD dwFileSize = GetDWord( 2, header );
	DWORD dwOffset = GetDWord( 10, header );
	WORD wNumPlanes = GetWord( 26, header );
	WORD wNumBits = GetWord( 28, header );
	DWORD dwCompression = GetDWord( 30, header );
	DWORD dwNumColors = GetDWord( 46, header );
	DWORD dwWidth = GetDWord( 18, header );
	DWORD dwHeight = GetDWord( 22, header );
	std::vector<DWORD> image( dwWidth * dwHeight );
	if ( wNumPlanes != 1 )
		return 0;
	DWORD *pImageData = 0;
	if ( wNumBits == 8 )                  // palettized image with 256 color palette
	{
		DWORD palette[256];
		dwNumColors = dwNumColors > 0 ? dwNumColors : 256;
		memset( palette, 0, dwNumColors * sizeof(DWORD) );
		for ( i=0; i<dwNumColors; ++i )
		{
			BYTE color[4];
			pStream->Read( color, 4 );
			palette[i] = DWORD( 255 << 24 ) | (DWORD(color[2]) << 16) | (DWORD(color[1]) << 8) | DWORD(color[0]);
		}
		pStream->Seek( nFileStart + dwOffset, STREAM_SEEK_SET );
		if ( dwCompression == 0 )           // unpacked values
		{
      int nEndOfLineSkip = (dwFileSize - 54 - dwNumColors*4) / dwHeight - dwWidth;
			std::vector<BYTE> dataline( dwWidth );
			pImageData = &(image[0]) + dwWidth*dwHeight - dwWidth - 1;
      for ( i=0; i<dwHeight; ++i )
      {
				if ( pStream->Read( &(dataline[0]), dwWidth ) != dwWidth )
					return 0;
				pImageData = &(image[0]) + ( dwHeight - i - 1 ) * dwWidth;
				for ( std::vector<BYTE>::const_iterator pos = dataline.begin(); pos != dataline.end(); ++pos )
					*pImageData++ = palette[ *pos ];
				if ( nEndOfLineSkip )
					pStream->Seek( nEndOfLineSkip, STREAM_SEEK_CUR );
      }
		}
		else if ( dwCompression == 1 )      // RLE packing
		{
			std::vector<BYTE> imagedata( dwWidth * dwHeight );
			bool bDone = false;
      BYTE *d = &(imagedata[0]);
      DWORD x = 0, y = dwHeight - 1;

      while ( !bDone )
      {
        BYTE rlepair[2];
				pStream->Read( rlepair, 2 );

        if ( *rlepair )                 // escaped
        {
          if ( rlepair[1] == 0 )        // end of line
          {
            x = 0;
            y--;
						d = &(imagedata[0]) + y*dwWidth;
          }
					else if ( rlepair[1] == 1 )   // end of bitmap
            bDone = true;
          else if ( rlepair[1] == 2 )   // goto xy
          {
						pStream->Read( rlepair, 2 );
            x += rlepair[0];
            y -= rlepair[1];
            d = &(imagedata[0]) + y*dwWidth + x;
          }
					else                          // run of data (even padded)
          {
						pStream->Read( d, rlepair[1] );

            if ( rlepair[1] & 1 )
							pStream->Read( rlepair, 1 );
            d += rlepair[1];
            x += rlepair[1];
          }
        }
				else                            // run of colors
        {
          memset( d, rlepair[1], rlepair[0] );
          d += rlepair[0];
          x += rlepair[0];
        }
      }
			pImageData = &( image[0] );
			for ( std::vector<BYTE>::const_iterator pos = imagedata.begin(); pos != imagedata.end(); ++pos )
				*pImageData++ = palette[ *pos ];
		}
	}
	else if ( (wNumBits == 16) || (wNumBits == 32) )  // treat palette as a 3 DWORDs with 'red', 'green' and 'blue' bit masks respectively
	{
		if ( dwCompression != BC_BITFIELDS )
			return 0;
		DWORD palette[3];
		pStream->Read( palette, sizeof(DWORD)*3 );
		SPixelConvertInfo pci( 0, palette[0], palette[1], palette[2] );
    int nEndOfLineSkip = ( dwFileSize - 54 - /* dwNumColors*4 */ 3*4 ) / dwHeight - dwWidth*2;
    WORD x, y;

		pStream->Seek( nFileStart + dwOffset, STREAM_SEEK_SET );

    for ( y=0; y<dwHeight; ++y )
    {
			DWORD *pImageData = &( image[0] ) + ( dwHeight - y - 1 ) * dwWidth;
      for ( x=0; x<dwWidth; ++x )
      {
        WORD data;
				pStream->Read( &data, 2 );
				*pImageData++ = pci.DecompColor( data ) | 0xFF000000;
      }
      if ( nEndOfLineSkip )   // each row is 0-padded to a 4-byte boundary
				pStream->Seek( nEndOfLineSkip, STREAM_SEEK_CUR ); 
    }
	}
	else if ( wNumBits == 24 )
	{
    int nEndOfLineSkip = ( dwFileSize - 54 )/dwHeight - dwWidth*3;
    WORD x, y;

		pStream->Seek( nFileStart + dwOffset, STREAM_SEEK_SET );

    for ( y=0; y<dwHeight; ++y )
    {
			DWORD *pImageData = &( image[0] ) + ( dwHeight - y - 1 ) * dwWidth;
      for ( x=0; x<dwWidth; ++x )
      {
        BYTE d[3];
				pStream->Read( d, 3 );
				*pImageData++ = 0xFF000000 | (DWORD(d[2]) << 16) | (DWORD(d[1]) << 8) | DWORD( d[0] );
      }

      if ( nEndOfLineSkip )   // each row is 0-padded to a 4-byte boundary
				pStream->Seek( nEndOfLineSkip, STREAM_SEEK_CUR ); 
    }
	}
	else if ( wNumBits == 1 )
  {
    int nEndOfLineSkip = ( dwFileSize - 54 )/dwHeight - dwWidth/8;
    WORD x, y;

		pStream->Seek( nFileStart + dwOffset, STREAM_SEEK_CUR );

    for ( y=0; y<dwHeight; ++y )
    {
			DWORD *pImageData = &( image[0] ) + ( dwHeight - y - 1 ) * dwWidth;
      for ( x=0; x<dwWidth/8; ++x )
      {
        BYTE d;
				pStream->Read( &d, 1 );
        for ( int i=0; i<8; ++i )
        {
          DWORD dwValue = (d & 0x80) == 0 ? 0 : 0xFF;
				  *pImageData++ = 0xFF000000 | (dwValue << 16) | (dwValue << 8) | dwValue;
          d <<= 1;
        }
      }

      if ( nEndOfLineSkip )   // each row is 0-padded to a 4-byte boundary
				pStream->Seek( nEndOfLineSkip, STREAM_SEEK_CUR ); 
    }
  }
  else
		return 0;

	return new CImage( dwWidth, dwHeight, image );
}
