// Portable data-only port of the MFC-era S3TC encoder (Sources/src/Image/S3TC.cpp @ 40645ad29^).
// MFC types removed, algorithm kept intact so the per-pixel delta against NDxt is honest.
#include "legacy_dxt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{
	using std::uint8_t;
	using std::uint16_t;
	using std::uint32_t;
	using std::uint64_t;

	int BlockCount( int size )
	{
		return ( size + 3 ) / 4;
	}

	int BlockBytes( NLegacyDxt::Format format )
	{
		return format == NLegacyDxt::Format::DXT1 ? 8 : 16;
	}

	uint8_t Expand5( uint32_t value )
	{
		return uint8_t( ( value << 3 ) | ( value >> 2 ) );
	}

	uint8_t Expand6( uint32_t value )
	{
		return uint8_t( ( value << 2 ) | ( value >> 4 ) );
	}

	uint16_t PackRGB565( uint8_t r, uint8_t g, uint8_t b )
	{
		return uint16_t( ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) );
	}

	uint32_t PackARGB( uint8_t a, uint8_t r, uint8_t g, uint8_t b )
	{
		return ( uint32_t( a ) << 24 ) | ( uint32_t( r ) << 16 ) | ( uint32_t( g ) << 8 ) | uint32_t( b );
	}

	uint32_t UnpackRGB565( uint16_t color )
	{
		return PackARGB( 255,
			Expand5( ( color >> 11 ) & 31 ),
			Expand6( ( color >> 5 ) & 63 ),
			Expand5( color & 31 ) );
	}

	uint8_t GetA( uint32_t color ) { return uint8_t( color >> 24 ); }
	uint8_t GetR( uint32_t color ) { return uint8_t( color >> 16 ); }
	uint8_t GetG( uint32_t color ) { return uint8_t( color >> 8 ); }
	uint8_t GetB( uint32_t color ) { return uint8_t( color ); }

	uint32_t InterpolateColor( uint32_t c0, uint32_t c1, int w0, int w1, int div )
	{
		return PackARGB(
			uint8_t( ( GetA( c0 ) * w0 + GetA( c1 ) * w1 ) / div ),
			uint8_t( ( GetR( c0 ) * w0 + GetR( c1 ) * w1 ) / div ),
			uint8_t( ( GetG( c0 ) * w0 + GetG( c1 ) * w1 ) / div ),
			uint8_t( ( GetB( c0 ) * w0 + GetB( c1 ) * w1 ) / div ) );
	}

	int ColorDistance( uint32_t lhs, uint32_t rhs )
	{
		const int dr = int( GetR( lhs ) ) - int( GetR( rhs ) );
		const int dg = int( GetG( lhs ) ) - int( GetG( rhs ) );
		const int db = int( GetB( lhs ) ) - int( GetB( rhs ) );
		return dr * dr + dg * dg + db * db;
	}

	void BuildColorPalette( uint16_t c0, uint16_t c1, bool threeColorMode, uint32_t palette[4] )
	{
		palette[0] = UnpackRGB565( c0 );
		palette[1] = UnpackRGB565( c1 );
		if ( threeColorMode )
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

	void CollectBlock( const NLegacyDxt::SurfaceDesc &input, int blockX, int blockY, uint32_t pixels[16] )
	{
		const uint8_t *base = static_cast<const uint8_t*>( input.data );
		for ( int y = 0; y < 4; ++y )
		{
			const int srcY = std::min( blockY * 4 + y, input.height - 1 );
			const uint32_t *line = reinterpret_cast<const uint32_t*>( base + srcY * input.pitch );
			for ( int x = 0; x < 4; ++x )
			{
				const int srcX = std::min( blockX * 4 + x, input.width - 1 );
				pixels[y * 4 + x] = line[srcX];
			}
		}
	}

	// S3TC's DXT1 path ran in colour-key mode (MFC's CompressDXTN passed
	// S3TC_ENCODE_RGB_COLOR_KEY with alpha reference 0): pixels with alpha <= the
	// reference are left out of the endpoints and written as punch-through, and every
	// DXT1 block uses the three-colour palette. DXT3 and DXT5 colour blocks use four.
	constexpr uint32_t kAlphaReference = 0;

	void ChooseColorEndpoints( const uint32_t pixels[16], bool bAllowTransparent, uint16_t &c0, uint16_t &c1 )
	{
		int minLum = 256 * 3;
		int maxLum = -1;
		uint32_t minColor = 0;
		uint32_t maxColor = 0;
		bool bFound = false;
		for ( int i = 0; i < 16; ++i )
		{
			if ( bAllowTransparent && GetA( pixels[i] ) <= kAlphaReference )
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
			minColor = maxColor = 0;
		c0 = PackRGB565( GetR( maxColor ), GetG( maxColor ), GetB( maxColor ) );
		c1 = PackRGB565( GetR( minColor ), GetG( minColor ), GetB( minColor ) );
		// If a block is a single colour the two endpoints collide; nudge one so the block keeps
		// the palette mode it asked for.
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

	uint32_t EncodeColorIndices( const uint32_t pixels[16], const uint32_t palette[4], bool bThreeColorMode )
	{
		uint32_t indices = 0;
		for ( int i = 15; i >= 0; --i )
		{
			uint32_t bestIndex = 0;
			if ( bThreeColorMode && GetA( pixels[i] ) <= kAlphaReference )
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
						bestIndex = uint32_t( j );
					}
				}
			}
			indices = ( indices << 2 ) | bestIndex;
		}
		return indices;
	}

	void EncodeColorBlock( const uint32_t pixels[16], bool bThreeColorMode, uint8_t *outBlock )
	{
		uint16_t c0 = 0;
		uint16_t c1 = 0;
		ChooseColorEndpoints( pixels, bThreeColorMode, c0, c1 );
		uint32_t palette[4];
		BuildColorPalette( c0, c1, bThreeColorMode, palette );
		const uint32_t indices = EncodeColorIndices( pixels, palette, bThreeColorMode );
		outBlock[0] = uint8_t( c0 & 0xff );
		outBlock[1] = uint8_t( c0 >> 8 );
		outBlock[2] = uint8_t( c1 & 0xff );
		outBlock[3] = uint8_t( c1 >> 8 );
		std::memcpy( outBlock + 4, &indices, sizeof( indices ) );
	}

	void EncodeDxt3Alpha( const uint32_t pixels[16], uint8_t *outBlock )
	{
		std::memset( outBlock, 0, 8 );
		for ( int i = 0; i < 16; ++i )
		{
			const uint8_t alpha4 = uint8_t( GetA( pixels[i] ) >> 4 );
			outBlock[i / 2] |= uint8_t( alpha4 << ( ( i & 1 ) * 4 ) );
		}
	}

	void BuildAlphaPalette( uint8_t a0, uint8_t a1, uint8_t palette[8] )
	{
		palette[0] = a0;
		palette[1] = a1;
		if ( a0 > a1 )
		{
			palette[2] = uint8_t( ( 6 * a0 + 1 * a1 ) / 7 );
			palette[3] = uint8_t( ( 5 * a0 + 2 * a1 ) / 7 );
			palette[4] = uint8_t( ( 4 * a0 + 3 * a1 ) / 7 );
			palette[5] = uint8_t( ( 3 * a0 + 4 * a1 ) / 7 );
			palette[6] = uint8_t( ( 2 * a0 + 5 * a1 ) / 7 );
			palette[7] = uint8_t( ( 1 * a0 + 6 * a1 ) / 7 );
		}
		else
		{
			palette[2] = uint8_t( ( 4 * a0 + 1 * a1 ) / 5 );
			palette[3] = uint8_t( ( 3 * a0 + 2 * a1 ) / 5 );
			palette[4] = uint8_t( ( 2 * a0 + 3 * a1 ) / 5 );
			palette[5] = uint8_t( ( 1 * a0 + 4 * a1 ) / 5 );
			palette[6] = 0;
			palette[7] = 255;
		}
	}

	void EncodeDxt5Alpha( const uint32_t pixels[16], uint8_t *outBlock )
	{
		uint8_t alphaMin = 255;
		uint8_t alphaMax = 0;
		for ( int i = 0; i < 16; ++i )
		{
			const uint8_t alpha = GetA( pixels[i] );
			alphaMin = std::min<uint8_t>( alphaMin, alpha );
			alphaMax = std::max<uint8_t>( alphaMax, alpha );
		}
		uint8_t a0 = alphaMax;
		uint8_t a1 = alphaMin;
		if ( a0 == a1 )
		{
			if ( a1 > 0 )
				--a1;
			else
				a0 = 255;
		}
		uint8_t palette[8];
		BuildAlphaPalette( a0, a1, palette );
		uint64_t indices = 0;
		for ( int i = 15; i >= 0; --i )
		{
			int bestIndex = 0;
			int bestDistance = 0x7fffffff;
			for ( int j = 0; j < 8; ++j )
			{
				const int distance = std::abs( int( GetA( pixels[i] ) ) - int( palette[j] ) );
				if ( distance < bestDistance )
				{
					bestDistance = distance;
					bestIndex = j;
				}
			}
			indices = ( indices << 3 ) | uint64_t( bestIndex );
		}
		outBlock[0] = a0;
		outBlock[1] = a1;
		for ( int i = 0; i < 6; ++i )
			outBlock[2 + i] = uint8_t( indices >> ( i * 8 ) );
	}

	void DecodeColorBlock( const uint8_t *block, uint32_t pixels[16] )
	{
		const uint16_t c0 = uint16_t( block[0] | ( block[1] << 8 ) );
		const uint16_t c1 = uint16_t( block[2] | ( block[3] << 8 ) );
		const bool threeColorMode = c0 <= c1;
		uint32_t palette[4];
		BuildColorPalette( c0, c1, threeColorMode, palette );
		uint32_t indices = 0;
		std::memcpy( &indices, block + 4, sizeof( indices ) );
		for ( int i = 0; i < 16; ++i )
			pixels[i] = palette[( indices >> ( i * 2 ) ) & 0x3];
	}

	void DecodeDxt3Alpha( const uint8_t *block, uint32_t pixels[16] )
	{
		for ( int i = 0; i < 16; ++i )
		{
			const uint8_t alpha4 = uint8_t( ( block[i / 2] >> ( ( i & 1 ) * 4 ) ) & 0x0f );
			pixels[i] = ( pixels[i] & 0x00ffffff ) | ( uint32_t( alpha4 * 17 ) << 24 );
		}
	}

	void DecodeDxt5Alpha( const uint8_t *block, uint32_t pixels[16] )
	{
		uint8_t palette[8];
		BuildAlphaPalette( block[0], block[1], palette );
		uint64_t indices = 0;
		for ( int i = 0; i < 6; ++i )
			indices |= uint64_t( block[2 + i] ) << ( i * 8 );
		for ( int i = 0; i < 16; ++i )
		{
			const uint8_t alpha = palette[( indices >> ( i * 3 ) ) & 0x7];
			pixels[i] = ( pixels[i] & 0x00ffffff ) | ( uint32_t( alpha ) << 24 );
		}
	}

	void WriteDecodedBlock( const uint32_t pixels[16], int blockX, int blockY, int width, int height, uint32_t *dst )
	{
		for ( int y = 0; y < 4; ++y )
		{
			const int dstY = blockY * 4 + y;
			if ( dstY >= height )
				continue;
			for ( int x = 0; x < 4; ++x )
			{
				const int dstX = blockX * 4 + x;
				if ( dstX >= width )
					continue;
				dst[dstY * width + dstX] = pixels[y * 4 + x];
			}
		}
	}
}

