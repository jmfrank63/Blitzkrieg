// S03 T07: measures the DXT tolerance the D-11 comparator's _c.dds gate reads, on shipped textures.
//
// For every file in kFiles (shipped _c.dds per format, picked across content kinds and including
// real alpha: DXT1 punch-through tilesets, DXT3 chapter maps, fonts and cursors, DXT5 units,
// buildings, particles and roads), every mip is decoded with NDxt into ARGB8888, the reference.
// The reference is then encoded twice, by NDxt (the port's encoder) and by NLegacyDxt (the MFC-era
// S3TC encoder in Sources/src/ResourceModel/spike/legacy_dxt.cpp), and decoded again with NDxt:
//
//   ndxt_reencode   reference against NDxt(reference): what re-encoding a shipped texture costs;
//   legacy_vs_ndxt  NLegacyDxt(reference) against NDxt(reference): the port's export against an
//                   MFC golden made from the same source image.
//
// Absolute per-channel deltas are reduced per file to colour (R, G, B pooled) and alpha max and
// p99 (NResourceModel::SDxtDelta). The gate per format is the largest of each statistic over its
// files and both measurements, so every listed file re-encoded by NDxt passes it.
//
// argv: <json> <histogram dir> [--check]
//   without --check  writes <json> (zig build measure-dxt-tolerance);
//   with --check     measures again and fails unless <json> is byte-identical (zig build
//                    test-dxt-tolerance), so the committed numbers cannot drift from the code.
// Paths in kFiles are relative to the working directory, the repository root.
#include "ResourceModel/dxt_gate.h"
#include "ResourceModel/spike/legacy_dxt.h"
#include "Image/DxtCodec.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace NResourceModel;

namespace
{
	struct SFile
	{
		const char *pszFormat;
		const char *pszPath;
	};

	const SFile kFiles[] = {
		{ "DXT1", "Data/Terrain/sets/1/tileset_c.dds" },                                   // punch-through alpha
		{ "DXT1", "Data/Terrain/sets/4/tileset_c.dds" },                                   // punch-through alpha
		{ "DXT1", "Data/Units/Technics/USSR/Artillery/57_mm_ZIS_2/2_c.dds" },
		{ "DXT1", "Data/Units/Technics/Allies/SPG/Sexton_II_GB/2a_c.dds" },
		{ "DXT1", "Data/Units/Technics/German/Artillery/CoastBattery_Todt/1_c.dds" },
		{ "DXT1", "Data/Scenarios/ScenarioMissions/ussr/moscow/map_c.dds" },
		{ "DXT3", "Data/Scenarios/Chapters/Allies/Ardennes/map_c.dds" },                   // alpha
		{ "DXT3", "Data/Scenarios/Campaigns/Allies/map_c.dds" },                           // alpha
		{ "DXT3", "Data/Fonts/medium/1_c.dds" },                                           // alpha
		{ "DXT3", "Data/Effects/Particles/smokeSEQ2_c.dds" },                              // alpha
		{ "DXT3", "Data/Cursor/attack_c.dds" },                                            // alpha
		{ "DXT3", "Data/Effects/Sprites/Flash/1_c.dds" },                                  // opaque
		{ "DXT5", "Data/Units/Humans/Allies/Bren/1_c.dds" },                               // alpha
		{ "DXT5", "Data/Buildings/ussr/summer/admhouse/1_c.dds" },                         // alpha
		{ "DXT5", "Data/Bridges/asphaltbridge/01/1_c.dds" },                               // alpha
		{ "DXT5", "Data/Objects/Flora/Africa/Summer/Palm01/1_c.dds" },                     // alpha
		{ "DXT5", "Data/Effects/Particles/Oblomok01_c.dds" },                              // alpha
		{ "DXT5", "Data/Terrain/sets/4/Roads3D/road_asphalt_ground_c.dds" },               // alpha
		{ "DXT5", "Data/Medals/German/Africa/1_c.dds" },                                   // alpha
		{ "DXT5", "Data/Water/water_c.dds" },                                              // opaque
	};
	const char *const kFormats[] = { "DXT1", "DXT3", "DXT5" };

	struct SMeasured
	{
		const SFile *pFile;
		int nWidth, nHeight, nMips;
		long long nAlphaPixels; // reference pixels with alpha below 255
		SDxtStats ndxt, legacy;
	};

	NLegacyDxt::Format ToLegacy( const std::string &szFormat )
	{
		return szFormat == "DXT1" ? NLegacyDxt::Format::DXT1 : szFormat == "DXT3" ? NLegacyDxt::Format::DXT3 : NLegacyDxt::Format::DXT5;
	}

	NDxt::Format ToNDxt( const std::string &szFormat )
	{
		return szFormat == "DXT1" ? NDxt::Format::DXT1 : szFormat == "DXT3" ? NDxt::Format::DXT3 : NDxt::Format::DXT5;
	}

	[[noreturn]] void Fail( const std::string &szMessage )
	{
		std::fprintf( stderr, "measure-dxt-tolerance: %s\n", szMessage.c_str() );
		std::exit( 2 );
	}

