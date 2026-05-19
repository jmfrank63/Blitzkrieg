#include "StdAfx.h"

#include "S3TC.h"

#include <cstring>

namespace
{
	DWORD g_dwAlphaReference = 0;

	inline int BlockCount( int nSize )
	{
		return ( nSize + 3 ) / 4;
	}

	inline BYTE Expand5( DWORD value )
	{
		return BYTE( ( value << 3 ) | ( value >> 2 ) );
	}

	inline BYTE Expand6( DWORD value )
	{
		return BYTE( ( value << 2 ) | ( value >> 4 ) );
	}

	inline WORD PackRGB565( BYTE r, BYTE g, BYTE b )
	{
		return WORD( ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) );
	}

	inline DWORD PackARGB( BYTE a, BYTE r, BYTE g, BYTE b )
	{
		return ( DWORD(a) << 24 ) | ( DWORD(r) << 16 ) | ( DWORD(g) << 8 ) | DWORD(b);
	}

	inline DWORD UnpackRGB565( WORD color )
	{
		return PackARGB( 255, Expand5( ( color >> 11 ) & 31 ), Expand6( ( color >> 5 ) & 63 ), Expand5( color & 31 ) );
	}

	inline BYTE GetA( DWORD color ) { return BYTE( color >> 24 ); }
	inline BYTE GetR( DWORD color ) { return BYTE( color >> 16 ); }
	inline BYTE GetG( DWORD color ) { return BYTE( color >> 8 ); }
	inline BYTE GetB( DWORD color ) { return BYTE( color ); }

	inline DWORD InterpolateColor( DWORD c0, DWORD c1, int w0, int w1, int div )
	{
		return PackARGB(
			BYTE( ( GetA( c0 ) * w0 + GetA( c1 ) * w1 ) / div ),
			BYTE( ( GetR( c0 ) * w0 + GetR( c1 ) * w1 ) / div ),
			BYTE( ( GetG( c0 ) * w0 + GetG( c1 ) * w1 ) / div ),
			BYTE( ( GetB( c0 ) * w0 + GetB( c1 ) * w1 ) / div ) );
	}

	inline int ColorDistance( DWORD lhs, DWORD rhs )
	{
		const int dr = int( GetR( lhs ) ) - int( GetR( rhs ) );
		const int dg = int( GetG( lhs ) ) - int( GetG( rhs ) );
		const int db = int( GetB( lhs ) ) - int( GetB( rhs ) );
		return dr * dr + dg * dg + db * db;
	}

	void BuildColorPalette( WORD c0, WORD c1, bool bThreeColorMode, DWORD palette[4] )
	{
		palette[0] = UnpackRGB565( c0 );
		palette[1] = UnpackRGB565( c1 );
		if ( bThreeColorMode )
		{
			palette[2] = InterpolateColor( palette[0], palette[1], 1, 1, 2 );
			palette[3] = 0;
		}
		else
		{
			palette[2] = InterpolateColor( palette[0], palette[1], 2, 1, 3 );
			palette[3] = InterpolateColor( palette[0], palette[1], 1, 2, 3 );
		}
	}

	void CollectBlock( const DDSURFACEDESC *pIn, int blockX, int blockY, DWORD pixels[16] )
	{
		const DWORD *pSrc = reinterpret_cast<const DWORD*>( pIn->lpSurface );
		const int nPitch = pIn->lPitch / 4;
		for ( int y = 0; y < 4; ++y )
		{
			const int srcY = Min<int>( blockY * 4 + y, int( pIn->dwHeight ) - 1 );
			for ( int x = 0; x < 4; ++x )
			{
				const int srcX = Min<int>( blockX * 4 + x, int( pIn->dwWidth ) - 1 );
				pixels[y * 4 + x] = pSrc[srcY * nPitch + srcX];
			}
		}
	}

	void ChooseColorEndpoints( const DWORD pixels[16], bool bAllowTransparent, WORD &c0, WORD &c1 )
	{
		int minLum = 256 * 3;
		int maxLum = -1;
		DWORD minColor = 0;
		DWORD maxColor = 0;
		bool bFound = false;
		for ( int i = 0; i < 16; ++i )
		{
			if ( bAllowTransparent && ( GetA( pixels[i] ) <= g_dwAlphaReference ) )
				continue;
			const int lum = int( GetR( pixels[i] ) ) + int( GetG( pixels[i] ) ) + int( GetB( pixels[i] ) );
			if ( lum < minLum )
			{
				minLum = lum;
				minColor = pixels[i];
			}
			if ( lum > maxLum )
			{
				maxLum = lum;
				maxColor = pixels[i];
			}
			bFound = true;
		}
		if ( !bFound )
		{
			minColor = maxColor = 0;
		}
		c0 = PackRGB565( GetR( maxColor ), GetG( maxColor ), GetB( maxColor ) );
		c1 = PackRGB565( GetR( minColor ), GetG( minColor ), GetB( minColor ) );
		if ( c0 == c1 )
		{
			if ( c1 > 0 )
				--c1;
			else
				++c0;
		}
		if ( bAllowTransparent )
		{
			if ( c0 > c1 )
				std::swap( c0, c1 );
		}
		else if ( c0 < c1 )
		{
			std::swap( c0, c1 );
		}
	}

	DWORD EncodeColorIndices( const DWORD pixels[16], const DWORD palette[4], bool bThreeColorMode )
	{
		DWORD indices = 0;
		for ( int i = 15; i >= 0; --i )
		{
			DWORD bestIndex = 0;
			if ( bThreeColorMode && ( GetA( pixels[i] ) <= g_dwAlphaReference ) )
			{
				bestIndex = 3;
			}
			else
			{
				int bestDistance = 0x7fffffff;
				const int nPaletteSize = bThreeColorMode ? 3 : 4;
				for ( int j = 0; j < nPaletteSize; ++j )
				{
					const int distance = ColorDistance( pixels[i], palette[j] );
					if ( distance < bestDistance )
					{
						bestDistance = distance;
						bestIndex = DWORD( j );
					}
				}
			}
			indices = ( indices << 2 ) | bestIndex;
		}
		return indices;
	}

	void EncodeColorBlock( const DWORD pixels[16], bool bThreeColorMode, BYTE *pOutBlock )
	{
		WORD c0 = 0;
		WORD c1 = 0;
		ChooseColorEndpoints( pixels, bThreeColorMode, c0, c1 );
		DWORD palette[4];
		BuildColorPalette( c0, c1, bThreeColorMode, palette );
		const DWORD indices = EncodeColorIndices( pixels, palette, bThreeColorMode );
		pOutBlock[0] = BYTE( c0 & 0xff );
		pOutBlock[1] = BYTE( c0 >> 8 );
		pOutBlock[2] = BYTE( c1 & 0xff );
		pOutBlock[3] = BYTE( c1 >> 8 );
		std::memcpy( pOutBlock + 4, &indices, sizeof( indices ) );
	}

	void EncodeDXT3AlphaBlock( const DWORD pixels[16], BYTE *pOutBlock )
	{
		for ( int i = 0; i < 8; ++i )
			pOutBlock[i] = 0;
		for ( int i = 0; i < 16; ++i )
		{
			const BYTE alpha4 = BYTE( GetA( pixels[i] ) >> 4 );
			pOutBlock[i / 2] |= BYTE( alpha4 << ( ( i & 1 ) * 4 ) );
		}
	}

	void BuildAlphaPalette( BYTE a0, BYTE a1, BYTE palette[8] )
	{
		palette[0] = a0;
		palette[1] = a1;
		if ( a0 > a1 )
		{
			palette[2] = BYTE( ( 6 * a0 + 1 * a1 ) / 7 );
			palette[3] = BYTE( ( 5 * a0 + 2 * a1 ) / 7 );
			palette[4] = BYTE( ( 4 * a0 + 3 * a1 ) / 7 );
			palette[5] = BYTE( ( 3 * a0 + 4 * a1 ) / 7 );
			palette[6] = BYTE( ( 2 * a0 + 5 * a1 ) / 7 );
			palette[7] = BYTE( ( 1 * a0 + 6 * a1 ) / 7 );
		}
		else
		{
			palette[2] = BYTE( ( 4 * a0 + 1 * a1 ) / 5 );
			palette[3] = BYTE( ( 3 * a0 + 2 * a1 ) / 5 );
			palette[4] = BYTE( ( 2 * a0 + 3 * a1 ) / 5 );
			palette[5] = BYTE( ( 1 * a0 + 4 * a1 ) / 5 );
			palette[6] = 0;
			palette[7] = 255;
		}
	}

	void EncodeDXT5AlphaBlock( const DWORD pixels[16], BYTE *pOutBlock )
	{
		BYTE alphaMin = 255;
		BYTE alphaMax = 0;
		for ( int i = 0; i < 16; ++i )
		{
			const BYTE alpha = GetA( pixels[i] );
			alphaMin = Min<BYTE>( alphaMin, alpha );
			alphaMax = Max<BYTE>( alphaMax, alpha );
		}
		BYTE a0 = alphaMax;
		BYTE a1 = alphaMin;
		if ( a0 == a1 )
		{
			if ( a1 > 0 )
				--a1;
			else
				a0 = 255;
		}
		BYTE palette[8];
		BuildAlphaPalette( a0, a1, palette );
		unsigned __int64 indices = 0;
		for ( int i = 15; i >= 0; --i )
		{
			int bestIndex = 0;
			int bestDistance = 0x7fffffff;
			for ( int j = 0; j < 8; ++j )
			{
				const int distance = abs( int( GetA( pixels[i] ) ) - int( palette[j] ) );
				if ( distance < bestDistance )
				{
					bestDistance = distance;
					bestIndex = j;
				}
			}
			indices = ( indices << 3 ) | unsigned __int64( bestIndex );
		}
		pOutBlock[0] = a0;
		pOutBlock[1] = a1;
		for ( int i = 0; i < 6; ++i )
			pOutBlock[2 + i] = BYTE( indices >> ( i * 8 ) );
	}

	void DecodeColorBlock( const BYTE *pBlock, DWORD pixels[16], bool &bThreeColorMode )
	{
		const WORD c0 = WORD( pBlock[0] | ( pBlock[1] << 8 ) );
		const WORD c1 = WORD( pBlock[2] | ( pBlock[3] << 8 ) );
		bThreeColorMode = c0 <= c1;
		DWORD palette[4];
		BuildColorPalette( c0, c1, bThreeColorMode, palette );
		DWORD indices = 0;
		std::memcpy( &indices, pBlock + 4, sizeof( indices ) );
		for ( int i = 0; i < 16; ++i )
		{
			const DWORD index = ( indices >> ( i * 2 ) ) & 0x3;
			pixels[i] = palette[index];
		}
	}

	void DecodeDXT3AlphaBlock( const BYTE *pBlock, DWORD pixels[16] )
	{
		for ( int i = 0; i < 16; ++i )
		{
			const BYTE alpha4 = BYTE( ( pBlock[i / 2] >> ( ( i & 1 ) * 4 ) ) & 0x0f );
			pixels[i] = ( pixels[i] & 0x00ffffff ) | ( DWORD( alpha4 * 17 ) << 24 );
		}
	}

	void DecodeDXT5AlphaBlock( const BYTE *pBlock, DWORD pixels[16] )
	{
		BYTE palette[8];
		BuildAlphaPalette( pBlock[0], pBlock[1], palette );
		unsigned __int64 indices = 0;
		for ( int i = 0; i < 6; ++i )
			indices |= unsigned __int64( pBlock[2 + i] ) << ( i * 8 );
		for ( int i = 0; i < 16; ++i )
		{
			const BYTE alpha = palette[( indices >> ( i * 3 ) ) & 0x7];
			pixels[i] = ( pixels[i] & 0x00ffffff ) | ( DWORD( alpha ) << 24 );
		}
	}

	void WriteDecodedBlock( const DWORD pixels[16], int blockX, int blockY, const DDSURFACEDESC *pOut )
	{
		DWORD *pDst = reinterpret_cast<DWORD*>( pOut->lpSurface );
		const int nPitch = pOut->lPitch / 4;
		for ( int y = 0; y < 4; ++y )
		{
			const int dstY = blockY * 4 + y;
			if ( dstY >= int( pOut->dwHeight ) )
				continue;
			for ( int x = 0; x < 4; ++x )
			{
				const int dstX = blockX * 4 + x;
				if ( dstX >= int( pOut->dwWidth ) )
					continue;
				pDst[dstY * nPitch + dstX] = pixels[y * 4 + x];
			}
		}
	}

	BYTE ExpandComponent( DWORD value, DWORD mask )
	{
		if ( mask == 0 )
			return 255;
		DWORD shift = 0;
		while ( ( ( mask >> shift ) & 1 ) == 0 )
			++shift;
		DWORD bits = 0;
		while ( ( ( mask >> ( shift + bits ) ) & 1 ) != 0 )
			++bits;
		const DWORD maxValue = ( 1u << bits ) - 1u;
		const DWORD normalized = ( value & mask ) >> shift;
		return BYTE( ( normalized * 255u + maxValue / 2u ) / maxValue );
	}

	void DecodePackedPixels( const DDSURFACEDESC *pIn, const DDSURFACEDESC *pOut )
	{
		DWORD *pDst = reinterpret_cast<DWORD*>( pOut->lpSurface );
		for ( int y = 0; y < int( pIn->dwHeight ); ++y )
		{
			const BYTE *pSrcLine = reinterpret_cast<const BYTE*>( pIn->lpSurface ) + y * pIn->lPitch;
			for ( int x = 0; x < int( pIn->dwWidth ); ++x )
			{
				DWORD raw = 0;
				if ( pIn->ddpfPixelFormat.dwRGBBitCount == 16 )
					raw = reinterpret_cast<const WORD*>( pSrcLine )[x];
				else
					raw = reinterpret_cast<const DWORD*>( pSrcLine )[x];
				pDst[y * ( pOut->lPitch / 4 ) + x] = PackARGB(
					ExpandComponent( raw, pIn->ddpfPixelFormat.dwRGBAlphaBitMask ),
					ExpandComponent( raw, pIn->ddpfPixelFormat.dwRBitMask ),
					ExpandComponent( raw, pIn->ddpfPixelFormat.dwGBitMask ),
					ExpandComponent( raw, pIn->ddpfPixelFormat.dwBBitMask ) );
			}
		}
	}
}

