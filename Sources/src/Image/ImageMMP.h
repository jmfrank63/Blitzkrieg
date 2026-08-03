#ifndef __IMAGEMMP_H__
#define __IMAGEMMP_H__
#include "..\Formats\fmtTexture.h"
class CImageDDS : public IDDSImage
{
	OBJECT_MINIMAL_METHODS( CImageDDS );
	std::vector< std::vector<BYTE> > mips;
	int nSizeX, nSizeY;
	SDDSPixelFormat format;
public:
	CImageDDS( int nSizeX, int nSizeY, const SDDSPixelFormat &_format );
	virtual int STDCALL GetSizeX( const int nMipLevel ) const { return nSizeX >> nMipLevel; }
	virtual int STDCALL GetSizeY( const int nMipLevel ) const { return nSizeY >> nMipLevel; }
	virtual int STDCALL GetNumMipLevels() const { return mips.size(); }
	virtual const struct SDDSPixelFormat* STDCALL GetDDSFormat() const { return &format; }
	virtual EGFXPixelFormat STDCALL GetGFXFormat() const;
	virtual int STDCALL GetBPP() const { return ::GetBPP( GetGFXFormat() ); }
	virtual const void* STDCALL GetLFB( const int nMipLevel ) const { return &( mips[nMipLevel][0] ); }
	virtual void* STDCALL GetLFB( const int nMipLevel = 0 ) { return &( mips[nMipLevel][0] ); }
	bool AddMipLevel( const void *pData, int nLength );
	bool AddMipLevels( const IDDSImage *pImage );
	void SetNumMipLevels( const int nNumLevels )
	{
		const int nBPP = GetBPP();
		mips.resize( nNumLevels );
		for ( int i = 0; i < nNumLevels; ++i )
			mips[i].resize( (nSizeX >> i) * (nSizeY >> i) * nBPP / 8 );
	}
	std::vector<BYTE>& GetMipLevel( const int nLevel ) { return mips[nLevel]; }
	const std::vector<BYTE>& GetMipLevel( const int nLevel ) const { return mips[nLevel]; }
	std::vector<BYTE>& AddEmptyMipLevel() { mips.resize( mips.size() + 1 ); return mips.back(); }
};
const bool GetDDSPixelFormat( EGFXPixelFormat format, SDDSPixelFormat *pFormat );
namespace NImage
{
	bool RecognizeFormatDDS( IDataStream *pStream );
	CImageDDS* LoadImageDDS( IDataStream *pStream );
	bool SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImage );
};
#endif // __IMAGEMMP_H__
