#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "../../Sources/src/Image/DxtCodec.h"

namespace
{
	void Require( bool condition, const char *message )
	{
		if ( !condition )
		{
			std::cerr << message << std::endl;
			std::exit( 1 );
		}
	}

	void RequireNear( std::uint32_t actual, std::uint32_t expected, int tolerance, const char *message )
	{
		for ( int shift = 0; shift != 32; shift += 8 )
		{
			const int a = ( actual >> shift ) & 0xff;
			const int e = ( expected >> shift ) & 0xff;
			if ( std::abs( a - e ) > tolerance )
			{
				std::cerr << message << " actual=0x" << std::hex << actual << " expected=0x" << expected << std::dec << std::endl;
				std::exit( 1 );
			}
		}
	}

	void TestDxt1KnownSolidRedBlock()
	{
		std::uint8_t block[8] = {};
		block[0] = 0x00;
		block[1] = 0xf8;
		std::uint32_t pixels[16] = {};
		NDxt::DxtSurfaceDesc input = { 4, 4, 8, block };

		NDxt::Decode( input, NDxt::Format::DXT1, pixels );

		for ( int i = 0; i != 16; ++i )
			Require( pixels[i] == 0xffff0000u, "DXT1 solid red block did not decode to opaque red" );
	}

	void TestDxt3KnownSolidRedBlock()
	{
		std::uint8_t block[16] = {};
		std::memset( block, 0xff, 8 );
		block[8] = 0x00;
		block[9] = 0xf8;
		std::uint32_t pixels[16] = {};
		NDxt::DxtSurfaceDesc input = { 4, 4, 16, block };

		NDxt::Decode( input, NDxt::Format::DXT3, pixels );

		for ( int i = 0; i != 16; ++i )
			Require( pixels[i] == 0xffff0000u, "DXT3 solid red block did not decode to opaque red" );
	}

	void TestDxt5KnownSolidRedBlock()
	{
		std::uint8_t block[16] = {};
		block[0] = 0xff;
		block[1] = 0x00;
		block[8] = 0x00;
		block[9] = 0xf8;
		std::uint32_t pixels[16] = {};
		NDxt::DxtSurfaceDesc input = { 4, 4, 16, block };

		NDxt::Decode( input, NDxt::Format::DXT5, pixels );

		for ( int i = 0; i != 16; ++i )
			Require( pixels[i] == 0xffff0000u, "DXT5 solid red block did not decode to opaque red" );
	}

	void TestRoundTripSolidColors()
	{
		const std::uint32_t source[16] = {
			0xff3366ccu, 0xff3366ccu, 0xff3366ccu, 0xff3366ccu,
			0xff3366ccu, 0xff3366ccu, 0xff3366ccu, 0xff3366ccu,
			0xff3366ccu, 0xff3366ccu, 0xff3366ccu, 0xff3366ccu,
			0xff3366ccu, 0xff3366ccu, 0xff3366ccu, 0xff3366ccu
		};
		std::vector<std::uint8_t> encoded( NDxt::GetEncodedSize( 4, 4, NDxt::Format::DXT5 ) );
		std::uint32_t decoded[16] = {};

		NDxt::DxtSurfaceDesc input = { 4, 4, 16, source };
		NDxt::Encode( input, NDxt::Format::DXT5, &encoded[0] );
		NDxt::DxtSurfaceDesc encodedInput = { 4, 4, static_cast<int>( encoded.size() ), &encoded[0] };
		NDxt::Decode( encodedInput, NDxt::Format::DXT5, decoded );

		for ( int i = 0; i != 16; ++i )
			RequireNear( decoded[i], 0xff3366ccu, 8, "DXT5 solid color round-trip drifted too far" );
	}
}

int main()
{
	Require( NDxt::GetEncodedSize( 1, 1, NDxt::Format::DXT1 ) == 8, "DXT1 size for tiny image must be one block" );
	Require( NDxt::GetEncodedSize( 5, 5, NDxt::Format::DXT5 ) == 64, "DXT5 size must round up to 4x4 blocks" );
	Require( NDxt::GetDecodedSize( 5, 5 ) == 100, "Decoded size must be width * height * 4" );
	TestDxt1KnownSolidRedBlock();
	TestDxt3KnownSolidRedBlock();
	TestDxt5KnownSolidRedBlock();
	TestRoundTripSolidColors();
	return 0;
}
