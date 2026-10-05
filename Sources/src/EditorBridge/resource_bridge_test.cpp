// test-resource-bridge: Project+Tree C ABI tier.
//
// T01 scaffolded this file as a smoke that stops after BkResNew(wpn) +
// BkResClose. T02 extends it: open each of the 21 fixtures through
// BkResOpen, save it back through BkResSave (safe-save read-back), and
// byte-compare the saved file against the fixture. For a representative
// subset (wpn, msh, pcp - stats-only, keyframe, image fronts) it also
// exercises delete -> restore -> save and asserts byte-identity.
//
// On a host without a GPU the start reports BK_EDITOR_NO_DEVICE and the
// test exits 0 with "skipped: no GPU device", mirroring editor_bridge_test.cpp.
// CI runners that have a device set BK_REQUIRE_ENGINE=1; a skip is then a
// failure, so a regression on those runners cannot hide as a skip.
//
// argv:
//   [0] self
//   [1] staged install root (contains Data/consts.xml); defaults to the
//       executable's own directory, like the editor_bridge test.
//   [2] fixture source root: tools/zig/fixtures/resource_editor
//   [3] scratch output root: zig-out/local-test/resource_editor/t02
#include "StdAfx.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "resource_bridge.h"
#include "bridge_session.h"
#include "../ResourceModel/references.h"
#include "../ResourceModel/exporter.h"
#include "../zlib/zlib.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

static int g_nFailures = 0;

static bool Check( bool bCondition, const char *pszWhat )
{
	if ( !bCondition )
	{
		std::printf( "FAIL: %s\n", pszWhat );
		++g_nFailures;
	}
	return bCondition;
}

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

static int SkipOrFail( const std::string &szWhy )
{
	const char *pszRequire = std::getenv( "BK_REQUIRE_ENGINE" );
	if ( pszRequire != 0 && *pszRequire != 0 && std::strcmp( pszRequire, "0" ) != 0 )
	{
		std::printf( "FAIL: resource-bridge: %s, and BK_REQUIRE_ENGINE is set\n", szWhy.c_str() );
		return 1;
	}
	std::printf( "resource-bridge: skipped: %s\n", szWhy.c_str() );
	return 0;
}

static bool ReadBytes( const std::string &szPath, std::string &out )
{
	std::ifstream f( szPath, std::ios::binary );
	if ( !f ) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

// The 21 fixture extensions, in EXTENSIONS.md / kind-table order. The index
// here must match the BkResKind ordinal: a mismatch between the test's table
// and the bridge's would silently align with the wrong root.
struct Fixture { const char *pszExt; int nKindOrdinal; };
static const Fixture kFixtures[] = {
	{ "wpn", 0  }, { "mcp", 1  }, { "trc", 2  }, { "scp", 3  },
	{ "spt", 4  }, { "unt", 5  }, { "msh", 6  }, { "obt", 7  },
	{ "fnc", 8  }, { "bld", 9  }, { "bdg", 10 }, { "pcp", 11 },
	{ "eff", 12 }, { "til", 13 }, { "3rd", 14 }, { "3rv", 15 },
	{ "mip", 16 }, { "chc", 17 }, { "cgc", 18 }, { "mdc", 19 },
	{ "gui", 20 },
};
static const int kFixtureCount = int( sizeof(kFixtures) / sizeof(kFixtures[0]) );

static bool RoundTripOne( BkResSession *pSession, const std::string &szFixtureRoot,
                          const std::string &szScratchRoot, const Fixture &fx )
{
	const std::string szIn = szFixtureRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
	const std::string szOutDir = szScratchRoot + "/" + fx.pszExt;
	const std::string szOut = szOutDir + "/project." + fx.pszExt;
	std::error_code ec;
	std::filesystem::create_directories( szOutDir, ec );
	// Make sure stale state from a prior run cannot mask a regression.
	std::filesystem::remove( szOut, ec );
	std::filesystem::remove( szOut + ".bak", ec );
	std::filesystem::remove( szOut + ".tmp", ec );

	std::string szWhat;
	bool ok = true;

	szWhat = std::string( fx.pszExt ) + ": BkResOpen";
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return false;
	}

	BkResKind kind = -2;
	szWhat = std::string( fx.pszExt ) + ": BkResKindOf";
	ok = Check( BkResKindOf( pSession, &kind ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	szWhat = std::string( fx.pszExt ) + ": kind ordinal matches";
	ok = Check( kind == fx.nKindOrdinal, szWhat.c_str() ) && ok;

	// Count nodes so the two-pass contract exercises both branches.
	int nCount = -1;
	szWhat = std::string( fx.pszExt ) + ": BkResNodes count (null buffer)";
	ok = Check( BkResNodes( pSession, 0, 0, &nCount ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	szWhat = std::string( fx.pszExt ) + ": at least a root node";
	ok = Check( nCount >= 1, szWhat.c_str() ) && ok;
	std::vector<BkResNodeRecord> nodes( nCount );
	szWhat = std::string( fx.pszExt ) + ": BkResNodes fill";
	ok = Check( BkResNodes( pSession, nodes.data(), nCount, &nCount ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;

	szWhat = std::string( fx.pszExt ) + ": BkResSave";
	if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		return false;
	}

	std::string szBefore, szAfter;
	if ( !Check( ReadBytes( szIn, szBefore ), "fixture readable" ) ) { BkResClose( pSession ); return false; }
	if ( !Check( ReadBytes( szOut, szAfter ), "saved file readable" ) ) { BkResClose( pSession ); return false; }
	szWhat = std::string( fx.pszExt ) + ": byte-identical round-trip";
	if ( !Check( szBefore == szAfter, szWhat.c_str() ) )
	{
		std::printf( "   in=%zu bytes, out=%zu bytes\n", szBefore.size(), szAfter.size() );
		ok = false;
	}

	// A re-open of the saved copy must round-trip too - "the game reads it unchanged"
	// invariant extended to the editor's own reader.
	szWhat = std::string( fx.pszExt ) + ": re-open saved copy";
	ok = Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	BkResClose( pSession );
	return ok;
}

// D014 item 4: BkResSave onto a destination that already exists. The safe-save
// renames <path>.tmp over it, so this proves std::filesystem::rename replaces
// an existing file (POSIX here; MSVC uses MoveFileEx with replace on Windows,
// which CI must confirm). The old bytes must land in <path>.bak, no .tmp stays,
// and saving onto the project's own open path must work the same way.
static void SaveOverExisting( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const std::string szDir = szScratchRoot + "/save_over_existing";
	const std::string szIn = szFixtureRoot + "/wpn/project.wpn";
	const std::string szOut = szDir + "/project.wpn";
	fs::remove_all( szDir, ec );
	fs::create_directories( szDir, ec );
	std::printf( "save-over-existing: start dir=%s\n", szDir.c_str() );

	std::string szExpected;
	if ( !Check( ReadBytes( szIn, szExpected ), "save-over-existing: fixture readable" ) )
		return;
	const std::string szStale = "stale bytes that are not a project\n";
	{
		std::ofstream f( szOut, std::ios::binary | std::ios::trunc );
		f << szStale;
	}

	// 1. A different file sits at the destination.
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "save-over-existing: opens the fixture" ) )
		return;
	Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "save-over-existing: BkResSave over an existing file answers OK" );
	std::string szGot, szBak;
	Check( ReadBytes( szOut, szGot ) && szGot == szExpected, "save-over-existing: the destination holds the new bytes" );
	Check( ReadBytes( szOut + ".bak", szBak ) && szBak == szStale, "save-over-existing: .bak holds the replaced bytes" );
	Check( !fs::exists( szOut + ".tmp", ec ), "save-over-existing: no .tmp is left" );

	// 2. Onto the project's own open path: the session now points at szOut, so
	// the second save replaces the file just written; .bak becomes the previous save.
	Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "save-over-existing: BkResSave onto the open path answers OK" );
	szGot.clear();
	szBak.clear();
	Check( ReadBytes( szOut, szGot ) && szGot == szExpected, "save-over-existing: the own-path save keeps the bytes" );
	Check( ReadBytes( szOut + ".bak", szBak ) && szBak == szExpected, "save-over-existing: .bak holds the previous save, replaced not appended" );
	Check( !fs::exists( szOut + ".tmp", ec ), "save-over-existing: no .tmp is left after the own-path save" );
	Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "save-over-existing: the replaced file re-opens" );
	BkResClose( pSession );
	std::printf( "save-over-existing: done\n" );
}

