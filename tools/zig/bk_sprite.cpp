// bk-sprite: spcomp's sprite composer on every platform (Phase 8, D-20). Reads
// spcomp's sprite-sequence description (anims.txt and the rendered frames
// beside it), packs every frame into one picture with the engine's
// ComposeImages and writes <out>.tga through the engine's TGA writer and
// <out>.san (SSpriteAnimationFormat) through the structure saver, so the
// files are what the game's own readers expect (D-03).
//
// spcomp kept its animations in a map by name; the format the game reads now
// is a vector by animation slot, so each animation goes to the slot the
// Resource Editor's exporter gives its name (NCompose::GetActionFromName).
//
// The command-line rules are bk_tool.zig's (D-04), kept by hand as in
// bk_font.cpp: --help anywhere prints the usage and exits 0, bad usage prints
// it on stderr and exits 2, a failure is one "bk-sprite: reason" line on
// stderr and exit 1, success one line on stdout.
#include "StdAfx.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/Anim/Animation.h"
#include "../../Sources/src/Formats/fmtAnimation.h"
#include "../../Sources/src/Image/Image.h"
#include "../../Sources/src/ResourceModel/anim_names.h"

namespace fs = std::filesystem;

static const char USAGE[] =
	"usage: bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]\n"
	"\n"
	"compose     packs the frames a sprite-sequence description names into\n"
	"            <out-name>.tga and describes the animations in <out-name>.san,\n"
	"            as spcomp did\n"
	"\n"
	"<desc-dir> holds anims.txt, one [section] per animation:\n"
	"  dirs=<n>                 number of directions (required)\n"
	"  dir<k> seq=<i>,<j>,...   frame indices of direction k (required)\n"
	"  frametime=<ms>           one frame's show time (default 125)\n"
	"  speed=<f>                translation speed (default 0)\n"
	"  cycled=<0|1>             cycled or one-shot (default 1)\n"
	"  shift=<x>,<y>            every frame's shift (default 0,0)\n"
	"  dir<k> shift=<x>,<y>     direction k's frames' shift (default: shift)\n"
	"  frame<i> shift=<x>,<y>   frame i's shift (default: its direction's)\n"
	"Frame i of animation <name> is <desc-dir>/<name>/<iii>.tga; an animation\n"
	"named default has the one frame <desc-dir>/1.tga. The section names are\n"
	"the animation slots (idle, run, death, ...; default and effect are idle).\n"
	"\n"
	"--force     replace <out-name>.tga and .san when they are already there\n"
	"--modules   where the engine's StreamIO and Image libraries are (default:\n"
	"            beside bk-sprite, then ../lib, then ..)\n";

static int Usage()
{
	fputs( USAGE, stderr );
	return 2;
}

static int Fail( const std::string &szReason )
{
	fprintf( stderr, "bk-sprite: %s\n", szReason.c_str() );
	return 1;
}

// A path as the tool prints it: the host's separators.
static std::string Shown( fs::path path )
{
	return path.make_preferred().string();
}

// spcomp's SDirDesc and SAnimationDesc, with the shifts zeroed (spcomp left
// them unset when anims.txt gave none).
struct SDirDesc
{
	std::vector<short> frames;            // frame indices for this direction sequence
	CVec2 ptFrameShift = VNULL2;          // shift for all frames, which participates in this dir sequence
};
struct SAnimationDesc
{
	std::string szName;                   // animation name
	int nFirstSprite = 0;
	int nLastSprite = 0;
	std::vector<SDirDesc> dirs;           // direction descriptions
	std::unordered_map<int, CVec2> frames;  // each frame unique shift
	int nFrameTime = 0;                   // general one frame show time
	CVec2 ptFrameShift = VNULL2;          // general one frame shift
	float fSpeed = 0.0f;                  // translation speed (for animations with movement)
	bool bCycled = true;                  // cycled or one-shot animation
	int nSlot = 0;                        // where it goes in SSpriteAnimationFormat::animations
};

// anims.txt the way spcomp's IDataBase table read it (CIniFile::LoadTables
// and CTableAccessor): lines trimmed, blank lines and ';' comments skipped,
// [row] starting a row, name=value trimmed, an entry with an empty name or
// value dropped, a row named again adding to the first, a name given again
// taking the last value; numbers as atoi and atof read them, arrays split on
// ','. A row exists from its first entry on, so an empty [row] is no
// animation. The engine's StreamIO port has no ini table to ask
// (OpenIniDataTable answers nothing there), so the text is read here.
struct SIniTable
{
	std::vector<std::string> rows;      // in the order they first appear
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> entries;

