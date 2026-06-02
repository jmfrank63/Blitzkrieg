#ifndef __IMAGEPROCESSOR_H__
#define __IMAGEPROCESSOR_H__
class CImageProcessor : public IImageProcessor
{
	OBJECT_NORMAL_METHODS( CImageProcessor );
public:
	virtual IImage* STDCALL LoadImage( IDataStream *pStream ) const;
	virtual IDDSImage* STDCALL LoadDDSImage( IDataStream *pStream ) const;
	virtual bool STDCALL SaveImageAsPNG( IDataStream *pStream, const IImage *pImage ) const;
	virtual bool STDCALL SaveImageAsTGA( IDataStream *pStream, const IImage *pImage ) const;
	virtual bool STDCALL SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImage ) const;
	virtual IImage* STDCALL CreateImage( int nSizeX, int nSizeY );
	virtual IImage* STDCALL CreateImage( int nSizeX, int nSizeY, void *pData );
	virtual IImage* STDCALL CreateScale( const IImage *pImage, float fScaleFactor, EImageScaleMethod method ) const;
	virtual IImage* STDCALL CreateScale( const IImage *pImage, float fScaleX, float fScaleY, EImageScaleMethod method ) const;
	virtual IImage* STDCALL CreateScaleBySize( const IImage *pImage, int nSizeX, int nSizeY, EImageScaleMethod method ) const;
	virtual IImage* STDCALL CreateMip( const IImage *pImage, int nLevel ) const;
	virtual IImage* STDCALL CreateGammaCorrection( IImage *pSrc, float fBrightness, float fContrast, float fGamma );
	virtual IDDSImage* STDCALL Compress( const IImage *pSrcImage, EGFXPixelFormat format ) const;
	virtual IImage* STDCALL Decompress( const IDDSImage *pSrcImage ) const;
	virtual IDDSImage* STDCALL GenerateAndCompress( const IImage *pSrcImage, EGFXPixelFormat format, int nNumMipLevels ) const;
	virtual void STDCALL RestoreImage( IImage *pImage, const SColor &bg );
	virtual IImage* STDCALL ComposeImages( IImage **pImages, RECT *pRects,RECT *pRectsMain ,int nNumImages, int nSizeX, int nSizeY ) const;
	virtual IImage* STDCALL GenerateImage( int nSizeX, int nSizeY, int nType );
};
#endif // __IMAGEPROCESSOR_H__