extern "C" int S3TCgetEncodeSize( const DDSURFACEDESC *pIn, DWORD dwEncodeType )
{
	const int nBlockSize = ( dwEncodeType & ( S3TC_ENCODE_ALPHA_EXPLICIT | S3TC_ENCODE_ALPHA_INTERPOLATED ) ) ? 16 : 8;
	return BlockCount( int( pIn->dwWidth ) ) * BlockCount( int( pIn->dwHeight ) ) * nBlockSize;
}

extern "C" int S3TCgetDecodeSize( const DDSURFACEDESC *pIn )
{
	UNREFERENCED_PARAMETER( pIn );
	return int( pIn->dwWidth * pIn->dwHeight * 4 );
}

extern "C" void S3TCsetAlphaReference( DWORD dwAlphaReference )
{
	g_dwAlphaReference = dwAlphaReference;
}

extern "C" void S3TCencode( const DDSURFACEDESC *pIn, const void *pReserved, DDSURFACEDESC *pOut, void *pOutBits, DWORD dwEncodeType, const float *pWeights )
{
	UNREFERENCED_PARAMETER( pReserved );
	UNREFERENCED_PARAMETER( pWeights );
	BYTE *pDst = reinterpret_cast<BYTE*>( pOutBits );
	const int nBlocksX = BlockCount( int( pIn->dwWidth ) );
	const int nBlocksY = BlockCount( int( pIn->dwHeight ) );
	const bool bExplicitAlpha = ( dwEncodeType & S3TC_ENCODE_ALPHA_EXPLICIT ) != 0;
	const bool bInterpolatedAlpha = ( dwEncodeType & S3TC_ENCODE_ALPHA_INTERPOLATED ) != 0;
	const bool bColorKey = ( dwEncodeType & S3TC_ENCODE_RGB_COLOR_KEY ) != 0;
	for ( int blockY = 0; blockY < nBlocksY; ++blockY )
	{
		for ( int blockX = 0; blockX < nBlocksX; ++blockX )
		{
			DWORD pixels[16];
			CollectBlock( pIn, blockX, blockY, pixels );
			if ( bExplicitAlpha )
			{
				EncodeDXT3AlphaBlock( pixels, pDst );
				EncodeColorBlock( pixels, false, pDst + 8 );
				pDst += 16;
			}
			else if ( bInterpolatedAlpha )
			{
				EncodeDXT5AlphaBlock( pixels, pDst );
				EncodeColorBlock( pixels, false, pDst + 8 );
				pDst += 16;
			}
			else
			{
				EncodeColorBlock( pixels, bColorKey, pDst );
				pDst += 8;
			}
		}
	}
	if ( pOut != 0 )
	{
		Zero( *pOut );
		pOut->dwSize = sizeof( *pOut );
		pOut->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_LPSURFACE | DDSD_LINEARSIZE;
		pOut->dwWidth = pIn->dwWidth;
		pOut->dwHeight = pIn->dwHeight;
		pOut->lPitch = S3TCgetEncodeSize( pIn, dwEncodeType );
		pOut->lpSurface = pOutBits;
		pOut->ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
		pOut->ddpfPixelFormat.dwFlags = DDPF_FOURCC;
		if ( bExplicitAlpha )
			pOut->ddpfPixelFormat.dwFourCC = MAKEFOURCC( 'D', 'X', 'T', '3' );
		else if ( bInterpolatedAlpha )
			pOut->ddpfPixelFormat.dwFourCC = MAKEFOURCC( 'D', 'X', 'T', '5' );
		else
			pOut->ddpfPixelFormat.dwFourCC = MAKEFOURCC( 'D', 'X', 'T', '1' );
	}
}