	const std::string *Find( const char *pszRow, const char *pszEntry ) const
	{
		const auto row = entries.find( pszRow );
		if ( row == entries.end() )
			return 0;
		const auto entry = row->second.find( pszEntry );
		return entry == row->second.end() ? 0 : &entry->second;
	}
	int GetInt( const char *pszRow, const char *pszEntry, int nDefault ) const
	{
		const std::string *pValue = Find( pszRow, pszEntry );
		return pValue == 0 ? nDefault : atoi( pValue->c_str() );
	}
	float GetFloat( const char *pszRow, const char *pszEntry, float fDefault ) const
	{
		const std::string *pValue = Find( pszRow, pszEntry );
		return pValue == 0 ? fDefault : float( atof( pValue->c_str() ) );
	}
	template <class TYPE>
	void GetArray( const char *pszRow, const char *pszEntry, std::vector<TYPE> &values ) const
	{
		values.clear();
		const std::string *pValue = Find( pszRow, pszEntry );
		if ( pValue == 0 )
			return;
		std::vector<std::string> szItems;
		NStr::SplitStringWithMultipleBrackets( *pValue, szItems, ',' );
		for ( int i = 0; i < szItems.size(); ++i )
			values.push_back( std::is_integral<TYPE>::value ? TYPE( atoi( szItems[i].c_str() ) ) : TYPE( atof( szItems[i].c_str() ) ) );
	}
};

