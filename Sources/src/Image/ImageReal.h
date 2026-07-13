#ifndef __IMAGEREAL_H__
#define __IMAGEREAL_H__
#include "Image.h"
#include <vector>
class CImage : public IImage
{
	OBJECT_MINIMAL_METHODS( CImage );
	typedef std::vector<SColor> CImageData;
	CImageData data;
	int nSizeX;
	int nSizeY;
public:
	CImage( int _nSizeX, int _nSizeY, const std::vector<DWORD> &_data );
	CImage( int _nSizeX, int _nSizeY ) : nSizeX( _nSizeX ), nSizeY( _nSizeY ), data( static_cast<size_t>(_nSizeX) * static_cast<size_t>(_nSizeY) ) {  }
	virtual int STDCALL GetSizeX() const { return nSizeX; }
	virtual int STDCALL GetSizeY() const { return nSizeY; }
	virtual void STDCALL Set( SColor color );
	virtual void STDCALL SetAlpha( BYTE alpha );
	virtual bool STDCALL SetAlpha( const IImage *pAlpha );
	virtual void STDCALL SetColor( DWORD color );
	virtual bool STDCALL SetColor( const IImage *pColor );

	SColor Get( int nX, int nY ) const { return data[static_cast<size_t>(nY)*nSizeX + nX]; }
	void Set( int nX, int nY, DWORD dwColor ) { data[static_cast<size_t>(nY)*nSizeX + nX].color = dwColor; }
	void Set( DWORD dwColor ) { std::fill(data.begin(), data.end(), dwColor); }
	void SetAlpha( int nX, int nY, DWORD dwColor ) { size_t idx = static_cast<size_t>(nY)*nSizeX + nX; data[idx] = (data[idx].color & 0x00FFFFFF) | (dwColor & 0xFF000000); }
	const SColor& operator()( int nX, int nY ) const { return data[static_cast<size_t>(nY)*nSizeX + nX]; }
	SColor& operator()( int nX, int nY ) { return data[static_cast<size_t>(nY)*nSizeX + nX]; }
	const SColor* operator[]( int nY ) const { return &( data[static_cast<size_t>(nY)*nSizeX] ); }
	SColor* operator[]( int nY ) { return &( data[static_cast<size_t>(nY)*nSizeX] ); }
	virtual const SColor* STDCALL GetLFB() const { return &( data[0] ); }
	virtual SColor* STDCALL GetLFB() { return &( data[0] ); }
	virtual const SColor* STDCALL GetLine( int nLine ) const { return &( data[nLine*nSizeX] ); }
	virtual SColor* STDCALL GetLine( int nLine ) { return &( data[nLine*nSizeX] ); }
	virtual IImage* STDCALL Duplicate() const;
	virtual bool STDCALL CopyFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY );
	virtual bool STDCALL CopyFromAB( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY );
	virtual bool STDCALL ModulateAlphaFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY );
	virtual bool STDCALL ModulateColorFrom( IImage *pSrc, const RECT *pSrcRect, int nPosX, int nPosY );
	virtual void STDCALL FlipY();
	virtual void STDCALL Invert();
	virtual void STDCALL InvertAlpha();
	virtual void STDCALL SharpenAlpha( BYTE ref );
};
struct SPixelConvertInfo
{
public:
	DWORD dwBitDepth;                     // bit depth of this format
	DWORD dwAMask;		                    // bit mask
	DWORD dwABits;		                    // # of bits in mask
	DWORD dwAShift;		                    // # of bits to shift down to canonical position
	DWORD dwRMask;		                    // bit mask
	DWORD dwRBits;		                    // # of bits in mask
	DWORD dwRShift;		                    // # of bits to shift down to canonical position
	DWORD dwGMask;		                    // bit mask
	DWORD dwGBits;		                    // # of bits in mask
	DWORD dwGShift;		                    // # of bits to shift down to canonical position
	DWORD dwBMask;		                    // bit mask
	DWORD dwBBits;		                    // # of bits in mask
	DWORD dwBShift;		                    // # of bits to shift down to canonical position
public:
	SPixelConvertInfo() {  }
	SPixelConvertInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask ) { InitMaskInfo( dwABitMask, dwRBitMask, dwGBitMask, dwBBitMask ); }
	bool InitMaskInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask );
	DWORD ComposeColor( DWORD dwColor ) const;
	DWORD ComposeColorSlow( DWORD dwColor ) const;
	DWORD DecompColor( DWORD dwColor ) const;
};
#endif // __IMAGEREAL_H__