static bool DeleteRestoreOne( BkResSession *pSession, const std::string &szFixtureRoot,
                              const std::string &szScratchRoot, const Fixture &fx )
{
	const std::string szIn = szFixtureRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
	const std::string szOutDir = szScratchRoot + "/" + fx.pszExt;
	const std::string szOut = szOutDir + "/project.deleterestore." + fx.pszExt;
	std::error_code ec;
	std::filesystem::create_directories( szOutDir, ec );
	std::filesystem::remove( szOut, ec );
	std::filesystem::remove( szOut + ".bak", ec );
	std::filesystem::remove( szOut + ".tmp", ec );

	std::string szWhat;
	bool ok = true;

	szWhat = std::string( fx.pszExt ) + " [dr]: BkResOpen";
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return false;
	}

	int nCount = 0;
	BkResNodes( pSession, 0, 0, &nCount );
	if ( !Check( nCount >= 2, "has at least one non-root node to delete" ) ) { BkResClose( pSession ); return false; }
	std::vector<BkResNodeRecord> nodes( nCount );
	BkResNodes( pSession, nodes.data(), nCount, &nCount );
	// Find the first direct child of root. The root has id 1.
	int nVictim = 0, nParent = 0, nIndex = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( nodes[i].parent == 1 )
		{
			nVictim = nodes[i].id;
			nParent = nodes[i].parent;
			break;
		}
	}
	if ( !Check( nVictim != 0, "found a victim node under the root" ) ) { BkResClose( pSession ); return false; }

	// Two-pass size then write.
	int nBlobSize = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResDeleteNode (size)";
	ok = Check( BkResDeleteNode( pSession, nVictim, 0, 0, &nBlobSize ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nBlobSize > 0, "blob size is positive" ) && ok;
	// At this point the node is already removed (second phase of DeleteNode).
	// Re-open to get a clean copy and then exercise the "size + fill in one call"
	// shape callers actually use.
	BkResClose( pSession );
	BkResOpen( pSession, szIn.c_str() );
	BkResNodes( pSession, nodes.data(), nCount, &nCount );
	for ( int i = 0; i < nCount; ++i )
		if ( nodes[i].parent == 1 ) { nVictim = nodes[i].id; nParent = nodes[i].parent; nIndex = 0; break; }
	std::vector<unsigned char> blob( nBlobSize );
	int nWrittenSize = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResDeleteNode (fill)";
	ok = Check( BkResDeleteNode( pSession, nVictim, blob.data(), (int)blob.size(), &nWrittenSize ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nWrittenSize == nBlobSize, "written size matches sized pass" ) && ok;

	int nRestoredId = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResRestoreNode";
	ok = Check( BkResRestoreNode( pSession, blob.data(), nWrittenSize, nParent, nIndex, &nRestoredId ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nRestoredId != 0, "restored id is non-zero" ) && ok;

	szWhat = std::string( fx.pszExt ) + " [dr]: BkResSave";
	if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		return false;
	}
	std::string szBefore, szAfter;
	ReadBytes( szIn, szBefore );
	ReadBytes( szOut, szAfter );
	szWhat = std::string( fx.pszExt ) + " [dr]: byte-identical after delete+restore+save";
	if ( !Check( szBefore == szAfter, szWhat.c_str() ) )
	{
		std::printf( "   in=%zu bytes, out=%zu bytes\n", szBefore.size(), szAfter.size() );
		ok = false;
	}
	BkResClose( pSession );
	return ok;
}

// Plays a user for the lock tests (BK_RESOURCE_EDITOR_USER); null clears it.
static void SetLockUser( const char *pszUser )
{
#if defined(_WIN32) || defined(_WIN64)
	_putenv_s( "BK_RESOURCE_EDITOR_USER", pszUser != 0 ? pszUser : "" );
#else
	if ( pszUser != 0 )
		setenv( "BK_RESOURCE_EDITOR_USER", pszUser, 1 );
	else
		unsetenv( "BK_RESOURCE_EDITOR_USER" );
#endif
}

static std::vector<BkResNodeRecord> AllNodes( BkResSession *pSession )
{
	int nCount = 0;
	BkResNodes( pSession, 0, 0, &nCount );
	std::vector<BkResNodeRecord> nodes( nCount > 0 ? nCount : 0 );
	if ( nCount > 0 )
		BkResNodes( pSession, nodes.data(), nCount, &nCount );
	return nodes;
}

static bool SameZero( BkResSession *pSession, int nNode, const BkResPoint2 &want )
{
	BkResPoint2 got = { -1.0f, -1.0f };
	return BkResGetZeroPoint( pSession, nNode, &got ) == BK_EDITOR_OK && got.x == want.x && got.y == want.y;
}

static bool SameCells( BkResSession *pSession, int nNode, const unsigned char *pWant, int nW, int nH )
{
	unsigned char got[64] = {};
	int w = 0, h = 0;
	return BkResGetPassabilityCells( pSession, nNode, got, (int)sizeof( got ), &w, &h ) == BK_EDITOR_OK
		&& w == nW && h == nH && std::memcmp( got, pWant, size_t( nW * nH ) ) == 0;
}

static bool SameShoot( BkResSession *pSession, int nNode, const BkResAimedPoint &want )
{
	BkResAimedPoint got[2] = {};
	int n = 0;
	return BkResGetShootPoints( pSession, nNode, got, 2, &n ) == BK_EDITOR_OK && n == 1
		&& got[0].at.x == want.at.x && got[0].at.y == want.at.y && got[0].angle == want.angle && got[0].cone == want.cone;
}

// The class types of the nodes MFC keeps formation slots and span anchors
// on: ETIT_BASE (0x11000000) + 166 and + 222..224, from tree_item_types.h.
static const int kSquadFormationProps = 0x11000000 + 166;
static const int kBridgeBeginSpans    = 0x11000000 + 222;
static const int kBridgeCenterSpans   = 0x11000000 + 223;
static const int kBridgeEndSpans      = 0x11000000 + 224;
// Mission objectives, chapter missions and places, campaign chapters: the
// containers whose children MFC gives a map cross (+ 232, 242, 246, 252).
static const int kMissionObjectives   = 0x11000000 + 232;
static const int kChapterMissions     = 0x11000000 + 242;
static const int kChapterPlaces       = 0x11000000 + 246;
static const int kCampaignChapters    = 0x11000000 + 252;
// Particle tracks MFC keeps a framesList on (generate density + 140, speed
// + 144), and the effect's animations list (+ 33).
static const int kParticleDensity     = 0x11000000 + 140;
static const int kParticleSpeed       = 0x11000000 + 144;
static const int kEffectAnimations    = 0x11000000 + 33;

static bool SameEntry( const BkResPoint2 &a, const BkResPoint2 &b ) { return a.x == b.x && a.y == b.y; }
static bool SameEntry( const BkResVec3 &a, const BkResVec3 &b ) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// The k-th entry of the i-th owner's test list, with fractions a lossy text
// form would not bring back.
static void MakeEntry( BkResPoint2 &out, size_t i, size_t k )
{
	out = { 16 * 32.0f + float( k ) * 24.5f, 8 * 32.0f - float( i ) * 0.125f };
}
static void MakeEntry( BkResVec3 &out, size_t i, size_t k )
{
	out = { float( k ) * 0.1f, 1.0f / float( 3 + i ), -float( i + k ) * 0.375f };
}

template <typename T>
static bool SameList( BkResSession *pSession, BkEditorStatus ( *pGet )( BkResSession *, int, T *, int, int * ), int nNode,
                      const std::vector<T> &want )
{
	int nCount = -1;
	if ( pGet( pSession, nNode, 0, 0, &nCount ) != BK_EDITOR_OK || nCount != int( want.size() ) )
		return false;
	std::vector<T> got( want.size() + 1 );
	if ( pGet( pSession, nNode, got.data(), int( got.size() ), &nCount ) != BK_EDITOR_OK || nCount != int( want.size() ) )
		return false;
	for ( size_t i = 0; i < want.size(); ++i )
		if ( !SameEntry( got[i], want[i] ) )
			return false;
	return true;
}

// A point or vec3 list channel set on the nodes MFC keeps it on (below the
// root): set -> read, save -> reopen -> read, and a resave of the reopened
// project is byte-identical.
template <typename T>
static void PointListsOnOwnerNodes( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot,
                                    const char *pszExt, const char *pszWhat,
                                    BkEditorStatus ( *pGet )( BkResSession *, int, T *, int, int * ),
                                    BkEditorStatus ( *pSet )( BkResSession *, int, const T *, int ),
                                    const std::vector<int> &ownerTypes )
{
	const std::string szIn = szFixtureRoot + "/" + pszExt + "/project." + pszExt;
	const std::string szDir = szScratchRoot + "/" + pszExt + "-" + pszWhat;
	const std::string szSaved = szDir + "/project." + pszExt;
	const std::string szResaved = szDir + "/project.resaved." + pszExt;
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );
	const std::string szTag = std::string( pszWhat ) + ": ";
	auto What = [&]( const char *pszCheck ) { static std::string s; s = szTag + pszCheck; return s.c_str(); };

	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, What( "BkResOpen" ) ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	std::vector<int> owners;
	for ( int nType : ownerTypes )
		for ( const BkResNodeRecord &n : AllNodes( pSession ) )
			if ( n.class_type == nType && n.parent != 0 ) { owners.push_back( n.id ); break; }
	if ( !Check( owners.size() == ownerTypes.size(), What( "the fixture has every owner node below the root" ) ) )
	{
		BkResClose( pSession );
		return;
	}
	// Distinct lists per owner.
	std::vector<std::vector<T>> lists;
	for ( size_t i = 0; i < owners.size(); ++i )
	{
		std::vector<T> list( i + 2 );
		for ( size_t k = 0; k < list.size(); ++k )
			MakeEntry( list[k], i, k );
		lists.push_back( list );
	}
	for ( size_t i = 0; i < owners.size(); ++i )
		Check( pSet( pSession, owners[i], lists[i].data(), int( lists[i].size() ) ) == BK_EDITOR_OK, What( "set on an owner node" ) );
	for ( size_t i = 0; i < owners.size(); ++i )
		Check( SameList<T>( pSession, pGet, owners[i], lists[i] ), What( "read back what was set" ) );
	Check( SameList<T>( pSession, pGet, 1, {} ), What( "the root's list stays empty" ) );

	// Two-pass rules and argument refusals.
	T shortBuf[1] = {};
	int nCount = -1;
	Check( pGet( pSession, owners.back(), shortBuf, 1, &nCount ) == BK_EDITOR_REFUSED && nCount == int( lists.back().size() ),
		What( "a short buffer is refused and the total still reported" ) );
	Check( pSet( pSession, owners[0], lists[0].data(), -1 ) == BK_EDITOR_BAD_ARGUMENT, What( "a negative count is a bad argument" ) );
	Check( pSet( pSession, owners[0], 0, 2 ) == BK_EDITOR_BAD_ARGUMENT, What( "a null buffer for a non-empty list is a bad argument" ) );
	Check( pSet( pSession, 99999, lists[0].data(), 1 ) == BK_EDITOR_REFUSED, What( "an unknown node is refused" ) );
	Check( SameList<T>( pSession, pGet, owners[0], lists[0] ), What( "a refused set changes nothing" ) );

	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "save" ) ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	BkResClose( pSession );
	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "reopen" ) ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	for ( size_t i = 0; i < owners.size(); ++i )
		Check( SameList<T>( pSession, pGet, owners[i], lists[i] ), What( "an owner node's list survives save+reopen" ) );
	Check( SameList<T>( pSession, pGet, 1, {} ), What( "the root's list is still empty after reopen" ) );
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, What( "save the reopened project" ) );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, What( "open -> save of a project with the list is byte-identical" ) );

	// Clearing a list (count 0) also persists.
	Check( pSet( pSession, owners[0], 0, 0 ) == BK_EDITOR_OK, What( "clear one owner's list" ) );
	Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "save after the clear" ) );
	BkResClose( pSession );
	Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "reopen after the clear" ) );
	Check( SameList<T>( pSession, pGet, owners[0], {} ), What( "the cleared list stays empty" ) );
	if ( owners.size() > 1 )
		Check( SameList<T>( pSession, pGet, owners[1], lists[1] ), What( "the other owners keep their lists" ) );
	BkResClose( pSession );
}

static void GeometryOnChildNodes( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	const std::string szIn = szFixtureRoot + "/wpn/project.wpn";
	const std::string szDir = szScratchRoot + "/geometry";
	const std::string szSaved = szDir + "/project.wpn";
	const std::string szResaved = szDir + "/project.resaved.wpn";
	const std::string szBeforeDelete = szDir + "/project.before-delete.wpn";
	const std::string szAfterRestore = szDir + "/project.after-restore.wpn";
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );

	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "geometry: BkResOpen wpn" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	// A child of the root, and the deepest node (last in pre-order) so the
	// test reaches below the first level whenever the tree has one.
	std::vector<BkResNodeRecord> nodes = AllNodes( pSession );
	int nChild = 0, nDeep = 0;
	for ( const BkResNodeRecord &n : nodes )
		if ( n.parent == 1 && nChild == 0 && n.child_count > 0 )
			nChild = n.id;
	if ( nChild == 0 )
		for ( const BkResNodeRecord &n : nodes )
			if ( n.parent == 1 ) { nChild = n.id; break; }
	if ( !nodes.empty() )
		nDeep = nodes.back().id;
	if ( !Check( nChild != 0 && nDeep != 0 && nDeep != 1, "geometry: wpn has nodes below the root" ) )
	{
		BkResClose( pSession );
		return;
	}
	const BkResPoint2 zeroChild = { 7.5f, -2.25f };
	const BkResPoint2 zeroDeep = { 1.0f, 2.0f };
	const BkResPoint2 zeroRoot = { 3.0f, 4.0f };
	const unsigned char cells[6] = { 9, 8, 7, 6, 5, 4 };
	const BkResAimedPoint shoot = { { 0.5f, 0.25f }, 90, 30 };
	Check( BkResSetZeroPoint( pSession, 1, &zeroRoot ) == BK_EDITOR_OK, "geometry: set the root's zero point" );
	Check( BkResSetZeroPoint( pSession, nChild, &zeroChild ) == BK_EDITOR_OK, "geometry: set a child's zero point" );
	Check( BkResSetPassabilityCells( pSession, nChild, cells, 3, 2 ) == BK_EDITOR_OK, "geometry: set a child's cells" );
	Check( BkResSetZeroPoint( pSession, nDeep, &zeroDeep ) == BK_EDITOR_OK, "geometry: set the deepest node's zero point" );
	Check( BkResSetShootPoints( pSession, nDeep, &shoot, 1 ) == BK_EDITOR_OK, "geometry: set the deepest node's shoot point" );
	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "geometry: save" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	BkResClose( pSession );

	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "geometry: reopen" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Check( AllNodes( pSession ).size() == nodes.size(), "geometry: reopen shows the same nodes, no geometry node" );
	Check( SameZero( pSession, 1, zeroRoot ), "geometry: the root's zero point survives save+reopen" );
	Check( SameZero( pSession, nChild, zeroChild ), "geometry: a child's zero point survives save+reopen" );
	Check( SameCells( pSession, nChild, cells, 3, 2 ), "geometry: a child's cells survive save+reopen" );
	Check( SameZero( pSession, nDeep, zeroDeep ), "geometry: the deepest node's zero point survives save+reopen" );
	Check( SameShoot( pSession, nDeep, shoot ), "geometry: the deepest node's shoot point survives save+reopen" );
	// Saving the reopened project again changes nothing.
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "geometry: save the reopened project" );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, "geometry: open -> save of a project with geometry is byte-identical" );

	// Delete the child (its subtree holds the deepest node's geometry too),
	// then restore it where it was: the save matches the one before.
	Check( BkResSave( pSession, szBeforeDelete.c_str() ) == BK_EDITOR_OK, "geometry: save before delete" );
	nodes = AllNodes( pSession );
	int nIndex = 0;
	for ( const BkResNodeRecord &n : nodes )
	{
		if ( n.id == nChild )
			break;
		if ( n.parent == 1 )
			++nIndex;
	}
	int nSize = 0;
	BkResDeleteNode( pSession, nChild, 0, 0, &nSize );
	std::vector<unsigned char> blob( nSize > 0 ? nSize : 1 );
	Check( BkResDeleteNode( pSession, nChild, blob.data(), nSize, &nSize ) == BK_EDITOR_OK, "geometry: delete the child" );
	BkResPoint2 gone = { 0, 0 };
	Check( BkResGetZeroPoint( pSession, nChild, &gone ) != BK_EDITOR_OK || ( gone.x == 0 && gone.y == 0 ),
		"geometry: the deleted node's geometry is gone" );
	int nRestored = 0;
	Check( BkResRestoreNode( pSession, blob.data(), nSize, 1, nIndex, &nRestored ) == BK_EDITOR_OK && nRestored == nChild,
		"geometry: restore the child under its old id" );
	Check( SameZero( pSession, nChild, zeroChild ) && SameCells( pSession, nChild, cells, 3, 2 ),
		"geometry: restore brings the child's geometry back" );
	Check( SameZero( pSession, nDeep, zeroDeep ) && SameShoot( pSession, nDeep, shoot ),
		"geometry: restore brings the subtree's geometry back" );
	Check( BkResSave( pSession, szAfterRestore.c_str() ) == BK_EDITOR_OK, "geometry: save after restore" );
	ReadBytes( szBeforeDelete, szA );
	ReadBytes( szAfterRestore, szB );
	if ( !Check( !szA.empty() && szA == szB, "geometry: delete -> restore -> save is byte-identical to before the delete" ) )
		std::printf( "   before=%zu bytes, after=%zu bytes\n", szA.size(), szB.size() );
	BkResClose( pSession );
}

