// test-bk-sprite's judge. The build runs the installed bk-sprite twice over
// the fixture description (tools/zig/fixtures/small-tools/sprite) into
// zig-out/local-test/bk-sprite (see addBkSprite); this reads what it wrote
// back with the engine's readers:
//   - the .san through the structure saver, as SSpriteAnimationFormat::Load
//     reads it (no signature chunk, the format in chunk 1): idle in slot
//     ANIMATION_IDLE and death in ANIMATION_DEATH with anims.txt's
//     directions, frame time, speed and cycling, every other slot empty;
//   - the .tga through the engine's TGA reader: each frame's rect maps onto
//     the frame's non-transparent box in the packed picture, pixel for pixel;
//   - each rect's offset is that box's place in its frame minus the shift
//     anims.txt gives the frame (general, direction and frame shifts, the
//     last direction naming a frame deciding, as in spcomp);
//   - the two runs wrote the same bytes (D-04: deterministic output).
// Usage: bk-sprite-test <module root> <fixture dir> <out name> <second out name>
#include "StdAfx.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/Anim/Animation.h"
#include "../../Sources/src/Formats/fmtAnimation.h"
#include "../../Sources/src/Image/Image.h"

namespace fs = std::filesystem;

static int g_nFailures = 0;

static bool Check( bool bCondition, const std::string &szWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", szWhat.c_str() );
		++g_nFailures;
	}
	return bCondition;
}

