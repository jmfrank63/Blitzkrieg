// bk-font: FontGen.exe's job on every platform (Phase 8, D-12). Renders a
// TrueType face with the FontAtlas library and writes 1.tfd through the
// engine's structure saver and 1.tga and 1_h.dds through the engine's image
// processor, so the files are what the game's own readers expect.
//
// The command-line rules are bk_tool.zig's (D-04), kept here by hand because
// this tool is C++ on the engine libraries (D-03): --help anywhere prints the
// usage and exits 0, bad usage prints it on stderr and exits 2, a failure is
// one "bk-font: reason" line on stderr and exit 1, success one line on stdout.
#include "StdAfx.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bk_l10n.h"
#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/FontAtlas/FontAtlas.h"
#include "../../Sources/src/FontAtlas/Shaper.h"
#include "../../Sources/src/Image/Image.h"

namespace fs = std::filesystem;

// The synopsis stays English; the paragraphs under it and the --lang line come from the tables.
static std::string UsageText()
{
	return std::string( "usage: bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]\n"
		"               [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]\n"
		"               [--clusters <table>] [--script <tag>] [--language <tag>]\n"
		"               [--fallback-ttf <file>] [--base <dir>]\n"
		"               [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]\n" )
		+ "\n" + BK_T( "font.usage.intro" ) + "\n"
		+ "\n" + BK_T( "font.usage.options" ) + "\n"
		+ "\n" + BkFormat( "tool.usage.lang", { bk_l10n_valid_codes() } ) + "\n";
}

static int Usage()
{
	fputs( UsageText().c_str(), stderr );
	return 2;
}

static int Fail( const std::string &szReason )
{
	fprintf( stderr, "bk-font: %s\n", szReason.c_str() );
	return 1;
}

static bool ParseInt( const char *pszText, int *pValue )
{
	char *pszEnd = 0;
	const long nValue = strtol( pszText, &pszEnd, 10 );
	if ( pszEnd == pszText || *pszEnd != 0 || nValue < -100000 || nValue > 100000 )
		return false;
	*pValue = int( nValue );
	return true;
}

// FontGen's -<charset> names for the code pages FontAtlas renders.
static bool ParseCharset( const std::string &szName, int *pCodePage )
{
	if ( szName == "ansi" || szName == "default" ) *pCodePage = 1252;
	else if ( szName == "easteurope" ) *pCodePage = 1250;
	else if ( szName == "russian" ) *pCodePage = 1251;
	else if ( !ParseInt( szName.c_str(), pCodePage ) ) return false;
	BYTE cCharSet = 0;
	return FontAtlas::GetCharSet( *pCodePage, &cCharSet );
}

// The ranges behind --set. Each set carries ASCII 32..126 so digits and
// punctuation come from the same face; a code point the face lacks is dropped
// by FontAtlas, so the ranges can be generous.
static bool AddSet( const std::string &szName, std::vector<WORD> *pCodes )
{
	for ( WORD c = 32; c < 127; ++c )
		pCodes->push_back( c );
	if ( szName == "latin1" )
		for ( WORD c = 160; c < 256; ++c ) pCodes->push_back( c );
	else if ( szName == "cyrillic" )
	{
		for ( WORD c = 0x0400; c <= 0x04FF; ++c ) pCodes->push_back( c );
		const WORD punct[] = { 0x00A0, 0x00AB, 0x00BB, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x201E, 0x2026, 0x2116 };
		pCodes->insert( pCodes->end(), punct, punct + sizeof( punct ) / sizeof( punct[0] ) );
	}
	else if ( szName == "devanagari" )
	{
		for ( WORD c = 0x0900; c <= 0x097F; ++c ) pCodes->push_back( c );
		pCodes->push_back( 0x00A0 );
		pCodes->push_back( 0x200C );
		pCodes->push_back( 0x200D );
		pCodes->push_back( 0x25CC );
	}
	else
		return false;
	return true;
}

