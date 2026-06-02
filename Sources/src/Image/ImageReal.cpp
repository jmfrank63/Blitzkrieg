#include "StdAfx.h"

#include "ImageReal.h"
CImage::CImage( int _nSizeX, int _nSizeY, const std::vector<DWORD> &_data )
	: nSizeX( _nSizeX ), nSizeY( _nSizeY ) 
{ 
	data.resize( _data.size() );
	for ( int i=0; i<data.size(); ++i )
		data[i].color = _data[i];
}
void CImage::Set( SColor color )
{
	std::fill( data.begin(), data.end(), color );
}
bool CImage::SetAlpha( const IImage *pAlpha )
{
  if ( (nSizeX != pAlpha->GetSizeX()) || (nSizeY != pAlpha->GetSizeY()) )
    return false;
	const SColor *pAlphaColors = pAlpha->GetLFB();
  for ( int i=0; i<nSizeX*nSizeY; ++i )
    data[i].color = (data[i].color & 0x00ffffff) | (pAlphaColors[i].color & 0xff000000);
  return true;
}
void CImage::SetAlpha( BYTE alpha )
{
	DWORD dwAlpha = DWORD( alpha ) << 24;
	for ( CImageData::iterator pos = data.begin(); pos != data.end(); ++pos )
		pos->color = (pos->color & 0x00ffffff) | dwAlpha;
}
void CImage::SetColor( DWORD color )
{
	color &= 0x00ffffff;
	for ( CImageData::iterator pos = data.begin(); pos != data.end(); ++pos )
		pos->color = (pos->color & 0xff000000) | color;
}
bool CImage::SetColor( const IImage *pColor )
{
  if ( (nSizeX != pColor->GetSizeX()) || (nSizeY != pColor->GetSizeY()) )
    return false;
	const SColor *pColors = pColor->GetLFB();
  for ( int i=0; i<nSizeX*nSizeY; ++i )
    data[i].color = (data[i].color & 0xff000000) | (pColors[i].color & 0x00ffffff);
  return true;
}
void CImage::FlipY()
{
  CImageData dataline( nSizeX );
  for ( int i=0; i<nSizeY/2; ++i )
  {
    memcpy( &(dataline[0]), &(data[i*nSizeX]), nSizeX*sizeof(DWORD) );
    memcpy( &(data[i*nSizeX]), &(data[(nSizeY - i - 1)*nSizeX]), nSizeX*sizeof(DWORD) );
    memcpy( &(data[(nSizeY - i - 1)*nSizeX]), &(dataline[0]), nSizeX*sizeof(DWORD) );
  }
}
void CImage::Invert()
{
  for ( int i=0; i<nSizeX*nSizeY; ++i )
	{
		data[i].r = 255 - data[i].r;
		data[i].g = 255 - data[i].g;
		data[i].b = 255 - data[i].b;
		data[i].a = 255 - data[i].a;
	}
}
void CImage::InvertAlpha()
{
  for ( int i=0; i<nSizeX*nSizeY; ++i )
		data[i].a = 255 - data[i].a;
}
void CImage::SharpenAlpha( BYTE ref )
{
  for ( int i=0; i<nSizeX*nSizeY; ++i )
		data[i].a = data[i].a >= ref ? 255 : 0;
}
IImage* CImage::Duplicate() const
{
	CImage *pImage = new CImage( nSizeX, nSizeY );
	memcpy( &(pImage->data[0]), &(data[0]), nSizeX * nSizeY * sizeof(SColor) );
	return pImage;
}
inline int Width( const RECT &rect ) { return rect.right - rect.left; }
inline int Height( const RECT &rect ) { return rect.bottom - rect.top; }
bool CImage::CopyFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY )
{
	RECT rcRect;
	if ( pSrcRect == 0 )
		SetRect( &rcRect, 0, 0, pSrc->GetSizeX(), pSrc->GetSizeY() );
	else
		rcRect = *pSrcRect;
	if ( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()) )
	{
		NI_ASSERT_T( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()), "Wrong image size" );
		return false;
	}
	for ( int j=0; j<Height(rcRect); ++j )
	{
		const SColor *pSrcColors = pSrc->GetLine( rcRect.top + j ) + rcRect.left;
		SColor *pDstColors = GetLine( nPosY + j ) + nPosX;
		memcpy( pDstColors, pSrcColors, Width(rcRect) * sizeof(SColor) );
	}
	return true;
}
bool CImage::CopyFromAB( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY )
{
	RECT rcRect;
	if ( pSrcRect == 0 )
		SetRect( &rcRect, 0, 0, pSrc->GetSizeX(), pSrc->GetSizeY() );
	else
		rcRect = *pSrcRect;
	if ( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()) )
	{
		NI_ASSERT_T( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()), "Wrong image size" );
		return false;
	}
	for ( int j=0; j<Height(rcRect); ++j )
	{
		const SColor *pSrcColors = pSrc->GetLine( rcRect.top + j ) + rcRect.left;
		SColor *pDstColors = GetLine( nPosY + j ) + nPosX;
		for ( int i=0; i<Width(rcRect); ++i )
		{
			pDstColors[i].r = ( DWORD( pSrcColors[i].r )*DWORD( pSrcColors[i].a ) + DWORD( pDstColors[i].r )*DWORD( 255 - pSrcColors[i].a ) ) / 255;
			pDstColors[i].g = ( DWORD( pSrcColors[i].g )*DWORD( pSrcColors[i].a ) + DWORD( pDstColors[i].g )*DWORD( 255 - pSrcColors[i].a ) ) / 255;
			pDstColors[i].b = ( DWORD( pSrcColors[i].b )*DWORD( pSrcColors[i].a ) + DWORD( pDstColors[i].b )*DWORD( 255 - pSrcColors[i].a ) ) / 255;
			pDstColors[i].a = Max( pSrcColors[i].a, pDstColors[i].a );
		}
	}
	return true;
}
bool CImage::ModulateAlphaFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY )
{
	RECT rcRect;
	if ( pSrcRect == 0 )
		SetRect( &rcRect, 0, 0, pSrc->GetSizeX(), pSrc->GetSizeY() );
	else
		rcRect = *pSrcRect;
	if ( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()) )
	{
		NI_ASSERT_T( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()), "Wrong image size" );
		return false;
	}
	for ( int j=0; j<Height(rcRect); ++j )
	{
		const SColor *pSrcColors = pSrc->GetLine( rcRect.top + j ) + rcRect.left;
		SColor *pDstColors = GetLine( nPosY + j ) + nPosX;
		for ( int i=0; i<Width(rcRect); ++i )
		{
			pDstColors[i].r = DWORD( pDstColors[i].r ) * DWORD( pSrcColors[i].a ) / 255;
			pDstColors[i].g = DWORD( pDstColors[i].g ) * DWORD( pSrcColors[i].a ) / 255;
			pDstColors[i].b = DWORD( pDstColors[i].b ) * DWORD( pSrcColors[i].a ) / 255;
			pDstColors[i].a = DWORD( pDstColors[i].a ) * DWORD( pSrcColors[i].a ) / 255;
		}
	}
	return true;
}
bool CImage::ModulateColorFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY )
{
	RECT rcRect;
	if ( pSrcRect == 0 )
		SetRect( &rcRect, 0, 0, pSrc->GetSizeX(), pSrc->GetSizeY() );
	else
		rcRect = *pSrcRect;
	if ( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()) )
	{
		NI_ASSERT_T( (nPosX + Width(rcRect) > GetSizeX()) || (nPosY + Height(rcRect) > GetSizeY()), "Wrong image size" );
		return false;
	}
	for ( int j=0; j<Height(rcRect); ++j )
	{
		const SColor *pSrcColors = pSrc->GetLine( rcRect.top + j ) + rcRect.left;
		SColor *pDstColors = GetLine( nPosY + j ) + nPosX;
		for ( int i=0; i<Width(rcRect); ++i )
		{
			pDstColors[i].r = DWORD( pDstColors[i].r ) * DWORD( pSrcColors[i].r ) / 255;
			pDstColors[i].g = DWORD( pDstColors[i].g ) * DWORD( pSrcColors[i].g ) / 255;
			pDstColors[i].b = DWORD( pDstColors[i].b ) * DWORD( pSrcColors[i].b ) / 255;
			pDstColors[i].a = DWORD( pDstColors[i].a ) * DWORD( pSrcColors[i].a ) / 255;
		}
	}
	return true;
}
bool SPixelConvertInfo::InitMaskInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask )
{
	DWORD dwMask, dwBitShift, dwBitCount;
	memset( this, 0, sizeof(*this) );
	dwMask = dwABitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwAMask  = dwABitMask;
	dwABits  = dwBitCount;
	dwAShift = dwBitShift;
	dwMask = dwRBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwRMask  = dwRBitMask;
	dwRBits  = dwBitCount;
	dwRShift = dwBitShift;
	dwMask = dwGBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwGMask  = dwGBitMask;
	dwGBits  = dwBitCount;
	dwGShift = dwBitShift;
	dwMask = dwBBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwBMask  = dwBBitMask;
	dwBBits  = dwBitCount;
	dwBShift = dwBitShift;
	return true;
}
DWORD SPixelConvertInfo::ComposeColor( DWORD dwColor ) const
{
	DWORD a = (dwColor >> 24) & 0xFF;
	a >>= (8 - dwABits);
	a <<= dwAShift;
	DWORD r = (dwColor >> 16) & 0xFF;			// Convert to BYTE
	r >>= (8 - dwRBits);		              // throw away low precision bits
	r <<= dwRShift;					              // move to new position
	DWORD g = (dwColor >> 8) & 0xFF;
	g >>= (8 - dwGBits);
	g <<= dwGShift;
	DWORD b = dwColor & 0xFF;
	b >>= (8 - dwBBits);
	b <<= dwBShift;
	return (r | g | b | a);
}
DWORD SPixelConvertInfo::ComposeColorSlow( DWORD dwColor ) const
{
	DWORD a = DWORD( double( ( dwColor >> 24 ) & 0xFF ) / double( 1 << (8 - dwABits) ) ) << dwAShift;
	DWORD r = DWORD( double( ( dwColor >> 16 ) & 0xFF ) / double( 1 << (8 - dwRBits) ) ) << dwRShift;
	DWORD g = DWORD( double( ( dwColor >>  8 ) & 0xFF ) / double( 1 << (8 - dwGBits) ) ) << dwGShift;
	DWORD b = DWORD( double( ( dwColor       ) & 0xFF ) / double( 1 << (8 - dwBBits) ) ) << dwBShift;
	return (r | g | b | a);
}
DWORD SPixelConvertInfo::DecompColor( DWORD dwColor ) const
{
	DWORD a = ((dwColor & dwAMask) >> dwAShift);
	a <<= (8 - dwABits);
	a <<= 24;
	DWORD r = ((dwColor & dwRMask) >> dwRShift);
	r <<= (8 - dwRBits);
	r <<= 16;
	DWORD g = ((dwColor & dwGMask) >> dwGShift);
	g <<= (8 - dwGBits);
	g <<= 8;
	DWORD b = ((dwColor & dwBMask) >> dwBShift);
	b <<= (8 - dwBBits);
	return (r | g | b | a);
}
