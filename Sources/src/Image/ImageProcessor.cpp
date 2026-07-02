#include "StdAfx.h"

#include "ImageProcessor.h"

#include "ImageBMP.h"
#include "ImagePNG.h"
#include "ImageTGA.h"
#include "ImageMMP.h"
#include "DxtCodec.h"

namespace
{
	NDxt::Format GetDxtFormat( EGFXPixelFormat format )
	{
		switch ( format )
		{
			case GFXPF_DXT1:
				return NDxt::Format::DXT1;
			case GFXPF_DXT2:
				return NDxt::Format::DXT2;
			case GFXPF_DXT3:
				return NDxt::Format::DXT3;
			case GFXPF_DXT4:
				return NDxt::Format::DXT4;
			case GFXPF_DXT5:
				return NDxt::Format::DXT5;
		}
		return NDxt::Format::DXT1;
	}

	bool IsDxtFormat( EGFXPixelFormat format )
	{
		return ( format >= GFXPF_DXT1 ) && ( format <= GFXPF_DXT5 );
	}

	bool InitRawPixelConvertInfo( EGFXPixelFormat format, SPixelConvertInfo *pInfo )
	{
		switch ( format )
		{
			case GFXPF_ARGB8888:
				pInfo->InitMaskInfo( 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff );
				return true;
			case GFXPF_ARGB1555:
				pInfo->InitMaskInfo( 0x00008000, 0x00007c00, 0x000003e0, 0x0000001f );
				return true;
			case GFXPF_ARGB4444:
				pInfo->InitMaskInfo( 0x0000f000, 0x00000f00, 0x000000f0, 0x0000000f );
				return true;
			case GFXPF_ARGB0565:
				pInfo->InitMaskInfo( 0x00000000, 0x0000f800, 0x000007e0, 0x0000001f );
				return true;
		}
		return false;
	}
}
IImage* CImageProcessor::LoadImage( IDataStream *pStream ) const
{
	NI_ASSERT_T( pStream != 0, "Can't load to NULL stream" );
  if ( NImage::RecognizeFormatPNG(pStream) )
    return NImage::LoadImagePNG( pStream );
  else if ( NImage::RecognizeFormatBMP(pStream) )
    return NImage::LoadImageBMP( pStream );
  else if ( NImage::RecognizeFormatTGA(pStream) )
    return NImage::LoadImageTGA( pStream );
	else 
		return 0;
}
IDDSImage* CImageProcessor::LoadDDSImage( IDataStream *pStream ) const
{
	return NImage::LoadImageDDS( pStream );
}
bool CImageProcessor::SaveImageAsPNG( IDataStream *pStream, const IImage *pImage ) const
{
	return NImage::SaveImageAsPNG( pStream, pImage );
}
bool CImageProcessor::SaveImageAsTGA( IDataStream *pStream, const IImage *pImage ) const
{
	return NImage::SaveImageAsTGA( pStream, pImage );
}
bool CImageProcessor::SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImage ) const
{
	return NImage::SaveImageAsDDS( pStream, pImage );
}
void ImageScale( const CImage *pSrcImg, CImage *pDstImg, EImageScaleMethod method );
IImage* CImageProcessor::CreateScale( const IImage *pImage, float fScaleFactor, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( pImage->GetSizeX()*fScaleFactor, pImage->GetSizeY()*fScaleFactor );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
IImage* CImageProcessor::CreateScale( const IImage *pImage, float fScaleX, float fScaleY, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( pImage->GetSizeX()*fScaleX, pImage->GetSizeY()*fScaleY );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
IImage* CImageProcessor::CreateScaleBySize( const IImage *pImage, int nSizeX, int nSizeY, EImageScaleMethod method ) const
{
	CImage *pScale = new CImage( nSizeX, nSizeY );
	ImageScale( static_cast<const CImage*>(pImage), pScale, method );
	return pScale;
}
IImage* CImageProcessor::CreateMip( const IImage *pImage, int nLevel ) const
{
	return CreateScale( pImage, 1.0 / double( 1UL << nLevel ), ISM_LANCZOS3 );
}
IDDSImage* CompressDXTN( const IImage *pImage, EGFXPixelFormat format )
{
	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	const NDxt::Format dxtFormat = GetDxtFormat( format );
	int nNumCompressedBytes = NDxt::GetEncodedSize( pImage->GetSizeX(), pImage->GetSizeY(), dxtFormat );
	CImageDDS *pImageMMP = new CImageDDS( pImage->GetSizeX(), pImage->GetSizeY(), ddsformat );
	std::vector<BYTE> &outdata = pImageMMP->AddEmptyMipLevel();
	outdata.resize( nNumCompressedBytes );
	NDxt::DxtSurfaceDesc input = { pImage->GetSizeX(), pImage->GetSizeY(), pImage->GetSizeX() * 4, pImage->GetLFB() };
	NDxt::Encode( input, dxtFormat, &( outdata[0] ) );
	return pImageMMP;
}
IDDSImage* CompressRGBA( const IImage *pImage, EGFXPixelFormat format )
{
	SPixelConvertInfo pci;
	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	if ( !InitRawPixelConvertInfo( format, &pci ) )
		return false;
	int nSizeX = pImage->GetSizeX();
	int nSizeY = pImage->GetSizeY();
	int nBPP = ::GetBPP( format );
	
	CImageDDS *pImageMMP = new CImageDDS( nSizeX, nSizeY, ddsformat );
	std::vector<BYTE> &outdata = pImageMMP->AddEmptyMipLevel();
	outdata.resize( nSizeX * nSizeY * nBPP / 8 );

	const DWORD *pSrc = reinterpret_cast<const DWORD*>( pImage->GetLFB() );
	if ( nBPP == 16 )
	{
		WORD *pDst = reinterpret_cast<WORD*>( &( outdata[0] ) );
		for ( int i=0; i<nSizeX*nSizeY; ++i, ++pDst )
			*pDst = pci.ComposeColorSlow( pSrc[i] );
		return pImageMMP;
	}
	else if ( nBPP == 32 )
	{
		DWORD *pDst = reinterpret_cast<DWORD*>( &( outdata[0] ) );
		memcpy( pDst, pSrc, nSizeX*nSizeY*nBPP/8 );
		return pImageMMP;
	}
	delete pImageMMP;
	return 0;
}
IDDSImage* CImageProcessor::Compress( const IImage *pImage, EGFXPixelFormat format ) const
{
	if ( (format >= GFXPF_DXT1) && (format <= GFXPF_DXT5) )
		return CompressDXTN( pImage, format );
	else if ( (format >= GFXPF_ARGB8888) && (format <= GFXPF_ARGB0565) )
		return CompressRGBA( pImage, format );
	else
		return 0;
}
IImage* CImageProcessor::Decompress( const IDDSImage *pImage ) const
{
	if ( pImage->GetGFXFormat() == GFXPF_ARGB8888 ) 
	{
		CImage *pDstImage = new CImage( pImage->GetSizeX(0), pImage->GetSizeY(0) );
		memcpy( pDstImage->GetLFB(), pImage->GetLFB(), pImage->GetSizeX(0)*pImage->GetSizeY(0)*sizeof(SColor) );
		return pDstImage;
	}
	if ( IsDxtFormat( pImage->GetGFXFormat() ) )
	{
		std::vector<DWORD> outdata( NDxt::GetDecodedSize( pImage->GetSizeX( 0 ), pImage->GetSizeY( 0 ) ) / 4 );
		const int blockSize = pImage->GetBPP() == 4 ? 8 : 16;
		const int nPitch = ( pImage->GetSizeX( 0 ) + 3 ) / 4 * blockSize;
		NDxt::DxtSurfaceDesc input = { pImage->GetSizeX( 0 ), pImage->GetSizeY( 0 ), nPitch, pImage->GetLFB( 0 ) };
		NDxt::Decode( input, GetDxtFormat( pImage->GetGFXFormat() ), &( outdata[0] ) );
		return new CImage( pImage->GetSizeX(0), pImage->GetSizeY(0), outdata );
	}

	SPixelConvertInfo pci;
	if ( !InitRawPixelConvertInfo( pImage->GetGFXFormat(), &pci ) )
		return 0;
	const int nNumPixels = pImage->GetSizeX(0) * pImage->GetSizeY(0);
	std::vector<DWORD> outdata( nNumPixels );
	const int nBPP = pImage->GetBPP();
	if ( nBPP == 16 )
	{
		const WORD *pSrc = reinterpret_cast<const WORD*>( pImage->GetLFB( 0 ) );
		for ( int i = 0; i != nNumPixels; ++i )
		{
			outdata[i] = pci.DecompColor( pSrc[i] );
			if ( pImage->GetGFXFormat() == GFXPF_ARGB0565 )
				outdata[i] |= 0xff000000;
		}
	}
	else if ( nBPP == 32 )
	{
		memcpy( &( outdata[0] ), pImage->GetLFB( 0 ), nNumPixels * sizeof( DWORD ) );
	}
	else
		return 0;
	CImage *pDstImage = new CImage( pImage->GetSizeX(0), pImage->GetSizeY(0), outdata );
	return pDstImage;
}
IDDSImage* CImageProcessor::GenerateAndCompress( const IImage *pSrcImage, EGFXPixelFormat format, int nNumMipLevels ) const
{
	SDDSPixelFormat ddsformat;
	GetDDSPixelFormat( format, &ddsformat );
	CImageDDS *pResultMMP = new CImageDDS( pSrcImage->GetSizeX(), pSrcImage->GetSizeY(), ddsformat );

	CPtr<IDDSImage> pMMP = Compress( pSrcImage, format );
	pResultMMP->AddMipLevels( pMMP );
	for ( int i=1; i<nNumMipLevels; ++i )
	{
		CPtr<IImage> pScaled = CreateMip( pSrcImage, i );
		CPtr<IDDSImage> pMMP = Compress( pScaled, format );
		pResultMMP->AddMipLevels( pMMP );
	}
	return pResultMMP;
}
IImage* CImageProcessor::CreateImage( int nSizeX, int nSizeY )
{
	return new CImage( nSizeX, nSizeY );
}
IImage* CImageProcessor::CreateImage( int nSizeX, int nSizeY, void *pData )
{
	CImage *pImage = new CImage( nSizeX, nSizeY );
	memcpy( pImage->GetLFB(), pData, nSizeX*nSizeY*4 );
	return pImage;
}
void CImageProcessor::RestoreImage( IImage *pImage, const SColor &bg )
{
	SColor *pColors = pImage->GetLFB();
	float fBGr = float( bg.r ), fBGg = float( bg.g ), fBGb = float( bg.b );
	for ( int i=0; i<pImage->GetSizeX()*pImage->GetSizeY(); ++i )
	{
		if ( pColors[i].a != 0 )
		{
			float fAlpha = float( pColors[i].a ) / 255.0f;
			float fValue = ( float(pColors[i].r) - fBGr * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].r = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
			fValue = ( float(pColors[i].g) - fBGg * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].g = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
			fValue = ( float(pColors[i].b) - fBGb * (1.0f - fAlpha) ) / fAlpha;
			pColors[i].b = BYTE( Max( 0.0f, Min( fValue, 255.0f ) ) );
		}
	}
}
IImage* CImageProcessor::GenerateImage( int nSizeX, int nSizeY, int nType )
{
	IImage *pImage = 0;
	switch ( nType )
	{
		case IGT_WHITE:
			pImage = CreateImage( nSizeX, nSizeY );
			pImage->Set( bit_cast<SColor>( 0xffffffff ) );
			break;
		case IGT_BLACK:
			pImage = CreateImage( nSizeX, nSizeY );
			pImage->Set( bit_cast<SColor>( 0xff000000 ) );
			break;
		case IGT_CHECKER:
			pImage = CreateImage( nSizeX, nSizeY );
			for ( int i=0; i<nSizeY; ++i )
			{
				SColor *pColors = pImage->GetLine( i );
				bool bOddY = ( ( i / (nSizeY / 16) ) & 1 ) != 0;
				for ( int j=0; j<nSizeX; ++j )
				{
					bool bOddX = ( ( j / (nSizeX / 16) ) & 1 ) != 0;
					pColors[j] = ( bOddX == bOddY ) ? 0xffffffff : 0xff000000;
				}
			}
			break;
		case IGT_SHADOW_INDEX1:
			NI_ASSERT_T( 0, "still not realized" );
			break;
		case IGT_SHADOW_INDEX2:
			NI_ASSERT_T( 0, "still not realized" );
			break;
	}
	return pImage;
}
inline BYTE GetGammaCorrection( BYTE val, float fBrightness, float fPower, float fA, float fB )
{
  const float fVal = float( val ) / 255.0f;
  const float fGammaValue = pow( fVal, fPower );
  const float fContrastValue = Clamp( fA*fGammaValue + fB, 0.0f, 1.0f );
  const float fResult = Clamp( fContrastValue + fBrightness, 0.0f, 1.0f );
	return BYTE( fResult * 255.0f );
}
IImage* CImageProcessor::CreateGammaCorrection( IImage *pSrc, float fBrightness, float fContrast, float fGamma )
{
	if ( (fBrightness == 0) && (fContrast == 0) && (fGamma == 0) )
		return CreateImage( pSrc->GetSizeX(), pSrc->GetSizeY(), pSrc->GetLFB() );
	IImage *pDst = CreateImage( pSrc->GetSizeX(), pSrc->GetSizeY() );
  fBrightness = Clamp( fBrightness, -1.0f, 1.0f ) * 0.5f; // to avoid complete dark and complete white values
  fContrast = Clamp( fContrast, -1.0f, 1.0f ) * 0.5f;
  fGamma = Clamp( fGamma, -1.0f, 1.0f ) * 0.5f;
  float fA = 1.0f + 4.0f*fabs( fContrast );
  if ( fContrast < 0 )
    fA = 1.0f / fA;
  float fB = 0.5f*( 1.0f - fA );
  float fPower = 1;
  {
    if ( fGamma > 0 )
      fPower = 1.0f / ( 5.0f*fGamma + 1 );
    else if ( fGamma < 0 )
      fPower = 1.0f / ( 0.5f*fGamma + 1 );
  }
	for ( int i = 0; i != pSrc->GetSizeY(); ++i )
	{
		SColor *pDstColor = pDst->GetLine( i );
		SColor *pSrcColor = pSrc->GetLine( i );
		for ( int j = 0; j != pSrc->GetSizeX(); ++j )
		{
			pDstColor[j].a = pSrcColor[j].a;
			pDstColor[j].r = GetGammaCorrection( pSrcColor[j].r, fBrightness, fPower, fA, fB );
			pDstColor[j].g = GetGammaCorrection( pSrcColor[j].g, fBrightness, fPower, fA, fB );
			pDstColor[j].b = GetGammaCorrection( pSrcColor[j].b, fBrightness, fPower, fA, fB );
		}
	}
	return pDst;
}