static std::vector<char> ReadBytes( const fs::path &path )
{
	std::ifstream in( path, std::ios::binary );
	return std::vector<char>( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
}

static CPtr<IImage> LoadTga( const fs::path &path )
{
	CPtr<IDataStream> pStream = OpenFileStream( NToolHost::EnginePath( path ), STREAM_ACCESS_READ );
	if ( !Check( pStream != 0, "the engine opens " + path.string() ) )
		return 0;
	CPtr<IImage> pImage = GetImageProcessor()->LoadImage( pStream );
	Check( pImage != 0, "the engine's TGA reader loads " + path.string() );
	return pImage;
}

// What anims.txt says about one animation.
struct SExpected
{
	const char *pszName;
	int nSlot;
	std::vector<std::vector<short>> dirs;
	int nFrameTime;
	float fSpeed;
	bool bCycled;
	std::vector<CVec2> shifts;          // per frame
};

static void CheckAnimation( const SExpected &expected, const SSpriteAnimationFormat::SSpriteAnimation &anim, IImage *pAtlas, const fs::path &fixture )
{
	const std::string szName = expected.pszName;
	Check( anim.nFrameTime == expected.nFrameTime, szName + ": frame time" );
	Check( anim.fSpeed == expected.fSpeed, szName + ": speed" );
	Check( anim.bCycled == expected.bCycled, szName + ": cycled" );
	if ( !Check( anim.dirs.size() == expected.dirs.size(), szName + ": number of directions" ) )
		return;
	for ( int i = 0; i < anim.dirs.size(); ++i )
		Check( anim.dirs[i].frames == expected.dirs[i], szName + NStr::Format( ": dir%d frames", i ) );
	if ( !Check( anim.rects.size() == expected.shifts.size(), szName + ": one rect per frame" ) )
		return;
	CImageAccessor atlas = pAtlas;
	for ( int j = 0; j < anim.rects.size(); ++j )
	{
		const std::string szFrame = szName + NStr::Format( " frame %d", j );
		CPtr<IImage> pFrame = LoadTga( fixture / szName / NStr::Format( "%.3d.tga", j ) );
		if ( pFrame == 0 )
			continue;
		CImageAccessor frame = pFrame;
		int nMinX = pFrame->GetSizeX(), nMinY = pFrame->GetSizeY(), nMaxX = -1, nMaxY = -1;
		for ( int y = 0; y < pFrame->GetSizeY(); ++y )
			for ( int x = 0; x < pFrame->GetSizeX(); ++x )
				if ( frame[y][x].a != 0 )
				{
					nMinX = Min( nMinX, x ); nMaxX = Max( nMaxX, x );
					nMinY = Min( nMinY, y ); nMaxY = Max( nMaxY, y );
				}
		const SSpriteRect &rect = anim.rects[j];
		// maps hold the packed rect's pixel centres; the rect is inclusive.
		const float fLeft = rect.maps.x1 * pAtlas->GetSizeX() - 0.5f, fRight = rect.maps.x2 * pAtlas->GetSizeX() - 0.5f;
		const float fTop = rect.maps.y1 * pAtlas->GetSizeY() - 0.5f, fBottom = rect.maps.y2 * pAtlas->GetSizeY() - 0.5f;
		const int nLeft = int( floorf( fLeft + 0.5f ) ), nRight = int( floorf( fRight + 0.5f ) );
		const int nTop = int( floorf( fTop + 0.5f ) ), nBottom = int( floorf( fBottom + 0.5f ) );
		Check( fabsf( fLeft - nLeft ) < 1e-3f && fabsf( fRight - nRight ) < 1e-3f && fabsf( fTop - nTop ) < 1e-3f && fabsf( fBottom - nBottom ) < 1e-3f,
		       szFrame + ": maps are pixel centres" );
		if ( !Check( nRight - nLeft == nMaxX - nMinX && nBottom - nTop == nMaxY - nMinY, szFrame + ": the packed rect is the frame's box" ) )
			continue;
		if ( !Check( nLeft >= 0 && nTop >= 0 && nRight < pAtlas->GetSizeX() && nBottom < pAtlas->GetSizeY(), szFrame + ": the packed rect is inside the picture" ) )
			continue;
		bool bSame = true;
		for ( int y = 0; y <= nMaxY - nMinY; ++y )
			for ( int x = 0; x <= nMaxX - nMinX; ++x )
				bSame = bSame && DWORD( atlas[nTop + y][nLeft + x] ) == DWORD( frame[nMinY + y][nMinX + x] );
		Check( bSame, szFrame + ": the packed pixels are the frame's" );
		const CVec2 &shift = expected.shifts[j];
		Check( rect.rect.x1 == short( nMinX - shift.x ) && rect.rect.y1 == short( nMinY - shift.y ) &&
		       rect.rect.x2 == short( nMaxX - shift.x ) && rect.rect.y2 == short( nMaxY - shift.y ),
		       szFrame + NStr::Format( ": offset (%d,%d)-(%d,%d)", int( rect.rect.x1 ), int( rect.rect.y1 ), int( rect.rect.x2 ), int( rect.rect.y2 ) ) );
	}
}

int main( int argc, char *argv[] )
{
	if ( argc != 5 )
	{
		printf( "usage: bk-sprite-test <module root> <fixture dir> <out name> <second out name>\n" );
		return 2;
	}
	if ( !NDataOnly::StartStreamIO( argv[1] ) || !NDataOnly::StartImage( argv[1] ) )
		return 1;
	const fs::path fixture = NToolHost::InputPath( argv[2] );
	const fs::path out = NToolHost::InputPath( argv[3] ), second = NToolHost::InputPath( argv[4] );
	const fs::path tga = fs::path( out ) += ".tga", san = fs::path( out ) += ".san";
	{
		SSpriteAnimationFormat animations;
		bool bRead = false;
		{
			CPtr<IDataStream> pStream = OpenFileStream( NToolHost::EnginePath( san ), STREAM_ACCESS_READ );
			if ( Check( pStream != 0, "the engine opens " + san.string() ) )
			{
				CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				CSaverAccessor saver = pSaver;
				DWORD dwSignature = 0;
				saver.Add( 127, &dwSignature );
				Check( dwSignature == 0, "no signature chunk, as SSpriteAnimationFormat::Load wants" );
				saver.Add( 1, &animations );
				bRead = true;
			}
		}
		CPtr<IImage> pAtlas = LoadTga( tga );
		if ( bRead && pAtlas != 0 && Check( animations.animations.size() == ANIMATION_DEATH + 1, "slots up to ANIMATION_DEATH" ) )
		{
			const CVec2 dir1( 3, 7 ), frame1( 1, 2 );
			const SExpected idle = { "idle", ANIMATION_IDLE, { { 0, 1, 2 }, { 2, 1, 0 } }, 100, 0.0f, true, { dir1, frame1, dir1 } };
			const SExpected death = { "death", ANIMATION_DEATH, { { 0, 1 } }, 150, 1.5f, false, { VNULL2, VNULL2 } };
			for ( int i = 0; i < animations.animations.size(); ++i )
				if ( i != ANIMATION_IDLE && i != ANIMATION_DEATH )
					Check( animations.animations[i].rects.empty() && animations.animations[i].dirs.empty(), NStr::Format( "slot %d is empty", i ) );
			CheckAnimation( idle, animations.animations[ANIMATION_IDLE], pAtlas, fixture );
			CheckAnimation( death, animations.animations[ANIMATION_DEATH], pAtlas, fixture );
		}
	}
	for ( const char *pszExtension : { ".tga", ".san" } )
	{
		const std::vector<char> first = ReadBytes( fs::path( out ) += pszExtension ), again = ReadBytes( fs::path( second ) += pszExtension );
		Check( !first.empty() && first == again, std::string( "two runs write the same " ) + pszExtension );
	}
	NDataOnly::Stop();
	if ( g_nFailures != 0 )
	{
		printf( "bk-sprite-test: %d failures\n", g_nFailures );
		return 1;
	}
	return 0;
}