// T10: References, MOD settings + PAK, Export (+ batch), Import.
//
// The exporters themselves are ported by each sub-editor's slice; until then
// every kind answers REFUSED, which this tier pins for all 21 fixtures, and
// the golden comparison is reported as pending, never as a pass. The export
// plumbing (export root, staging, move into place, report) is proved with
// test-only exporters registered through NResourceModel::RegisterExporter.

namespace T10
{

static bool g_bLastForce = false, g_bLastStatsOnly = false;

static bool WriteText( const std::filesystem::path &file, const std::string &szText )
{
	std::error_code ec;
	std::filesystem::create_directories( file.parent_path(), ec );
	std::ofstream f( file, std::ios::binary | std::ios::trunc );
	f.write( szText.data(), std::streamsize( szText.size() ) );
	return bool( f );
}

// A stand-in exporter: two files and a warning, and what it was asked.
static bool GoodExporter( const NResourceModel::Project &project, const NResourceModel::SExportContext &context,
                          NResourceModel::SExportOutcome &outcome )
{
	g_bLastForce = context.bForce;
	g_bLastStatsOnly = context.bStatsOnly;
	const std::filesystem::path root( context.szStagingRoot );
	WriteText( root / "medals/t10/1.xml", project.document.root.name );
	WriteText( root / "medals/t10/name.txt", "T10" );
	outcome.nWritten = 2;
	outcome.warnings.push_back( "seeded warning" );
	return true;
}

// Writes half an export, then fails: none of it may reach data/.
static bool FailingExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context,
                             NResourceModel::SExportOutcome &outcome )
{
	WriteText( std::filesystem::path( context.szStagingRoot ) / "medals/t10/half.xml", "half" );
	outcome.szError = "planted failure";
	return false;
}

static std::vector<BkResPropRecord> AllProps( BkResSession *pSession, int nNode )
{
	int nCount = 0;
	BkResProps( pSession, nNode, 0, 0, &nCount );
	std::vector<BkResPropRecord> props( nCount > 0 ? nCount : 0 );
	if ( nCount > 0 )
		BkResProps( pSession, nNode, props.data(), nCount, &nCount );
	return props;
}

// The value text of the first property named szName anywhere in the tree.
static bool FindProp( BkResSession *pSession, const char *pszName, std::string &szValue )
{
	for ( const BkResNodeRecord &node : AllNodes( pSession ) )
		for ( const BkResPropRecord &prop : AllProps( pSession, node.id ) )
			if ( std::strcmp( prop.default_name, pszName ) == 0 )
			{
				szValue = prop.value_text;
				return true;
			}
	return false;
}

static unsigned Get16( const std::string &s, std::size_t n ) { return (unsigned char)s[n] | ( (unsigned char)s[n + 1] << 8 ); }
static unsigned long Get32( const std::string &s, std::size_t n ) { return Get16( s, n ) | ( (unsigned long)Get16( s, n + 2 ) << 16 ); }

// An independent reader of the archive BkResPackMod wrote: the central
// directory, then every entry inflated with zlib and compared with its
// source file and CRC. Collects the methods seen.
static bool ReadZipBack( const std::string &szZip, const std::filesystem::path &dataDir, std::size_t nExpected,
                         bool &bSawDeflate, bool &bSawStored, std::string &szWhy )
{
	std::string zip;
	if ( !ReadBytes( szZip, zip ) || zip.size() < 22 || Get32( zip, zip.size() - 22 ) != 0x06054b50 )
	{
		szWhy = "no end of central directory";
		return false;
	}
	const std::size_t nEntries = Get16( zip, zip.size() - 12 );
	std::size_t nPos = Get32( zip, zip.size() - 6 );
	if ( nEntries != nExpected )
	{
		szWhy = "entries " + std::to_string( nEntries ) + " != files " + std::to_string( nExpected );
		return false;
	}
	for ( std::size_t i = 0; i < nEntries; ++i )
	{
		if ( Get32( zip, nPos ) != 0x02014b50 )
		{
			szWhy = "bad central header";
			return false;
		}
		const unsigned nMethod = Get16( zip, nPos + 10 );
		const unsigned long nCrc = Get32( zip, nPos + 16 ), nPacked = Get32( zip, nPos + 20 ), nSize = Get32( zip, nPos + 24 );
		const unsigned nName = Get16( zip, nPos + 28 ), nExtra = Get16( zip, nPos + 30 ), nComment = Get16( zip, nPos + 32 );
		const std::size_t nLocal = Get32( zip, nPos + 42 );
		const std::string szName = zip.substr( nPos + 46, nName );
		nPos += 46 + nName + nExtra + nComment;
		if ( szName.empty() || szName.back() == '/' || szName.find( '\\' ) != std::string::npos )
		{
			szWhy = "entry name '" + szName + "' is a directory or has backslashes";
			return false;
		}
		const std::size_t nData = nLocal + 30 + Get16( zip, nLocal + 26 ) + Get16( zip, nLocal + 28 );
		std::string szGot;
		if ( nMethod == 0 )
		{
			bSawStored = true;
			szGot = zip.substr( nData, nPacked );
		}
		else if ( nMethod == 8 )
		{
			bSawDeflate = true;
			std::string szIn = zip.substr( nData, nPacked );
			szIn.push_back( 0 );   // zlib 1.1.x raw inflate wants one byte past the stream
			szGot.assign( nSize, '\0' );
			z_stream z;
			std::memset( &z, 0, sizeof( z ) );
			inflateInit2( &z, -MAX_WBITS );
			z.next_in = reinterpret_cast<Bytef *>( &szIn[0] );
			z.avail_in = uInt( szIn.size() );
			z.next_out = reinterpret_cast<Bytef *>( szGot.empty() ? &szIn[0] : &szGot[0] );
			z.avail_out = uInt( szGot.size() );
			const int nResult = inflate( &z, Z_FINISH );
			inflateEnd( &z );
			if ( nResult != Z_STREAM_END && !( nResult == Z_BUF_ERROR && z.total_out == nSize ) )
			{
				szWhy = szName + ": inflate " + std::to_string( nResult );
				return false;
			}
		}
		std::string szWant;
		ReadBytes( ( dataDir / szName ).string(), szWant );
		const unsigned long nWantCrc = crc32( crc32( 0L, Z_NULL, 0 ), reinterpret_cast<const Bytef *>( szWant.data() ), uInt( szWant.size() ) );
		if ( szGot != szWant || nCrc != nWantCrc || nSize != szWant.size() )
		{
			szWhy = szName + " differs from its source";
			return false;
		}
	}
	return true;
}

