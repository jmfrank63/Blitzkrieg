// Project-XML round-trip spike for the 21 ResourceEditor extensions (spec D-07).
// For each repo-owned fixture: parse, serialise (.rtout1), re-parse, re-serialise (.rtout2), require the
// two outputs byte-identical and the first output equal to the fixture itself. Then plant an unknown
// <FutureBlob attr="x">body</FutureBlob> under the root and require it to survive the same round trip.
// usage: resource-xml-roundtrip <fixtures-dir> <out-dir>
#include "ResourceModel/spike/xml_spike.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const char *const kExtensions[] = {
	"3rd", "3rv", "bdg", "bld", "cgc", "chc", "eff", "fnc", "gui", "mcp", "mdc",
	"mip", "msh", "obt", "pcp", "scp", "spt", "til", "trc", "unt", "wpn" };

static std::string Lower( std::string s )
{
	for ( char &c : s ) c = (char)std::tolower( (unsigned char)c );
	return s;
}

// Resolves one path component at a time, ignoring case, so a fixture lookup works on any file system.
static fs::path DataFile( const fs::path &root, const std::string &szRel )
{
	fs::path cur = root;
	std::stringstream parts( szRel );
	std::string part;
	while ( std::getline( parts, part, '/' ) )
	{
		fs::path direct = cur / part;
		if ( fs::exists( direct ) ) { cur = direct; continue; }
		bool found = false;
		std::error_code ec;
		for ( const auto &e : fs::directory_iterator( cur, ec ) )
			if ( Lower( e.path().filename().string() ) == Lower( part ) ) { cur = e.path(); found = true; break; }
		if ( !found ) cur = direct;
	}
	return cur;
}

static bool ReadFile( const fs::path &p, std::string &out )
{
	std::ifstream f( p, std::ios::binary );
	if ( !f ) return false;
	out.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
	return true;
}

static void WriteFile( const fs::path &p, const std::string &data )
{
	std::error_code ec;
	fs::create_directories( p.parent_path(), ec );
	std::ofstream f( p, std::ios::binary );
	f.write( data.data(), (std::streamsize)data.size() );
}

static std::string HexWindow( const std::string &s, size_t off )
{
	size_t b = off > 16 ? off - 16 : 0, e = off + 16 < s.size() ? off + 16 : s.size();
	std::string out;
	char buf[8];
	for ( size_t k = b; k < e; ++k )
	{
		std::snprintf( buf, sizeof buf, "%s%02x", k == off ? "[" : "", (unsigned char)s[k] );
		out += buf;
		out += k == off ? "] " : " ";
	}
	return out;
}

static bool Compare( const std::string &szLabel, const std::string &a, const std::string &b )
{
	if ( a.size() == b.size() && std::memcmp( a.data(), b.data(), a.size() ) == 0 ) return true;
	size_t k = 0, n = a.size() < b.size() ? a.size() : b.size();
	while ( k < n && a[k] == b[k] ) ++k;
	std::fprintf( stderr, "FAIL %s: first divergence at byte %zu (sizes %zu vs %zu)\n  A: %s\n  B: %s\n",
		szLabel.c_str(), k, a.size(), b.size(), HexWindow( a, k ).c_str(), HexWindow( b, k ).c_str() );
	return false;
}

static bool RoundTrip( const std::string &szLabel, const std::string &szInput, const fs::path &outBase,
	std::string *pOut1, const std::string *pExpectFirst )
{
	XmlSpike::Document d1, d2;
	std::string err;
	if ( !XmlSpike::Parse( szInput, d1, err ) ) { std::fprintf( stderr, "FAIL %s: parse: %s\n", szLabel.c_str(), err.c_str() ); return false; }
	std::string out1 = XmlSpike::Serialise( d1 );
	if ( !XmlSpike::Parse( out1, d2, err ) ) { std::fprintf( stderr, "FAIL %s: re-parse: %s\n", szLabel.c_str(), err.c_str() ); WriteFile( outBase.string() + ".rtout1", out1 ); return false; }
	std::string out2 = XmlSpike::Serialise( d2 );
	bool ok = Compare( szLabel + " rtout1 vs rtout2", out1, out2 );
	if ( ok && pExpectFirst ) ok = Compare( szLabel + " fixture vs rtout1", *pExpectFirst, out1 );
	if ( !ok )
	{
		WriteFile( outBase.string() + ".rtout1", out1 );
		WriteFile( outBase.string() + ".rtout2", out2 );
	}
	if ( pOut1 ) *pOut1 = out1;
	return ok;
}

int main( int argc, char **argv )
{
	if ( argc < 3 ) { std::fprintf( stderr, "usage: %s <fixtures-dir> <out-dir>\n", argv[0] ); return 2; }
	const fs::path fixtures = argv[1], outDir = argv[2];
	int failures = 0;
	for ( const char *ext : kExtensions )
	{
		std::string e = ext, xml;
		fs::path src = DataFile( fixtures, e + "/project." + e );
		if ( !ReadFile( src, xml ) ) { std::fprintf( stderr, "FAIL %s: cannot read %s\n", ext, src.string().c_str() ); ++failures; continue; }
		fs::path base = outDir / e;
		if ( RoundTrip( e, xml, base, nullptr, &xml ) ) std::fprintf( stderr, "PASS %s\n", ext );
		else ++failures;

		// Plant an unknown node just before the root's closing tag and check it survives.
		XmlSpike::Document doc;
		std::string err;
		XmlSpike::Parse( xml, doc, err );
		size_t close = xml.rfind( "</" + doc.root.name + ">" );
		std::string planted = xml.substr( 0, close ) + "\t<FutureBlob attr=\"x\">body</FutureBlob>\r\n" + xml.substr( close );
		std::string out1;
		bool ok = close != std::string::npos && RoundTrip( e + " unknown-node", planted, outDir / ( e + "-future" ), &out1, &planted );
		if ( ok )
		{
			XmlSpike::Document d;
			XmlSpike::Parse( out1, d, err );
			const XmlSpike::Node *blob = XmlSpike::FindChild( d.root, "FutureBlob" );
			ok = blob && blob->attrs.size() == 1 && blob->attrs[0].first == "attr" && blob->attrs[0].second == "x"
				&& blob->children.size() == 1 && blob->children[0].text == "body";
			if ( !ok ) std::fprintf( stderr, "FAIL %s unknown-node: FutureBlob lost or altered\n", ext );
		}
		if ( ok ) std::fprintf( stderr, "PASS %s unknown-node preservation\n", ext );
		else ++failures;
	}
	std::fprintf( stderr, failures ? "FAILED: %d check(s)\n" : "ALL PASS (21 extensions x 2 checks)\n", failures );
	return failures ? 1 : 0;
}
