// T04: measures the per-pixel delta between the MFC-era DXT encoder (ported into
// Sources/src/ResourceModel/spike/legacy_dxt.cpp from Sources/src/Image/S3TC.cpp @ 40645ad29^) and
// the portable NDxt encoder. Deterministic: three fixed 256x256 ARGB8888 images (smooth gradient,
// high-frequency hashed noise, sharp mask) are encoded by both encoders and decoded by NDxt, the
// absolute per-channel delta is collected, and the max_delta / p99 for each of DXT1, DXT3, DXT5 is
// written to tools/zig/fixtures/resource_editor/dxt-tolerance.json with the schema
//   { "schema_version": 1, "formats": { "DXT1": {"max_delta":N,"p99":M}, "DXT3": {...}, "DXT5": {...} } }
// Per-format histograms land in zig-out/local-test/resource_editor/dxt/histogram-<fmt>.csv so a
// future golden regression can be read as a distribution, not a scalar.
#include "ResourceModel/spike/legacy_dxt.h"
#include "Image/DxtCodec.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	constexpr int kImageSize = 256;
	constexpr int kPixelsPerImage = kImageSize * kImageSize;

	struct Image
	{
		std::string name;
		std::vector<uint32_t> pixels; // ARGB8888, row-major, tightly packed
	};

	// Small LCG so the fixture is reproducible across platforms and compilers; std::mt19937 is
	// deterministic but needlessly heavy for this seed-and-sample use.
	class Lcg
	{
	public:
		explicit Lcg( uint32_t seed ) : m_state( seed ? seed : 1u ) {}
		uint32_t Next()
		{
			m_state = m_state * 1664525u + 1013904223u;
			return m_state;
		}
	private:
		uint32_t m_state;
	};

	uint32_t Pack( uint8_t a, uint8_t r, uint8_t g, uint8_t b )
	{
		return ( uint32_t( a ) << 24 ) | ( uint32_t( r ) << 16 ) | ( uint32_t( g ) << 8 ) | uint32_t( b );
	}

	Image MakeGradient()
	{
		Image img;
		img.name = "gradient";
		img.pixels.resize( kPixelsPerImage );
		for ( int y = 0; y < kImageSize; ++y )
		{
			for ( int x = 0; x < kImageSize; ++x )
			{
				const uint8_t r = uint8_t( x );                           // horizontal red ramp
				const uint8_t g = uint8_t( y );                           // vertical green ramp
				const uint8_t b = uint8_t( ( x + y ) / 2 );               // diagonal blue ramp
				const uint8_t a = uint8_t( std::min( 255, x + y ) );      // alpha ramp
				img.pixels[y * kImageSize + x] = Pack( a, r, g, b );
			}
		}
		return img;
	}

	Image MakeNoise()
	{
		Image img;
		img.name = "noise";
		img.pixels.resize( kPixelsPerImage );
		Lcg rng( 0xC0FFEEu );
		for ( int i = 0; i < kPixelsPerImage; ++i )
		{
			const uint32_t r0 = rng.Next();
			const uint32_t r1 = rng.Next();
			img.pixels[i] = Pack(
				uint8_t( r1 >> 24 ),
				uint8_t( r0 >> 24 ),
				uint8_t( r0 >> 16 ),
				uint8_t( r0 >> 8 ) );
		}
		return img;
	}

	Image MakeMask()
	{
		Image img;
		img.name = "mask";
		img.pixels.resize( kPixelsPerImage );
		// Sharp alpha mask with a hard circular edge plus bands of RGB; punchthrough content
		// is where DXT1 (1-bit alpha) and DXT3/5 encoders disagree most visibly.
		const int cx = kImageSize / 2;
		const int cy = kImageSize / 2;
		const int radiusSq = ( kImageSize / 3 ) * ( kImageSize / 3 );
		for ( int y = 0; y < kImageSize; ++y )
		{
			for ( int x = 0; x < kImageSize; ++x )
			{
				const int dx = x - cx;
				const int dy = y - cy;
				const bool inside = ( dx * dx + dy * dy ) < radiusSq;
				const uint8_t a = inside ? 255u : 0u;
				const uint8_t r = ( ( x >> 4 ) & 1 ) ? 255u : 32u;
				const uint8_t g = ( ( y >> 4 ) & 1 ) ? 224u : 16u;
				const uint8_t b = ( ( ( x + y ) >> 5 ) & 1 ) ? 192u : 48u;
				img.pixels[y * kImageSize + x] = Pack( a, r, g, b );
			}
		}
		return img;
	}

	const char *FormatName( NDxt::Format f )
	{
		switch ( f )
		{
			case NDxt::Format::DXT1: return "DXT1";
			case NDxt::Format::DXT3: return "DXT3";
			case NDxt::Format::DXT5: return "DXT5";
			default: return "DXT?";
		}
	}

	NLegacyDxt::Format ToLegacy( NDxt::Format f )
	{
		switch ( f )
		{
			case NDxt::Format::DXT1: return NLegacyDxt::Format::DXT1;
			case NDxt::Format::DXT3: return NLegacyDxt::Format::DXT3;
			case NDxt::Format::DXT5: return NLegacyDxt::Format::DXT5;
			default: return NLegacyDxt::Format::DXT1;
		}
	}

	bool DecodeAll(
		const std::vector<uint32_t> &pixels,
		NDxt::Format format,
		std::vector<uint32_t> &legacyDecoded,
		std::vector<uint32_t> &modernDecoded,
		std::string &diagnostic )
	{
		const int ndxtEncodedSize = NDxt::GetEncodedSize( kImageSize, kImageSize, format );
		const int legacyEncodedSize = NLegacyDxt::GetEncodedSize(
			kImageSize, kImageSize, ToLegacy( format ) );
		if ( ndxtEncodedSize != legacyEncodedSize )
		{
			std::ostringstream msg;
			msg << "encoded size mismatch for " << FormatName( format )
				<< ": ndxt=" << ndxtEncodedSize << " legacy=" << legacyEncodedSize;
			diagnostic = msg.str();
			return false;
		}

		std::vector<uint8_t> legacyEncoded;
		legacyEncoded.resize( size_t( legacyEncodedSize ) );
		std::vector<uint8_t> modernEncoded;
		modernEncoded.resize( size_t( ndxtEncodedSize ) );

		NLegacyDxt::SurfaceDesc legacyIn;
		legacyIn.width = kImageSize;
		legacyIn.height = kImageSize;
		legacyIn.pitch = kImageSize * 4;
		legacyIn.data = pixels.data();
		NLegacyDxt::Encode( legacyIn, ToLegacy( format ), legacyEncoded.data() );

		NDxt::DxtSurfaceDesc modernIn;
		modernIn.width = kImageSize;
		modernIn.height = kImageSize;
		modernIn.pitch = kImageSize * 4;
		modernIn.data = pixels.data();
		NDxt::Encode( modernIn, format, modernEncoded.data() );

		legacyDecoded.assign( kPixelsPerImage, 0u );
		modernDecoded.assign( kPixelsPerImage, 0u );

		NDxt::DxtSurfaceDesc legacyDecIn;
		legacyDecIn.width = kImageSize;
		legacyDecIn.height = kImageSize;
		legacyDecIn.pitch = 0;
		legacyDecIn.data = legacyEncoded.data();
		NDxt::Decode( legacyDecIn, format, legacyDecoded.data() );

		NDxt::DxtSurfaceDesc modernDecIn = legacyDecIn;
		modernDecIn.data = modernEncoded.data();
		NDxt::Decode( modernDecIn, format, modernDecoded.data() );

		return true;
	}

	struct FormatResult
	{
		int maxDelta;
		int p99;
		std::vector<int> histogram; // 256 bins, counts per absolute delta value
	};

	FormatResult MeasureFormat( const std::vector<Image> &images, NDxt::Format format )
	{
		FormatResult r;
		r.maxDelta = 0;
		r.p99 = 0;
		r.histogram.assign( 256, 0 );
		for ( const Image &img : images )
		{
			std::vector<uint32_t> legacyDecoded;
			std::vector<uint32_t> modernDecoded;
			std::string diag;
			if ( !DecodeAll( img.pixels, format, legacyDecoded, modernDecoded, diag ) )
			{
				std::fprintf( stderr, "encode/decode fault: format=%s image=%s detail=%s\n",
					FormatName( format ), img.name.c_str(), diag.c_str() );
				std::exit( 2 );
			}
			bool firstFaultLogged = false;
			for ( int i = 0; i < kPixelsPerImage; ++i )
			{
				const uint32_t a = legacyDecoded[size_t( i )];
				const uint32_t b = modernDecoded[size_t( i )];
				const uint8_t deltas[4] = {
					uint8_t( std::abs( int( ( a >> 24 ) & 0xff ) - int( ( b >> 24 ) & 0xff ) ) ),
					uint8_t( std::abs( int( ( a >> 16 ) & 0xff ) - int( ( b >> 16 ) & 0xff ) ) ),
					uint8_t( std::abs( int( ( a >>  8 ) & 0xff ) - int( ( b >>  8 ) & 0xff ) ) ),
					uint8_t( std::abs( int( ( a       ) & 0xff ) - int( ( b       ) & 0xff ) ) )
				};
				for ( int c = 0; c < 4; ++c )
				{
					r.histogram[deltas[c]] += 1;
					if ( deltas[c] > 0 && !firstFaultLogged && deltas[c] >= 32 )
					{
						// Diagnostics gate: large per-channel deltas are rare and useful. Log at
						// most one per image/format so a future regression surfaces the pixel.
						const int x = i % kImageSize;
						const int y = i / kImageSize;
						std::fprintf( stderr,
							"delta>=32 format=%s image=%s pixel=(%d,%d) legacy=%08x modern=%08x\n",
							FormatName( format ), img.name.c_str(), x, y, a, b );
						firstFaultLogged = true;
					}
				}
			}
		}
		// max_delta is the highest bin with a nonzero count; p99 is the smallest D such that
		// 99% of the collected per-channel deltas are <= D.
		long long total = 0;
		for ( int bin : r.histogram )
			total += bin;
		long long threshold = ( total * 99 + 99 ) / 100; // ceil(total * 0.99)
		long long running = 0;
		bool p99Set = false;
		for ( int d = 0; d < 256; ++d )
		{
			running += r.histogram[d];
			if ( r.histogram[d] > 0 )
				r.maxDelta = d;
			if ( !p99Set && running >= threshold )
			{
				r.p99 = d;
				p99Set = true;
			}
		}
		return r;
	}

	void WriteHistogram( const fs::path &outDir, const char *formatName, const FormatResult &r )
	{
		std::error_code ec;
		fs::create_directories( outDir, ec );
		fs::path path = outDir / ( std::string( "histogram-" ) + formatName + ".csv" );
		std::ofstream f( path, std::ios::binary );
		if ( !f )
		{
			std::fprintf( stderr, "cannot open %s\n", path.string().c_str() );
			std::exit( 2 );
		}
		// CRLF so matches .gitattributes if ever tracked; the output lives under zig-out but
		// stays consistent with the rest of the tree.
		f << "delta,count\r\n";
		for ( int d = 0; d < 256; ++d )
			f << d << "," << r.histogram[size_t( d )] << "\r\n";
	}
}

