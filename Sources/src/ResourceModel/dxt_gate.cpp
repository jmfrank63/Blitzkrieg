#include "dxt_gate.h"
#include "../Image/DxtCodec.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace NResourceModel
{

namespace
{

constexpr size_t kHeaderSize = 128;

unsigned Field( const std::string &bytes, size_t nOffset )
{
	unsigned nValue = 0;
	for ( int i = 3; i >= 0; --i )
		nValue = ( nValue << 8 ) | static_cast<unsigned char>( bytes[nOffset + i] );
	return nValue;
}

bool ToFormat( const std::string &szFourCC, NDxt::Format *pFormat )
{
	static const struct { const char *pszName; NDxt::Format format; } kFormats[] = {
		{ "DXT1", NDxt::Format::DXT1 }, { "DXT2", NDxt::Format::DXT2 }, { "DXT3", NDxt::Format::DXT3 },
		{ "DXT4", NDxt::Format::DXT4 }, { "DXT5", NDxt::Format::DXT5 },
	};
	for ( const auto &entry : kFormats )
		if ( szFourCC == entry.pszName )
		{
			*pFormat = entry.format;
			return true;
		}
	return false;
}

int MipCount( const std::string &bytes )
{
	return (std::max)( 1u, Field( bytes, 28 ) );
}

int P99( const std::vector<long long> &histogram )
{
	long long nTotal = 0;
	for ( long long nCount : histogram )
		nTotal += nCount;
	const long long nThreshold = ( nTotal * 99 + 99 ) / 100;
	long long nRunning = 0;
	for ( int d = 0; d < 256; ++d )
	{
		nRunning += histogram[d];
		if ( nRunning >= nThreshold )
			return d;
	}
	return 255;
}

int Max( const std::vector<long long> &histogram )
{
	for ( int d = 255; d > 0; --d )
		if ( histogram[d] > 0 )
			return d;
	return 0;
}

// The integer after "szKey": inside szObject. False when the key is absent.
bool ReadInt( const std::string &szObject, const std::string &szKey, int *pValue )
{
	const size_t nKey = szObject.find( "\"" + szKey + "\"" );
	if ( nKey == std::string::npos )
		return false;
	size_t nAt = szObject.find( ':', nKey + szKey.size() + 2 );
	if ( nAt == std::string::npos )
		return false;
	++nAt;
	while ( nAt < szObject.size() && isspace( static_cast<unsigned char>( szObject[nAt] ) ) )
		++nAt;
	if ( nAt >= szObject.size() || !isdigit( static_cast<unsigned char>( szObject[nAt] ) ) )
		return false;
	*pValue = atoi( szObject.c_str() + nAt );
	return true;
}

}

bool DecodeDds( const std::string &bytes, SDdsImage *pImage, std::string *pszError )
{
	*pImage = SDdsImage();
	if ( bytes.size() < kHeaderSize || bytes.compare( 0, 4, "DDS " ) != 0 )
	{
		*pszError = "not a DDS file";
		return false;
	}
	if ( ( Field( bytes, 80 ) & 4 ) == 0 )
	{
		*pszError = "uncompressed DDS, no FourCC";
		return false;
	}
	pImage->szFourCC = bytes.substr( 84, 4 );
	NDxt::Format format;
	if ( !ToFormat( pImage->szFourCC, &format ) )
	{
		*pszError = "FourCC " + pImage->szFourCC + " is not DXT1-5";
		return false;
	}
	const int nWidth = static_cast<int>( Field( bytes, 16 ) ), nHeight = static_cast<int>( Field( bytes, 12 ) );
	if ( nWidth <= 0 || nHeight <= 0 || nWidth > 16384 || nHeight > 16384 )
	{
		*pszError = "bad size " + std::to_string( nWidth ) + "x" + std::to_string( nHeight );
		return false;
	}
	size_t nAt = kHeaderSize;
	const int nMips = MipCount( bytes );
	for ( int i = 0; i < nMips; ++i )
	{
		SDdsMip mip;
		mip.nWidth = (std::max)( 1, nWidth >> i );
		mip.nHeight = (std::max)( 1, nHeight >> i );
		const size_t nSize = static_cast<size_t>( NDxt::GetEncodedSize( mip.nWidth, mip.nHeight, format ) );
		if ( bytes.size() < nAt + nSize )
		{
			*pszError = "mip " + std::to_string( i ) + " needs " + std::to_string( nSize ) + " bytes at offset " + std::to_string( nAt ) +
			            ", the file has " + std::to_string( bytes.size() );
			return false;
		}
		mip.pixels.resize( static_cast<size_t>( mip.nWidth ) * mip.nHeight );
		const NDxt::DxtSurfaceDesc in = { mip.nWidth, mip.nHeight, 0, bytes.data() + nAt };
		NDxt::Decode( in, format, mip.pixels.data() );
		pImage->mips.push_back( std::move( mip ) );
		nAt += nSize;
	}
	return true;
}

bool EncodeDds( const std::string &header, const SDdsImage &image, std::string *pBytes, std::string *pszError )
{
	NDxt::Format format;
	if ( header.size() < kHeaderSize || !ToFormat( image.szFourCC, &format ) )
	{
		*pszError = "EncodeDds needs a 128-byte header and a DXT1-5 image";
		return false;
	}
	*pBytes = header.substr( 0, kHeaderSize );
	for ( const SDdsMip &mip : image.mips )
	{
		std::string blocks( static_cast<size_t>( NDxt::GetEncodedSize( mip.nWidth, mip.nHeight, format ) ), '\0' );
		const NDxt::DxtSurfaceDesc in = { mip.nWidth, mip.nHeight, mip.nWidth * 4, mip.pixels.data() };
		NDxt::Encode( in, format, &blocks[0] );
		*pBytes += blocks;
	}
	return true;
}

void SDxtDelta::Add( int nMip, const SDdsMip &left, const SDdsMip &right )
{
	const size_t nPixels = (std::min)( left.pixels.size(), right.pixels.size() );
	for ( size_t i = 0; i < nPixels; ++i )
	{
		const unsigned a = left.pixels[i], b = right.pixels[i];
		int nPixelWorst = 0;
		for ( int nShift = 0; nShift < 32; nShift += 8 )
		{
			const int d = std::abs( static_cast<int>( ( a >> nShift ) & 0xff ) - static_cast<int>( ( b >> nShift ) & 0xff ) );
			++( nShift == 24 ? alpha : colour )[d];
			nPixelWorst = (std::max)( nPixelWorst, d );
		}
		if ( nPixelWorst > nWorst )
		{
			nWorst = nPixelWorst;
			nWorstMip = nMip;
			nWorstX = static_cast<int>( i % static_cast<size_t>( left.nWidth ) );
			nWorstY = static_cast<int>( i / static_cast<size_t>( left.nWidth ) );
			nWorstLeft = a;
			nWorstRight = b;
		}
	}
}

SDxtStats SDxtDelta::Stats() const
{
	SDxtStats stats;
	stats.nColourMax = Max( colour );
	stats.nColourP99 = P99( colour );
	stats.nAlphaMax = Max( alpha );
	stats.nAlphaP99 = P99( alpha );
	return stats;
}

const SDxtStats *SDxtTolerance::Find( const std::string &szFourCC ) const
{
	for ( const auto &format : formats )
		if ( format.first == szFourCC )
			return &format.second;
	return nullptr;
}

bool LoadDxtTolerance( const std::string &szJsonFile, SDxtTolerance *pTolerance, std::string *pszError )
{
	*pTolerance = SDxtTolerance();
	std::ifstream file( szJsonFile, std::ios::binary );
	if ( !file )
	{
		*pszError = "cannot open " + szJsonFile;
		return false;
	}
	const std::string text( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
	int nSchema = 0;
	if ( !ReadInt( text, "schema_version", &nSchema ) || nSchema != 2 )
	{
		*pszError = szJsonFile + ": schema_version is not 2";
		return false;
	}
	const size_t nGate = text.find( "\"gate\"" );
	const size_t nOpen = nGate == std::string::npos ? std::string::npos : text.find( '{', nGate );
	if ( nOpen == std::string::npos )
	{
		*pszError = szJsonFile + ": no \"gate\" object";
		return false;
	}
	// The gate object holds one flat object per format: "DXT1": { ... }.
	size_t nAt = nOpen + 1;
	for ( ;; )
	{
		const size_t nNext = text.find_first_of( "\"}", nAt );
		if ( nNext == std::string::npos )
		{
			*pszError = szJsonFile + ": the \"gate\" object is not closed";
			return false;
		}
		if ( text[nNext] == '}' )
			break;
		const size_t nNameEnd = text.find( '"', nNext + 1 );
		const size_t nBody = nNameEnd == std::string::npos ? std::string::npos : text.find( '{', nNameEnd );
		const size_t nBodyEnd = nBody == std::string::npos ? std::string::npos : text.find( '}', nBody );
		if ( nBodyEnd == std::string::npos )
		{
			*pszError = szJsonFile + ": a \"gate\" entry is not an object";
			return false;
		}
		const std::string szFormat = text.substr( nNext + 1, nNameEnd - nNext - 1 );
		const std::string szBody = text.substr( nBody, nBodyEnd - nBody + 1 );
		SDxtStats stats;
		const struct { const char *pszKey; int *pValue; } kKeys[] = {
			{ "colour_max_delta", &stats.nColourMax }, { "colour_p99", &stats.nColourP99 },
			{ "alpha_max_delta", &stats.nAlphaMax }, { "alpha_p99", &stats.nAlphaP99 },
		};
		for ( const auto &key : kKeys )
			if ( !ReadInt( szBody, key.pszKey, key.pValue ) )
			{
				*pszError = szJsonFile + ": gate " + szFormat + " has no integer \"" + key.pszKey + "\"";
				return false;
			}
		pTolerance->formats.emplace_back( szFormat, stats );
		nAt = nBodyEnd + 1;
	}
	if ( pTolerance->formats.empty() )
	{
		*pszError = szJsonFile + ": the \"gate\" object is empty";
		return false;
	}
	pTolerance->bLoaded = true;
	return true;
}

}