namespace NLegacyDxt
{
	int GetEncodedSize( int width, int height, Format format )
	{
		return BlockCount( width ) * BlockCount( height ) * BlockBytes( format );
	}

	int GetDecodedSize( int width, int height )
	{
		return width * height * 4;
	}

	void Encode( const SurfaceDesc &input, Format format, void *output )
	{
		uint8_t *dst = static_cast<uint8_t*>( output );
		const int blocksX = BlockCount( input.width );
		const int blocksY = BlockCount( input.height );
		for ( int by = 0; by < blocksY; ++by )
		{
			for ( int bx = 0; bx < blocksX; ++bx )
			{
				uint32_t pixels[16];
				CollectBlock( input, bx, by, pixels );
				switch ( format )
				{
					case Format::DXT1:
						EncodeColorBlock( pixels, true, dst );
						dst += 8;
						break;
					case Format::DXT3:
						EncodeDxt3Alpha( pixels, dst );
						EncodeColorBlock( pixels, false, dst + 8 );
						dst += 16;
						break;
					case Format::DXT5:
						EncodeDxt5Alpha( pixels, dst );
						EncodeColorBlock( pixels, false, dst + 8 );
						dst += 16;
						break;
				}
			}
		}
	}

	void Decode( const SurfaceDesc &input, Format format, void *outputPixels )
	{
		const uint8_t *src = static_cast<const uint8_t*>( input.data );
		uint32_t *dst = static_cast<uint32_t*>( outputPixels );
		const int blocksX = BlockCount( input.width );
		const int blocksY = BlockCount( input.height );
		const int blockBytes = BlockBytes( format );
		const int pitch = input.pitch > 0 ? input.pitch : blocksX * blockBytes;
		for ( int by = 0; by < blocksY; ++by )
		{
			for ( int bx = 0; bx < blocksX; ++bx )
			{
				const uint8_t *block = src + by * pitch + bx * blockBytes;
				uint32_t pixels[16];
				switch ( format )
				{
					case Format::DXT1:
						DecodeColorBlock( block, pixels );
						break;
					case Format::DXT3:
						DecodeColorBlock( block + 8, pixels );
						DecodeDxt3Alpha( block, pixels );
						break;
					case Format::DXT5:
						DecodeColorBlock( block + 8, pixels );
						DecodeDxt5Alpha( block, pixels );
						break;
				}
				WriteDecodedBlock( pixels, bx, by, input.width, input.height, dst );
			}
		}
	}
}
