#ifndef __IMAGE_H__
#define __IMAGE_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../GFX/GFXTypes.h"
enum
{
	IMAGE_BASE_VALUE	= 0x10070000,
	IMAGE_IMAGE				= IMAGE_BASE_VALUE + 1,
	IMAGE_PROCESSOR		= IMAGE_BASE_VALUE + 2,

	IMAGE_FORCE_DWORD = 0x7fffffff
};
enum EImageScaleMethod
{
	ISM_FILTER   = 1,
	ISM_BOX      = 2,
	ISM_TRIANGLE = 3,
	ISM_BELL     = 4,
	ISM_BSPLINE  = 5,
	ISM_LANCZOS3 = 6,
	ISM_MITCHELL = 7,

	ISM_FORCE_DWORD = 0x7fffffff
};
enum EImageGenerationType
{
	IGT_WHITE         = 1,
	IGT_BLACK         = 2,
	IGT_CHECKER       = 3,
	IGT_SHADOW_INDEX1 = 4,
	IGT_SHADOW_INDEX2 = 5,

	IGT_FORCE_DWORD   = 0x7fffffff
};
struct SColor
{
	union
	{
		DWORD color;
		struct
		{
			DWORD b : 8;
			DWORD g : 8;
			DWORD r : 8;
			DWORD a : 8;
		};
	};
	SColor() : color( 0 ) {  }
	SColor( DWORD dwColor )	: color( dwColor ) {  }
	SColor( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	operator DWORD() const { return color; }
};
interface IImage : public IRefCount
{
	virtual int STDCALL GetSizeX() const = 0;
	virtual int STDCALL GetSizeY() const = 0;
	virtual void STDCALL Set( SColor color ) = 0;
	virtual void STDCALL SetAlpha( BYTE alpha ) = 0;
	virtual bool STDCALL SetAlpha( const IImage *pAlpha ) = 0;
	virtual void STDCALL SetColor( DWORD color ) = 0;
	virtual bool STDCALL SetColor( const IImage *pColor ) = 0;
	virtual const SColor* STDCALL GetLFB() const = 0;
	virtual SColor* STDCALL GetLFB() = 0;
	virtual const SColor* STDCALL GetLine( int nLine ) const = 0;
	virtual SColor* STDCALL GetLine( int nLine ) = 0;
	virtual IImage* STDCALL Duplicate() const = 0;
	virtual bool STDCALL CopyFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY ) = 0;
	virtual bool STDCALL CopyFromAB( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY ) = 0;
	virtual bool STDCALL ModulateAlphaFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY ) = 0;
	virtual bool STDCALL ModulateColorFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY ) = 0;
	virtual void STDCALL FlipY() = 0;
	virtual void STDCALL Invert() = 0;
	virtual void STDCALL InvertAlpha() = 0;
	virtual void STDCALL SharpenAlpha( BYTE ref ) = 0;
};
interface IDDSImage : public IRefCount
{
	virtual int STDCALL GetSizeX( const int nMipLevel = 0 ) const = 0;
	virtual int STDCALL GetSizeY( const int nMipLevel = 0 ) const = 0;
	virtual int STDCALL GetNumMipLevels() const = 0;
	virtual const struct SDDSPixelFormat* STDCALL GetDDSFormat() const = 0;
	virtual EGFXPixelFormat STDCALL GetGFXFormat() const = 0;
	virtual int STDCALL GetBPP() const = 0;
	virtual const void* STDCALL GetLFB( const int nMipLevel = 0 ) const = 0;
	virtual void* STDCALL GetLFB( const int nMipLevel = 0 ) = 0;
};
interface IImageProcessor : public IRefCount
{
	enum { tidTypeID = IMAGE_PROCESSOR };
	virtual IImage* STDCALL LoadImage( IDataStream *pStream ) const = 0;
	virtual IDDSImage* STDCALL LoadDDSImage( IDataStream *pStream ) const = 0;
	virtual bool STDCALL SaveImageAsPNG( IDataStream *pStream, const IImage *pImage ) const = 0;
	virtual bool STDCALL SaveImageAsTGA( IDataStream *pStream, const IImage *pImage ) const = 0;
	virtual bool STDCALL SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImage ) const = 0;
	virtual IImage* STDCALL CreateImage( int nSizeX, int nSizeY ) = 0;
	virtual IImage* STDCALL CreateImage( int nSizeX, int nSizeY, void *pData ) = 0;
	virtual IImage* STDCALL CreateScale( const IImage *pImage, float fScaleFactor, EImageScaleMethod method ) const = 0;
	virtual IImage* STDCALL CreateScale( const IImage *pImage, float fScaleX, float fScaleY, EImageScaleMethod method ) const = 0;
	virtual IImage* STDCALL CreateScaleBySize( const IImage *pImage, int nSizeX, int nSizeY, EImageScaleMethod method ) const = 0;
	virtual IImage* STDCALL CreateMip( const IImage *pImage, int nLevel ) const = 0;
	virtual IImage* STDCALL CreateGammaCorrection( IImage *pSrc, float fBrightness, float fContrast, float fGamma ) = 0;
	virtual IDDSImage* STDCALL Compress( const IImage *pSrcImage, EGFXPixelFormat format ) const = 0;
	virtual IImage* STDCALL Decompress( const IDDSImage *pSrcImage ) const = 0;
	virtual IDDSImage* STDCALL GenerateAndCompress( const IImage *pSrcImage, EGFXPixelFormat format, int nNumMipLevels ) const = 0;
	virtual void STDCALL RestoreImage( IImage *pImage, const SColor &bg ) = 0;
	virtual IImage* STDCALL ComposeImages( IImage **pImages, RECT *pRects, RECT *pRectsMain, int nNumImages, int nSizeX = 0, int nSizeY = 0 ) const = 0;
	virtual IImage* STDCALL GenerateImage( int nSizeX, int nSizeY, int nType ) = 0;
};
inline IImageProcessor* GetImageProcessor()
{
	return GetSingleton<IImageProcessor>();
}
template < class TColor = SColor, class TImage = IImage, class TImagePtr = CPtr<TImage> >
class CTImageAccessor
{
	TImagePtr pImage;
	std::vector<TColor*> rows;
	void Set( TImage *_pImage )
	{
		pImage = _pImage;
		if ( pImage )
		{
			rows.resize( pImage->GetSizeY() );
			TColor *pColors = reinterpret_cast<TColor*>( pImage->GetLFB() );
			const int nSizeX = pImage->GetSizeX();
			for ( int i=0; i<rows.size(); ++i )
				rows[i] = pColors + i*nSizeX;
		}
	}
public:
	CTImageAccessor() {  }
	CTImageAccessor( const TImagePtr &_pImage ) { Set( _pImage ); }
	CTImageAccessor( TImage *_pImage ) { Set(_pImage); }
	const CTImageAccessor& operator=( const TImagePtr &_pImage ) { Set( _pImage ); return *this; }
	const CTImageAccessor& operator=( TImage *_pImage ) { Set( _pImage ); return *this; }
	const CTImageAccessor& operator=( const CTImageAccessor &accessor ) { Set( accessor.pImage ); return *this; }
	operator TImage*() const { return pImage; }
	TImage* operator->() const { return pImage; }
	bool operator==( const CTImageAccessor &ptr ) const { return ( pImage == ptr.pImage ); }
	bool operator==( TImage *pNewObject ) const { return ( pImage == pNewObject ); }
	bool operator!=( const CTImageAccessor &ptr ) const { return ( pImage != ptr.pImage ); }
	bool operator!=( TImage *pNewObject ) const { return ( pImage != pNewObject ); }
	const TColor* operator[]( int nY ) const { return rows[nY]; }
	TColor* operator[]( int nY ) { return rows[nY]; }
	const TColor& operator()( int nX, int nY ) const { return rows[nY][nX]; }
	TColor& operator()( int nX, int nY ) { return rows[nY][nX]; }
};
typedef CTImageAccessor<> CImageAccessor;
typedef CTImageAccessor<SColor, IImage, IImage*> CImagePtrAccessor;
enum ECompressionType
{
	COMPRESSION_DXT,
	COMPRESSION_LOW_QUALITY,
	COMPRESSION_HIGH_QUALITY,
};
#endif // __IMAGE_H__