extern "C" void S3TCdecode( const DDSURFACEDESC *pIn, DDSURFACEDESC *pOut, void *pOutBits )
{
	if ( pOut != 0 )
	{
		Zero( *pOut );
		pOut->dwSize = sizeof( *pOut );
		pOut->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_LPSURFACE | DDSD_PITCH;
		pOut->dwWidth = pIn->dwWidth;
		pOut->dwHeight = pIn->dwHeight;
		pOut->lPitch = pIn->dwWidth * 4;
		pOut->lpSurface = pOutBits;
		pOut->ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
		pOut->ddpfPixelFormat.dwFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
		pOut->ddpfPixelFormat.dwRGBBitCount = 32;
		pOut->ddpfPixelFormat.dwRBitMask = 0x00FF0000;
		pOut->ddpfPixelFormat.dwGBitMask = 0x0000FF00;
		pOut->ddpfPixelFormat.dwBBitMask = 0x000000FF;
		pOut->ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
	}

	DDSURFACEDESC output = *pOut;
	if ( output.lpSurface == 0 )
		output.lpSurface = pOutBits;
	if ( ( pIn->ddpfPixelFormat.dwFlags & DDPF_FOURCC ) == 0 )
	{
		DecodePackedPixels( pIn, &output );
		return;
	}

	const BYTE *pSrc = reinterpret_cast<const BYTE*>( pIn->lpSurface );
	const int nBlocksX = BlockCount( int( pIn->dwWidth ) );
	const int nBlocksY = BlockCount( int( pIn->dwHeight ) );
	const DWORD fourCC = pIn->ddpfPixelFormat.dwFourCC;
	for ( int blockY = 0; blockY < nBlocksY; ++blockY )
	{
		for ( int blockX = 0; blockX < nBlocksX; ++blockX )
		{
			DWORD pixels[16];
			bool bThreeColorMode = false;
			if ( fourCC == MAKEFOURCC( 'D', 'X', 'T', '1' ) )
			{
				DecodeColorBlock( pSrc, pixels, bThreeColorMode );
				pSrc += 8;
			}
			else if ( ( fourCC == MAKEFOURCC( 'D', 'X', 'T', '2' ) ) || ( fourCC == MAKEFOURCC( 'D', 'X', 'T', '3' ) ) )
			{
				DecodeColorBlock( pSrc + 8, pixels, bThreeColorMode );
				DecodeDXT3AlphaBlock( pSrc, pixels );
				pSrc += 16;
			}
			else
			{
				DecodeColorBlock( pSrc + 8, pixels, bThreeColorMode );
				DecodeDXT5AlphaBlock( pSrc, pixels );
				pSrc += 16;
			}
			WriteDecodedBlock( pixels, blockX, blockY, &output );
		}
	}
}
