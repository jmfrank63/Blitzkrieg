#include "StdAfx.h"

#include "ImageMMP.h"

#include "..\Formats\fmtTexture.h"
namespace NImage
{
bool RecognizeFormatDDS( IDataStream *pStream )
{
	DWORD dwSignature = 0;
	const int nCounter = pStream->Read( &dwSignature, sizeof(dwSignature) );
	pStream->Seek( -nCounter, STREAM_SEEK_CUR );
	if ( nCounter != sizeof(dwSignature) )
		return false;
	return dwSignature == SDDSHeader::SIGNATURE;
}
CImageDDS* LoadImageDDS( IDataStream *pStream )
{
	DWORD dwSignature = 0;
	const int nCounter = pStream->Read( &dwSignature, sizeof(dwSignature) );
	if ( dwSignature != SDDSHeader::SIGNATURE ) 
	{
		pStream->Seek( -nCounter, STREAM_SEEK_CUR );
		return 0;
	}
	SDDSHeader header;
	pStream->Read( &header, sizeof(header) );
	const int nNumMipLevels = header.dwHeaderFlags & DDS_HEADER_FLAGS_MIPMAP ? (header.dwMipMapCount == 0 ? 1 : header.dwMipMapCount) : 1;
	CImageDDS *pImage = new CImageDDS( header.dwWidth, header.dwHeight, header.ddspf );
	pImage->SetNumMipLevels( nNumMipLevels );
	for ( int i = 0; i < nNumMipLevels; ++i )
	{
		std::vector<BYTE> &level = pImage->GetMipLevel( i );
		const int nCheck = pStream->Read( &(level[0]), level.size() );
		NI_ASSERT_T( nCheck == level.size(), NStr::Format("Can't read all data for mip level %d (read %d bytes instead of %d)", i, nCheck, level.size()) );
	}
	return pImage;
}
bool SaveImageAsDDS( IDataStream *pStream, const IDDSImage *pImg )
{
	const CImageDDS *pImage = static_cast<const CImageDDS*>( pImg );
	SDDSHeader header;
	header.ddspf = *( pImage->GetDDSFormat() );
	header.dwWidth = pImage->GetSizeX( 0 );
	header.dwHeight = pImage->GetSizeY( 0 );
	header.dwMipMapCount = pImage->GetNumMipLevels();
	header.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
	header.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;
	if ( header.ddspf.dwFlags == DDS_FOURCC )
	{
		header.dwHeaderFlags |= DDS_HEADER_FLAGS_LINEARSIZE;
		header.dwPitchOrLinearSize = header.dwWidth * header.dwHeight * pImage->GetBPP() / 8;
	}
	else
	{
		header.dwHeaderFlags |= DDS_HEADER_FLAGS_PITCH;
		header.dwPitchOrLinearSize = header.dwWidth * pImage->GetBPP() / 8;
	}
	const DWORD dwSignature = SDDSHeader::SIGNATURE;
	pStream->Write( &dwSignature, sizeof(dwSignature) );
	const int nCheck = pStream->Write( &header, sizeof(header) );
	NI_ASSERT_T( nCheck == sizeof(header), "Can't write header" );
	if ( nCheck != sizeof(header) ) 
		return false;
	for ( int i = 0; i < int(header.dwMipMapCount); ++i )
	{
		const std::vector<BYTE> &level = pImage->GetMipLevel( i );
		const int nCheck = pStream->Write( &(level[0]), level.size() );
		NI_ASSERT_T( nCheck == level.size(), NStr::Format("Can't write mip level %d", i) );
		if ( nCheck != level.size() ) 
			return false;
	}
	return true;
}
};
CImageDDS::CImageDDS( int _nSizeX, int _nSizeY, const SDDSPixelFormat &_format )
: nSizeX( _nSizeX ), nSizeY( _nSizeY ), format( _format )
{
	int nSize = Min( nSizeX, nSizeY );
	int nPow2 = GetMSB( nSize );
	mips.reserve( nPow2 + 1 );
}
EGFXPixelFormat CImageDDS::GetGFXFormat() const
{
	if ( format.dwFlags & DDS_FOURCC ) 
	{
		switch ( format.dwFourCC ) 
		{
			case MAKEFOURCC('D','X','T','1'):	return GFXPF_DXT1;
			case MAKEFOURCC('D','X','T','2'):	return GFXPF_DXT2;
			case MAKEFOURCC('D','X','T','3'):	return GFXPF_DXT3;
			case MAKEFOURCC('D','X','T','4'):	return GFXPF_DXT4;
			case MAKEFOURCC('D','X','T','5'):	return GFXPF_DXT5;
		}
	}
	else if ( (format.dwFlags & DDS_ARGB) == DDS_ARGB )
	{
		switch ( format.dwRGBBitCount ) 
		{
			case 32: return GFXPF_ARGB8888;
			case 16: return format.dwRBitMask == 0x00007c00 ? GFXPF_ARGB1555 : GFXPF_ARGB4444;
		}
	}
	else if ( (format.dwFlags & DDS_RGB) == DDS_RGB )
	{
		if ( format.dwRBitMask == 0x0000f800 ) 
			return GFXPF_ARGB0565;
	}
	return GFXPF_UNKNOWN;
}
bool CImageDDS::AddMipLevel( const void *pData, int nLength )
{
	mips.resize( mips.size() + 1 );
	mips.back().resize( nLength );
	memcpy( &( mips.back()[0] ), pData, nLength );
	return true;
}
bool CImageDDS::AddMipLevels( const IDDSImage *pImage )
{
	if ( ( pImage->GetSizeX(0) != GetSizeX(mips.size()) ) || ( pImage->GetSizeY(0) != GetSizeY(mips.size()) ) )
		return false;
	int nSize = Min( nSizeX, nSizeY );
	int nMaxMips = GetMSB( nSize );
	if ( format.dwFlags == DDS_FOURCC  )
		nMaxMips -= 2;
	int nNumMipLevels = Min( int(nMaxMips - mips.size()), pImage->GetNumMipLevels() );
	for ( int i=0; i<nNumMipLevels; ++i )
		AddMipLevel( pImage->GetLFB( i ), GetSizeX(mips.size() + 1) * GetSizeY(mips.size() + 1) * GetBPP() / 8 );

	return true;
}
const bool GetDDSPixelFormat( EGFXPixelFormat format, SDDSPixelFormat *pFormat )
{
	switch ( format ) 
	{
		case GFXPF_DXT1:
			*pFormat = DDSPF_DXT1;
			break;
		case GFXPF_DXT2:
			*pFormat = DDSPF_DXT2;
			break;
		case GFXPF_DXT3:
			*pFormat = DDSPF_DXT3;
			break;
		case GFXPF_DXT4:
			*pFormat = DDSPF_DXT4;
			break;
		case GFXPF_DXT5:
			*pFormat = DDSPF_DXT5;
			break;
		case GFXPF_ARGB8888:
			*pFormat = DDSPF_A8R8G8B8;
			break;
		case GFXPF_ARGB4444:
			*pFormat = DDSPF_A4R4G4B4;
			break;
		case GFXPF_ARGB1555:
			*pFormat = DDSPF_A1R5G5B5;
			break;
		case GFXPF_ARGB0565:
			*pFormat = DDSPF_R5G6B5;
			break;
		default:
			return false;
	}
	return true;
}
