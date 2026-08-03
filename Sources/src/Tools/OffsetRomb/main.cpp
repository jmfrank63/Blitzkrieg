#include "StdAfx.h"

#include "../Image/Image.h"
inline int Height( const RECT &rect ) { return rect.bottom - rect.top; }
inline int Width( const RECT &rect ) { return rect.right - rect.left; }
template <class TYPE>
inline void FlipX( TYPE *p1, TYPE *p2 )
{
	TYPE val;
	while ( p1 < p2 )
	{
		val = *p1;
		*p1 = *p2;
		*p2 = val;
		++p1;
		--p2;
	}
}
template <class TYPE>
inline void FlipY( TYPE *p1, TYPE *p2, int nLength )
{
	static std::vector<TYPE> buffer;
	buffer.resize( nLength );
	memcpy( &(buffer[0]), p2, nLength * sizeof(TYPE) );
	memcpy( p2, p1, nLength * sizeof(TYPE) );
	memcpy( p1, &(buffer[0]), nLength * sizeof(TYPE) );
}
inline void FlipRombX( CImageAccessor &image, const RECT &rect )
{
	const int nHalfX = ( Width( rect ) + 1 ) / 2;
	const int nHalfY = ( Height( rect ) + 1 ) / 2;
	for ( int i=0; i<nHalfY; ++i )
	{
		FlipX( &(image[rect.top + i][rect.left + nHalfX - i*2 - 1]), &(image[rect.top + i][rect.left + nHalfX + i*2]) );
		FlipX( &(image[rect.bottom - i][rect.left + nHalfX - i*2 - 1]), &(image[rect.bottom - i][rect.left + nHalfX + i*2]) );
	}
	if ( (nHalfY & 1) == 0 )
		FlipX( &(image[rect.top + nHalfY][rect.left]), &(image[rect.top + nHalfY][rect.right]) );
}
inline void FlipRombY( CImageAccessor &image, const RECT &rect )
{
	const int nHalfX = ( Width( rect ) + 1 ) / 2;
	const int nHalfY = ( Height( rect ) + 1 ) / 2;
	for ( int i=0; i<nHalfY; ++i )
		FlipY( &(image[rect.top + i][rect.left + nHalfX - i*2 - 1]), &(image[rect.bottom - i][rect.left + nHalfX - i*2 - 1]), i*4 + 2 );
}
class CBrazenhaimLine
{
	int x1, y1, x2, y2;
	int xerr, yerr;
	int xlen, ylen, len;
	int xinc, yinc;
public:
	CBrazenhaimLine( int _x1, int _y1, int _x2, int _y2 )
		: x1( _x1 ), y1( _y1 ), x2( _x2 ), y2( _y2 ), xerr( 0 ), yerr( 0 ), xlen( x2 - x1 ), ylen( y2 - y1 )
	{  
		xinc = Sign( xlen );
		yinc = Sign( ylen );
		xlen = abs( xlen ) + 1;
		ylen = abs( ylen ) + 1;
		len = Max( xlen, ylen );
	}
	bool Next()
	{
		if ( (x1 == x2) && (y1 == y2) )
			return false;
		xerr += xlen;
		if ( xerr >= len )
			x1 += xinc, xerr -= len;
		yerr += ylen;
		if ( yerr >= len )
			y1 += yinc, yerr -= len;
		return true;
	}
	int GetX() const { return x1; }
	int GetY() const { return y1; }
};
void FlipLines( CImageAccessor &image, CBrazenhaimLine &line1, CBrazenhaimLine &line2 )
{
	do 
	{
		SColor val = image[line1.GetY()][line1.GetX()];
		image[line1.GetY()][line1.GetX()] = image[line2.GetY()][line2.GetX()];
		image[line2.GetY()][line2.GetX()] = val;
	} while( line1.Next() && line2.Next() );
}
void FlipRomb1( CImageAccessor &image, const RECT &rect )
{
	int nHalfX = Width( rect ) / 2;
	int nHalfY = Height( rect ) / 2;
	{
		int nTopX1 = rect.left + nHalfX - 1, nTopY1 = rect.top, nTopX2 = rect.left + nHalfX*2 - 1, nTopY2 = rect.top + nHalfY;
		int nBotX1 = rect.left + nHalfX/2, nBotY1 = rect.top + nHalfY/2, nBotX2 = rect.left + nHalfX/2*3, nBotY2 = rect.top + nHalfY/2*3;

		while ( (nTopX1 != nBotX1) && (nTopX2 != nBotX2) && (nTopY1 != nBotY1) && (nTopY2 != nBotY2) )
		{
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopY1 += 1;  nTopY2 += 1;
			nBotX1 += 2;  nBotX2 += 2;
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopX1 -= 2;  nTopX2 -= 2;
			nBotY1 -= 1;  nBotY2 -= 1;
		}
	}
	{
		int nTopX1 = rect.left + nHalfX/2, nTopY1 = rect.top + nHalfY/2, nTopX2 = rect.left + nHalfX/2*3, nTopY2 = rect.top + nHalfY/2*3;
		int nBotX1 = rect.left, nBotY1 = rect.top + nHalfY, nBotX2 = rect.left + nHalfX - 1, nBotY2 = rect.top + nHalfY*2;

		while ( (nTopX1 != nBotX1) && (nTopX2 != nBotX2) && (nTopY1 != nBotY1) && (nTopY2 != nBotY2) )
		{
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopY1 += 1;  nTopY2 += 1;
			nBotX1 += 2;  nBotX2 += 2;
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopX1 -= 2;  nTopX2 -= 2;
			nBotY1 -= 1;  nBotY2 -= 1;
		}
	}
}
void FlipRomb2( CImageAccessor &image, const RECT &rect )
{
	int nHalfX = Width( rect ) / 2;
	int nHalfY = Height( rect ) / 2;
	{
		int nTopX1 = rect.left, nTopY1 = rect.top + nHalfY, nTopX2 = rect.left + nHalfX, nTopY2 = rect.top;
		int nBotX1 = rect.left + nHalfX/2, nBotY1 = rect.top + nHalfY/2*3, nBotX2 = rect.left + nHalfX/2*3 + 1, nBotY2 = rect.top + nHalfY/2;

		while ( (nTopX1 != nBotX1) && (nTopX2 != nBotX2) && (nTopY1 != nBotY1) && (nTopY2 != nBotY2) )
		{
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopX1 += 2;  nTopX2 += 2;
			nBotY1 -= 1;  nBotY2 -= 1;
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopY1 += 1;  nTopY2 += 1;
			nBotX1 -= 2;  nBotX2 -= 2;
		}
	}
	{
		int nTopX1 = rect.left + nHalfX/2, nTopY1 = rect.top + nHalfY/2*3, nTopX2 = rect.left + nHalfX/2*3 + 1, nTopY2 = rect.top + nHalfY/2;
		int nBotX1 = rect.left + nHalfX - 1, nBotY1 = rect.bottom - 1, nBotX2 = rect.right - 1, nBotY2 = rect.top + nHalfY;

		while ( (nTopX1 != nBotX1) && (nTopX2 != nBotX2) && (nTopY1 != nBotY1) && (nTopY2 != nBotY2) )
		{
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopX1 += 2;  nTopX2 += 2;
			nBotY1 -= 1;  nBotY2 -= 1;
			{
				CBrazenhaimLine line1( nTopX1, nTopY1, nTopX2, nTopY2 );
				CBrazenhaimLine line2( nBotX1, nBotY1, nBotX2, nBotY2 );
				FlipLines( image, line1, line2 );
			}
			nTopY1 += 1;  nTopY2 += 1;
			nBotX1 -= 2;  nBotX2 -= 2;
		}
	}
}
int main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		printf( "Romboid tile offsetter\n" );
		printf( "Written by Yuri V. Blazhevich\n" );
		printf( "(C) Nival Interactive, 2001\n" );
		printf( "Usage: OffsetRomb.exe <input file name> [<output file name>]\n" );
		return -1;
	}
	std::string szInputFileName = argv[1], szOutputFileName = argc > 2 ? argv[2] : argv[1];
	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IDataStorage> pStorage = OpenStorage( ".\\", STREAM_ACCESS_READ | STREAM_ACCESS_WRITE );
	CPtr<IDataStream> pStream = pStorage->OpenStream( szInputFileName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_TF( pStream != 0, NStr::Format("Can't open image file \"%s\"", szInputFileName.c_str()), return 0xDEAD );
	CPtr<IImage> pImage = pIP->LoadImage( pStream );
	CImageAccessor image = pImage;
	int nSizeY = pImage->GetSizeY();
	int nSizeX = pImage->GetSizeX();
	RECT rect;
	SetRect( &rect, 0, 0, nSizeX, nSizeY );
	FlipRomb1( image, rect );
	FlipRomb2( image, rect );
	pStream = pStorage->OpenStream( szOutputFileName.c_str(), STREAM_ACCESS_WRITE );
	NI_ASSERT_TF( pStream != 0, NStr::Format("Can't open image file \"%s\" to write", szOutputFileName.c_str()), return 0xDEAD );
	pIP->SaveImageAsTGA( pStream, pImage );
	return 0;
}