	// Encodes one mip with the given encoder and decodes it back with NDxt.
	SDdsMip RoundTrip( const SDdsMip &reference, const std::string &szFormat, bool bLegacy )
	{
		const NDxt::Format format = ToNDxt( szFormat );
		std::vector<unsigned char> blocks( static_cast<size_t>( NDxt::GetEncodedSize( reference.nWidth, reference.nHeight, format ) ) );
		if ( bLegacy )
		{
			if ( NLegacyDxt::GetEncodedSize( reference.nWidth, reference.nHeight, ToLegacy( szFormat ) ) != static_cast<int>( blocks.size() ) )
				Fail( "NLegacyDxt and NDxt disagree on the encoded size of " + szFormat );
			const NLegacyDxt::SurfaceDesc in = { reference.nWidth, reference.nHeight, reference.nWidth * 4, reference.pixels.data() };
			NLegacyDxt::Encode( in, ToLegacy( szFormat ), blocks.data() );
		}
		else
		{
			const NDxt::DxtSurfaceDesc in = { reference.nWidth, reference.nHeight, reference.nWidth * 4, reference.pixels.data() };
			NDxt::Encode( in, format, blocks.data() );
		}
		SDdsMip decoded;
		decoded.nWidth = reference.nWidth;
		decoded.nHeight = reference.nHeight;
		decoded.pixels.resize( reference.pixels.size() );
		const NDxt::DxtSurfaceDesc in = { reference.nWidth, reference.nHeight, 0, blocks.data() };
		NDxt::Decode( in, format, decoded.pixels.data() );
		return decoded;
	}

	void AddHistogram( std::vector<long long> &total, const std::vector<long long> &add )
	{
		for ( size_t i = 0; i < total.size(); ++i )
			total[i] += add[i];
	}

	std::string Stats( const SDxtStats &stats )
	{
		std::ostringstream out;
		out << "{ \"colour_max_delta\": " << stats.nColourMax << ", \"colour_p99\": " << stats.nColourP99
		    << ", \"alpha_max_delta\": " << stats.nAlphaMax << ", \"alpha_p99\": " << stats.nAlphaP99 << " }";
		return out.str();
	}

