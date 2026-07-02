#include "DxtCodec.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	using std::uint8_t;
	using std::uint16_t;
	using std::uint32_t;
	using std::uint64_t;

	struct Color
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};

	int Blocks( int value )
	{
		return ( value + 3 ) / 4;
	}

	bool UsesInterpolatedAlpha( NDxt::Format format )
	{
		return format == NDxt::Format::DXT4 || format == NDxt::Format::DXT5;
	}

	bool UsesExplicitAlpha( NDxt::Format format )
	{
		return format == NDxt::Format::DXT2 || format == NDxt::Format::DXT3;
	}

	bool HasAlphaBlock( NDxt::Format format )
	{
		return UsesExplicitAlpha( format ) || UsesInterpolatedAlpha( format );
	}

	int BlockSize( NDxt::Format format )
	{
		return HasAlphaBlock( format ) ? 16 : 8;
	}

	uint32_t PackArgb( const Color &color )
	{
		return ( uint32_t( color.a ) << 24 ) | ( uint32_t( color.r ) << 16 ) | ( uint32_t( color.g ) << 8 ) | uint32_t( color.b );
	}

	Color UnpackArgb( uint32_t value )
	{
		Color color;
		color.a = uint8_t( ( value >> 24 ) & 0xff );
		color.r = uint8_t( ( value >> 16 ) & 0xff );
		color.g = uint8_t( ( value >> 8 ) & 0xff );
		color.b = uint8_t( value & 0xff );
		return color;
	}

	uint16_t Pack565( const Color &color )
	{
		const uint16_t r = uint16_t( ( uint32_t( color.r ) * 31 + 127 ) / 255 );
		const uint16_t g = uint16_t( ( uint32_t( color.g ) * 63 + 127 ) / 255 );
		const uint16_t b = uint16_t( ( uint32_t( color.b ) * 31 + 127 ) / 255 );
		return uint16_t( ( r << 11 ) | ( g << 5 ) | b );
	}

	Color Unpack565( uint16_t value )
	{
		Color color;
		const uint32_t r = ( value >> 11 ) & 0x1f;
		const uint32_t g = ( value >> 5 ) & 0x3f;
		const uint32_t b = value & 0x1f;
		color.r = uint8_t( ( r << 3 ) | ( r >> 2 ) );
		color.g = uint8_t( ( g << 2 ) | ( g >> 4 ) );
		color.b = uint8_t( ( b << 3 ) | ( b >> 2 ) );
		color.a = 255;
		return color;
	}

	Color Lerp( const Color &a, const Color &b, int wa, int wb, int divisor )
	{
		Color result;
		result.r = uint8_t( ( int( a.r ) * wa + int( b.r ) * wb ) / divisor );
		result.g = uint8_t( ( int( a.g ) * wa + int( b.g ) * wb ) / divisor );
		result.b = uint8_t( ( int( a.b ) * wa + int( b.b ) * wb ) / divisor );
		result.a = uint8_t( ( int( a.a ) * wa + int( b.a ) * wb ) / divisor );
		return result;
	}

	void BuildColorPalette( uint16_t color0, uint16_t color1, bool dxt1, Color palette[4] )
	{
		palette[0] = Unpack565( color0 );
		palette[1] = Unpack565( color1 );
		if ( !dxt1 || color0 > color1 )
		{
			palette[2] = Lerp( palette[0], palette[1], 2, 1, 3 );
			palette[3] = Lerp( palette[0], palette[1], 1, 2, 3 );
		}
		else
		{
			palette[2] = Lerp( palette[0], palette[1], 1, 1, 2 );
			palette[3] = { 0, 0, 0, 0 };
		}
	}

	int ColorDistance( const Color &a, const Color &b )
	{
		const int dr = int( a.r ) - int( b.r );
		const int dg = int( a.g ) - int( b.g );
		const int db = int( a.b ) - int( b.b );
		return dr * dr + dg * dg + db * db;
	}

	int FindNearestColor( const Color &color, const Color palette[4] )
	{
		int bestIndex = 0;
		int bestDistance = ColorDistance( color, palette[0] );
		for ( int i = 1; i != 4; ++i )
		{
			const int distance = ColorDistance( color, palette[i] );
			if ( distance < bestDistance )
			{
				bestDistance = distance;
				bestIndex = i;
			}
		}
		return bestIndex;
	}

	void ReadBlock( const NDxt::DxtSurfaceDesc &input, int blockX, int blockY, Color pixels[16] )
	{
		const uint8_t *base = static_cast<const uint8_t*>( input.data );
		for ( int y = 0; y != 4; ++y )
		{
			const int srcY = std::min( blockY * 4 + y, input.height - 1 );
			const uint32_t *line = reinterpret_cast<const uint32_t*>( base + srcY * input.pitch );
			for ( int x = 0; x != 4; ++x )
			{
				const int srcX = std::min( blockX * 4 + x, input.width - 1 );
				pixels[y * 4 + x] = UnpackArgb( line[srcX] );
			}
		}
	}

	void WriteBlock( const Color pixels[16], int blockX, int blockY, int width, int height, uint32_t *output )
	{
		for ( int y = 0; y != 4; ++y )
		{
			const int dstY = blockY * 4 + y;
			if ( dstY >= height )
				continue;
			for ( int x = 0; x != 4; ++x )
			{
				const int dstX = blockX * 4 + x;
				if ( dstX >= width )
					continue;
				output[dstY * width + dstX] = PackArgb( pixels[y * 4 + x] );
			}
		}
	}

	void DecodeColorBlock( const uint8_t *block, bool dxt1, Color pixels[16] )
	{
		const uint16_t color0 = uint16_t( block[0] | ( block[1] << 8 ) );
		const uint16_t color1 = uint16_t( block[2] | ( block[3] << 8 ) );
		const uint32_t indices = uint32_t( block[4] ) | ( uint32_t( block[5] ) << 8 ) | ( uint32_t( block[6] ) << 16 ) | ( uint32_t( block[7] ) << 24 );
		Color palette[4];
		BuildColorPalette( color0, color1, dxt1, palette );
		for ( int i = 0; i != 16; ++i )
			pixels[i] = palette[( indices >> ( i * 2 ) ) & 3];
	}

	void EncodeColorBlock( const Color pixels[16], bool dxt1, uint8_t *block )
	{
		Color minColor = pixels[0];
		Color maxColor = pixels[0];
		int minLuma = 1000000;
		int maxLuma = -1;
		for ( int i = 0; i != 16; ++i )
		{
			const int luma = int( pixels[i].r ) * 299 + int( pixels[i].g ) * 587 + int( pixels[i].b ) * 114;
			if ( luma < minLuma )
			{
				minLuma = luma;
				minColor = pixels[i];
			}
			if ( luma > maxLuma )
			{
				maxLuma = luma;
				maxColor = pixels[i];
			}
		}

		uint16_t color0 = Pack565( maxColor );
		uint16_t color1 = Pack565( minColor );
		if ( !dxt1 && color0 < color1 )
			std::swap( color0, color1 );

		Color palette[4];
		BuildColorPalette( color0, color1, dxt1, palette );

		uint32_t indices = 0;
		for ( int i = 0; i != 16; ++i )
			indices |= uint32_t( FindNearestColor( pixels[i], palette ) ) << ( i * 2 );

		block[0] = uint8_t( color0 & 0xff );
		block[1] = uint8_t( color0 >> 8 );
		block[2] = uint8_t( color1 & 0xff );
		block[3] = uint8_t( color1 >> 8 );
		block[4] = uint8_t( indices & 0xff );
		block[5] = uint8_t( ( indices >> 8 ) & 0xff );
		block[6] = uint8_t( ( indices >> 16 ) & 0xff );
		block[7] = uint8_t( ( indices >> 24 ) & 0xff );
	}

	void DecodeDxt3Alpha( const uint8_t *block, Color pixels[16] )
	{
		for ( int i = 0; i != 16; ++i )
		{
			const uint8_t packed = block[i / 2];
			const uint8_t nibble = ( i & 1 ) ? ( packed >> 4 ) : ( packed & 0x0f );
			pixels[i].a = uint8_t( nibble * 17 );
		}
	}

	void EncodeDxt3Alpha( const Color pixels[16], uint8_t *block )
	{
		std::memset( block, 0, 8 );
		for ( int i = 0; i != 16; ++i )
		{
			const uint8_t nibble = uint8_t( ( int( pixels[i].a ) * 15 + 127 ) / 255 );
			if ( i & 1 )
				block[i / 2] |= uint8_t( nibble << 4 );
			else
				block[i / 2] |= nibble;
		}
	}

	void BuildAlphaPalette( uint8_t alpha0, uint8_t alpha1, uint8_t palette[8] )
	{
		palette[0] = alpha0;
		palette[1] = alpha1;
		if ( alpha0 > alpha1 )
		{
			for ( int i = 1; i != 7; ++i )
				palette[i + 1] = uint8_t( ( ( 7 - i ) * alpha0 + i * alpha1 ) / 7 );
		}
		else
		{
			for ( int i = 1; i != 5; ++i )
				palette[i + 1] = uint8_t( ( ( 5 - i ) * alpha0 + i * alpha1 ) / 5 );
			palette[6] = 0;
			palette[7] = 255;
		}
	}

	void DecodeDxt5Alpha( const uint8_t *block, Color pixels[16] )
	{
		uint8_t palette[8];
		BuildAlphaPalette( block[0], block[1], palette );
		uint64_t indices = 0;
		for ( int i = 0; i != 6; ++i )
			indices |= uint64_t( block[2 + i] ) << ( i * 8 );
		for ( int i = 0; i != 16; ++i )
			pixels[i].a = palette[( indices >> ( i * 3 ) ) & 7];
	}

	void EncodeDxt5Alpha( const Color pixels[16], uint8_t *block )
	{
		uint8_t minAlpha = pixels[0].a;
		uint8_t maxAlpha = pixels[0].a;
		for ( int i = 1; i != 16; ++i )
		{
			minAlpha = std::min( minAlpha, pixels[i].a );
			maxAlpha = std::max( maxAlpha, pixels[i].a );
		}
		block[0] = maxAlpha;
		block[1] = minAlpha == maxAlpha ? 0 : minAlpha;

		uint8_t palette[8];
		BuildAlphaPalette( block[0], block[1], palette );
		uint64_t indices = 0;
		for ( int i = 0; i != 16; ++i )
		{
			int bestIndex = 0;
			int bestDistance = std::abs( int( pixels[i].a ) - int( palette[0] ) );
			for ( int j = 1; j != 8; ++j )
			{
				const int distance = std::abs( int( pixels[i].a ) - int( palette[j] ) );
				if ( distance < bestDistance )
				{
					bestDistance = distance;
					bestIndex = j;
				}
			}
			indices |= uint64_t( bestIndex ) << ( i * 3 );
		}
		for ( int i = 0; i != 6; ++i )
			block[2 + i] = uint8_t( ( indices >> ( i * 8 ) ) & 0xff );
	}
}