int main( int argc, char **argv )
{
	fs::path jsonOut = "tools/zig/fixtures/resource_editor/dxt-tolerance.json";
	fs::path histogramDir = "zig-out/local-test/resource_editor/dxt";
	if ( argc >= 2 ) jsonOut = argv[1];
	if ( argc >= 3 ) histogramDir = argv[2];

	const std::vector<Image> images = { MakeGradient(), MakeNoise(), MakeMask() };

	const NDxt::Format formats[3] = { NDxt::Format::DXT1, NDxt::Format::DXT3, NDxt::Format::DXT5 };
	FormatResult results[3];
	for ( int i = 0; i < 3; ++i )
	{
		results[i] = MeasureFormat( images, formats[i] );
		WriteHistogram( histogramDir, FormatName( formats[i] ), results[i] );
	}

	std::ostringstream json;
	// CRLF to match .gitattributes `* text=auto eol=crlf`. Writing LF would make the test
	// non-idempotent across a fresh checkout (git would materialise CRLF, the test would then
	// overwrite with LF).
	const char *const eol = "\r\n";
	json << "{" << eol;
	json << "  \"schema_version\": 1," << eol;
	json << "  \"formats\": {" << eol;
	for ( int i = 0; i < 3; ++i )
	{
		json << "    \"" << FormatName( formats[i] ) << "\": {" << eol;
		json << "      \"max_delta\": " << results[i].maxDelta << "," << eol;
		json << "      \"p99\": " << results[i].p99 << eol;
		json << "    }";
		if ( i + 1 < 3 )
			json << ",";
		json << eol;
	}
	json << "  }" << eol;
	json << "}" << eol;
	std::string body = json.str();

	std::error_code ec;
	fs::create_directories( jsonOut.parent_path(), ec );
	std::ofstream f( jsonOut, std::ios::binary );
	if ( !f )
	{
		std::fprintf( stderr, "cannot open %s\n", jsonOut.string().c_str() );
		return 2;
	}
	f.write( body.data(), std::streamsize( body.size() ) );
	if ( !f )
	{
		std::fprintf( stderr, "write failed for %s\n", jsonOut.string().c_str() );
		return 2;
	}
	std::fprintf( stderr, "wrote %s (%zu bytes)\n", jsonOut.string().c_str(), body.size() );
	for ( int i = 0; i < 3; ++i )
		std::fprintf( stderr, "  %s: max_delta=%d p99=%d\n",
			FormatName( formats[i] ), results[i].maxDelta, results[i].p99 );
	return 0;
}