	void Widen( SDxtStats &gate, const SDxtStats &stats )
	{
		gate.nColourMax = std::max( gate.nColourMax, stats.nColourMax );
		gate.nColourP99 = std::max( gate.nColourP99, stats.nColourP99 );
		gate.nAlphaMax = std::max( gate.nAlphaMax, stats.nAlphaMax );
		gate.nAlphaP99 = std::max( gate.nAlphaP99, stats.nAlphaP99 );
	}
}

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		std::fprintf( stderr, "usage: %s <json> <histogram dir> [--check]\n", argv[0] );
		return 2;
	}
	const fs::path jsonPath = argv[1], histogramDir = argv[2];
	const bool bCheck = argc >= 4 && std::string( argv[3] ) == "--check";

	std::vector<SMeasured> measured;
	// Pooled histograms per format: ndxt colour, ndxt alpha, legacy colour, legacy alpha.
	std::vector<std::vector<std::vector<long long>>> pooled( 3, std::vector<std::vector<long long>>( 4, std::vector<long long>( 256, 0 ) ) );
	for ( const SFile &file : kFiles )
	{
		std::ifstream in( file.pszPath, std::ios::binary );
		if ( !in )
			Fail( std::string( "cannot open " ) + file.pszPath + " (run from the repository root)" );
		const std::string bytes( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
		SDdsImage image;
		std::string szError;
		if ( !DecodeDds( bytes, &image, &szError ) )
			Fail( std::string( file.pszPath ) + ": " + szError );
		if ( image.szFourCC != file.pszFormat )
			Fail( std::string( file.pszPath ) + " is " + image.szFourCC + ", listed as " + file.pszFormat );
		SDxtDelta ndxt, legacy;
		long long nAlphaPixels = 0;
		for ( size_t i = 0; i < image.mips.size(); ++i )
		{
			const SDdsMip &reference = image.mips[i];
			for ( unsigned nPixel : reference.pixels )
				nAlphaPixels += ( nPixel >> 24 ) != 0xff;
			const SDdsMip modern = RoundTrip( reference, image.szFourCC, false );
			ndxt.Add( static_cast<int>( i ), reference, modern );
			legacy.Add( static_cast<int>( i ), RoundTrip( reference, image.szFourCC, true ), modern );
		}
		const int nFormat = static_cast<int>( std::find( std::begin( kFormats ), std::end( kFormats ), image.szFourCC ) - std::begin( kFormats ) );
		AddHistogram( pooled[nFormat][0], ndxt.colour );
		AddHistogram( pooled[nFormat][1], ndxt.alpha );
		AddHistogram( pooled[nFormat][2], legacy.colour );
		AddHistogram( pooled[nFormat][3], legacy.alpha );
		measured.push_back( { &file, image.mips[0].nWidth, image.mips[0].nHeight, static_cast<int>( image.mips.size() ), nAlphaPixels,
		                      ndxt.Stats(), legacy.Stats() } );
		std::fprintf( stderr, "%s %s %dx%d mips=%zu alpha_pixels=%lld ndxt_reencode %s legacy_vs_ndxt %s\n", file.pszFormat, file.pszPath,
		              image.mips[0].nWidth, image.mips[0].nHeight, image.mips.size(), nAlphaPixels, Stats( measured.back().ndxt ).c_str(),
		              Stats( measured.back().legacy ).c_str() );
	}

	// CRLF, as .gitattributes materialises tracked text; LF would make --check fail on a fresh checkout.
	const char *const eol = "\r\n";
	std::ostringstream json;
	json << "{" << eol;
	json << "  \"schema_version\": 2," << eol;
	json << "  \"measured_by\": \"zig build measure-dxt-tolerance (tools/zig/dxt_tolerance_test.cpp); zig build test-dxt-tolerance re-measures and requires this file unchanged\"," << eol;
	json << "  \"method\": \"Every mip of each listed shipped _c.dds is decoded with NDxt into ARGB8888, the reference. "
	        "ndxt_reencode compares the reference with NDxt::Encode then NDxt::Decode of it. "
	        "legacy_vs_ndxt compares NLegacyDxt::Encode (the MFC-era S3TC encoder) with NDxt::Encode of the same reference, both decoded by NDxt. "
	        "Absolute per-channel deltas: colour pools R, G and B, alpha is A alone. max is the largest delta, p99 the smallest D with at least 99 percent of deltas <= D, per file. "
	        "The gate per format is the largest of each statistic over its files and both measurements; the D-11 comparator holds a port _c.dds to it against the golden.\"," << eol;
	json << "  \"files\": [" << eol;
	for ( size_t i = 0; i < measured.size(); ++i )
	{
		const SMeasured &m = measured[i];
		json << "    { \"path\": \"" << m.pFile->pszPath << "\", \"format\": \"" << m.pFile->pszFormat << "\", \"width\": " << m.nWidth
		     << ", \"height\": " << m.nHeight << ", \"mips\": " << m.nMips << ", \"alpha_pixels\": " << m.nAlphaPixels << "," << eol;
		json << "      \"ndxt_reencode\": " << Stats( m.ndxt ) << "," << eol;
		json << "      \"legacy_vs_ndxt\": " << Stats( m.legacy ) << " }" << ( i + 1 < measured.size() ? "," : "" ) << eol;
	}
	json << "  ]," << eol;
	json << "  \"gate\": {" << eol;
	for ( int f = 0; f < 3; ++f )
	{
		SDxtStats gate;
		for ( const SMeasured &m : measured )
			if ( m.pFile->pszFormat == std::string( kFormats[f] ) )
			{
				Widen( gate, m.ndxt );
				Widen( gate, m.legacy );
			}
		json << "    \"" << kFormats[f] << "\": " << Stats( gate ) << ( f + 1 < 3 ? "," : "" ) << eol;
		std::fprintf( stderr, "gate %s %s\n", kFormats[f], Stats( gate ).c_str() );
	}
	json << "  }" << eol;
	json << "}" << eol;
	const std::string body = json.str();

	// Pooled histograms, so a failing golden can be read as a distribution.
	std::error_code error;
	fs::create_directories( histogramDir, error );
	for ( int f = 0; f < 3; ++f )
	{
		std::ofstream csv( histogramDir / ( std::string( "histogram-" ) + kFormats[f] + ".csv" ), std::ios::binary );
		csv << "delta,ndxt_colour,ndxt_alpha,legacy_colour,legacy_alpha\r\n";
		for ( int d = 0; d < 256; ++d )
			csv << d << "," << pooled[f][0][d] << "," << pooled[f][1][d] << "," << pooled[f][2][d] << "," << pooled[f][3][d] << "\r\n";
	}

	if ( bCheck )
	{
		std::ifstream in( jsonPath, std::ios::binary );
		const std::string committed( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
		if ( committed != body )
		{
			size_t nAt = 0;
			while ( nAt < committed.size() && nAt < body.size() && committed[nAt] == body[nAt] )
				++nAt;
			const size_t nLine = body.rfind( '\n', nAt ) == std::string::npos ? 0 : body.rfind( '\n', nAt ) + 1;
			std::fprintf( stderr, "FAIL %s differs from the measurement at byte %zu; run zig build measure-dxt-tolerance and review.\n  measured: %s\n",
			              jsonPath.string().c_str(), nAt, body.substr( nLine, body.find( '\n', nAt ) - nLine ).c_str() );
			return 1;
		}
		std::fprintf( stderr, "PASS %s matches the measurement of %zu shipped textures\n", jsonPath.string().c_str(), measured.size() );
		return 0;
	}
	fs::create_directories( jsonPath.parent_path(), error );
	std::ofstream out( jsonPath, std::ios::binary | std::ios::trunc );
	out.write( body.data(), static_cast<std::streamsize>( body.size() ) );
	if ( !out )
		Fail( "write failed for " + jsonPath.string() );
	std::fprintf( stderr, "wrote %s (%zu bytes)\n", jsonPath.string().c_str(), body.size() );
	return 0;
}