namespace NDxt
{
	int GetEncodedSize( int width, int height, Format format )
	{
		return Blocks( width ) * Blocks( height ) * BlockSize( format );
	}

	int GetDecodedSize( int width, int height )
	{
		return width * height * 4;
	}

	void Encode( const DxtSurfaceDesc &input, Format format, void *output )
	{
		const int blocksX = Blocks( input.width );
		const int blocksY = Blocks( input.height );
		const int blockSize = BlockSize( format );
		uint8_t *dst = static_cast<uint8_t*>( output );
		for ( int by = 0; by != blocksY; ++by )
		{
			for ( int bx = 0; bx != blocksX; ++bx )
			{
				Color pixels[16];
				ReadBlock( input, bx, by, pixels );
				uint8_t *block = dst + ( by * blocksX + bx ) * blockSize;
				if ( UsesExplicitAlpha( format ) )
				{
					EncodeDxt3Alpha( pixels, block );
					EncodeColorBlock( pixels, false, block + 8 );
				}
				else if ( UsesInterpolatedAlpha( format ) )
				{
					EncodeDxt5Alpha( pixels, block );
					EncodeColorBlock( pixels, false, block + 8 );
				}
				else
				{
					EncodeColorBlock( pixels, true, block );
				}
			}
		}
	}

	void Decode( const DxtSurfaceDesc &input, Format format, void *outputPixels )
	{
		const int blocksX = Blocks( input.width );
		const int blocksY = Blocks( input.height );
		const int blockSize = BlockSize( format );
		const int rowPitch = input.pitch > 0 ? input.pitch : blocksX * blockSize;
		const uint8_t *src = static_cast<const uint8_t*>( input.data );
		uint32_t *dst = static_cast<uint32_t*>( outputPixels );
		for ( int by = 0; by != blocksY; ++by )
		{
			for ( int bx = 0; bx != blocksX; ++bx )
			{
				const uint8_t *block = src + by * rowPitch + bx * blockSize;
				Color pixels[16];
				if ( UsesExplicitAlpha( format ) )
				{
					DecodeColorBlock( block + 8, false, pixels );
					DecodeDxt3Alpha( block, pixels );
				}
				else if ( UsesInterpolatedAlpha( format ) )
				{
					DecodeColorBlock( block + 8, false, pixels );
					DecodeDxt5Alpha( block, pixels );
				}
				else
				{
					DecodeColorBlock( block, true, pixels );
				}
				WriteBlock( pixels, bx, by, input.width, input.height, dst );
			}
		}
	}
}