static std::string ReadIni( const fs::path &file, SIniTable *pTable )
{
	std::ifstream in( file, std::ios::binary );
	if ( !in )
		return "cannot read " + Shown( file );
	const std::string szText( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
	std::string szRow;
	int nLine = 0;
	for ( size_t nAt = 0; nAt <= szText.size(); )
	{
		size_t nEnd = szText.find( '\n', nAt );
		if ( nEnd == std::string::npos )
			nEnd = szText.size();
		std::string szLine = szText.substr( nAt, nEnd - nAt );
		nAt = nEnd + 1;
		++nLine;
		NStr::TrimBoth( szLine );
		if ( szLine.empty() || szLine[0] == ';' )
			continue;
		if ( szLine[0] == '[' && szLine[szLine.size() - 1] == ']' )
		{
			NStr::TrimLeft( szLine, '[' );
			NStr::TrimRight( szLine, ']' );
			szRow = szLine;
			continue;
		}
		const size_t nEquals = szLine.find( '=' );
		if ( nEquals == std::string::npos )
			return NStr::Format( "%s, line %d: no name=value", Shown( file ).c_str(), nLine );
		std::string szName = szLine.substr( 0, nEquals ), szValue = szLine.substr( nEquals + 1 );
		NStr::TrimBoth( szName );
		NStr::TrimBoth( szValue );
		if ( szRow.empty() || szName.empty() || szValue.empty() )
			continue;
		if ( pTable->entries.count( szRow ) == 0 )
			pTable->rows.push_back( szRow );
		pTable->entries[szRow][szName] = szValue;
	}
	return std::string();
}

// spcomp's reading of anims.txt, its asserts as failures. A frame index
// outside 0..frames-1 is refused too: spcomp wrote a .san that indexed past
// its rects.
static std::string ReadDescription( const fs::path &descDir, std::vector<SAnimationDesc> *pAnims )
{
	const fs::path table = descDir / "anims.txt";
	std::error_code ec;
	if ( !fs::is_regular_file( table, ec ) )
		return "no anims.txt in " + Shown( descDir );
	SIniTable reg;
	const std::string szError = ReadIni( table, &reg );
	if ( !szError.empty() )
		return szError;
	const std::vector<std::string> &szSections = reg.rows;
	if ( szSections.empty() )
		return Shown( table ) + " describes no animation";
	pAnims->resize( szSections.size() );
	std::unordered_map<int, std::string> slots;
	std::vector<float> fvals;
	for ( int i = 0; i < szSections.size(); ++i )
	{
		SAnimationDesc &anim = ( *pAnims )[i];
		const char *pszRow = szSections[i].c_str();
		anim.szName = szSections[i];
		bool bKnown = false;
		anim.nSlot = NResourceModel::NCompose::GetActionFromName( anim.szName, &bKnown );
		if ( !bKnown )
			return "animation \"" + anim.szName + "\" is not an animation slot of the sprite format";
		if ( slots.count( anim.nSlot ) != 0 )
			return "animations \"" + slots[anim.nSlot] + "\" and \"" + anim.szName + "\" go to the same animation slot";
		slots[anim.nSlot] = anim.szName;
		anim.nFrameTime = reg.GetInt( pszRow, "frametime", 125 );
		anim.fSpeed = reg.GetFloat( pszRow, "speed", 0 );
		anim.bCycled = reg.GetInt( pszRow, "cycled", true );
		const int nNumDirs = reg.GetInt( pszRow, "dirs", 0 );
		if ( nNumDirs <= 0 )
			return NStr::Format( "number of dirs must be >0 (in animation \"%s\")", pszRow );
		anim.dirs.resize( nNumDirs );
		reg.GetArray( pszRow, "shift", fvals );
		if ( fvals.size() == 2 )
			anim.ptFrameShift.Set( fvals[0], fvals[1] );
		else if ( fvals.size() != 0 )
			return NStr::Format( "wrong number of components in the general frame shift for animation \"%s\"", pszRow );
		for ( int j = 0; j < anim.dirs.size(); ++j )
		{
			SDirDesc &dir = anim.dirs[j];
			reg.GetArray( pszRow, NStr::Format( "dir%d seq", j ), dir.frames );
			if ( dir.frames.empty() )
				return NStr::Format( "number of frames must be >0 (for dir %d of animation \"%s\")", j, pszRow );
			reg.GetArray( pszRow, NStr::Format( "dir%d shift", j ), fvals );
			if ( fvals.size() == 2 )
				dir.ptFrameShift.Set( fvals[0], fvals[1] );
			else
				dir.ptFrameShift = anim.ptFrameShift;
			for ( int k = 0; k < dir.frames.size(); ++k )
			{
				reg.GetArray( pszRow, NStr::Format( "frame%d shift", dir.frames[k] ), fvals );
				if ( fvals.size() == 2 )
					anim.frames[dir.frames[k]].Set( fvals[0], fvals[1] );
				else
					anim.frames[dir.frames[k]] = dir.ptFrameShift;
			}
		}
		for ( int j = 0; j < anim.dirs.size(); ++j )
			for ( int k = 0; k < anim.dirs[j].frames.size(); ++k )
				if ( anim.dirs[j].frames[k] < 0 || anim.dirs[j].frames[k] >= int( anim.frames.size() ) )
					return NStr::Format( "frame %d of dir %d of animation \"%s\" is not one of its frames 0..%d", anim.dirs[j].frames[k], j, pszRow, int( anim.frames.size() ) - 1 );
	}
	return std::string();
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

// spcomp's main after the description: every frame loaded in animation order,
// packed by ComposeImages, the rects mapped into the packed picture and
// shifted, then both files written. An empty string on success, the reason
// otherwise; every engine object made here is released before it returns,
// ahead of NDataOnly::Stop.
static std::string Compose( const fs::path &descDir, std::vector<SAnimationDesc> &animdescs, const fs::path &tga, const fs::path &san, std::string *pSummary )
{
	IImageProcessor *pIP = GetImageProcessor();
	std::vector<CPtr<IImage>> holders;
	std::vector<IImage *> images;
	int nFirstFrame = 0, nLastFrame = 0, nMaxSlot = 0;
	for ( int i = 0; i < animdescs.size(); ++i )
		nMaxSlot = Max( nMaxSlot, animdescs[i].nSlot );
	SSpriteAnimationFormat animations;
	animations.animations.resize( nMaxSlot + 1 );
	for ( int i = 0; i < animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[animdesc.nSlot] );
		pAnimation->fSpeed = animdesc.fSpeed;
		pAnimation->bCycled = animdesc.bCycled;
		pAnimation->dirs.resize( animdesc.dirs.size() );
		pAnimation->nFrameTime = animdesc.nFrameTime;
		for ( int j = 0; j < pAnimation->dirs.size(); ++j )
			pAnimation->dirs[j].frames = animdesc.dirs[j].frames;
		pAnimation->rects.resize( animdesc.frames.size() );
		for ( int j = 0; j < pAnimation->rects.size(); ++j )
		{
			const fs::path frame = animdesc.szName == "default" ? descDir / "1.tga" : descDir / animdesc.szName / NStr::Format( "%.3d.tga", j );
			std::error_code ec;
			CPtr<IDataStream> pStream = fs::is_regular_file( frame, ec ) ? OpenFileStream( NToolHost::EnginePath( frame ), STREAM_ACCESS_READ ) : 0;
			if ( pStream == 0 )
				return "cannot open image " + Shown( frame );
			CPtr<IImage> pImage = pIP->LoadImage( pStream );
			if ( pImage == 0 )
				return "cannot load an image from " + Shown( frame );
			holders.push_back( pImage );
			images.push_back( pImage );
			++nLastFrame;
		}
		animdesc.nFirstSprite = nFirstFrame;
		animdesc.nLastSprite = nLastFrame;
		nFirstFrame = nLastFrame;
	}
	std::vector<RECT> rects( images.size() );
	std::vector<RECT> rectsMain( images.size() );
	CPtr<IImage> pImage = pIP->ComposeImages( &( images[0] ), &( rects[0] ), &( rectsMain[0] ), images.size() );
	if ( pImage == 0 )
		return "composing the frames failed";
	{
		CPtr<IDataStream> pStream;
		if ( !CreateOutput( tga, &pStream ) || !pIP->SaveImageAsTGA( pStream, pImage ) )
			return "cannot write " + Shown( tga );
	}
	for ( int i = 0; i < animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[animdesc.nSlot] );
		for ( int j = 0; j < pAnimation->rects.size(); ++j )
		{
			const RECT &rcSubRect = rects[animdesc.nFirstSprite + j];
			pAnimation->rects[j].maps.x1 = ( float( rcSubRect.left ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.x2 = ( float( rcSubRect.right ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.y1 = ( float( rcSubRect.top ) + 0.5f ) / float( pImage->GetSizeY() );
			pAnimation->rects[j].maps.y2 = ( float( rcSubRect.bottom ) + 0.5f ) / float( pImage->GetSizeY() );
			const RECT &rcBase = rectsMain[animdesc.nFirstSprite + j];
			pAnimation->rects[j].rect.Set( rcSubRect.left - rcBase.left - animdesc.frames[j].x,
			                               rcSubRect.top - rcBase.top - animdesc.frames[j].y,
			                               rcSubRect.right - rcBase.left - animdesc.frames[j].x,
			                               rcSubRect.bottom - rcBase.top - animdesc.frames[j].y );
		}
	}
	{
		CPtr<IDataStream> pStream;
		if ( !CreateOutput( san, &pStream ) )
			return "cannot write " + Shown( san );
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSaver;
		// The reader (SSpriteAnimationFormat::Load) takes chunk 1 as the format.
		saver.Add( 1, &animations );
	}
	*pSummary = NStr::Format( "%d animations, %d frames, %dx%d atlas", int( animdescs.size() ), int( images.size() ), pImage->GetSizeX(), pImage->GetSizeY() );
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
	if ( argc < 2 || strcmp( argv[1], "compose" ) != 0 )
		return Usage();
	std::vector<std::string> positional;
	std::string szModules;
	bool bForce = false;
	for ( int i = 2; i < argc; ++i )
	{
		const std::string szArg = argv[i];
		if ( szArg == "--force" ) bForce = true;
		else if ( szArg.compare( 0, 2, "--" ) != 0 ) positional.push_back( szArg );
		else if ( szArg == "--modules" && i + 1 < argc ) szModules = argv[++i];
		else return Usage();
	}
	if ( positional.size() != 2 || positional[1].empty() )
		return Usage();

	const fs::path descDir = NToolHost::InputPath( positional[0] );
	const fs::path out = NToolHost::InputPath( positional[1] );
	const fs::path tga = fs::path( out ) += ".tga";
	const fs::path san = fs::path( out ) += ".san";
	std::error_code ec;
	if ( !fs::is_directory( descDir, ec ) )
		return Fail( "no description directory " + Shown( descDir ) );
	if ( !bForce && ( fs::exists( tga, ec ) || fs::exists( san, ec ) ) )
		return Fail( Shown( fs::exists( tga, ec ) ? tga : san ) + " is already there; use --force to replace it" );
	if ( out.has_parent_path() )
	{
		fs::create_directories( out.parent_path(), ec );
		if ( ec )
			return Fail( "cannot create " + Shown( out.parent_path() ) + ": " + ec.message() );
	}

	// The startup reports its own reason on stderr before this one.
	const std::string szModuleRoot = NToolHost::ModuleRoot( szModules ).string();
	if ( !NDataOnly::StartStreamIO( szModuleRoot.c_str() ) || !NDataOnly::StartImage( szModuleRoot.c_str() ) )
		return Fail( "the engine libraries did not load from " + szModuleRoot + " (see --modules)" );
	std::string szSummary, szError;
	try
	{
		std::vector<SAnimationDesc> anims;
		szError = ReadDescription( descDir, &anims );
		if ( szError.empty() )
			szError = Compose( descDir, anims, tga, san, &szSummary );
	}
	catch ( ... )
	{
		szError = "the engine threw while reading or writing";
	}
	NDataOnly::Stop();
	if ( !szError.empty() )
		return Fail( szError );
	printf( "wrote %s and %s: %s\n", Shown( tga ).c_str(), Shown( san ).c_str(), szSummary.c_str() );
	return 0;
}