// Every distinct BMP character of a UTF-8 file, plus ASCII 32..126; line
// breaks, other controls and a byte order mark are not characters of the font.
static void AddCharsOf( const std::vector<BYTE> &data, std::vector<WORD> *pCodes )
{
	for ( WORD c = 32; c < 127; ++c )
		pCodes->push_back( c );
	const std::u16string text = BkUtf8ToUtf16( std::string( data.begin(), data.end() ) );
	for ( size_t i = 0; i < text.size(); ++i )
	{
		const unsigned c = text[i];
		if ( c < 32 || ( c >= 0x7F && c < 0xA0 ) || c == 0xFEFF || ( c >= 0xD800 && c < 0xE000 ) )
			continue;
		pCodes->push_back( WORD( c ) );
	}
}

static bool ReadFile( const fs::path &path, std::vector<BYTE> *pData )
{
	std::ifstream in( path, std::ios::binary );
	if ( !in )
		return false;
	pData->assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
	return true;
}

// The OS font folders a face name is looked up in, best effort.
static std::vector<fs::path> FontFolders()
{
	std::vector<fs::path> folders;
	const char *pszHome = getenv( "HOME" );
#if defined(_WIN32)
	if ( const char *pszWindows = getenv( "WINDIR" ) )
		folders.push_back( fs::path( pszWindows ) / "Fonts" );
	if ( const char *pszLocal = getenv( "LOCALAPPDATA" ) )
		folders.push_back( fs::path( pszLocal ) / "Microsoft" / "Windows" / "Fonts" );
#elif defined(__APPLE__)
	folders.push_back( "/System/Library/Fonts" );
	folders.push_back( "/Library/Fonts" );
	if ( pszHome )
		folders.push_back( fs::path( pszHome ) / "Library" / "Fonts" );
#else
	folders.push_back( "/usr/share/fonts" );
	folders.push_back( "/usr/local/share/fonts" );
	if ( const char *pszData = getenv( "XDG_DATA_HOME" ) )
		folders.push_back( fs::path( pszData ) / "fonts" );
	else if ( pszHome )
		folders.push_back( fs::path( pszHome ) / ".local" / "share" / "fonts" );
	if ( pszHome )
		folders.push_back( fs::path( pszHome ) / ".fonts" );
#endif
	return folders;
}

