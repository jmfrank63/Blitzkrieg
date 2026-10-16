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

#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/FontAtlas/FontAtlas.h"
#include "../../Sources/src/Image/Image.h"

namespace fs = std::filesystem;

static const char USAGE[] =
	"usage: bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]\n"
	"               [--no-aa] [--charset <name>] [--face-index <n>] [--out <dir>] [--force]\n"
	"               [--modules <dir>]\n"
	"\n"
	"Renders characters 32..255 of the charset's code page into <dir>/1.tga and\n"
	"describes them (metrics, texture rects, A/B/C spacing, kerning) in\n"
	"<dir>/1.tfd, as FontGen did, plus <dir>/1_h.dds, the texture the game\n"
	"reads: a font directory the game loads like Data/Fonts/small.\n"
	"\n"
	"--ttf        a .ttf/.otf/.ttc file, or a face name looked up in the system\n"
	"             font folders (best effort; a file is the reliable way)\n"
	"--height     cell height in pixels, ascent + descent (default 20)\n"
	"--weight     400 regular (default); 600 or more picks the bold face of a\n"
	"             name, or emboldens a face that is not bold\n"
	"--italic     picks the italic face of a name, or slants one that is not\n"
	"--no-aa      every pixel fully on or off\n"
	"--charset    ansi or default (code page 1252, default), easteurope (1250),\n"
	"             russian (1251), or the code page number\n"
	"--face-index the face inside a .ttc collection (default 0)\n"
	"--out        output directory, created if missing (default .)\n"
	"--force      replace an existing font in <dir>\n"
	"--modules    where the engine's StreamIO and Image libraries are (default:\n"
	"             beside bk-font, then ../lib)\n";

static int Usage()
{
	fputs( USAGE, stderr );
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
static std::string WriteFont( FontAtlas::SFontAtlas &atlas, const fs::path &tfd, const fs::path &tga, const fs::path &dds )
{
	{
		CPtr<IDataStream> pStream;
		if ( !CreateOutput( tfd, &pStream ) )
			return "cannot create " + fs::path( tfd ).make_preferred().string();
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSaver;
		// The reader (FontGpu::Load, CFont::Load) takes chunk 1 as the format.
		saver.Add( 1, &atlas.format );
	}
	CPtr<IImage> pImage = GetImageProcessor()->CreateImage( atlas.image.nSizeX, atlas.image.nSizeY, &atlas.image.pixels[0] );
	{
		CPtr<IDataStream> pStream;
		if ( pImage == 0 || !CreateOutput( tga, &pStream ) || !GetImageProcessor()->SaveImageAsTGA( pStream, pImage ) )
			return "cannot write " + fs::path( tga ).make_preferred().string();
	}
	// The game's texture manager reads only DDS (TextureGpu probes 1_<quality>,
	// then _h, _c, _l), so FontGen's 1.tga reached it through the resource
	// pipeline. 1_h.dds is that pipeline's lossless 32-bit level, which every
	// quality setting falls back to, written with the exporter's own codec.
	CPtr<IDDSImage> pDDS = GetImageProcessor()->Compress( pImage, GFXPF_ARGB8888 );
	CPtr<IDataStream> pStream;
	if ( pDDS == 0 || !CreateOutput( dds, &pStream ) || !GetImageProcessor()->SaveImageAsDDS( pStream, pDDS ) )
		return "cannot write " + fs::path( dds ).make_preferred().string();
	return std::string();
}

int main( int argc, char *argv[] )
{
	for ( int i = 1; i < argc; ++i )
		if ( strcmp( argv[i], "--help" ) == 0 || strcmp( argv[i], "-h" ) == 0 )
		{
			fputs( USAGE, stdout );
			return 0;
		}
	std::string szFont, szModules, szOut = ".";
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
		else if ( szArg == "--out" ) szOut = argv[++i];
		else if ( szArg == "--modules" ) szModules = argv[++i];
		else if ( szArg == "--height" ) { if ( !ParseInt( argv[++i], &request.nHeight ) ) return Usage(); }
		else if ( szArg == "--weight" ) { if ( !ParseInt( argv[++i], &request.nWeight ) ) return Usage(); }
		else if ( szArg == "--face-index" ) { if ( !ParseInt( argv[++i], &request.nFaceIndex ) || request.nFaceIndex < 0 ) return Usage(); bFaceIndex = true; }
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
			return Fail( "cannot read " + font.make_preferred().string() );
	}
	else if ( font.has_extension() || szFont.find_first_of( "/\\" ) != std::string::npos )
		return Fail( "no font file " + font.make_preferred().string() );
	else
	{
		int nFace = 0;
		if ( !FindFaceFile( szFont, request.nWeight >= 600, request.bItalic, &font, &data, &nFace ) )
			return Fail( "no font named \"" + szFont + "\" in the system font folders; pass the file with --ttf" );
		if ( !bFaceIndex )
			request.nFaceIndex = nFace;
		request.szFaceName = szFont;
	}
	request.pFontData = &data[0];
	request.nFontDataSize = int( data.size() );

	FontAtlas::SFontAtlas atlas;
	std::string szError;
	if ( !FontAtlas::Build( request, &atlas, &szError ) )
		return Fail( font.make_preferred().string() + ": " + szError );

	const fs::path out = NToolHost::InputPath( szOut );
	const fs::path tfd = out / "1.tfd";
	const fs::path tga = out / "1.tga";
	const fs::path dds = out / "1_h.dds";
	if ( !bForce && ( fs::exists( tfd, ec ) || fs::exists( tga, ec ) || fs::exists( dds, ec ) ) )
		return Fail( fs::path( out ).make_preferred().string() + " already holds a font; use --force to replace it" );
	fs::create_directories( out, ec );
	if ( ec )
		return Fail( "cannot create " + fs::path( out ).make_preferred().string() + ": " + ec.message() );

	// The startup reports its own reason on stderr before this one.
	const std::string szModuleRoot = NToolHost::ModuleRoot( szModules ).string();
	if ( !NDataOnly::StartStreamIO( szModuleRoot.c_str() ) || !NDataOnly::StartImage( szModuleRoot.c_str() ) )
		return Fail( "the engine libraries did not load from " + szModuleRoot + " (see --modules)" );
	const std::string szWriteError = WriteFont( atlas, tfd, tga, dds );
	NDataOnly::Stop();
	if ( !szWriteError.empty() )
		return Fail( szWriteError );
	printf( "wrote %s, 1.tga and 1_h.dds: %d characters, %d kerning pairs, %dx%d atlas\n",
		fs::path( tfd ).make_preferred().string().c_str(), int( atlas.format.chars.size() ), int( atlas.format.kerns.size() ),
		atlas.image.nSizeX, atlas.image.nSizeY );
	return 0;
}