static int GoldenFiles( const std::filesystem::path &golden )
{
	int nFiles = 0;
	std::error_code ec;
	for ( std::filesystem::recursive_directory_iterator it( golden, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		const std::string szName = it->path().filename().string();
		if ( it->is_regular_file( ec ) && szName != "README.md" && szName != ".gitkeep" )
			++nFiles;
	}
	return nFiles;
}

static void Run( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "t10";
	fs::remove_all( scratch, ec );
	fs::create_directories( scratch, ec );

	// References: every list as NResourceModel::References walks the staged
	// Data (no mod is active), handed through the ABI with index tokens.
	{
		int nCount = -1;
		Check( BkResRefList( pSession, 20, 0, 0, &nCount ) == BK_EDITOR_BAD_ARGUMENT, "refs: type 20 is a bad argument" );
		Check( BkResRefList( pSession, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "refs: a null count is a bad argument" );
		NResourceModel::References direct;
		std::error_code ecData;
		fs::path data = fs::path( szRoot ) / "Data";
		direct.rebuild( data );
		std::size_t nTotal = 0;
		for ( int t = 0; t < NResourceModel::kReferenceTypeCount; ++t )
		{
			const auto &want = direct.enumerate( static_cast<NResourceModel::EReferenceType>( t ) );
			nCount = -1;
			const bool bCounted = BkResRefList( pSession, t, 0, 0, &nCount ) == BK_EDITOR_OK;
			std::vector<BkResReferenceEntry> got( nCount > 0 ? nCount : 0 );
			const bool bRead = got.empty() || BkResRefList( pSession, t, got.data(), nCount, &nCount ) == BK_EDITOR_OK;
			bool bSame = bCounted && bRead && nCount == int( want.size() );
			for ( std::size_t i = 0; bSame && i < want.size(); ++i )
				bSame = got[i].token == int( i ) && want[i].compare( 0, sizeof( got[i].name ) - 1, got[i].name ) == 0;
			Check( bSame, ( "refs: list " + std::string( NResourceModel::ReferenceTypeName( static_cast<NResourceModel::EReferenceType>( t ) ) ) +
			                " matches References over Data" ).c_str() );
			std::printf( "REF %s count=%d\n", NResourceModel::ReferenceTypeName( static_cast<NResourceModel::EReferenceType>( t ) ), nCount );
			nTotal += want.size();
			if ( nCount > 1 )
			{
				BkResReferenceEntry one;
				Check( BkResRefList( pSession, t, &one, 1, &nCount ) == BK_EDITOR_REFUSED, "refs: a short buffer is refused" );
			}
		}
		nCount = 0;
		BkResRefList( pSession, int( NResourceModel::EReferenceType::E_WEAPONS_REF ), 0, 0, &nCount );
		Check( nCount > 0 && nTotal > 0, "refs: the staged Data lists weapons" );
	}

	// MOD settings: the default export dir, then MFC's mod.xml written by the
	// engine's saver and read back by its reader; never into shipped Data.
	const fs::path modDir = scratch / "MyTestMod";
	const fs::path modData = modDir / "data";
	{
		BkResModSettings settings;
		Check( BkResModSettingsGet( pSession, &settings ) == BK_EDITOR_OK, "mod: Get answers OK" );
		Check( std::strstr( settings.export_dir, "mymod" ) != 0, "mod: the default export dir is mods/mymod, as MFC" );
		Check( BkResModSettingsGet( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "mod: a null Get is a bad argument" );

		std::string szShippedModXml;
		ReadBytes( szRoot + "/Data/mod.xml", szShippedModXml );
		BkResModSettings shipped = {};
		std::snprintf( shipped.export_dir, sizeof( shipped.export_dir ), "%s", szRoot.c_str() );
		std::snprintf( shipped.name, sizeof( shipped.name ), "must not land" );
		Check( BkResModSettingsSet( pSession, &shipped ) == BK_EDITOR_REFUSED, "mod: the base root (its data is Data/) is refused" );
		std::snprintf( shipped.export_dir, sizeof( shipped.export_dir ), "%s/Data", szRoot.c_str() );
		Check( BkResModSettingsSet( pSession, &shipped ) == BK_EDITOR_REFUSED, "mod: Data/ itself is refused" );
		std::string szAfter;
		ReadBytes( szRoot + "/Data/mod.xml", szAfter );
		Check( szAfter == szShippedModXml, "mod: the shipped Data/mod.xml is untouched" );
		BkResModSettings empty = {};
		Check( BkResModSettingsSet( pSession, &empty ) == BK_EDITOR_BAD_ARGUMENT, "mod: an empty export dir is a bad argument" );

		BkResModSettings mine = {};
		std::snprintf( mine.export_dir, sizeof( mine.export_dir ), "%s", modDir.string().c_str() );
		std::snprintf( mine.name, sizeof( mine.name ), "T10 Mod" );
		std::snprintf( mine.version, sizeof( mine.version ), "1.2" );
		std::snprintf( mine.desc, sizeof( mine.desc ), "a test mod" );
		if ( !Check( BkResModSettingsSet( pSession, &mine ) == BK_EDITOR_OK, "mod: Set answers OK" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		std::string szModXml;
		Check( ReadBytes( ( modData / "mod.xml" ).string(), szModXml ) && szModXml.find( "T10 Mod" ) != std::string::npos,
		       "mod: data/mod.xml holds MODName" );
		Check( fs::is_regular_file( modData / "modobjects.xml", ec ), "mod: modobjects.xml is seeded from editor\\modobjects.xml" );
		BkResModSettings back;
		Check( BkResModSettingsGet( pSession, &back ) == BK_EDITOR_OK && std::strcmp( back.export_dir, mine.export_dir ) == 0 &&
		       std::strcmp( back.name, "T10 Mod" ) == 0 && std::strcmp( back.version, "1.2" ) == 0 && std::strcmp( back.desc, "a test mod" ) == 0,
		       "mod: Get reads back what Set wrote" );
	}

	// Export: the open project's kind has no exporter yet - refused, and the
	// golden comparison is pending for every fixture.
	{
		BkResExportReport report = {};
		BkResClose( pSession );
		Check( BkResExport( pSession, 0, &report ) == BK_EDITOR_REFUSED, "export: no project is refused" );
		Check( BkResNew( pSession, 19 ) == BK_EDITOR_OK, "export: BkResNew(mdc)" );
		NResourceModel::RegisterExporter( "mdc", &GoodExporter );
		Check( BkResExport( pSession, 0, &report ) == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), "save the project first" ) != 0,
		       "export: an unsaved project is refused" );
		NResourceModel::RegisterExporter( "mdc", nullptr );

		int nPending = 0;
		for ( int i = 0; i < kFixtureCount; ++i )
		{
			const std::string szExt = kFixtures[i].pszExt;
			const std::string szProject = szFixtureRoot + "/" + szExt + "/project." + szExt;
			if ( BkResOpen( pSession, szProject.c_str() ) != BK_EDITOR_OK )
			{
				Check( false, ( "export: open " + szExt ).c_str() );
				continue;
			}
			report = BkResExportReport();
			const BkEditorStatus status = BkResExport( pSession, BK_RES_EXPORT_FORCE, &report );
			Check( status == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), "not ported yet" ) != 0 && report.written == 0,
			       ( "export: ." + szExt + " says its exporter is not ported yet" ).c_str() );
			const int nGolden = GoldenFiles( fs::path( szFixtureRoot ) / szExt / "golden" );
			if ( nGolden == 0 )
				std::printf( "GOLDEN %s pending: golden missing (run tools/zig/win-home/export-goldens.ps1 on win-home)\n", szExt.c_str() );
			else
				std::printf( "GOLDEN %s pending: %d golden files, the port has no exporter to compare them with yet\n", szExt.c_str(), nGolden );
			++nPending;
		}
		std::printf( "GOLDEN_SUMMARY extensions=%d pass=0 pending=%d\n", kFixtureCount, nPending );

		// The plumbing with a stand-in exporter: staged, moved into data/,
		// reported; a failing exporter leaves nothing behind.
		const fs::path project = scratch / "export" / "project.mdc";
		fs::create_directories( project.parent_path(), ec );
		fs::copy_file( szFixtureRoot + "/mdc/project.mdc", project, fs::copy_options::overwrite_existing, ec );
		Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "export: open the mdc copy" );
		NResourceModel::RegisterExporter( "mdc", &GoodExporter );
		BkResWarning warnings[4] = {};
		report = BkResExportReport();
		report.warnings = warnings;
		report.warnings_capacity = 4;
		if ( !Check( BkResExport( pSession, BK_RES_EXPORT_FORCE, &report ) == BK_EDITOR_OK, "export: a registered exporter exports" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		Check( report.written == 2 && report.warning_count == 1 && std::strcmp( warnings[0].text, "seeded warning" ) == 0,
		       "export: the report carries the exporter's counts and warning" );
		std::string szExported;
		Check( ReadBytes( ( modData / "medals/t10/1.xml" ).string(), szExported ) && szExported == "Medal_Composer_Project" &&
		       fs::is_regular_file( modData / "medals/t10/name.txt", ec ), "export: the files are moved into the export root's data/" );
		Check( !fs::exists( modDir / ".bk-export-staging", ec ), "export: the staging folder is gone" );
		Check( g_bLastForce && !g_bLastStatsOnly, "export: FORCE reaches the exporter" );
		report = BkResExportReport();
		Check( BkResExportStatsOnly( pSession, 0, &report ) == BK_EDITOR_OK && g_bLastStatsOnly && !g_bLastForce && report.warning_count == 1,
		       "export: stats only reaches the exporter; a null warnings buffer still gets the total" );

		NResourceModel::RegisterExporter( "mdc", &FailingExporter );
		Check( BkResExport( pSession, 0, &report ) == BK_EDITOR_FAILED && std::strstr( BkEditorLastMessage( pSession ), "planted failure" ) != 0,
		       "export: a failing exporter is FAILED with its reason" );
		Check( !fs::exists( modData / "medals/t10/half.xml", ec ) && !fs::exists( modDir / ".bk-export-staging", ec ),
		       "export: a failed export leaves no file in data/ and no staging" );

		// Batch: an mdc (exported), a wpn and a unt (not ported: skipped with
		// a warning each), then -os re-saving a wpn unchanged.
		NResourceModel::RegisterExporter( "mdc", &GoodExporter );
		const fs::path src = scratch / "batch-src";
		fs::create_directories( src / "nested", ec );
		fs::copy_file( szFixtureRoot + "/mdc/project.mdc", src / "nested" / "medal.mdc", fs::copy_options::overwrite_existing, ec );
		fs::copy_file( szFixtureRoot + "/wpn/project.wpn", src / "weapon.wpn", fs::copy_options::overwrite_existing, ec );
		fs::copy_file( szFixtureRoot + "/unt/project.unt", src / "unit.unt", fs::copy_options::overwrite_existing, ec );
		const fs::path dst = scratch / "BatchOut";
		BkResWarning batchWarnings[8] = {};
		report = BkResExportReport();
		report.warnings = batchWarnings;
		report.warnings_capacity = 8;
		if ( !Check( BkResBatch( pSession, -1, src.string().c_str(), dst.string().c_str(), BK_RES_EXPORT_FORCE, &report ) == BK_EDITOR_OK,
		             "batch: all kinds answers OK" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		int nNotPorted = 0;
		for ( int i = 0; i < report.warning_count && i < 8; ++i )
			if ( std::strstr( batchWarnings[i].text, "not ported yet" ) != 0 )
				++nNotPorted;
		Check( report.written == 2 && report.skipped == 2 && nNotPorted == 2, "batch: mdc exported, wpn and unt skipped as not ported" );
		Check( fs::is_regular_file( dst / "data" / "medals/t10/1.xml", ec ), "batch: the export lands in dst/data/" );
		std::string szBefore, szResaved;
		ReadBytes( ( src / "weapon.wpn" ).string(), szBefore );
		report = BkResExportReport();
		Check( BkResBatch( pSession, 0, src.string().c_str(), dst.string().c_str(), BK_RES_EXPORT_OPEN_SAVE, &report ) == BK_EDITOR_OK && report.written == 1,
		       "batch: -os re-saves the one wpn" );
		Check( ReadBytes( ( src / "weapon.wpn" ).string(), szResaved ) && szResaved == szBefore, "batch: -os leaves an unedited project byte-identical" );
		Check( BkResBatch( pSession, 21, src.string().c_str(), dst.string().c_str(), 0, &report ) == BK_EDITOR_BAD_ARGUMENT, "batch: kind 21 is a bad argument" );
		Check( BkResBatch( pSession, -1, ( scratch / "no-such" ).string().c_str(), dst.string().c_str(), 0, &report ) == BK_EDITOR_DATA_MISSING,
		       "batch: a missing source folder is DATA_MISSING" );
		Check( BkResBatch( pSession, -1, src.string().c_str(), szRoot.c_str(), 0, &report ) == BK_EDITOR_REFUSED, "batch: the shipped Data as destination is refused" );
		NResourceModel::RegisterExporter( "mdc", nullptr );
		BkResClose( pSession );
	}

	// PAK: the mod's data/ zipped natively (the bridge mounts it through the
	// engine's zip storage itself); read back here with zlib, independently.
	{
		std::string szNoise( 4096, '\0' );
		unsigned nSeed = 12345;
		for ( char &c : szNoise )
		{
			nSeed = nSeed * 1103515245u + 12345u;
			c = char( nSeed >> 24 );
		}
		WriteText( modData / "units/humans/t10/noise.bin", szNoise );   // incompressible: stored
		WriteText( modData / "units/humans/t10/1.xml", std::string( 2000, 'x' ) );
		const std::string szZip = ( scratch / "MyTestMod.pak" ).string();
		if ( !Check( BkResPackMod( pSession, szZip.c_str() ) == BK_EDITOR_OK, "pak: BkResPackMod answers OK" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		std::size_t nFiles = 0;
		for ( fs::recursive_directory_iterator it( modData, ec ), end; !ec && it != end; it.increment( ec ) )
			if ( it->is_regular_file( ec ) )
				++nFiles;
		bool bDeflate = false, bStored = false;
		std::string szWhy;
		if ( !Check( ReadZipBack( szZip, modData, nFiles, bDeflate, bStored, szWhy ), "pak: every entry inflates to its source" ) )
			std::printf( "   detail: %s\n", szWhy.c_str() );
		Check( bDeflate && bStored, "pak: deflate where it is smaller, stored where it is not" );
		Check( !fs::exists( scratch / ".bk-pack-verify", ec ), "pak: no verification folder is left" );
		Check( BkResPackMod( pSession, ( modData / "inside.pak" ).string().c_str() ) == BK_EDITOR_REFUSED, "pak: an archive inside data/ is refused" );
		Check( BkResPackMod( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "pak: a null path is a bad argument" );
	}

	// Import from game data: a shipped infantry folder, read by the engine's
	// operator&, put into a fresh tree by the GetRPGStats port.
	{
		const std::string szGunner = szRoot + "/Data/Units/Humans/German/Gunner";
		if ( !Check( BkResImportFromGame( pSession, 5, szGunner.c_str() ) == BK_EDITOR_OK, "import: unt from Gunner" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResKind kind = -1;
		Check( BkResKindOf( pSession, &kind ) == BK_EDITOR_OK && kind == 5, "import: the open project is an infantry project" );
		Check( AllNodes( pSession ).size() > 1, "import: the default tree is built" );
		std::string szName, szType, szHealth, szWeapon;
		Check( FindProp( pSession, "Name", szName ) && szName == "German_Gunner", ( "import: Name is the KeyName (" + szName + ")" ).c_str() );
		Check( FindProp( pSession, "Type", szType ) && szType == "engineer", ( "import: Type is engineer (" + szType + ")" ).c_str() );
		Check( FindProp( pSession, "Health", szHealth ) && std::strtof( szHealth.c_str(), 0 ) == 10.0f, ( "import: Health is MaxHP 10 (" + szHealth + ")" ).c_str() );
		const fs::path saved = scratch / "import" / "gunner.unt";
		fs::create_directories( saved.parent_path(), ec );
		Check( BkResSave( pSession, saved.string().c_str() ) == BK_EDITOR_OK && BkResOpen( pSession, saved.string().c_str() ) == BK_EDITOR_OK,
		       "import: the imported project saves and reopens" );
		std::string szReopened;
		Check( FindProp( pSession, "Name", szReopened ) && szReopened == "German_Gunner", "import: the KeyName survives save and reopen" );

		Check( BkResImportFromGame( pSession, 4, szGunner.c_str() ) == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), ".san" ) != 0,
		       "import: sprite is refused with the reason" );
		Check( BkResImportFromGame( pSession, 0, szGunner.c_str() ) == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), "not ported yet" ) != 0,
		       "import: wpn is refused as not ported yet" );
		Check( BkResKindOf( pSession, &kind ) == BK_EDITOR_OK && kind == 5, "import: a refused import keeps the open project" );
		Check( BkResImportFromGame( pSession, 5, ( scratch / "no-such" ).string().c_str() ) == BK_EDITOR_DATA_MISSING, "import: a folder without 1.xml is DATA_MISSING" );
		Check( BkResImportFromGame( pSession, 5, 0 ) == BK_EDITOR_BAD_ARGUMENT, "import: a null path is a bad argument" );
		Check( BkResImportFromGame( pSession, 21, szGunner.c_str() ) == BK_EDITOR_BAD_ARGUMENT, "import: kind 21 is a bad argument" );
		BkResClose( pSession );
	}
}

}

// S05 T01: export promotion is all-or-nothing (D-09, D014 item 1). A
// stand-in exporter stages several files; a directory standing at the second
// file's target makes its move fail on Linux and Windows alike. The files
// moved before it must go back - a replaced file restored from its backup, a
// new file and its new folders removed - so the export root is byte-identical
// to before, with no staging or backup folder left.

namespace ExportRollback
{

// What the stand-in exporter stages, in promotion (sorted) order.
static std::vector<std::string> g_staged;

static bool StagingExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context,
                             NResourceModel::SExportOutcome &outcome )
{
	for ( const std::string &szRelative : g_staged )
		T10::WriteText( std::filesystem::path( context.szStagingRoot ) / szRelative, "new " + szRelative );
	outcome.nWritten = int( g_staged.size() );
	return true;
}

// Every file and folder below root: generic relative path -> bytes, folders
// marked with a trailing slash.
static std::vector<std::pair<std::string, std::string>> Snapshot( const std::filesystem::path &root )
{
	std::vector<std::pair<std::string, std::string>> tree;
	std::error_code ec;
	for ( std::filesystem::recursive_directory_iterator it( root, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		const std::string szRelative = std::filesystem::relative( it->path(), root, ec ).generic_string();
		std::string szBytes;
		if ( it->is_directory( ec ) )
			tree.push_back( std::make_pair( szRelative + "/", std::string() ) );
		else if ( ReadBytes( it->path().string(), szBytes ) )
			tree.push_back( std::make_pair( szRelative, szBytes ) );
		else
			tree.push_back( std::make_pair( szRelative, std::string( "<unreadable>" ) ) );
	}
	std::sort( tree.begin(), tree.end() );
	return tree;
}

static void PrintDifference( const std::vector<std::pair<std::string, std::string>> &before,
                             const std::vector<std::pair<std::string, std::string>> &after )
{
	for ( const auto &entry : before )
		if ( std::find( after.begin(), after.end(), entry ) == after.end() )
			std::printf( "   before only: %s\n", entry.first.c_str() );
	for ( const auto &entry : after )
		if ( std::find( before.begin(), before.end(), entry ) == before.end() )
			std::printf( "   after only: %s\n", entry.first.c_str() );
}

// One forced failure: stage files, export, expect FAILED naming szBlocked and
// the rollback, and an export root identical to the snapshot taken before.
static void ExpectRollback( BkResSession *pSession, const std::filesystem::path &modDir, const char *pszCase, const std::string &szBlocked )
{
	const auto before = Snapshot( modDir );
	BkResExportReport report = {};
	const BkEditorStatus status = BkResExport( pSession, BK_RES_EXPORT_FORCE, &report );
	const std::string szMessage = BkEditorLastMessage( pSession );
	if ( !Check( status == BK_EDITOR_FAILED, ( std::string( "export-rollback: " ) + pszCase + ": a blocked move fails the export" ).c_str() ) )
		std::printf( "   detail: status %d, %s\n", int( status ), szMessage.c_str() );
	if ( !Check( szMessage.find( szBlocked ) != std::string::npos && szMessage.find( "rolled back" ) != std::string::npos,
	             ( std::string( "export-rollback: " ) + pszCase + ": the message names the failing file and the rollback" ).c_str() ) )
		std::printf( "   detail: %s\n", szMessage.c_str() );
	const auto after = Snapshot( modDir );
	if ( !Check( before == after, ( std::string( "export-rollback: " ) + pszCase + ": the export root is byte-identical to before" ).c_str() ) )
		PrintDifference( before, after );
	std::error_code ec;
	Check( !std::filesystem::exists( modDir / ".bk-export-staging", ec ) && !std::filesystem::exists( modDir / ".bk-export-backup", ec ),
	       ( std::string( "export-rollback: " ) + pszCase + ": no staging or backup folder is left" ).c_str() );
}

static void Run( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "export_rollback";
	fs::remove_all( scratch, ec );
	fs::create_directories( scratch, ec );
	const fs::path modDir = scratch / "RollbackMod";
	const fs::path data = modDir / "data";

	BkResModSettings settings = {};
	std::snprintf( settings.export_dir, sizeof( settings.export_dir ), "%s", modDir.string().c_str() );
	std::snprintf( settings.name, sizeof( settings.name ), "Rollback Mod" );
	if ( !Check( BkResModSettingsSet( pSession, &settings ) == BK_EDITOR_OK, "export-rollback: the mod settings point at the scratch mod" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	const fs::path project = scratch / "project.mdc";
	fs::copy_file( szFixtureRoot + "/mdc/project.mdc", project, fs::copy_options::overwrite_existing, ec );
	Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "export-rollback: open the mdc copy" );
	NResourceModel::RegisterExporter( "mdc", &StagingExporter );

	// Live files the export would replace or leave alone, and a folder (with
	// a file in it) where the second staged file wants to go.
	T10::WriteText( data / "medals/rb/1-replaced.xml", "old 1" );
	T10::WriteText( data / "medals/rb/2-blocked.xml/inside.txt", "a folder in the way" );
	T10::WriteText( data / "medals/rb/3-replaced.xml", "old 3" );
	T10::WriteText( data / "medals/rb/untouched.txt", "not part of the export" );

	// Case 1: the first file replaces a live file, the second is blocked.
	g_staged = { "medals/rb/1-replaced.xml", "medals/rb/2-blocked.xml", "medals/rb/3-replaced.xml", "medals/rb/4-new.xml" };
	ExpectRollback( pSession, modDir, "replace-then-fail", "medals/rb/2-blocked.xml" );

	// Case 2: the first file is new, in folders that did not exist; both
	// the file and its folders must go.
	g_staged = { "medals/a-new/deep/0-new.xml", "medals/rb/2-blocked.xml", "medals/rb/3-replaced.xml" };
	ExpectRollback( pSession, modDir, "new-then-fail", "medals/rb/2-blocked.xml" );

	// With the folder out of the way the same export goes through whole and
	// leaves no backup behind.
	fs::remove_all( data / "medals/rb/2-blocked.xml", ec );
	g_staged = { "medals/rb/1-replaced.xml", "medals/rb/2-blocked.xml", "medals/rb/3-replaced.xml", "medals/rb/4-new.xml" };
	BkResExportReport report = {};
	if ( !Check( BkResExport( pSession, BK_RES_EXPORT_FORCE, &report ) == BK_EDITOR_OK, "export-rollback: the unblocked export succeeds" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	bool bAll = true;
	for ( const std::string &szRelative : g_staged )
	{
		std::string szBytes;
		bAll = bAll && ReadBytes( ( data / szRelative ).string(), szBytes ) && szBytes == "new " + szRelative;
	}
	std::string szUntouched;
	Check( bAll && ReadBytes( ( data / "medals/rb/untouched.txt" ).string(), szUntouched ) && szUntouched == "not part of the export",
	       "export-rollback: every staged file is promoted, other files stay" );
	Check( !fs::exists( modDir / ".bk-export-staging", ec ) && !fs::exists( modDir / ".bk-export-backup", ec ),
	       "export-rollback: a successful export leaves no staging or backup folder" );

	NResourceModel::RegisterExporter( "mdc", nullptr );
	BkResClose( pSession );
}

}

// T11: the preview group (D-16). The real exporters come with each kind's
// sub-editor slice, so the preview's own path - export into the preview
// folder, mount it over the data, build through IVisObjBuilder, draw on the
// empty scene - is proved with stand-in exporters that copy one shipped
// resource of the kind into the staging root, as an exporter would write it.
// Every capture is measured by code: the share of pixels that are neither
// black nor the renderer's magenta fallback (>= 1%), and the share that differ
// from the empty preview frame (the object really drew).

namespace T11
{

static std::filesystem::path g_dataRoot;

// pszRelative resolved below root one component at a time, ignoring case:
// the shipped Data keeps MFC-era mixed case and Linux does not fold it
// (AGENTS.md, the DataFile helper of editor_bridge_test.cpp).
static std::filesystem::path FoldedPath( const std::filesystem::path &root, const char *pszRelative )
{
	std::filesystem::path current = root;
	std::error_code ec;
	for ( const std::filesystem::path &part : std::filesystem::path( pszRelative ) )
	{
		std::filesystem::path next = current / part;
		if ( !std::filesystem::exists( next, ec ) )
			for ( std::filesystem::directory_iterator it( current, ec ), end; !ec && it != end; it.increment( ec ) )
			{
				std::string a = it->path().filename().string(), b = part.string();
				std::transform( a.begin(), a.end(), a.begin(), ::tolower );
				std::transform( b.begin(), b.end(), b.begin(), ::tolower );
				if ( a == b ) { next = it->path(); break; }
			}
		current = next;
	}
	return current;
}

// Copies the regular files of a shipped folder below the staging root.
static bool CopyFolder( const char *pszShipped, const std::filesystem::path &target, NResourceModel::SExportOutcome &outcome )
{
	std::error_code ec;
	const std::filesystem::path source = FoldedPath( g_dataRoot, pszShipped );
	std::filesystem::create_directories( target, ec );
	for ( std::filesystem::directory_iterator it( source, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		if ( !it->is_regular_file() )
			continue;
		std::string szName = it->path().filename().string();
		std::transform( szName.begin(), szName.end(), szName.begin(), ::tolower );
		std::filesystem::copy_file( it->path(), target / szName, std::filesystem::copy_options::overwrite_existing, ec );
		if ( ec )
			break;
		++outcome.nWritten;
	}
	if ( ec || outcome.nWritten == 0 )
		outcome.szError = std::string( "cannot copy " ) + source.string();
	return !ec && outcome.nWritten > 0;
}

static bool MeshExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context, NResourceModel::SExportOutcome &outcome )
{
	outcome.szObjectName = "editor\\preview\\mesh\\1";
	return CopyFolder( "Units/Technics/German/SPG/Jagdpanther_SdKfz173", std::filesystem::path( context.szStagingRoot ) / "editor/preview/mesh", outcome );
}

static bool SpriteExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context, NResourceModel::SExportOutcome &outcome )
{
	outcome.szObjectName = "editor\\preview\\sprite\\1";
	return CopyFolder( "Buildings/europe/summer/e_house11_3", std::filesystem::path( context.szStagingRoot ) / "editor/preview/sprite", outcome );
}

// An effect is one xml whose particles stay in the shipped data below.
static bool EffectExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context, NResourceModel::SExportOutcome &outcome )
{
	std::error_code ec;
	const std::filesystem::path target = std::filesystem::path( context.szStagingRoot ) / "editor/preview/effect.xml";
	std::filesystem::create_directories( target.parent_path(), ec );
	std::filesystem::copy_file( FoldedPath( g_dataRoot, "Effects/Effects/flame_smoke.xml" ), target, std::filesystem::copy_options::overwrite_existing, ec );
	outcome.nWritten = ec ? 0 : 1;
	outcome.szObjectName = "editor\\preview\\effect";
	if ( ec )
		outcome.szError = "cannot copy flame_smoke.xml: " + ec.message();
	return !ec;
}

static bool NamelessExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &, NResourceModel::SExportOutcome & )
{
	return true;
}

// A bridge-written capture (32-bit, top row first, BGRA) as RGB triples;
// false for anything else.
static bool ReadCapture( const std::string &szPath, std::vector<unsigned char> &rgb, int &nWidth, int &nHeight )
{
	std::string bytes;
	if ( !ReadBytes( szPath, bytes ) || bytes.size() < 18 )
		return false;
	const unsigned char *h = (const unsigned char *)bytes.data();
	if ( h[2] != 2 || h[16] != 32 || ( h[17] & 0x20 ) == 0 )
		return false;
	nWidth = h[12] | ( h[13] << 8 );
	nHeight = h[14] | ( h[15] << 8 );
	const std::size_t nPixels = std::size_t( nWidth ) * nHeight;
	if ( nPixels == 0 || bytes.size() < 18 + h[0] + nPixels * 4 )
		return false;
	const unsigned char *p = h + 18 + h[0];
	rgb.resize( nPixels * 3 );
	for ( std::size_t i = 0; i < nPixels; ++i )
	{
		rgb[i * 3 + 0] = p[i * 4 + 2];
		rgb[i * 3 + 1] = p[i * 4 + 1];
		rgb[i * 3 + 2] = p[i * 4 + 0];
	}
	return true;
}

// preview_scene_spike.cpp's measure: neither solid black nor magenta.
static double NonBlackNonMagentaShare( const std::vector<unsigned char> &rgb )
{
	const std::size_t nPixels = rgb.size() / 3;
	std::size_t nInteresting = 0;
	for ( std::size_t i = 0; i < nPixels; ++i )
	{
		const unsigned char r = rgb[i * 3], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
		if ( !( r == 0 && g == 0 && b == 0 ) && !( r == 255 && g == 0 && b == 255 ) )
			++nInteresting;
	}
	return nPixels == 0 ? 0.0 : double( nInteresting ) / double( nPixels );
}

// The share of pixels in which any channel differs by more than 8.
static double ChangedShare( const std::vector<unsigned char> &a, const std::vector<unsigned char> &b )
{
	if ( a.size() != b.size() || a.empty() )
		return -1.0;
	std::size_t nChanged = 0;
	for ( std::size_t i = 0; i < a.size(); i += 3 )
		if ( std::abs( a[i] - b[i] ) > 8 || std::abs( a[i + 1] - b[i + 1] ) > 8 || std::abs( a[i + 2] - b[i + 2] ) > 8 )
			++nChanged;
	return double( nChanged ) / double( a.size() / 3 );
}

// The temp folders this process's previews left behind.
static int PreviewFolders()
{
	int nFound = 0;
	std::error_code ec;
#if defined(_WIN32) || defined(_WIN64)
	const std::string szPrefix = "bk-resource-preview-" + std::to_string( (unsigned long)GetCurrentProcessId() ) + "-";
#else
	const std::string szPrefix = "bk-resource-preview-" + std::to_string( (unsigned long)getpid() ) + "-";
#endif
	for ( std::filesystem::directory_iterator it( std::filesystem::temp_directory_path( ec ), ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->path().filename().string().rfind( szPrefix, 0 ) == 0 )
			++nFound;
	return nFound;
}

struct Capture
{
	const char *pszLabel;    // the capture's name, as the S01 spike named it
	const char *pszExt;      // the fixture kind
	int nKind;
	NResourceModel::FExporter pfnExporter;
};

static void Run( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	g_dataRoot = fs::path( szRoot ) / "Data";
	const fs::path scratch = fs::path( szScratchRoot ) / "preview-scene";
	fs::remove_all( scratch, ec );
	fs::create_directories( scratch, ec );
	std::ofstream log( scratch / "preview.log", std::ios::out | std::ios::trunc );
	auto Log = [&]( const std::string &sz ) { std::printf( "%s\n", sz.c_str() ); log << sz << '\n'; };

	// Refusals before anything is built.
	{
		Check( BkResPreviewShow( pSession ) == BK_EDITOR_REFUSED, "preview: Show before Begin is refused" );
		Check( BkResPreviewPlayback( pSession, 1 ) == BK_EDITOR_REFUSED, "preview: Playback before Show is refused" );
		Check( BkResPreviewCamera( pSession, 0, 0, 0 ) == BK_EDITOR_REFUSED, "preview: Camera before Begin is refused" );
		Check( BkResPreviewBegin( pSession, 21 ) == BK_EDITOR_BAD_ARGUMENT, "preview: kind 21 is a bad argument" );
		Check( BkResPreviewBegin( pSession, -1 ) == BK_EDITOR_BAD_ARGUMENT, "preview: kind -1 is a bad argument" );
		Check( BkResPreviewBegin( pSession, 0 ) == BK_EDITOR_REFUSED, "preview: a weapon has no preview" );
		Check( std::strstr( BkEditorLastMessage( pSession ), ".wpn" ) != 0, "preview: the refusal names the kind" );
		Check( BkResPreviewStop( pSession ) == BK_EDITOR_OK, "preview: Stop with none active is OK" );
	}

	const Capture kCaptures[] = {
		{ "mesh",     "msh", 6,  &MeshExporter },
		{ "sprite",   "spt", 4,  &SpriteExporter },
		{ "particle", "eff", 12, &EffectExporter },
	};
	for ( const Capture &capture : kCaptures )
	{
		const std::string szLabel = capture.pszLabel;
		const fs::path projectDir = scratch / capture.pszExt;
		fs::create_directories( projectDir, ec );
		const fs::path project = projectDir / ( std::string( "project." ) + capture.pszExt );
		fs::copy_file( fs::path( szFixtureRoot ) / capture.pszExt / project.filename(), project, fs::copy_options::overwrite_existing, ec );
		Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, ( "preview: opens the " + szLabel + " fixture" ).c_str() );
		Check( BkResPreviewBegin( pSession, capture.nKind ) == BK_EDITOR_OK, ( "preview: Begin " + szLabel ).c_str() );

		// No exporter yet: refused, and nothing is drawn.
		NResourceModel::RegisterExporter( capture.pszExt, nullptr );
		Check( BkResPreviewShow( pSession ) == BK_EDITOR_REFUSED, ( "preview: " + szLabel + " without an exporter is refused" ).c_str() );
		NResourceModel::RegisterExporter( capture.pszExt, &NamelessExporter );
		Check( BkResPreviewShow( pSession ) == BK_EDITOR_FAILED, ( "preview: " + szLabel + " export naming no visual fails" ).c_str() );

		const fs::path empty = scratch / ( szLabel + "-empty.tga" );
		Check( BkEditorCaptureFrame( pSession, empty.string().c_str() ) == BK_EDITOR_OK, ( "preview: the empty " + szLabel + " frame captures" ).c_str() );

		NResourceModel::RegisterExporter( capture.pszExt, capture.pfnExporter );
		const BkEditorStatus nShow = BkResPreviewShow( pSession );
		Check( nShow == BK_EDITOR_OK, ( "preview: Show " + szLabel + ": " + BkEditorLastMessage( pSession ) ).c_str() );
		Check( BkResPreviewPlayback( pSession, 1 ) == BK_EDITOR_OK, ( "preview: Run " + szLabel ).c_str() );
		// About a second of frames, so the effect's particles (which start
		// 200-800 ms in) and the sprite's animation have run.
		const auto start = std::chrono::steady_clock::now();
		while ( std::chrono::steady_clock::now() - start < std::chrono::milliseconds( 1000 ) )
			BkEditorFrame( pSession );
		const fs::path tga = scratch / ( szLabel + ".tga" );
		const BkEditorStatus nCapture = BkEditorCaptureFrame( pSession, tga.string().c_str() );
		const long long nMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - start ).count();
		Check( BkResPreviewPlayback( pSession, 0 ) == BK_EDITOR_OK, ( "preview: Stop playback " + szLabel ).c_str() );

		std::vector<unsigned char> emptyRgb, rgb, refRgb;
		int nW = 0, nH = 0, nRefW = 0, nRefH = 0;
		const bool bRead = nCapture == BK_EDITOR_OK && ReadCapture( tga.string(), rgb, nW, nH ) && ReadCapture( empty.string(), emptyRgb, nW, nH );
		Check( bRead, ( "preview: the " + szLabel + " capture reads back" ).c_str() );
		const double fShare = bRead ? NonBlackNonMagentaShare( rgb ) : -1.0;
		const double fChanged = bRead ? ChangedShare( rgb, emptyRgb ) : -1.0;
		// The committed capture of the same scene (preview-scene/<label>.tga),
		// for the record: another GPU or driver draws other pixels, so the
		// comparison is logged, not asserted.
		const fs::path reference = fs::path( szFixtureRoot ) / "preview-scene" / ( szLabel + ".tga" );
		const double fVsReference = bRead && ReadCapture( reference.string(), refRgb, nRefW, nRefH ) ? ChangedShare( rgb, refRgb ) : -1.0;
		Log( "preview-scene: " + szLabel
		   + " fixture=tools/zig/fixtures/resource_editor/" + capture.pszExt + "/project." + capture.pszExt
		   + " show_status=" + std::to_string( int( nShow ) )
		   + " capture_status=" + std::to_string( int( nCapture ) )
		   + " non-black-non-magenta=" + std::to_string( fShare )
		   + " changed-vs-empty=" + std::to_string( fChanged )
		   + " changed-vs-reference=" + std::to_string( fVsReference )
		   + " duration_ms=" + std::to_string( nMs )
		   + " path=" + tga.string() );
		Check( fShare >= 0.01, ( "preview: the " + szLabel + " capture is >= 1% non-black-non-magenta" ).c_str() );
		Check( fChanged >= 0.001, ( "preview: the " + szLabel + " object drew (>= 0.1% of the frame changed)" ).c_str() );

		Check( BkResPreviewCamera( pSession, 12 * 32.0f + 64.0f, 12 * 32.0f, 2 ) == BK_EDITOR_OK, ( "preview: Camera " + szLabel ).c_str() );
		NResourceModel::RegisterExporter( capture.pszExt, nullptr );
		Check( BkResPreviewStop( pSession ) == BK_EDITOR_OK, ( "preview: Stop " + szLabel ).c_str() );
		BkResClose( pSession );
	}
	Check( BkResPreviewCamera( pSession, 0, 0, 0 ) == BK_EDITOR_REFUSED, "preview: Camera after Stop is refused" );
	Check( PreviewFolders() == 0, "preview: Stop removes the preview folders" );

	// A preview begun for one kind does not show another kind's project.
	{
		const fs::path project = scratch / "spt" / "project.spt";
		Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "preview: reopens the sprite" );
		Check( BkResPreviewBegin( pSession, 6 ) == BK_EDITOR_OK, "preview: Begin mesh" );
		Check( BkResPreviewShow( pSession ) == BK_EDITOR_REFUSED, "preview: a mesh preview refuses a sprite project" );
		Check( BkResPreviewStop( pSession ) == BK_EDITOR_OK, "preview: Stop" );
		BkResClose( pSession );
	}
}

}

int main( int argc, char **argv )
{
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif

	// A real hidden window, never a null handle. SDL_WINDOW_NOT_FOCUSABLE
	// keeps a Linux compositor from stealing focus to a window the user
	// never asked for; SDL_WINDOW_HIDDEN keeps it off the taskbar. Both are
	// what the Map Editor's hidden tiers use (AGENTS.md: Linux pitfalls).
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( std::strstr( pszError, "video driver" ) != 0 || std::strstr( pszError, "No available" ) != 0 )
			return SkipOrFail( std::string( "no video driver (" ) + pszError + ")" );
		std::printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "resource-bridge-test", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE );
	if ( pWindow == 0 )
	{
		std::printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}

	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::string szFixtureRoot = argc > 2 ? argv[2] : ( szSelfDir + "/fixtures/resource_editor" );
	const std::string szScratchRoot = argc > 3 ? argv[3] : ( szSelfDir + "/local-test/resource_editor/t02" );

	FILE *pProbe = std::fopen( ( std::string( pszRoot ) + "/Data/consts.xml" ).c_str(), "rb" );
	if ( pProbe == 0 )
	{
		const int nSkipped = SkipOrFail( std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	std::fclose( pProbe );
	// Also need the fixtures. Without them the Project+Tree sub-step cannot run;
	// still a skip (not a fail) because this tier runs on hosts that only have
	// the staged install laid down.
	FILE *pFixProbe = std::fopen( ( szFixtureRoot + "/EXTENSIONS.md" ).c_str(), "rb" );
	if ( pFixProbe == 0 )
	{
		const int nSkipped = SkipOrFail( std::string( "no resource fixtures at " ) + szFixtureRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	std::fclose( pFixProbe );

	BkEditorSession *pSession = 0;
	const BkEditorStatus start = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( start == BK_EDITOR_NO_DEVICE )
	{
		const int nSkipped = SkipOrFail( std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( !Check( start == BK_EDITOR_OK, "the bridge starts" ) )
	{
		std::printf( "resource-bridge: %s\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	// Smoke: BkResNew works for a fresh wpn project, kind round-trips, close.
	Check( BkResNew( pSession, 0 ) == BK_EDITOR_OK, "BkResNew(wpn) answers OK" );
	BkResKind newKind = -1;
	Check( BkResKindOf( pSession, &newKind ) == BK_EDITOR_OK, "BkResKindOf after New answers OK" );
	Check( newKind == 0, "KindOf after BkResNew(wpn) is 0" );
	int nFreshCount = -1;
	Check( BkResNodes( pSession, 0, 0, &nFreshCount ) == BK_EDITOR_OK, "BkResNodes answers OK with a null buffer" );
	Check( nFreshCount == 1, "a fresh project exposes just the root node" );
	Check( BkResClose( pSession ) == BK_EDITOR_OK, "BkResClose answers OK" );

	// Round-trip every fixture.
	for ( int i = 0; i < kFixtureCount; ++i )
		RoundTripOne( pSession, szFixtureRoot, szScratchRoot, kFixtures[i] );
	SaveOverExisting( pSession, szFixtureRoot, szScratchRoot );

	// Delete->restore->save on three kinds whose fixture is an MFC item tree
	// (S03 T02); msh and pcp join when T03/T04 replace their stub fixtures.
	const char *pszRep[] = { "wpn", "trc", "unt" };
	for ( int r = 0; r < 3; ++r )
	{
		Fixture fx = {};
		for ( int i = 0; i < kFixtureCount; ++i )
			if ( std::strcmp( kFixtures[i].pszExt, pszRep[r] ) == 0 ) { fx = kFixtures[i]; break; }
		DeleteRestoreOne( pSession, szFixtureRoot, szScratchRoot, fx );
	}

	// MFC's lock (D-08): `locked_<user>` in the project's folder. Two
	// sessions play two users through the BK_RESOURCE_EDITOR_USER seam; the
	// second session is a bare BkEditorSession, which the data-only lock
	// entries accept as well as a started one.
	{
		const std::string szDir = szScratchRoot + "/lock";
		const std::string szProject = szDir + "/project.wpn";
		std::error_code ec;
		std::filesystem::remove_all( szDir, ec );
		std::filesystem::create_directories( szDir, ec );
		std::filesystem::copy_file( szFixtureRoot + "/wpn/project.wpn", szProject, ec );
		BkEditorSession *pOther = new BkEditorSession();

		SetLockUser( "alice" );
		Check( BkResOpen( pSession, szProject.c_str() ) == BK_EDITOR_OK, "lock: alice opens" );
		Check( BkResLock( pSession ) == BK_EDITOR_OK, "lock: alice locks" );
		Check( std::filesystem::exists( szDir + "/locked_alice", ec ), "lock: locked_alice is in the project's folder" );
		Check( !std::filesystem::exists( szProject + ".lock", ec ), "lock: no <path>.lock" );
		Check( BkResLock( pSession ) == BK_EDITOR_OK, "lock: alice's own lock is hers again, as in MFC" );
		char owner[256] = {};
		Check( BkResLockOwner( pSession, owner, (int)sizeof( owner ) ) == BK_EDITOR_OK && std::strcmp( owner, "alice" ) == 0,
			"lock: BkResLockOwner names alice" );

		SetLockUser( "bob" );
		Check( BkResOpen( pOther, szProject.c_str() ) == BK_EDITOR_OK, "lock: bob opens" );
		Check( BkResLock( pOther ) == BK_EDITOR_REFUSED, "lock: bob is refused while alice holds it" );
		Check( std::strstr( BkEditorLastMessage( pOther ), "alice" ) != 0, "lock: the refusal names alice" );
		Check( !std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: a refused lock leaves no file" );
		owner[0] = 0;
		Check( BkResLockOwner( pOther, owner, (int)sizeof( owner ) ) == BK_EDITOR_OK && std::strcmp( owner, "alice" ) == 0,
			"lock: bob sees alice as the owner" );
		Check( BkResLockTakeOver( pOther ) == BK_EDITOR_OK, "lock: bob takes the lock over" );
		Check( !std::filesystem::exists( szDir + "/locked_alice", ec ), "lock: the take-over removes locked_alice" );
		Check( std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: the take-over writes locked_bob" );
		Check( BkResClose( pOther ) == BK_EDITOR_OK, "lock: bob closes" );
		Check( !std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: close removes locked_bob" );

		SetLockUser( "alice" );
		Check( BkResClose( pSession ) == BK_EDITOR_OK, "lock: alice closes" );
		bool bStray = false;
		for ( const auto &entry : std::filesystem::directory_iterator( szDir, ec ) )
		{
			const std::string szName = entry.path().filename().string();
			if ( szName.compare( 0, 7, "locked_" ) == 0 || szName.find( ".lock" ) != std::string::npos )
				bStray = true;
		}
		Check( !bStray, "lock: no lock file is left behind" );
		SetLockUser( 0 );
		delete pOther;
	}

	// T05: cells family round-trip against bld. Writes passability, locked
	// tiles, and transparency lines on the root node of the bld fixture;
	// reads back; saves; reopens the saved copy; reads again; asserts the
	// values persisted byte-identically through NResourceModel::Load/Save.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		const std::string szOutDir = szScratchRoot + "/bld";
		const std::string szOut = szOutDir + "/project.cells.bld";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "cells: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1; // RebuildIds hands the root id 1.
			// A 3x2 passability grid with a recognisable byte pattern.
			const int nW = 3, nH = 2;
			unsigned char cells[6] = { 1, 2, 3, 4, 5, 6 };
			Check( BkResSetPassabilityCells( pSession, nRoot, cells, nW, nH ) == BK_EDITOR_OK,
			       "cells: BkResSetPassabilityCells" );

			int rw = 0, rh = 0;
			unsigned char read_cells[16] = {};
			Check( BkResGetPassabilityCells( pSession, nRoot, 0, 0, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetPassabilityCells size" );
			Check( rw == nW && rh == nH, "cells: passability w/h round-trip" );
			Check( BkResGetPassabilityCells( pSession, nRoot, read_cells, (int)sizeof( read_cells ), &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetPassabilityCells read" );
			Check( std::memcmp( read_cells, cells, 6 ) == 0, "cells: passability bytes round-trip" );

			// A 2x2 locked-tiles grid.
			unsigned char locked[4] = { 0, 1, 1, 0 };
			Check( BkResSetLockedTiles( pSession, nRoot, locked, 2, 2 ) == BK_EDITOR_OK,
			       "cells: BkResSetLockedTiles" );
			unsigned char read_locked[4] = {};
			Check( BkResGetLockedTiles( pSession, nRoot, read_locked, 4, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetLockedTiles" );
			Check( rw == 2 && rh == 2, "cells: locked w/h round-trip" );
			Check( std::memcmp( read_locked, locked, 4 ) == 0, "cells: locked bytes round-trip" );

			// A short transparency-lines list.
			BkResPoint2 lines[3] = { { 0.5f, 1.5f }, { 2.0f, 3.0f }, { 4.25f, 5.75f } };
			Check( BkResSetTransparencyLines( pSession, nRoot, lines, 3 ) == BK_EDITOR_OK,
			       "cells: BkResSetTransparencyLines" );
			int nLineCount = -1;
			Check( BkResGetTransparencyLines( pSession, nRoot, 0, 0, &nLineCount ) == BK_EDITOR_OK,
			       "cells: BkResGetTransparencyLines size" );
			Check( nLineCount == 3, "cells: transparency line count" );
			BkResPoint2 read_lines[3] = {};
			Check( BkResGetTransparencyLines( pSession, nRoot, read_lines, 3, &nLineCount ) == BK_EDITOR_OK,
			       "cells: BkResGetTransparencyLines fill" );
			bool bLinesOk = true;
			for ( int i = 0; i < 3; ++i )
				if ( read_lines[i].x != lines[i].x || read_lines[i].y != lines[i].y ) bLinesOk = false;
			Check( bLinesOk, "cells: transparency bytes round-trip" );

			// Save, reopen, re-read. The values must persist through XML.
			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "cells: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "cells: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "cells: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			std::memset( read_cells, 0, sizeof( read_cells ) );
			rw = 0; rh = 0;
			Check( BkResGetPassabilityCells( pSession, nRoot, read_cells, (int)sizeof( read_cells ), &rw, &rh ) == BK_EDITOR_OK,
			       "cells: re-read passability" );
			Check( rw == nW && rh == nH && std::memcmp( read_cells, cells, 6 ) == 0,
			       "cells: passability survives save+reopen" );

			std::memset( read_locked, 0, sizeof( read_locked ) );
			rw = 0; rh = 0;
			Check( BkResGetLockedTiles( pSession, nRoot, read_locked, 4, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: re-read locked" );
			Check( rw == 2 && rh == 2 && std::memcmp( read_locked, locked, 4 ) == 0,
			       "cells: locked tiles survive save+reopen" );

			std::memset( read_lines, 0, sizeof( read_lines ) );
			nLineCount = 0;
			Check( BkResGetTransparencyLines( pSession, nRoot, read_lines, 3, &nLineCount ) == BK_EDITOR_OK,
			       "cells: re-read transparency" );
			bLinesOk = ( nLineCount == 3 );
			for ( int i = 0; i < 3 && bLinesOk; ++i )
				if ( read_lines[i].x != lines[i].x || read_lines[i].y != lines[i].y ) bLinesOk = false;
			Check( bLinesOk, "cells: transparency lines survive save+reopen" );

			BkResClose( pSession );
		}
	}

	// T06: points + aimed-points family round-trip against bld (zero point,
	// entrance, shoot points) and scp (shoot points). Set each channel on the
	// root node of the fixture, read back, save, reopen, read again. The
	// values must round-trip through the `_bk_geometry` persistence layer.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		const std::string szOutDir = szScratchRoot + "/bld";
		const std::string szOut = szOutDir + "/project.points.bld";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "points: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;

			// 1.2345678f needs nine significant digits: a six-digit %g would lose it.
			BkResPoint2 zero = { 1.2345678f, -2.5f };
			Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "points: BkResSetZeroPoint" );
			BkResPoint2 read_zero = { 0, 0 };
			Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK, "points: BkResGetZeroPoint" );
			Check( read_zero.x == zero.x && read_zero.y == zero.y, "points: zero point round-trip" );

			BkResPoint2 entrance = { 7.0f, 11.0f };
			Check( BkResSetEntrance( pSession, nRoot, &entrance ) == BK_EDITOR_OK, "points: BkResSetEntrance" );
			BkResPoint2 read_entrance = { 0, 0 };
			Check( BkResGetEntrance( pSession, nRoot, &read_entrance ) == BK_EDITOR_OK, "points: BkResGetEntrance" );
			Check( read_entrance.x == entrance.x && read_entrance.y == entrance.y, "points: entrance round-trip" );

			BkResAimedPoint shoots[2] = {
				{ { 0.5f, 1.5f }, 90, 15 },
				{ { 3.0f, 4.0f }, 180, 30 },
			};
			Check( BkResSetShootPoints( pSession, nRoot, shoots, 2 ) == BK_EDITOR_OK, "points: BkResSetShootPoints" );
			int nShootCount = -1;
			Check( BkResGetShootPoints( pSession, nRoot, 0, 0, &nShootCount ) == BK_EDITOR_OK, "points: BkResGetShootPoints size" );
			Check( nShootCount == 2, "points: shoot count" );
			BkResAimedPoint read_shoots[2] = {};
			Check( BkResGetShootPoints( pSession, nRoot, read_shoots, 2, &nShootCount ) == BK_EDITOR_OK, "points: BkResGetShootPoints fill" );
			bool bShootOk = true;
			for ( int i = 0; i < 2; ++i )
				if ( read_shoots[i].at.x != shoots[i].at.x || read_shoots[i].at.y != shoots[i].at.y
					|| read_shoots[i].angle != shoots[i].angle || read_shoots[i].cone != shoots[i].cone )
					bShootOk = false;
			Check( bShootOk, "points: shoot points round-trip" );

			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "points: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			read_zero = { 0, 0 };
			Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK, "points: re-read zero" );
			Check( read_zero.x == zero.x && read_zero.y == zero.y, "points: zero point survives save+reopen" );

			read_entrance = { 0, 0 };
			Check( BkResGetEntrance( pSession, nRoot, &read_entrance ) == BK_EDITOR_OK, "points: re-read entrance" );
			Check( read_entrance.x == entrance.x && read_entrance.y == entrance.y, "points: entrance survives save+reopen" );

			std::memset( read_shoots, 0, sizeof( read_shoots ) );
			nShootCount = -1;
			Check( BkResGetShootPoints( pSession, nRoot, read_shoots, 2, &nShootCount ) == BK_EDITOR_OK, "points: re-read shoot" );
			bShootOk = ( nShootCount == 2 );
			for ( int i = 0; i < 2 && bShootOk; ++i )
				if ( read_shoots[i].at.x != shoots[i].at.x || read_shoots[i].at.y != shoots[i].at.y
					|| read_shoots[i].angle != shoots[i].angle || read_shoots[i].cone != shoots[i].cone )
					bShootOk = false;
			Check( bShootOk, "points: shoot points survive save+reopen" );

			BkResClose( pSession );
		}
	}

	// T06: same aimed-points shape against scp (squad). The squad item class
	// doesn't carry zero/entrance in a general sense; shoot_points stands in
	// as a representative aimed channel so an scp fixture is covered too.
	{
		const std::string szIn = szFixtureRoot + "/scp/project.scp";
		const std::string szOutDir = szScratchRoot + "/scp";
		const std::string szOut = szOutDir + "/project.points.scp";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "points-scp: BkResOpen scp" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;
			BkResAimedPoint fires[1] = { { { 2.0f, 6.0f }, 45, 10 } };
			Check( BkResSetFirePoints( pSession, nRoot, fires, 1 ) == BK_EDITOR_OK, "points-scp: BkResSetFirePoints" );
			int nFireCount = -1;
			BkResAimedPoint read_fires[1] = {};
			Check( BkResGetFirePoints( pSession, nRoot, read_fires, 1, &nFireCount ) == BK_EDITOR_OK, "points-scp: BkResGetFirePoints" );
			Check( nFireCount == 1 && read_fires[0].at.x == fires[0].at.x && read_fires[0].at.y == fires[0].at.y
				&& read_fires[0].angle == fires[0].angle && read_fires[0].cone == fires[0].cone,
				"points-scp: fire points round-trip" );

			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points-scp: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "points-scp: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points-scp: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			std::memset( read_fires, 0, sizeof( read_fires ) );
			nFireCount = -1;
			Check( BkResGetFirePoints( pSession, nRoot, read_fires, 1, &nFireCount ) == BK_EDITOR_OK, "points-scp: re-read fire" );
			Check( nFireCount == 1 && read_fires[0].at.x == fires[0].at.x && read_fires[0].at.y == fires[0].at.y
				&& read_fires[0].angle == fires[0].angle && read_fires[0].cone == fires[0].cone,
				"points-scp: fire points survive save+reopen" );
			BkResClose( pSession );
		}
	}

	// Node ids are stable: a delete or restore elsewhere in the tree leaves
	// every other id - and the geometry keyed by it - where it was, and a
	// restored node gets its old id back, so the undo history stays valid.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "ids: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;
			BkResPoint2 zero = { 3.0f, 4.0f };
			Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "ids: BkResSetZeroPoint" );
			// The stub fixture's <fixture> element is frame data outside the
			// childs list, so the root has no child nodes; give it two to
			// delete around.
			const char szExtra[] = "<extra/>";
			int nExtra = 0;
			Check( BkResRestoreNode( pSession, reinterpret_cast<const unsigned char *>( szExtra ), int( sizeof( szExtra ) - 1 ), nRoot, 0, &nExtra ) == BK_EDITOR_OK,
				"ids: add a first child" );
			Check( BkResRestoreNode( pSession, reinterpret_cast<const unsigned char *>( szExtra ), int( sizeof( szExtra ) - 1 ), nRoot, 1, &nExtra ) == BK_EDITOR_OK,
				"ids: add a second child" );
			int nCount = 0;
			BkResNodes( pSession, 0, 0, &nCount );
			std::vector<BkResNodeRecord> nodes( nCount > 0 ? nCount : 1 );
			BkResNodes( pSession, nodes.data(), nCount, &nCount );
			std::vector<int> children;
			for ( int i = 0; i < nCount; ++i )
				if ( nodes[i].parent == nRoot )
					children.push_back( nodes[i].id );
			if ( Check( children.size() >= 2, "ids: the fixture root has two children" ) )
			{
				const int nFirst = children[0], nSecond = children[1];
				int nBlobSize = 0;
				BkResDeleteNode( pSession, nFirst, 0, 0, &nBlobSize );
				std::vector<unsigned char> blob( nBlobSize > 0 ? nBlobSize : 1 );
				Check( BkResDeleteNode( pSession, nFirst, blob.data(), nBlobSize, &nBlobSize ) == BK_EDITOR_OK, "ids: BkResDeleteNode" );
				int nAfter = 0;
				BkResNodes( pSession, 0, 0, &nAfter );
				Check( nAfter == nCount - 1, "ids: the delete removed exactly one node" );
				std::vector<BkResNodeRecord> after( nAfter > 0 ? nAfter : 1 );
				BkResNodes( pSession, after.data(), nAfter, &nAfter );
				bool bSecondKept = false, bFirstGone = true;
				for ( int i = 0; i < nAfter; ++i )
				{
					if ( after[i].id == nSecond && after[i].parent == nRoot ) bSecondKept = true;
					if ( after[i].id == nFirst ) bFirstGone = false;
				}
				Check( bSecondKept, "ids: the surviving sibling keeps its id" );
				Check( bFirstGone, "ids: the deleted id is gone" );
				BkResPoint2 read_zero = { 0, 0 };
				Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK && read_zero.x == zero.x && read_zero.y == zero.y,
					"ids: the root's geometry survives a child delete" );
				int nRestored = 0;
				Check( BkResRestoreNode( pSession, blob.data(), nBlobSize, nRoot, 0, &nRestored ) == BK_EDITOR_OK, "ids: BkResRestoreNode" );
				Check( nRestored == nFirst, "ids: a restored node gets its old id back" );
				BkResNodes( pSession, after.data(), 0, &nAfter );
				Check( nAfter == nCount, "ids: the restore brought the node count back" );
			}
			BkResClose( pSession );
		}
		int nClosedCount = -1;
		Check( BkResNodes( pSession, 0, 0, &nClosedCount ) == BK_EDITOR_REFUSED && nClosedCount == 0, "BkResNodes refuses when no project is open" );
	}

	// Geometry on a node below the root is saved in that node's own element,
	// comes back on reopen, and travels with the node through delete -> restore.
	GeometryOnChildNodes( pSession, szFixtureRoot, szScratchRoot );

	// The squad editor's formation slots and the bridge editor's span marks,
	// on the nodes MFC keeps them on.
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "scp", "formation", BkResGetFormationPositions,
		BkResSetFormationPositions, { kSquadFormationProps } );
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "bdg", "span-marks", BkResGetBridgeSpanMarks,
		BkResSetBridgeSpanMarks, { kBridgeBeginSpans, kBridgeCenterSpans, kBridgeEndSpans } );
	// Map crosses on missions, chapters and campaigns, and the particle and
	// effect keyframe lists.
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "mip", "objectives", BkResGetMissionObjectives,
		BkResSetMissionObjectives, { kMissionObjectives } );
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "chc", "chapter-crosses", BkResGetChapterCrosses,
		BkResSetChapterCrosses, { kChapterMissions, kChapterPlaces } );
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "cgc", "campaign-crosses", BkResGetCampaignCrosses,
		BkResSetCampaignCrosses, { kCampaignChapters } );
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "pcp", "particle-keyframes", BkResGetParticleKeyframes,
		BkResSetParticleKeyframes, { kParticleDensity, kParticleSpeed } );
	PointListsOnOwnerNodes( pSession, szFixtureRoot, szScratchRoot, "eff", "effect-keyframes", BkResGetEffectKeyframes,
		BkResSetEffectKeyframes, { kEffectAnimations } );

	T10::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );

	ExportRollback::Run( pSession, szFixtureRoot, szScratchRoot );

	T11::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );

	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "the bridge stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();

	if ( g_nFailures == 0 )
		std::printf( "resource-bridge: Project+Tree OK (%d fixtures)\n", kFixtureCount );
	else
		std::printf( "resource-bridge: %d failures\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