// The first font file (in sorted path order, so the same machine always
// picks the same file) holding a face of that family name.
static bool FindFaceFile( const std::string &szName, bool bBold, bool bItalic, fs::path *pFile, std::vector<BYTE> *pData, int *pFaceIndex )
{
	std::vector<fs::path> files;
	const std::vector<fs::path> folders = FontFolders();
	for ( size_t i = 0; i < folders.size(); ++i )
	{
		std::error_code ec;
		for ( fs::recursive_directory_iterator it( folders[i], fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
		{
			std::string szExtension = it->path().extension().string();
			NStr::ToLower( szExtension );
			if ( szExtension == ".ttf" || szExtension == ".otf" || szExtension == ".ttc" )
				files.push_back( it->path() );
		}
	}
	std::sort( files.begin(), files.end() );
	for ( size_t i = 0; i < files.size(); ++i )
	{
		std::vector<BYTE> data;
		if ( !ReadFile( files[i], &data ) || data.empty() )
			continue;
		const int nFace = FontAtlas::FindFace( &data[0], int( data.size() ), szName, bBold, bItalic );
		if ( nFace >= 0 )
		{
			*pFile = files[i];
			pData->swap( data );
			*pFaceIndex = nFace;
			return true;
		}
	}
	return false;
}

static bool CreateOutput( const fs::path &file, CPtr<IDataStream> *pStream )
{
	std::string szDir = file.parent_path().generic_string();
	if ( szDir.empty() || szDir[szDir.size() - 1] != '/' )
		szDir += '/';
	CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	*pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
	return *pStream != 0;
}

// Every file through the engine, so they are what its readers expect. An
// empty string on success, the reason otherwise; every engine object made
// here is released before it returns, ahead of NDataOnly::Stop.
// The font of a folder as the game reads it: 1.tfd and the lossless atlas
// 1_h.dds, through the engine's own readers. False with the reason otherwise;
// every engine object made here is released before it returns.
static bool ReadBaseFont( const fs::path &dir, SFontFormat *pFormat, int *pSizeX, int *pSizeY, std::vector<DWORD> *pPixels, std::string *pszError )
{
	const std::string szDir = dir.generic_string() + "/";
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pFormatStream = pStorage != 0 ? pStorage->OpenStream( "1.tfd", STREAM_ACCESS_READ ) : 0;
	if ( pFormatStream == 0 )
	{
		*pszError = "no 1.tfd";
		return false;
	}
	{
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pFormatStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, pFormat );
	}
	CPtr<IDataStream> pAtlasStream = pStorage->OpenStream( "1_h.dds", STREAM_ACCESS_READ );
	CPtr<IDDSImage> pDDS = pAtlasStream != 0 ? GetImageProcessor()->LoadDDSImage( pAtlasStream ) : 0;
	CPtr<IImage> pImage = pDDS != 0 ? GetImageProcessor()->Decompress( pDDS ) : 0;
	if ( pImage == 0 || pFormat->chars.empty() )
	{
		*pszError = pImage == 0 ? "no readable 1_h.dds" : "1.tfd holds no characters";
		return false;
	}
	*pSizeX = pImage->GetSizeX();
	*pSizeY = pImage->GetSizeY();
	const DWORD *pLFB = reinterpret_cast<const DWORD*>( pImage->GetLFB() );
	pPixels->assign( pLFB, pLFB + size_t( *pSizeX ) * *pSizeY );
	return true;
}

static std::string WriteFont( FontAtlas::SFontAtlas &atlas, const fs::path &tfd, const fs::path &tga, const fs::path &dds )
{
	{
		CPtr<IDataStream> pStream;
		if ( !CreateOutput( tfd, &pStream ) )
			return BkFormat( "font.err.cannot_create", { fs::path( tfd ).make_preferred().string() } );
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSaver;
		// The reader (FontGpu::Load, CFont::Load) takes chunk 1 as the format.
		saver.Add( 1, &atlas.format );
	}
	CPtr<IImage> pImage = GetImageProcessor()->CreateImage( atlas.image.nSizeX, atlas.image.nSizeY, &atlas.image.pixels[0] );
	{
		CPtr<IDataStream> pStream;
		if ( pImage == 0 || !CreateOutput( tga, &pStream ) || !GetImageProcessor()->SaveImageAsTGA( pStream, pImage ) )
			return BkFormat( "cxx.err.cannot_write", { fs::path( tga ).make_preferred().string() } );
	}
	// The game's texture manager reads only DDS (TextureGpu probes 1_<quality>,
	// then _h, _c, _l), so FontGen's 1.tga reached it through the resource
	// pipeline. 1_h.dds is that pipeline's lossless 32-bit level, which every
	// quality setting falls back to, written with the exporter's own codec.
	CPtr<IDDSImage> pDDS = GetImageProcessor()->Compress( pImage, GFXPF_ARGB8888 );
	CPtr<IDataStream> pStream;
	if ( pDDS == 0 || !CreateOutput( dds, &pStream ) || !GetImageProcessor()->SaveImageAsDDS( pStream, pDDS ) )
		return BkFormat( "cxx.err.cannot_write", { fs::path( dds ).make_preferred().string() } );
	return std::string();
}

int main( int argc, char *argv[] )
{
	NToolHost::BinaryStdio();
	const int nLanguage = NToolHost::StartLanguage( &argc, &argv, "bk-font", UsageText );
	if ( nLanguage >= 0 )
		return nLanguage;
	for ( int i = 1; i < argc; ++i )
		if ( strcmp( argv[i], "--help" ) == 0 || strcmp( argv[i], "-h" ) == 0 )
		{
			fputs( UsageText().c_str(), stdout );
			return 0;
		}
	std::string szFont, szModules, szOut = ".", szChars, szClusters, szFallback, szBase;
	std::vector<std::string> sets;
	FontAtlas::SFontRequest request;
	bool bForce = false;
	bool bFaceIndex = false;
	for ( int i = 1; i < argc; ++i )
	{
		const std::string szArg = argv[i];
		const bool bHasValue = i + 1 < argc;
		if ( szArg == "--italic" ) request.bItalic = true;
		else if ( szArg == "--no-aa" ) request.bAntialias = false;
		else if ( szArg == "--force" ) bForce = true;
		else if ( !bHasValue ) return Usage();
		else if ( szArg == "--ttf" ) szFont = argv[++i];
		else if ( szArg == "--fallback-ttf" ) szFallback = argv[++i];
		else if ( szArg == "--base" ) szBase = argv[++i];
		else if ( szArg == "--out" ) szOut = argv[++i];
		else if ( szArg == "--modules" ) szModules = argv[++i];
		else if ( szArg == "--height" ) { if ( !ParseInt( argv[++i], &request.nHeight ) ) return Usage(); }
		else if ( szArg == "--weight" ) { if ( !ParseInt( argv[++i], &request.nWeight ) ) return Usage(); }
		else if ( szArg == "--face-index" ) { if ( !ParseInt( argv[++i], &request.nFaceIndex ) || request.nFaceIndex < 0 ) return Usage(); bFaceIndex = true; }
		else if ( szArg == "--chars" ) szChars = argv[++i];
		else if ( szArg == "--set" ) sets.push_back( argv[++i] );
		else if ( szArg == "--clusters" ) szClusters = argv[++i];
		else if ( szArg == "--script" ) request.szScript = argv[++i];
		else if ( szArg == "--language" ) request.szLanguage = argv[++i];
		else if ( szArg == "--charset" ) { if ( !ParseCharset( argv[++i], &request.nCodePage ) ) return Usage(); }
		else return Usage();
	}
	if ( szFont.empty() || szOut.empty() )
		return Usage();

	fs::path font = NToolHost::InputPath( szFont );
	std::vector<BYTE> data;
	std::error_code ec;
	if ( fs::is_regular_file( font, ec ) )
	{
		if ( !ReadFile( font, &data ) || data.empty() )
			return Fail( BkFormat( "font.err.cannot_read", { font.make_preferred().string() } ) );
	}
	else if ( font.has_extension() || szFont.find_first_of( "/\\" ) != std::string::npos )
		return Fail( BkFormat( "font.err.no_file", { font.make_preferred().string() } ) );
	else
	{
		int nFace = 0;
		if ( !FindFaceFile( szFont, request.nWeight >= 600, request.bItalic, &font, &data, &nFace ) )
			return Fail( BkFormat( "font.err.no_name", { szFont } ) );
		if ( !bFaceIndex )
			request.nFaceIndex = nFace;
		request.szFaceName = szFont;
	}
	for ( size_t i = 0; i < sets.size(); ++i )
		if ( !AddSet( sets[i], &request.codePoints ) )
			return Usage();
	if ( !szChars.empty() )
	{
		std::vector<BYTE> text;
		fs::path chars = NToolHost::InputPath( szChars );
		if ( !ReadFile( chars, &text ) )
			return Fail( BkFormat( "font.err.cannot_read", { chars.make_preferred().string() } ) );
		AddCharsOf( text, &request.codePoints );
	}
	if ( !szClusters.empty() )
	{
		std::vector<BYTE> text;
		fs::path table = NToolHost::InputPath( szClusters );
		if ( !ReadFile( table, &text ) )
			return Fail( BkFormat( "font.err.cannot_read", { table.make_preferred().string() } ) );
		CBkClusterTable clusters;
		std::string szTableError;
		if ( !clusters.Load( std::string( text.begin(), text.end() ), &szTableError ) )
			return Fail( BkFormat( "tool.err.path_reason", { table.make_preferred().string(), szTableError } ) );
		request.clusters = clusters.Entries();
		if ( request.codePoints.empty() )
			AddSet( "latin1", &request.codePoints );
	}
	request.pFontData = &data[0];
	request.nFontDataSize = int( data.size() );
	std::vector<BYTE> fallback;
	if ( !szFallback.empty() )
	{
		fs::path fallback_path = NToolHost::InputPath( szFallback );
		if ( !ReadFile( fallback_path, &fallback ) || fallback.empty() )
			return Fail( BkFormat( "font.err.cannot_read", { fallback_path.make_preferred().string() } ) );
		request.pFallbackData = &fallback[0];
		request.nFallbackDataSize = int( fallback.size() );
	}

	// The startup reports its own reason on stderr before this one.
	const std::string szModuleRoot = NToolHost::ModuleRoot( szModules ).string();
	if ( !NDataOnly::StartStreamIO( szModuleRoot.c_str() ) || !NDataOnly::StartImage( szModuleRoot.c_str() ) )
		return Fail( BkFormat( "cxx.err.libs", { szModuleRoot } ) );
	const auto stopAndFail = [&]( const std::string &szReason ) { NDataOnly::Stop(); return Fail( szReason ); };

	// --base: the font of <dir> stays as it is and only the characters it lacks
	// are built, at its height, to be added to it.
	SFontFormat base;
	int nBaseSizeX = 0, nBaseSizeY = 0;
	std::vector<DWORD> basePixels;
	if ( !szBase.empty() )
	{
		const fs::path base_dir = NToolHost::InputPath( szBase );
		std::string szReadError;
		if ( !ReadBaseFont( base_dir, &base, &nBaseSizeX, &nBaseSizeY, &basePixels, &szReadError ) )
			return stopAndFail( BkFormat( "tool.err.path_reason", { fs::path( base_dir ).make_preferred().string(), szReadError } ) );
		request.nHeight = base.metrics.nHeight;
		std::vector<WORD> missing;
		for ( size_t i = 0; i < request.codePoints.size(); ++i )
			if ( base.chars.find( request.codePoints[i] ) == base.chars.end() )
				missing.push_back( request.codePoints[i] );
		request.codePoints.swap( missing );
	}

	FontAtlas::SFontAtlas atlas;
	std::string szError;
	if ( !szBase.empty() && request.codePoints.empty() && request.clusters.empty() )
	{
		atlas.format = base;
		atlas.image.nSizeX = nBaseSizeX;
		atlas.image.nSizeY = nBaseSizeY;
		atlas.image.pixels = basePixels;
	}
	else
	{
		FontAtlas::SFontAtlas built;
		if ( !FontAtlas::Build( request, &built, &szError ) )
			return stopAndFail( BkFormat( "tool.err.path_reason", { font.make_preferred().string(), szError } ) );
		if ( szBase.empty() )
			atlas = built;
		else if ( !FontAtlas::AddToBase( base, nBaseSizeX, nBaseSizeY, basePixels, built, &atlas, &szError ) )
			return stopAndFail( BkFormat( "tool.err.path_reason", { NToolHost::InputPath( szBase ).make_preferred().string(), szError } ) );
	}

	const fs::path out = NToolHost::InputPath( szOut );
	const fs::path tfd = out / "1.tfd";
	const fs::path tga = out / "1.tga";
	const fs::path dds = out / "1_h.dds";
	if ( !bForce && ( fs::exists( tfd, ec ) || fs::exists( tga, ec ) || fs::exists( dds, ec ) ) )
		return stopAndFail( BkFormat( "font.err.exists", { fs::path( out ).make_preferred().string() } ) );
	fs::create_directories( out, ec );
	if ( ec )
		return stopAndFail( BkFormat( "cxx.err.create", { fs::path( out ).make_preferred().string(), ec.message() } ) );

	const std::string szWriteError = WriteFont( atlas, tfd, tga, dds );
	NDataOnly::Stop();
	if ( !szWriteError.empty() )
		return Fail( szWriteError );
	std::string szClusterNote;
	if ( !szClusters.empty() )
		szClusterNote = NStr::Format( ", clusters %d of %d", atlas.nClusters, CBkClusterTable::LAST - CBkClusterTable::FIRST + 1 );
	if ( !szFallback.empty() )
		szClusterNote += NStr::Format( ", %d from the fallback", atlas.nFallback );
	printf( "wrote %s, 1.tga and 1_h.dds: %d characters, %d kerning pairs%s, %dx%d atlas\n",
		fs::path( tfd ).make_preferred().string().c_str(), int( atlas.format.chars.size() ), int( atlas.format.kerns.size() ),
		szClusterNote.c_str(), atlas.image.nSizeX, atlas.image.nSizeY );
	return 0;
}
