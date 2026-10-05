#pragma once
// The decoded-pixel gate for DXT textures (_c.dds), spec D-11 / S03 T07.
//
// The port's exporter encodes with NDxt; MFC's encoded with the S3TC code
// ported into spike/legacy_dxt. The same source image therefore gives
// different blocks, and a port export can never be byte-equal to an MFC
// golden. Instead both are decoded with NDxt and their per-channel deltas are
// held to a tolerance measured on shipped textures
// (tools/zig/fixtures/resource_editor/dxt-tolerance.json, written by
// `zig build measure-dxt-tolerance`). R, G and B are pooled as colour and A
// is measured on its own, because alpha comes from a separate block in DXT3
// and DXT5 and from punch-through in DXT1.

#include <string>
#include <vector>

namespace NResourceModel
{

// One decoded mip level, ARGB8888 row-major.
struct SDdsMip
{
	int nWidth = 0;
	int nHeight = 0;
	std::vector<unsigned> pixels;
};

struct SDdsImage
{
	std::string szFourCC;  // "DXT1", "DXT3", "DXT5", ... or empty for an uncompressed surface
	std::vector<SDdsMip> mips;
};

// Decodes every mip level of a DXT1/2/3/4/5 DDS. False with a reason for a
// file that is not a DDS, is uncompressed (szFourCC stays empty), or is
// shorter than its mip chain needs.
bool DecodeDds( const std::string &bytes, SDdsImage *pImage, std::string *pszError );
// Encodes decoded mips back into a DDS with the same header, by NDxt.
bool EncodeDds( const std::string &header, const SDdsImage &image, std::string *pBytes, std::string *pszError );

// Max and p99 of the absolute per-channel deltas between two images.
struct SDxtStats
{
	int nColourMax = 0;
	int nColourP99 = 0;
	int nAlphaMax = 0;
	int nAlphaP99 = 0;
};

// Histograms of the deltas; Stats() reduces them. p99 is the smallest D such
// that at least 99% of the deltas are <= D.
struct SDxtDelta
{
	std::vector<long long> colour = std::vector<long long>( 256, 0 );
	std::vector<long long> alpha = std::vector<long long>( 256, 0 );
	// The first pixel whose colour or alpha delta is the largest seen, for the
	// failure message.
	int nWorstMip = -1, nWorstX = 0, nWorstY = 0, nWorst = -1;
	unsigned nWorstLeft = 0, nWorstRight = 0;

	void Add( int nMip, const SDdsMip &left, const SDdsMip &right );
	SDxtStats Stats() const;
};

// The gate per format, read from dxt-tolerance.json.
struct SDxtTolerance
{
	bool bLoaded = false;
	std::vector<std::pair<std::string, SDxtStats>> formats;
	const SDxtStats *Find( const std::string &szFourCC ) const;
};

// Reads the "gate" object of dxt-tolerance.json (schema_version 2). False with
// the missing key or the reason when the file cannot be used.
bool LoadDxtTolerance( const std::string &szJsonFile, SDxtTolerance *pTolerance, std::string *pszError );

}
