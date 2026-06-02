#include "StdAfx.h"

#include "ImageTGA.h"
enum ETGAImageType
{
	TGAIT_NOIMAGEDATA				= 0,
	TGAIT_COLOR_MAPPED			= 1,
	TGAIT_TRUE_COLOR				= 2,
	TGAIT_BLACK_WHITE				= 3,
	TGAIT_RLE_COLOR_MAPPED	= 9,
	TGAIT_RLE_TRUE_COLOR		= 10,
	TGAIT_RLE_BLACK_WHITE		= 11
};
#pragma pack ( 1 )
struct SColor24 
{ 
	BYTE b;
	BYTE g;
	BYTE r; 
};
struct SColorMapSpecification 
{
	WORD wFirstEntryIndex;								// Index of the first color map entry. Index refers to the starting entry in loading the color map.
	WORD wColorMapLength;									// Total number of color map entries included
	BYTE cColorMapEntrySize;							// Establishes the number of bits per entry. Typically 15, 16, 24 or 32-bit values are used

};
struct SImageDescriptor
{
	BYTE cAlphaChannelBits : 4;						// the number of attribute bits per pixel
	BYTE cLeftToRightOrder : 1;						// left-to-right ordering 
	BYTE cTopToBottomOrder : 1;						// top-to-bottom ordering 
	BYTE cUnused           : 2;						// Must be zero to insure future compatibility
};
struct SImageSpecification
{
	WORD wXOrigin;												// absolute horizontal coordinate for the lower left corner of the image as it is positioned on a display device having an origin at the lower left of the screen 
	WORD wYOrigin;												// absolute vertical coordinate for the lower left corner of the image as it is positioned on a display device having an origin at the lower left of the screen
	WORD wImageWidth;											// width of the image in pixels
	WORD wImageHeight;										// height of the image in pixels
	BYTE cPixelDepth;											// number of bits per pixel. This number includes the Attribute or Alpha channel bits. Common values are 8, 16, 24 and 32 but other pixel depths could be used.
	SImageDescriptor descriptor;					//
};
struct STGAFileHeader
{
	BYTE cIDLength;												// identifies the number of bytes contained in Field 6, the Image ID Field
	BYTE cColorMapType;										// indicates the type of color map (if any) included with the image
	BYTE cImageType;											// TGA File Format can be used to store Pseudo-Color, True-Color and Direct-Color images of various pixel depths
	SColorMapSpecification colormap;      //
	SImageSpecification imagespec;				// 
};
struct STGAFileFooter
{
	DWORD dwExtensionAreaOffset;					// offset from the beginning of the file to the start of the Extension  Area
	DWORD dwDeveloperDirectoryOffset;			// offset from the beginning of the file to the start of the Developer Directory
	BYTE cSignature[16];									// "TRUEVISION-XFILE"
	BYTE cReservedCharacter;							// must be '.'
	BYTE cBinaryZeroStringTerminator;			// '\0'
};
#pragma pack()
bool NImage::RecognizeFormatTGA( IDataStream *pStream )
{
	pStream->Seek( -26, STREAM_SEEK_END );
	STGAFileFooter footer;
	pStream->Read( &footer, sizeof(footer) );
	char pszSignature[32];
	memcpy( pszSignature, footer.cSignature, 16 );
	pszSignature[16] = 0;
	bool bNewTGA = ( footer.cReservedCharacter == '.' ) && ( strcmp(pszSignature, "TRUEVISION-XFILE") == 0 );
	if ( bNewTGA )
		return true;
	STGAFileHeader hdr;
	pStream->Seek( 0, STREAM_SEEK_SET );
	pStream->Read( &hdr, sizeof(hdr) );
	pStream->Seek( 0, STREAM_SEEK_SET );
	bool bCheck = false;
	switch ( hdr.cImageType )
	{
		case TGAIT_NOIMAGEDATA:
			bCheck = false;
			break;
		case TGAIT_COLOR_MAPPED:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_TRUE_COLOR:
			bCheck = hdr.cColorMapType == 0;
			break;
		case TGAIT_BLACK_WHITE:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_RLE_COLOR_MAPPED:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_RLE_TRUE_COLOR:
			bCheck = hdr.cColorMapType == 0;
			break;
		case TGAIT_RLE_BLACK_WHITE:
			bCheck = hdr.cColorMapType == 0;
			break;
	}
	if ( !bCheck )
		return false;
	bCheck = hdr.imagespec.descriptor.cUnused == 0;
	if ( !bCheck )
		return false;

	return true;
}
class CTGAGrayConvertor
{
	std::vector<SColor> palette;
public:
	CTGAGrayConvertor( const STGAFileHeader &hdr, IDataStream *pStream )
	{
		palette.reserve( 256 );
		for ( int i = 0; i < 256; ++i )
			palette.push_back( SColor(255, i, i, i) );
	}
	const SColor& operator()( BYTE input ) const { return palette[input]; }
	bool IsReady() const { return !palette.empty(); }
};
template <class TIndex>
class CTGAPaletteConvertor
{
	std::vector<SColor> palette;
public:
	CTGAPaletteConvertor( const STGAFileHeader &hdr, IDataStream *pStream )
	{
		switch ( hdr.colormap.cColorMapEntrySize ) 
		{
			case 32:
				palette.resize( hdr.colormap.wColorMapLength );
				pStream->Read( &(palette[0]), sizeof(SColor) * palette.size() );
				break;
			case 24:
				{
					std::vector<SColor24> palette24( hdr.colormap.wColorMapLength );
					pStream->Read( &(palette24[0]), sizeof(SColor24) * palette24.size() );
					palette.reserve( hdr.colormap.wColorMapLength );
					for ( std::vector<SColor24>::const_iterator it = palette24.begin(); it != palette24.end(); ++it )
						palette.push_back( SColor(0xff, it->r, it->g, it->b) );
				}
				break;
			default:
				NI_ASSERT_T( 0, NStr::Format("unsupported bit depth (%d) - still not realized", hdr.colormap.cColorMapEntrySize) );
		}
	}
	const SColor& operator()( const TIndex &input ) const { return palette[input]; }
	bool IsReady() const { return !palette.empty(); }
};
class CTGARawColorConvertor
{
public:
	CTGARawColorConvertor( const STGAFileHeader &hdr, IDataStream *pStream ) {  }
	const SColor operator()( const SColor &input ) const { return input; }
	const SColor operator()( const SColor24 &input ) const { return 0xff000000 | (DWORD(input.r) << 16) | (DWORD(input.g) << 8) | DWORD(input.b); }
	bool IsReady() const { return true; }
};
template <class TColor, class TConvertor>
class CTGARawLoader
{
	static bool LoadLocal( SColor *pColors, const int nNumElements, const TConvertor &convertor, IDataStream *pStream, ... )
	{
		std::vector<TColor> buffer( nNumElements );
		const int nReadSizeInBytes = nNumElements * sizeof( TColor );
		const int check = pStream->Read( &(buffer[0]), nReadSizeInBytes );
		NI_ASSERT_TF( check == nReadSizeInBytes, NStr::Format("Can't load image - not enough bytes %d != %d", check, nReadSizeInBytes), return false );

		for ( int i = 0; i < nNumElements; ++i )
			*pColors++ = convertor( buffer[i] );
		return check == nReadSizeInBytes;
	}
	static bool LoadLocal( SColor *pColors, const int nNumElements, const TConvertor &convertor, IDataStream *pStream, SGenericNumber<4> *pp )
	{
		const int nReadSizeInBytes = nNumElements * sizeof( TColor );
		const int check = pStream->Read( pColors, nReadSizeInBytes );
		NI_ASSERT_TF( check == nReadSizeInBytes, NStr::Format("Can't load image - not enough bytes %d != %d", check, nReadSizeInBytes), return false );
		return check == nReadSizeInBytes;
	}
public:
	static bool Load( const STGAFileHeader &hdr, IImage *pImage, IDataStream *pStream )
	{
		TConvertor convertor( hdr, pStream );
		if ( !convertor.IsReady() ) 
			return false;
		const int nColorSize = sizeof( TColor );
		SGenericNumber<nColorSize> separator;
		return LoadLocal( pImage->GetLFB(), hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight, convertor, pStream, &separator );
	}
};
template <class TColor, class TConvertor>
class CTGARLELoader
{
	static bool LoadRaw( SColor *pColors, const int nNumElements, const TConvertor &convertor, IDataStream *pStream, ... )
	{
		TColor buffer[128];
		pStream->Read( &(buffer[0]), nNumElements * sizeof(TColor) );
		for ( int i = 0; i < nNumElements; ++i )
			*pColors++ = convertor( buffer[i] );
		return true;
	}
	static bool LoadRaw( SColor *pColors, const int nNumElements, const TConvertor &convertor, IDataStream *pStream, SGenericNumber<4> *pp )
	{
		pStream->Read( pColors, nNumElements * sizeof(TColor) );
		return true;
	}
public:
	static bool Load( const STGAFileHeader &hdr, IImage *pImage, IDataStream *pStream )
	{
		TConvertor convertor( hdr, pStream );
		if ( !convertor.IsReady() ) 
			return false;
		const int nBPP = hdr.imagespec.cPixelDepth;
		const int nReadSizeInBytes = hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight * nBPP / 8;
		SColor *pColors = pImage->GetLFB();
		int nReadedBytes = 0;
		BYTE cRepetitionCounter = 0;
		TColor pixelValue;
		while ( nReadedBytes < nReadSizeInBytes )
		{
			pStream->Read( &cRepetitionCounter, 1 );
			const int nNumElements = ( cRepetitionCounter & 0x7f ) + 1;
			if ( cRepetitionCounter & 0x80 )	// run-length packet
			{
				pStream->Read( &pixelValue, sizeof(TColor) );
				MemSetDWord( reinterpret_cast<DWORD*>(pColors), convertor(pixelValue), nNumElements );
			}
			else															// raw-data packet
			{
				const int nColorSize = sizeof( TColor );
				SGenericNumber<nColorSize> separator;
				LoadRaw( pColors, nNumElements, convertor, pStream, &separator );
			}
			pColors += nNumElements;
			nReadedBytes += nNumElements * sizeof( TColor );
		}
		NI_ASSERT_TF( nReadedBytes == nReadSizeInBytes, NStr::Format("Can't load image - not enough bytes %d != %d", nReadedBytes, nReadSizeInBytes), return false );
		return nReadedBytes == nReadSizeInBytes;
	}
};
bool LoadTrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	switch ( hdr.imagespec.cPixelDepth )
	{
		case 24:
			return CTGARawLoader<SColor24, CTGARawColorConvertor>::Load( hdr, pImage, pStream );
		case 32:
			return CTGARawLoader<SColor, CTGARawColorConvertor>::Load( hdr, pImage, pStream );
		default:
			NI_ASSERT_TF( 0, NStr::Format("unsupported bit depth (%d) - still not realized", hdr.imagespec.cPixelDepth), return false );
			return false;
	}
}
bool LoadColorMappedTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	return CTGARawLoader< BYTE, CTGAPaletteConvertor<BYTE> >::Load( hdr, pImage, pStream );
}
bool LoadBlackAndWhiteTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	NI_ASSERT_TF( hdr.imagespec.cPixelDepth == 8, "Can't read non-8 bit gray image", return false );
	return CTGARawLoader<BYTE, CTGAGrayConvertor>::Load( hdr, pImage, pStream );
}
bool LoadRLETrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	switch ( hdr.imagespec.cPixelDepth )
	{
		case 24:
			return CTGARLELoader<SColor24, CTGARawColorConvertor>::Load( hdr, pImage, pStream );
		case 32:
			return CTGARLELoader<SColor, CTGARawColorConvertor>::Load( hdr, pImage, pStream );
		default:
			NI_ASSERT_TF( 0, NStr::Format("unsupported bit depth (%d) - still not realized", hdr.imagespec.cPixelDepth), return false );
			return false;
	}
}
bool LoadRLEColorMappedTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	return CTGARLELoader<BYTE, CTGAPaletteConvertor<BYTE> >::Load( hdr, pImage, pStream );
}
bool LoadRLEBlackAndWhiteTGA( const STGAFileHeader &hdr, CImage *pImage, IDataStream *pStream )
{
	return CTGARLELoader<BYTE, CTGAGrayConvertor>::Load( hdr, pImage, pStream );
}
CImage* NImage::LoadImageTGA( IDataStream *pStream )
{
	STGAFileHeader hdr;
	pStream->Seek( 0, STREAM_SEEK_SET );
	pStream->Read( &hdr, sizeof(hdr) );
	if ( hdr.cIDLength != 0 )
		pStream->Seek( hdr.cIDLength, STREAM_SEEK_CUR );
	CImage *pImage = new CImage( hdr.imagespec.wImageWidth, hdr.imagespec.wImageHeight );
	bool bLoaded = false;
	switch ( hdr.cImageType )
	{
		case TGAIT_TRUE_COLOR:
			bLoaded = LoadTrueColorTGA( hdr, pImage, pStream );
			break;
		case TGAIT_COLOR_MAPPED:
			bLoaded = LoadColorMappedTGA( hdr, pImage, pStream );
			break;
		case TGAIT_BLACK_WHITE:
			bLoaded = LoadBlackAndWhiteTGA( hdr, pImage, pStream );
			break;
		case TGAIT_RLE_TRUE_COLOR:
			bLoaded = LoadRLETrueColorTGA( hdr, pImage, pStream );
			break;
		case TGAIT_RLE_COLOR_MAPPED:
			bLoaded = LoadRLEColorMappedTGA( hdr, pImage, pStream );
			break;
		case TGAIT_RLE_BLACK_WHITE:
			bLoaded = LoadRLEBlackAndWhiteTGA( hdr, pImage, pStream );
			break;
		default:
			{
				SStorageElementStats stats;
				pStream->GetStats( &stats );
				const char *pszImageName = stats.pszName == 0 ? "" : stats.pszName;
				NI_ASSERT_TF( 0, NStr::Format("Unsupported subformat %d for image \"%s\"", hdr.cImageType, pszImageName), return false );
			}
	}
	if ( bLoaded ) 
	{
		if ( hdr.imagespec.descriptor.cTopToBottomOrder == 0 )
			pImage->FlipY();
		return pImage;
	}
	else
	{
		pImage->AddRef();
		pImage->Release();
		return 0;
	}
}
bool NImage::SaveImageAsTGA( IDataStream *pStream, const IImage *pImage )
{
	STGAFileHeader hdr;
	Zero( hdr );
	hdr.cImageType = TGAIT_TRUE_COLOR;
	hdr.imagespec.cPixelDepth = 32;
	hdr.imagespec.wImageWidth = pImage->GetSizeX();
	hdr.imagespec.wImageHeight = pImage->GetSizeY();
	hdr.imagespec.descriptor.cTopToBottomOrder = 1;
	hdr.imagespec.descriptor.cAlphaChannelBits = 8;
	int check = pStream->Write( &hdr, sizeof(hdr) );
	if ( check != sizeof(hdr) )
	{
		pStream->Seek( -check, STREAM_SEEK_CUR );
		return false;
	}
	const int nNumElements = int(hdr.imagespec.wImageWidth) * int(hdr.imagespec.wImageHeight);
	const int nWriteSizeBytes = nNumElements * 4; // 4 = hdr.imagespec.cPixelDepth / 8
	NI_ASSERT_T( nWriteSizeBytes > 0, NStr::Format("image size %d : %d are too big (>2GB) - can't save it", int(hdr.imagespec.wImageWidth), int(hdr.imagespec.wImageHeight)) );
	check = pStream->Write( pImage->GetLFB(), nWriteSizeBytes );
	if ( check != nWriteSizeBytes )
		return false;
	STGAFileFooter footer;
	Zero( footer );
	memcpy( footer.cSignature, "TRUEVISION-XFILE", 16 );
	footer.cReservedCharacter = '.';
	check = pStream->Write( &footer, sizeof(footer) );
	return check == sizeof( footer );
}
