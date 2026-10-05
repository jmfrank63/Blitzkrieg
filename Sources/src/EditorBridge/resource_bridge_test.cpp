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
#include <functional>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "resource_bridge.h"
#include "bridge_session.h"
#include "../ResourceModel/references.h"
#include "../ResourceModel/exporter.h"
#include "../ResourceModel/compose.h"
#include "../ResourceModel/dxt_gate.h"
#include "../ResourceModel/image_export.h"
#include "../ResourceModel/comparator.h"
#include "../ResourceModel/project.h"
#include "../ResourceModel/items/squad/squad.h"
#include "../ResourceModel/items/fence/fence.h"
#include "../ResourceModel/key_frame_tree_item.h"
#include "../Main/GameStats.h"
#include "../Main/RPGStats.h"
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

// D014 item 2: the cells, point and aimed channels live where MFC keeps
// them (the table in the phase 6 spec's geometry section), so these tests
// read the saved file back the way MFC does: the building and object frame
// chunks through the engine's own CDataTreeXML and RPG stats structs
// (CBuildingFrame / CObjectFrame LoadRPGStats and LoadFrameOwnData), the
// item-owned ones through NResourceModel::Load and the S03 items.

// The class types of the nodes the MFC homes hang on (tree_item_types.h).
static const int kObjectRoot   = 0x11000000 + 51;
static const int kBuildingRoot = 0x11000000 + 91;
static const int kFenceProps   = 0x11000000 + 125;

// CObjectFrame::STransLine::operator& (ObjectFrm.cpp).
struct STestTransLine
{
	CVec2 p1, p2;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Point1", &p1 );
		saver.Add( "Point2", &p2 );
		return 0;
	}
};

// What MFC's LoadRPGStats and LoadFrameOwnData read from a project: the
// "desc" stats under the *_Composer_Project base node, and krest_pos (and,
// for an object, TransLines) in own_data.
template <class TStats>
static bool ReadAsMfc( const std::string &szFile, const char *pszBase, TStats &stats, CVec3 &krest,
                       std::vector<STestTransLine> *pLines )
{
	const std::string::size_type nCut = szFile.find_last_of( "/\\" );
	const std::string szDir = szFile.substr( 0, nCut + 1 );
	const std::string szName = szFile.substr( nCut + 1 );
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return false;
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ, pszBase );
	if ( pDT == 0 )
		return false;
	{
		CTreeAccessor tree = pDT;
		tree.Add( "desc", &stats );
	}
	if ( pDT->StartChunk( "own_data" ) == 0 )
		return false;
	{
		CTreeAccessor tree = pDT;
		tree.Add( "krest_pos", &krest );
		if ( pLines != 0 )
			tree.Add( "TransLines", pLines );
	}
	pDT->FinishChunk();
	return true;
}

static bool SameAimedList( BkResSession *pSession,
                           BkEditorStatus ( *pGet )( BkResSession *, int, BkResAimedPoint *, int, int * ),
                           int nNode, const std::vector<BkResAimedPoint> &want )
{
	std::vector<BkResAimedPoint> got( want.size() + 1 );
	int n = -1;
	if ( pGet( pSession, nNode, got.data(), int( got.size() ), &n ) != BK_EDITOR_OK || n != int( want.size() ) )
		return false;
	for ( size_t i = 0; i < want.size(); ++i )
		if ( got[i].at.x != want[i].at.x || got[i].at.y != want[i].at.y || got[i].angle != want[i].angle || got[i].cone != want[i].cone )
			return false;
	return true;
}

// The element name the port used for geometry before S05 put every channel
// where MFC keeps it. It is spelt in two parts so the source tree holds no
// copy of it (the slice's verification greps for one).
static const std::string kRetiredGeometryTag = std::string( "_bk" ) + "_geometry";

static bool HasNoPrivateGeometry( const std::string &szFile )
{
	std::string szBytes;
	return ReadBytes( szFile, szBytes ) && !szBytes.empty() && szBytes.find( kRetiredGeometryTag ) == std::string::npos;
}

static int FirstNodeOfType( BkResSession *pSession, int nType )
{
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.class_type == nType )
			return n.id;
	return 0;
}

static const NResourceModel::CTreeItem *FindItemOfType( const NResourceModel::CTreeItem &item, int nType )
{
	if ( item.GetItemType() == nType )
		return &item;
	for ( const auto &pChild : item.GetChildren() )
		if ( const NResourceModel::CTreeItem *p = FindItemOfType( *pChild, nType ) )
			return p;
	return 0;
}

// Opens szFile through the S03 reader and returns the first item of nType.
static const NResourceModel::CTreeItem *LoadItemOfType( const std::string &szFile, NResourceModel::Project &project, int nType )
{
	std::string szBytes, szError;
	if ( !ReadBytes( szFile, szBytes ) || !NResourceModel::Load( szBytes, project, szError ) || !project.root )
		return 0;
	return FindItemOfType( *project.root, nType );
}

// Deletes the node, restores it at the same place and checks a save after
// that equals a save before it.
static void DeleteRestoreKeepsBytes( BkResSession *pSession, int nNode, const std::string &szDir, const std::string &szExt,
                                     const std::string &szTag )
{
	const std::string szBefore = szDir + "/before-delete." + szExt;
	const std::string szAfter = szDir + "/after-restore." + szExt;
	Check( BkResSave( pSession, szBefore.c_str() ) == BK_EDITOR_OK, ( szTag + "save before delete" ).c_str() );
	int nParent = 0, nIndex = 0;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.id == nNode )
			nParent = n.parent;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
	{
		if ( n.id == nNode )
			break;
		if ( n.parent == nParent )
			++nIndex;
	}
	int nSize = 0;
	BkResDeleteNode( pSession, nNode, 0, 0, &nSize );
	std::vector<unsigned char> blob( nSize > 0 ? nSize : 1 );
	Check( BkResDeleteNode( pSession, nNode, blob.data(), nSize, &nSize ) == BK_EDITOR_OK, ( szTag + "delete" ).c_str() );
	int nRestored = 0;
	Check( BkResRestoreNode( pSession, blob.data(), nSize, nParent, nIndex, &nRestored ) == BK_EDITOR_OK && nRestored == nNode,
		( szTag + "restore under the old id" ).c_str() );
	Check( BkResSave( pSession, szAfter.c_str() ) == BK_EDITOR_OK, ( szTag + "save after restore" ).c_str() );
	std::string szA, szB;
	ReadBytes( szBefore, szA );
	ReadBytes( szAfter, szB );
	if ( !Check( !szA.empty() && szA == szB, ( szTag + "delete -> restore -> save is byte-identical" ).c_str() ) )
		std::printf( "   before=%zu bytes, after=%zu bytes\n", szA.size(), szB.size() );
	Check( HasNoPrivateGeometry( szAfter ), ( szTag + "no private geometry element after delete -> restore" ).c_str() );
}

// Values with at most six significant digits: MFC writes floats with %lg,
// so that is what a project file can carry.
static void BuildingGeometryInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	const std::string szIn = szFixtureRoot + "/bld/project.bld";
	const std::string szDir = szScratchRoot + "/mfc-geometry-bld";
	const std::string szSaved = szDir + "/project.bld";
	const std::string szResaved = szDir + "/project.resaved.bld";
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "bld-geometry: BkResOpen" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	const int nRoot = FirstNodeOfType( pSession, kBuildingRoot );
	Check( nRoot == 1, "bld-geometry: the root is the building root" );
	const unsigned char cells[6] = { 1, 0, 1, 2, 3, 0xff };
	const BkResPoint2 zero = { 724.5f, 362.25f };
	const BkResPoint2 entrance = { 12.5f, -3.25f };
	const std::vector<BkResAimedPoint> shoots = { { { -2.25f, 35.5f }, 270, 160 }, { { 7.75f, 89.125f }, 90, 30 } };
	const std::vector<BkResAimedPoint> fires = { { { 1.5f, 2.5f }, 45, 78 } };
	const std::vector<BkResAimedPoint> smokes = { { { -4.0f, 8.0f }, 180, 60 }, { { 0.0f, 0.5f }, 0, 78 } };
	const std::vector<BkResAimedPoint> explosions = { { { 3.0f, -1.0f }, 315, 45 }, { { 6.0f, 2.0f }, 135, 20 } };
	Check( BkResSetPassabilityCells( pSession, nRoot, cells, 3, 2 ) == BK_EDITOR_OK, "bld-geometry: set passability" );
	Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "bld-geometry: set zero point" );
	Check( BkResSetEntrance( pSession, nRoot, &entrance ) == BK_EDITOR_OK, "bld-geometry: set entrance" );
	Check( BkResSetShootPoints( pSession, nRoot, shoots.data(), int( shoots.size() ) ) == BK_EDITOR_OK, "bld-geometry: set shoot points" );
	Check( BkResSetFirePoints( pSession, nRoot, fires.data(), int( fires.size() ) ) == BK_EDITOR_OK, "bld-geometry: set fire points" );
	Check( BkResSetSmokePoints( pSession, nRoot, smokes.data(), int( smokes.size() ) ) == BK_EDITOR_OK, "bld-geometry: set smoke points" );
	Check( BkResSetDirectedExplosionPoints( pSession, nRoot, explosions.data(), int( explosions.size() ) ) == BK_EDITOR_OK,
		"bld-geometry: set directed explosions" );
	// The building frame keeps no locked-tiles list of its own (it is saved
	// as desc passability) and no transparency lines.
	const unsigned char locked[1] = { 1 };
	Check( BkResSetLockedTiles( pSession, nRoot, locked, 1, 1 ) == BK_EDITOR_REFUSED, "bld-geometry: locked tiles have no home on a building" );
	const BkResPoint2 line[2] = { { 0, 0 }, { 1, 1 } };
	Check( BkResSetTransparencyLines( pSession, nRoot, line, 2 ) == BK_EDITOR_REFUSED, "bld-geometry: transparency lines have no home on a building" );
	const int nChild = FirstNodeOfType( pSession, 0x11000000 + 92 );	// Basic Info, below the root
	Check( nChild != 0 && BkResSetZeroPoint( pSession, nChild, &zero ) == BK_EDITOR_REFUSED, "bld-geometry: a non-root building node has no zero point" );

	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "bld-geometry: save" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	Check( HasNoPrivateGeometry( szSaved ), "bld-geometry: the saved XML has no private geometry element" );

	// MFC's reader: CBuildingFrame::LoadRPGStats / LoadFrameOwnData.
	SBuildingRPGStats stats;
	CVec3 krest( 0, 0, 0 );
	if ( Check( ReadAsMfc( szSaved, "Building_Composer_Project", stats, krest, 0 ), "bld-geometry: the engine reads desc and own_data" ) )
	{
		bool bCells = stats.passability.GetSizeX() == 3 && stats.passability.GetSizeY() == 2;
		for ( int i = 0; bCells && i < 6; ++i )
			bCells = stats.passability[i / 3][i % 3] == cells[i];
		Check( bCells, "bld-geometry: desc passability is the set grid" );
		Check( krest.x == zero.x && krest.y == zero.y, "bld-geometry: own_data krest_pos is the zero point" );
		Check( stats.entrances.size() == 1 && stats.entrances[0].vPos.x == entrance.x && stats.entrances[0].vPos.y == entrance.y,
			"bld-geometry: desc Entrances[0] is the entrance" );
		bool bSlots = stats.slots.size() == shoots.size();
		for ( size_t i = 0; bSlots && i < shoots.size(); ++i )
			bSlots = stats.slots[i].vPos.x == shoots[i].at.x && stats.slots[i].vPos.y == shoots[i].at.y
				&& stats.slots[i].fDirection == float( shoots[i].angle ) && stats.slots[i].fAngle == float( shoots[i].cone );
		Check( bSlots, "bld-geometry: desc FireSlots carry position, direction and cone angle" );
		auto SameFire = []( const std::vector<SBuildingRPGStats::SFirePoint> &got, const std::vector<BkResAimedPoint> &want )
		{
			if ( got.size() != want.size() )
				return false;
			for ( size_t i = 0; i < want.size(); ++i )
				if ( got[i].vPos.x != want[i].at.x || got[i].vPos.y != want[i].at.y
					|| got[i].fDirection != float( want[i].angle ) || got[i].fVerticalAngle != float( want[i].cone ) )
					return false;
			return true;
		};
		Check( SameFire( stats.firePoints, fires ), "bld-geometry: desc FirePoints carry position, direction and vertical angle" );
		Check( SameFire( stats.smokePoints, smokes ), "bld-geometry: desc SmokePoints carry position, direction and vertical angle" );
		bool bExp = stats.dirExplosions.size() == explosions.size();
		for ( size_t i = 0; bExp && i < explosions.size(); ++i )
			bExp = stats.dirExplosions[i].vPos.x == explosions[i].at.x && stats.dirExplosions[i].vPos.y == explosions[i].at.y
				&& stats.dirExplosions[i].fDirection == float( explosions[i].angle )
				&& stats.dirExplosions[i].fVerticalAngle == float( explosions[i].cone );
		Check( bExp, "bld-geometry: desc DirExplosions carry position, direction and vertical angle" );
	}

	BkResClose( pSession );
	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "bld-geometry: reopen" ) )
		return;
	Check( SameCells( pSession, nRoot, cells, 3, 2 ), "bld-geometry: passability survives save+reopen" );
	Check( SameZero( pSession, nRoot, zero ), "bld-geometry: zero point survives save+reopen" );
	BkResPoint2 gotEntrance = { 0, 0 };
	Check( BkResGetEntrance( pSession, nRoot, &gotEntrance ) == BK_EDITOR_OK && gotEntrance.x == entrance.x && gotEntrance.y == entrance.y,
		"bld-geometry: entrance survives save+reopen" );
	Check( SameAimedList( pSession, BkResGetShootPoints, nRoot, shoots ), "bld-geometry: shoot points survive save+reopen" );
	Check( SameAimedList( pSession, BkResGetFirePoints, nRoot, fires ), "bld-geometry: fire points survive save+reopen" );
	Check( SameAimedList( pSession, BkResGetSmokePoints, nRoot, smokes ), "bld-geometry: smoke points survive save+reopen" );
	Check( SameAimedList( pSession, BkResGetDirectedExplosionPoints, nRoot, explosions ), "bld-geometry: directed explosions survive save+reopen" );
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "bld-geometry: save the reopened project" );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, "bld-geometry: open -> save of the project with geometry is byte-identical" );

	// A shorter list drops the tail, an empty one empties the chunk.
	Check( BkResSetShootPoints( pSession, nRoot, shoots.data(), 1 ) == BK_EDITOR_OK, "bld-geometry: shorten the shoot points" );
	Check( BkResSetSmokePoints( pSession, nRoot, 0, 0 ) == BK_EDITOR_OK, "bld-geometry: clear the smoke points" );
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "bld-geometry: save the shortened lists" );
	BkResClose( pSession );
	Check( BkResOpen( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "bld-geometry: reopen the shortened lists" );
	Check( SameAimedList( pSession, BkResGetShootPoints, nRoot, { shoots[0] } ), "bld-geometry: the shortened shoot list survives" );
	Check( SameAimedList( pSession, BkResGetSmokePoints, nRoot, {} ), "bld-geometry: the cleared smoke list survives" );
	Check( SameAimedList( pSession, BkResGetFirePoints, nRoot, fires ), "bld-geometry: the untouched fire list is kept" );

	// The frame data is the root's; deleting and restoring a child leaves it.
	DeleteRestoreKeepsBytes( pSession, nChild, szDir, "bld", "bld-geometry: " );
	BkResClose( pSession );
}

static void ObjectGeometryInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	const std::string szIn = szFixtureRoot + "/obt/project.obt";
	const std::string szDir = szScratchRoot + "/mfc-geometry-obt";
	const std::string szSaved = szDir + "/project.obt";
	const std::string szResaved = szDir + "/project.resaved.obt";
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "obt-geometry: BkResOpen" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	const int nRoot = FirstNodeOfType( pSession, kObjectRoot );
	Check( nRoot == 1, "obt-geometry: the root is the object root" );
	const unsigned char cells[4] = { 0, 7, 1, 0 };
	const BkResPoint2 zero = { 512.25f, 256.5f };
	// Two lines: a transparency line is a pair of points.
	const std::vector<BkResPoint2> lines = { { 10.5f, 20.25f }, { 30.0f, 40.75f }, { -5.0f, 6.5f }, { 7.0f, -8.125f } };
	Check( BkResSetPassabilityCells( pSession, nRoot, cells, 2, 2 ) == BK_EDITOR_OK, "obt-geometry: set passability" );
	Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "obt-geometry: set zero point" );
	Check( BkResSetTransparencyLines( pSession, nRoot, lines.data(), int( lines.size() ) ) == BK_EDITOR_OK, "obt-geometry: set transparency lines" );
	Check( BkResSetTransparencyLines( pSession, nRoot, lines.data(), 3 ) == BK_EDITOR_BAD_ARGUMENT, "obt-geometry: an odd point count is a bad argument" );
	const BkResAimedPoint shoot = { { 1, 1 }, 0, 0 };
	Check( BkResSetShootPoints( pSession, nRoot, &shoot, 1 ) == BK_EDITOR_REFUSED, "obt-geometry: an object has no shoot points" );
	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "obt-geometry: save" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	Check( HasNoPrivateGeometry( szSaved ), "obt-geometry: the saved XML has no private geometry element" );

	// MFC's reader: CObjectFrame::LoadRPGStats / LoadFrameOwnData.
	SObjectRPGStats stats;
	CVec3 krest( 0, 0, 0 );
	std::vector<STestTransLine> transLines;
	if ( Check( ReadAsMfc( szSaved, "Object_Composer_Project", stats, krest, &transLines ), "obt-geometry: the engine reads desc and own_data" ) )
	{
		bool bCells = stats.passability.GetSizeX() == 2 && stats.passability.GetSizeY() == 2;
		for ( int i = 0; bCells && i < 4; ++i )
			bCells = stats.passability[i / 2][i % 2] == cells[i];
		Check( bCells, "obt-geometry: desc passability is the set grid" );
		Check( krest.x == zero.x && krest.y == zero.y, "obt-geometry: own_data krest_pos is the zero point" );
		bool bLines = transLines.size() == 2;
		for ( size_t i = 0; bLines && i < 2; ++i )
			bLines = transLines[i].p1.x == lines[2 * i].x && transLines[i].p1.y == lines[2 * i].y
				&& transLines[i].p2.x == lines[2 * i + 1].x && transLines[i].p2.y == lines[2 * i + 1].y;
		Check( bLines, "obt-geometry: own_data TransLines hold the point pairs" );
	}

	BkResClose( pSession );
	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "obt-geometry: reopen" ) )
		return;
	Check( SameCells( pSession, nRoot, cells, 2, 2 ), "obt-geometry: passability survives save+reopen" );
	Check( SameZero( pSession, nRoot, zero ), "obt-geometry: zero point survives save+reopen" );
	Check( SameList<BkResPoint2>( pSession, BkResGetTransparencyLines, nRoot, lines ), "obt-geometry: transparency lines survive save+reopen" );
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "obt-geometry: save the reopened project" );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, "obt-geometry: open -> save of the project with geometry is byte-identical" );
	int nChild = 0;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.parent == nRoot ) { nChild = n.id; break; }
	if ( Check( nChild != 0, "obt-geometry: the root has a child" ) )
		DeleteRestoreKeepsBytes( pSession, nChild, szDir, "obt", "obt-geometry: " );
	BkResClose( pSession );
}

// Below the root: a squad formation's zero point (CSquadFormationPropsItem
// ZeroPos) and a fence segment's locked tiles (CFencePropsItem LockedTiles).
static void ItemGeometryInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	{
		const std::string szIn = szFixtureRoot + "/scp/project.scp";
		const std::string szDir = szScratchRoot + "/mfc-geometry-scp";
		const std::string szSaved = szDir + "/project.scp";
		std::error_code ec;
		std::filesystem::remove_all( szDir, ec );
		std::filesystem::create_directories( szDir, ec );
		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "scp-geometry: BkResOpen" ) )
			return;
		const int nFormation = FirstNodeOfType( pSession, kSquadFormationProps );
		BkResPoint2 read = { 0, 0 };
		Check( nFormation > 1 && BkResGetZeroPoint( pSession, nFormation, &read ) == BK_EDITOR_OK
			&& read.x == 724.077f && read.y == 362.039f, "scp-geometry: the formation's zero point reads the fixture's ZeroPos" );
		const BkResPoint2 zero = { 600.5f, 300.25f };
		Check( BkResSetZeroPoint( pSession, nFormation, &zero ) == BK_EDITOR_OK, "scp-geometry: set the formation's zero point" );
		Check( BkResSetZeroPoint( pSession, 1, &zero ) == BK_EDITOR_REFUSED, "scp-geometry: the squad root has no zero point" );
		const BkResAimedPoint fire = { { 2, 6 }, 45, 10 };
		Check( BkResSetFirePoints( pSession, 1, &fire, 1 ) == BK_EDITOR_REFUSED, "scp-geometry: a squad has no fire points" );
		if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "scp-geometry: save" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		Check( HasNoPrivateGeometry( szSaved ), "scp-geometry: the saved XML has no private geometry element" );
		NResourceModel::Project project;
		const auto *pItem = dynamic_cast<const NResourceModel::CSquadFormationPropsItem *>( LoadItemOfType( szSaved, project, kSquadFormationProps ) );
		Check( pItem != 0 && pItem->vZeroPos.x == zero.x && pItem->vZeroPos.y == zero.y && pItem->vZeroPos.z == 0,
			"scp-geometry: the S03 formation item reads the zero point as its ZeroPos" );
		BkResClose( pSession );
		Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "scp-geometry: reopen" );
		Check( SameZero( pSession, nFormation, zero ), "scp-geometry: the formation's zero point survives save+reopen" );
		DeleteRestoreKeepsBytes( pSession, nFormation, szDir, "scp", "scp-geometry: " );
		BkResClose( pSession );
	}
	{
		const std::string szIn = szFixtureRoot + "/fnc/project.fnc";
		const std::string szDir = szScratchRoot + "/mfc-geometry-fnc";
		const std::string szSaved = szDir + "/project.fnc";
		std::error_code ec;
		std::filesystem::remove_all( szDir, ec );
		std::filesystem::create_directories( szDir, ec );
		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "fnc-geometry: BkResOpen" ) )
			return;
		const int nProps = FirstNodeOfType( pSession, kFenceProps );
		// MFC stores only the tiles that are set, so the grid's last row and
		// column carry one each: a read grid ends at the furthest set tile.
		const unsigned char locked[6] = { 0, 1, 0, 2, 0, 1 };
		Check( nProps > 1 && BkResSetLockedTiles( pSession, nProps, locked, 3, 2 ) == BK_EDITOR_OK, "fnc-geometry: set a segment's locked tiles" );
		Check( BkResSetPassabilityCells( pSession, nProps, locked, 3, 2 ) == BK_EDITOR_REFUSED, "fnc-geometry: a fence segment has no passability grid" );
		if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "fnc-geometry: save" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		Check( HasNoPrivateGeometry( szSaved ), "fnc-geometry: the saved XML has no private geometry element" );
		NResourceModel::Project project;
		const auto *pItem = dynamic_cast<const NResourceModel::CFencePropsItem *>( LoadItemOfType( szSaved, project, kFenceProps ) );
		bool bTiles = pItem != 0 && pItem->lockedTiles.size() == 3;
		if ( bTiles )
		{
			const int want[3][3] = { { 1, 0, 1 }, { 0, 1, 2 }, { 2, 1, 1 } };
			int i = 0;
			for ( const NResourceModel::SAITile &tile : pItem->lockedTiles )
			{
				bTiles = bTiles && tile.nTileX == want[i][0] && tile.nTileY == want[i][1] && tile.nVal == want[i][2];
				++i;
			}
		}
		Check( bTiles, "fnc-geometry: the S03 fence item reads the grid as its LockedTiles" );
		BkResClose( pSession );
		Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "fnc-geometry: reopen" );
		unsigned char got[16] = {};
		int w = 0, h = 0;
		Check( BkResGetLockedTiles( pSession, nProps, got, (int)sizeof( got ), &w, &h ) == BK_EDITOR_OK && w == 3 && h == 2
			&& std::memcmp( got, locked, 6 ) == 0, "fnc-geometry: locked tiles survive save+reopen" );
		DeleteRestoreKeepsBytes( pSession, nProps, szDir, "fnc", "fnc-geometry: " );
		BkResClose( pSession );
	}
	// A weapon has no geometry at all.
	{
		const std::string szIn = szFixtureRoot + "/wpn/project.wpn";
		if ( Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "wpn-geometry: BkResOpen" ) )
		{
			const BkResPoint2 zero = { 1, 2 };
			Check( BkResSetZeroPoint( pSession, 1, &zero ) == BK_EDITOR_REFUSED, "wpn-geometry: a weapon has no zero point" );
			BkResPoint2 read = { 5, 5 };
			Check( BkResGetZeroPoint( pSession, 1, &read ) == BK_EDITOR_REFUSED, "wpn-geometry: reading it is refused too" );
			BkResClose( pSession );
		}
	}
}
// S05 T04 (D014 item 2): the list channels where MFC keeps them. Each case
// sets a list on the owner nodes (below the root unless the home is the
// root's frame), saves, reads the saved file back the way MFC does (the
// engine's own CDataTreeXML and stats structs for the frame chunks, the S03
// items through NResourceModel::Load for item fields), reopens and compares,
// re-saves byte-identically, and deletes -> restores a node.

static const int kBridgeRoot            = 0x11000000 + 220;
static const int kMissionObjectiveProps = 0x11000000 + 233;
static const int kChapterMissionProps   = 0x11000000 + 243;
static const int kChapterPlaceProps     = 0x11000000 + 247;
static const int kCampaignChapterProps  = 0x11000000 + 253;
static const int kEffectFuncParticles   = 0x11000000 + 35;
static const int kEffectAnimationProps  = 0x11000000 + 38;
static const int kEffectFuncProps       = 0x11000000 + 40;

template <typename T>
struct SListCase
{
	const char *pszExt;
	const char *pszWhat;
	BkEditorStatus ( *pGet )( BkResSession *, int, T *, int, int * );
	BkEditorStatus ( *pSet )( BkResSession *, int, const T *, int );
	std::vector<int> ownerTypes;		// the first node of each type owns lists[i]
	std::vector<std::vector<T>> lists;
	std::string szSplice;				// inserted after the project element's start tag
};

// A copy of the fixture in szDir, with szSplice (an MFC frame chunk) put
// where CParentFrame::OnFileSave writes the frame chunks: before the tree.
static bool CopyFixture( const std::string &szFixtureRoot, const char *pszExt, const std::string &szSplice, const std::string &szTo )
{
	std::string szBytes;
	if ( !ReadBytes( szFixtureRoot + "/" + pszExt + "/project." + pszExt, szBytes ) || szBytes.empty() )
		return false;
	if ( !szSplice.empty() )
	{
		const std::size_t nStart = szBytes.find( "_Composer_Project" );
		const std::size_t nLine = nStart == std::string::npos ? nStart : szBytes.find( '\n', nStart );
		if ( nLine == std::string::npos )
			return false;
		szBytes.insert( nLine + 1, "\t" + szSplice + "\r\n" );
	}
	std::ofstream f( szTo, std::ios::binary );
	f.write( szBytes.data(), std::streamsize( szBytes.size() ) );
	return bool( f );
}

template <typename T>
static void ListChannelInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot,
                                    const SListCase<T> &test, const std::function<void( const std::string & )> &mfcCheck,
                                    const std::function<void( const std::vector<int> & )> &extra )
{
	const std::string szDir = szScratchRoot + "/mfc-geometry-" + test.pszWhat;
	const std::string szIn = szDir + "/input." + test.pszExt;
	const std::string szSaved = szDir + "/project." + test.pszExt;
	const std::string szResaved = szDir + "/project.resaved." + test.pszExt;
	const std::string szTag = std::string( test.pszWhat ) + ": ";
	auto What = [&]( const char *pszCheck ) { static std::string s; s = szTag + pszCheck; return s.c_str(); };
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );
	if ( !Check( CopyFixture( szFixtureRoot, test.pszExt, test.szSplice, szIn ), What( "copy the fixture" ) ) )
		return;
	if ( !test.szSplice.empty() )
	{
		// The engine writes the chunk on one line, the fixture is indented:
		// one save gives the input a single layout. After that an unedited
		// project with the frame chunk saves byte-identically.
		const std::string szSpliced = szDir + "/spliced." + test.pszExt;
		std::filesystem::rename( szIn, szSpliced, ec );
		Check( BkResOpen( pSession, szSpliced.c_str() ) == BK_EDITOR_OK && BkResSave( pSession, szIn.c_str() ) == BK_EDITOR_OK,
			What( "normalise the layout of the spliced project" ) );
		BkResClose( pSession );
		Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK && BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK,
			What( "open and save the project with its frame chunk" ) );
		std::string szA, szB;
		ReadBytes( szIn, szA );
		ReadBytes( szSaved, szB );
		Check( !szA.empty() && szA == szB, What( "an unedited project with its frame chunk saves byte-identically" ) );
		BkResClose( pSession );
	}
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, What( "BkResOpen" ) ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	std::vector<int> owners;
	for ( int nType : test.ownerTypes )
		owners.push_back( FirstNodeOfType( pSession, nType ) );
	if ( !Check( std::find( owners.begin(), owners.end(), 0 ) == owners.end(), What( "the fixture has every owner node" ) ) )
	{
		BkResClose( pSession );
		return;
	}
	for ( size_t i = 0; i < owners.size(); ++i )
		if ( !Check( test.pSet( pSession, owners[i], test.lists[i].data(), int( test.lists[i].size() ) ) == BK_EDITOR_OK, What( "set on an owner node" ) ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	for ( size_t i = 0; i < owners.size(); ++i )
		Check( SameList<T>( pSession, test.pGet, owners[i], test.lists[i] ), What( "read back what was set" ) );
	if ( owners[0] != 1 )
	{
		int nCount = -1;
		Check( test.pGet( pSession, 1, 0, 0, &nCount ) == BK_EDITOR_REFUSED, What( "the root has no home for the channel" ) );
		Check( test.pSet( pSession, 1, test.lists[0].data(), int( test.lists[0].size() ) ) == BK_EDITOR_REFUSED, What( "a set on the root is refused" ) );
	}
	T shortBuf[1] = {};
	int nCount = -1;
	Check( test.pGet( pSession, owners[0], shortBuf, 1, &nCount ) == ( test.lists[0].size() > 1 ? BK_EDITOR_REFUSED : BK_EDITOR_OK )
		&& nCount == int( test.lists[0].size() ), What( "the two-pass read reports the total" ) );
	Check( test.pSet( pSession, owners[0], test.lists[0].data(), -1 ) == BK_EDITOR_BAD_ARGUMENT, What( "a negative count is a bad argument" ) );
	Check( test.pSet( pSession, 99999, test.lists[0].data(), 1 ) == BK_EDITOR_REFUSED, What( "an unknown node is refused" ) );
	if ( extra )
		extra( owners );
	Check( SameList<T>( pSession, test.pGet, owners[0], test.lists[0] ), What( "a refused set changes nothing" ) );

	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "save" ) ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	Check( HasNoPrivateGeometry( szSaved ), What( "the saved XML has no private geometry element" ) );
	mfcCheck( szSaved );
	BkResClose( pSession );

	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, What( "reopen" ) ) )
		return;
	for ( size_t i = 0; i < owners.size(); ++i )
		Check( SameList<T>( pSession, test.pGet, owners[i], test.lists[i] ), What( "an owner node's list survives save+reopen" ) );
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, What( "save the reopened project" ) );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, What( "open -> save of the project with the lists is byte-identical" ) );
	int nVictim = owners[0];
	if ( nVictim == 1 )
		for ( const BkResNodeRecord &n : AllNodes( pSession ) )
			if ( n.parent == 1 ) { nVictim = n.id; break; }
	DeleteRestoreKeepsBytes( pSession, nVictim, szDir, test.pszExt, szTag );
	BkResClose( pSession );
}

// A file stream on szFile through a file storage on its folder, as
// ReadAsMfc opens one (CreateFileStream splits paths at backslashes only).
static IDataStream *FileStream( const std::string &szFile, bool bWrite )
{
	const std::string::size_type nCut = szFile.find_last_of( "/\\" );
	const std::string szDir = szFile.substr( 0, nCut + 1 );
	const std::string szName = szFile.substr( nCut + 1 );
	CPtr<IDataStorage> pStorage = bWrite ? CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE )
		: OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return 0;
	return bWrite ? pStorage->CreateStream( szName.c_str(), STREAM_ACCESS_WRITE ) : pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
}

// MFC's reader for a frame chunk: CreateDataTreeSaver on the project and the
// chunk's operator&, as LoadRPGStats does for "RPG".
template <class TStats>
static bool ReadChunkAsMfc( const std::string &szFile, const char *pszBase, const char *pszChunk, TStats &stats )
{
	CPtr<IDataStream> pStream = FileStream( szFile, false );
	if ( pStream == 0 )
		return false;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ, pszBase );
	if ( pDT == 0 )
		return false;
	CTreeAccessor tree = pDT;
	tree.Add( pszChunk, &stats );
	return true;
}

// The RPG chunk MFC's SaveRPGStats writes for these stats, cut out of a file
// the engine's CDataTreeXML writes.
template <class TStats>
static std::string RpgChunk( const std::string &szScratch, const char *pszBase, TStats &stats )
{
	std::error_code ec;
	std::filesystem::create_directories( szScratch, ec );
	const std::string szFile = szScratch + "/rpg-chunk.xml";
	{
		CPtr<IDataStream> pStream = FileStream( szFile, true );
		if ( pStream == 0 )
			return std::string();
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE, pszBase );
		if ( pDT == 0 )
			return std::string();
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &stats );
	}
	std::string szBytes;
	ReadBytes( szFile, szBytes );
	const std::size_t nBegin = szBytes.find( "<RPG" );
	const std::size_t nEnd = szBytes.find( "</RPG>" );
	if ( nBegin == std::string::npos || nEnd == std::string::npos )
		return std::string();
	return szBytes.substr( nBegin, nEnd + 6 - nBegin );
}

static bool HasValue( const NResourceModel::CTreeItem *pItem, const char *pszName, float f )
{
	if ( pItem == 0 )
		return false;
	for ( const NResourceModel::SProp &prop : pItem->GetValues() )
		if ( prop.szDefaultName == pszName )
		{
			if ( prop.value.GetKind() == NResourceModel::CVariant::VK_FLOAT )
				return prop.value.AsFloat() == f;
			return prop.value.GetKind() == NResourceModel::CVariant::VK_INT && float( prop.value.AsInt() ) == f;
		}
	return false;
}

// A squad formation's slots: CSquadFormationPropsItem::units, SUnit::vPos.
static void FormationInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	SListCase<BkResPoint2> test = { "scp", "formation", BkResGetFormationPositions, BkResSetFormationPositions,
		{ kSquadFormationProps }, { { { 600.5f, 300.25f }, { 632.75f, 300.25f } } }, std::string() };
	ListChannelInMfcLayout<BkResPoint2>( pSession, szFixtureRoot, szScratchRoot, test, [&]( const std::string &szSaved )
	{
		NResourceModel::Project project;
		const auto *pItem = dynamic_cast<const NResourceModel::CSquadFormationPropsItem *>( LoadItemOfType( szSaved, project, kSquadFormationProps ) );
		bool bUnits = pItem != 0 && pItem->units.size() == 2;
		if ( bUnits )
		{
			const auto &first = pItem->units.front();
			const auto &second = pItem->units.back();
			bUnits = first.vPos.x == 600.5f && first.vPos.y == 300.25f && first.vPos.z == 0 && first.fDir == 0.5f
				&& second.vPos.x == 632.75f && second.vPos.y == 300.25f && second.vPos.z == 0 && second.fDir == 0;
		}
		Check( bUnits, "formation: the S03 item reads the slots as its units (Pos; the old slot keeps its Dir)" );
	}, nullptr );
	// A shorter list drops the tail slot.
	const std::string szSaved = szScratchRoot + "/mfc-geometry-formation/project.scp";
	const std::string szShort = szScratchRoot + "/mfc-geometry-formation/short.scp";
	if ( Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "formation: reopen to shorten" ) )
	{
		const int nFormation = FirstNodeOfType( pSession, kSquadFormationProps );
		const BkResPoint2 one = { 1.5f, 2.5f };
		Check( BkResSetFormationPositions( pSession, nFormation, &one, 1 ) == BK_EDITOR_OK
			&& BkResSave( pSession, szShort.c_str() ) == BK_EDITOR_OK, "formation: shorten and save" );
		BkResClose( pSession );
		NResourceModel::Project project;
		const auto *pItem = dynamic_cast<const NResourceModel::CSquadFormationPropsItem *>( LoadItemOfType( szShort, project, kSquadFormationProps ) );
		Check( pItem != 0 && pItem->units.size() == 1 && pItem->units.front().vPos.x == 1.5f && pItem->units.front().fDir == 0.5f,
			"formation: the shortened list keeps one unit" );
	}
}

// A bridge's span anchors: CBridgeFrame's own_data Begin, End, Front, Back.
struct STestBridgeOwnData
{
	CVec3 vBegin, vEnd;
	float fFront = -1, fBack = -1;
};

static bool ReadBridgeOwnData( const std::string &szFile, STestBridgeOwnData &out )
{
	CPtr<IDataStream> pStream = FileStream( szFile, false );
	if ( pStream == 0 )
		return false;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ, "Bridge_Composer_Project" );
	if ( pDT == 0 || pDT->StartChunk( "own_data" ) == 0 )
		return false;
	{
		CTreeAccessor tree = pDT;
		tree.Add( "Begin", &out.vBegin );
		tree.Add( "End", &out.vEnd );
		tree.Add( "Front", &out.fFront );
		tree.Add( "Back", &out.fBack );
	}
	pDT->FinishChunk();
	return true;
}

static void BridgeSpanMarksInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	{
		const std::string szIn = szFixtureRoot + "/bdg/project.bdg";
		int nCount = -1;
		Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK && BkResGetBridgeSpanMarks( pSession, 1, 0, 0, &nCount ) == BK_EDITOR_OK
			&& nCount == 0, "span-marks: a bridge without own_data has no marks" );
		BkResClose( pSession );
	}
	SListCase<BkResPoint2> test = { "bdg", "span-marks", BkResGetBridgeSpanMarks, BkResSetBridgeSpanMarks,
		{ kBridgeRoot }, { { { 200.5f, 512.25f }, { 800.75f, 512.25f }, { -40.5f, 40.125f } } }, std::string() };
	ListChannelInMfcLayout<BkResPoint2>( pSession, szFixtureRoot, szScratchRoot, test, [&]( const std::string &szSaved )
	{
		STestBridgeOwnData own;
		Check( ReadBridgeOwnData( szSaved, own ) && own.vBegin.x == 200.5f && own.vBegin.y == 512.25f && own.vBegin.z == 0
			&& own.vEnd.x == 800.75f && own.vEnd.y == 512.25f && own.fFront == -40.5f && own.fBack == 40.125f,
			"span-marks: the engine reads own_data Begin, End, Front and Back" );
	}, [&]( const std::vector<int> &owners )
	{
		const BkResPoint2 two[2] = { { 1, 2 }, { 3, 4 } };
		Check( BkResSetBridgeSpanMarks( pSession, owners[0], two, 2 ) == BK_EDITOR_BAD_ARGUMENT, "span-marks: anything but three points is a bad argument" );
		const BkResPoint2 three[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
		const int nBegin = FirstNodeOfType( pSession, kBridgeBeginSpans );
		Check( nBegin > 1 && BkResSetBridgeSpanMarks( pSession, nBegin, three, 3 ) == BK_EDITOR_REFUSED,
			"span-marks: a spans node has no home (the anchors are the frame's)" );
	} );
}

// Map crosses: the position values of each child of the container, plus the
// RPG copy MFC's LoadRPGStats copies over them. Without RPG the values are
// what MFC reads, and CreateDefaultChilds turns them back into the ints the
// defaults have, so that case uses whole numbers; with RPG the floats survive.
template <class TStats>
static void CrossesInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot,
                                SListCase<BkResPoint2> test, const char *pszBase, TStats rpgStats,
                                const std::vector<int> &propsTypes, const std::vector<std::pair<const char *, const char *>> &names,
                                const std::function<std::vector<CVec2>( const TStats & )> &rpgPositions )
{
	auto valuesCheck = [&, test]( const std::string &szSaved )
	{
		NResourceModel::Project project;
		std::string szBytes, szError;
		bool bValues = ReadBytes( szSaved, szBytes ) && NResourceModel::Load( szBytes, project, szError ) && project.root;
		for ( size_t i = 0; bValues && i < propsTypes.size(); ++i )
		{
			const NResourceModel::CTreeItem *pItem = FindItemOfType( *project.root, propsTypes[i] );
			bValues = HasValue( pItem, names[i].first, test.lists[i][0].x ) && HasValue( pItem, names[i].second, test.lists[i][0].y );
		}
		Check( bValues, ( std::string( test.pszWhat ) + ": the S03 items read the crosses as the children's position values" ).c_str() );
	};
	auto extra = [&, test]( const std::vector<int> &owners )
	{
		std::vector<BkResPoint2> two( 2, test.lists[0][0] );
		Check( test.pSet( pSession, owners[0], two.data(), 2 ) == BK_EDITOR_BAD_ARGUMENT,
			( std::string( test.pszWhat ) + ": a list must carry one cross per child" ).c_str() );
	};
	ListChannelInMfcLayout<BkResPoint2>( pSession, szFixtureRoot, szScratchRoot, test, valuesCheck, extra );

	// The same with the RPG chunk an MFC save writes, and fractional crosses.
	SListCase<BkResPoint2> rpgTest = test;
	const std::string szWhat = std::string( test.pszWhat ) + "-rpg";
	rpgTest.pszWhat = szWhat.c_str();
	for ( auto &list : rpgTest.lists )
		for ( auto &p : list )
			p = { p.x + 0.5f, p.y + 0.25f };
	rpgTest.szSplice = RpgChunk( szScratchRoot + "/rpg-" + test.pszExt, pszBase, rpgStats );
	if ( !Check( !rpgTest.szSplice.empty(), ( szWhat + ": the engine writes an RPG chunk" ).c_str() ) )
		return;
	ListChannelInMfcLayout<BkResPoint2>( pSession, szFixtureRoot, szScratchRoot, rpgTest, [&, rpgTest]( const std::string &szSaved )
	{
		TStats read;
		bool bRpg = ReadChunkAsMfc( szSaved, pszBase, "RPG", read );
		const std::vector<CVec2> got = bRpg ? rpgPositions( read ) : std::vector<CVec2>();
		bRpg = bRpg && got.size() == rpgTest.lists.size();
		for ( size_t i = 0; bRpg && i < got.size(); ++i )
			bRpg = got[i].x == rpgTest.lists[i][0].x && got[i].y == rpgTest.lists[i][0].y;
		Check( bRpg, ( szWhat + ": the engine reads the crosses from the RPG chunk" ).c_str() );
	}, extra );
}

// Particle tracks: CKeyFrameTreeItem::framesList, (time, value) per key.
static void ParticleKeyframesInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	SListCase<BkResVec3> test = { "pcp", "particle-keyframes", BkResGetParticleKeyframes, BkResSetParticleKeyframes,
		{ kParticleDensity, kParticleSpeed },
		{ { { 0, 0.5f, 0 }, { 0.25f, 1.5f, 0 }, { 1, 0.125f, 0 } }, { { 0, 2.5f, 0 }, { 0.75f, 3.25f, 0 } } }, std::string() };
	ListChannelInMfcLayout<BkResVec3>( pSession, szFixtureRoot, szScratchRoot, test, [&]( const std::string &szSaved )
	{
		NResourceModel::Project project;
		std::string szBytes, szError;
		bool bFrames = ReadBytes( szSaved, szBytes ) && NResourceModel::Load( szBytes, project, szError ) && project.root;
		const int nTypes[2] = { kParticleDensity, kParticleSpeed };
		for ( int i = 0; bFrames && i < 2; ++i )
		{
			const auto *pItem = dynamic_cast<const NResourceModel::CKeyFrameTreeItem *>( FindItemOfType( *project.root, nTypes[i] ) );
			bFrames = pItem != 0 && pItem->framesList.size() == test.lists[i].size();
			size_t k = 0;
			if ( bFrames )
				for ( const auto &frame : pItem->framesList )
				{
					bFrames = bFrames && frame.first == test.lists[i][k].x && frame.second == test.lists[i][k].y;
					++k;
				}
		}
		Check( bFrames, "particle-keyframes: the S03 items read the keys as their Key_frames" );
	}, [&]( const std::vector<int> &owners )
	{
		const BkResVec3 key = { 0, 1, 2 };
		Check( BkResSetParticleKeyframes( pSession, owners[0], &key, 1 ) == BK_EDITOR_BAD_ARGUMENT, "particle-keyframes: a key with z is a bad argument" );
	} );
}

// Effect parts: the X / Y / Z position values of each child of an
// Animations or Function Particles node (DT_DEC, whole numbers).
static void EffectPlacesInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	SListCase<BkResVec3> test = { "eff", "effect-keyframes", BkResGetEffectKeyframes, BkResSetEffectKeyframes,
		{ kEffectAnimations, kEffectFuncParticles }, { { { 12, -4, 3 } }, { { -7, 8, 0 } } }, std::string() };
	ListChannelInMfcLayout<BkResVec3>( pSession, szFixtureRoot, szScratchRoot, test, [&]( const std::string &szSaved )
	{
		NResourceModel::Project project;
		std::string szBytes, szError;
		bool bPlaces = ReadBytes( szSaved, szBytes ) && NResourceModel::Load( szBytes, project, szError ) && project.root;
		const int nTypes[2] = { kEffectAnimationProps, kEffectFuncProps };
		for ( int i = 0; bPlaces && i < 2; ++i )
		{
			const NResourceModel::CTreeItem *pItem = FindItemOfType( *project.root, nTypes[i] );
			bPlaces = HasValue( pItem, "X position", test.lists[i][0].x ) && HasValue( pItem, "Y position", test.lists[i][0].y )
				&& HasValue( pItem, "Z position", test.lists[i][0].z );
		}
		Check( bPlaces, "effect-keyframes: the S03 items read the places as their X / Y / Z position values" );
	}, [&]( const std::vector<int> &owners )
	{
		const BkResVec3 half = { 0.5f, 1, 2 };
		Check( BkResSetEffectKeyframes( pSession, owners[0], &half, 1 ) == BK_EDITOR_BAD_ARGUMENT, "effect-keyframes: a fractional place is a bad argument" );
		const BkResVec3 two[2] = { { 1, 2, 3 }, { 4, 5, 6 } };
		Check( BkResSetEffectKeyframes( pSession, owners[0], two, 2 ) == BK_EDITOR_BAD_ARGUMENT, "effect-keyframes: one place per child" );
	} );
}

static void ListGeometryInMfcLayout( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	FormationInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	BridgeSpanMarksInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	{
		SMissionStats stats;
		stats.objectives.resize( 1 );
		stats.objectives[0].vPosOnMap = CVec2( 0, 0 );
		CrossesInMfcLayout<SMissionStats>( pSession, szFixtureRoot, szScratchRoot,
			{ "mip", "objectives", BkResGetMissionObjectives, BkResSetMissionObjectives, { kMissionObjectives }, { { { 120, 240 } } }, std::string() },
			"Mission_Composer_Project", stats, { kMissionObjectiveProps }, { { "Objective position X", "Objective position Y" } },
			[]( const SMissionStats &s ) { std::vector<CVec2> v; for ( const auto &o : s.objectives ) v.push_back( o.vPosOnMap ); return v; } );
	}
	{
		SChapterStats stats;
		stats.missions.resize( 1 );
		stats.missions[0].vPosOnMap = CVec2( 0, 0 );
		stats.placeHolders.resize( 1 );
		stats.placeHolders[0].vPosOnMap = CVec2( 0, 0 );
		CrossesInMfcLayout<SChapterStats>( pSession, szFixtureRoot, szScratchRoot,
			{ "chc", "chapter-crosses", BkResGetChapterCrosses, BkResSetChapterCrosses, { kChapterMissions, kChapterPlaces },
				{ { { 10, 20 } }, { { 30, 40 } } }, std::string() },
			"Chapter_Composer_Project", stats, { kChapterMissionProps, kChapterPlaceProps },
			{ { "Mission position X", "Mission position Y" }, { "Place holder position X", "Place holder position Y" } },
			[]( const SChapterStats &s )
			{
				std::vector<CVec2> v;
				if ( s.missions.size() == 1 && s.placeHolders.size() == 1 )
					v = { s.missions[0].vPosOnMap, s.placeHolders[0].vPosOnMap };
				return v;
			} );
	}
	{
		SCampaignStats stats;
		stats.chapters.resize( 1 );
		CrossesInMfcLayout<SCampaignStats>( pSession, szFixtureRoot, szScratchRoot,
			{ "cgc", "campaign-crosses", BkResGetCampaignCrosses, BkResSetCampaignCrosses, { kCampaignChapters }, { { { 50, 60 } } }, std::string() },
			"Campaign_Composer_Project", stats, { kCampaignChapterProps }, { { "Chapter position X", "Chapter position Y" } },
			[]( const SCampaignStats &s ) { std::vector<CVec2> v; for ( const auto &c : s.chapters ) v.push_back( c.vPosOnMap ); return v; } );
	}
	ParticleKeyframesInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	EffectPlacesInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
}

// Every fixture saved after a set of every channel each node supports (a
// get that is not refused): the written XML has no private geometry element,
// and the project reopens.
static void EveryChannelOnEveryFixture( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	const std::string szDir = szScratchRoot + "/every-channel";
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );
	int nTotalSets = 0;
	for ( const Fixture &fx : kFixtures )
	{
		const std::string szIn = szFixtureRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
		const std::string szOut = szDir + "/project." + fx.pszExt;
		const std::string szTag = std::string( "every-channel " ) + fx.pszExt + ": ";
		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, ( szTag + "open" ).c_str() ) )
			continue;
		int nSets = 0;
		auto Count = [&]( BkEditorStatus n, const char *pszChannel )
		{
			if ( !Check( n == BK_EDITOR_OK, ( szTag + "set " + pszChannel ).c_str() ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			++nSets;
		};
		for ( const BkResNodeRecord &node : AllNodes( pSession ) )
		{
			const int nId = node.id;
			typedef BkEditorStatus ( *GridGet )( BkResSession *, int, unsigned char *, int, int *, int * );
			typedef BkEditorStatus ( *GridSet )( BkResSession *, int, const unsigned char *, int, int );
			const struct { GridGet pGet; GridSet pSet; const char *pszName; } grids[] = {
				{ BkResGetPassabilityCells, BkResSetPassabilityCells, "passability" }, { BkResGetLockedTiles, BkResSetLockedTiles, "locked tiles" } };
			for ( const auto &g : grids )
			{
				int w = 0, h = 0;
				if ( g.pGet( pSession, nId, 0, 0, &w, &h ) != BK_EDITOR_OK )
					continue;
				std::vector<unsigned char> cells( size_t( w ) * h + 1 );
				g.pGet( pSession, nId, cells.data(), int( cells.size() ), &w, &h );
				if ( w == 0 || h == 0 )
					cells = { 1, 0 }, w = 2, h = 1;
				Count( g.pSet( pSession, nId, cells.data(), w, h ), g.pszName );
			}
			typedef BkEditorStatus ( *PointGet )( BkResSession *, int, BkResPoint2 * );
			typedef BkEditorStatus ( *PointSet )( BkResSession *, int, const BkResPoint2 * );
			const struct { PointGet pGet; PointSet pSet; const char *pszName; } points[] = {
				{ BkResGetZeroPoint, BkResSetZeroPoint, "zero point" }, { BkResGetEntrance, BkResSetEntrance, "entrance" } };
			for ( const auto &p : points )
			{
				BkResPoint2 at = { 0, 0 };
				if ( p.pGet( pSession, nId, &at ) == BK_EDITOR_OK )
					Count( p.pSet( pSession, nId, &at ), p.pszName );
			}
			typedef BkEditorStatus ( *ListGet )( BkResSession *, int, BkResPoint2 *, int, int * );
			typedef BkEditorStatus ( *ListSet )( BkResSession *, int, const BkResPoint2 *, int );
			const struct { ListGet pGet; ListSet pSet; const char *pszName; int nFresh; } lists[] = {
				{ BkResGetTransparencyLines, BkResSetTransparencyLines, "transparency lines", 2 },
				{ BkResGetFormationPositions, BkResSetFormationPositions, "formation positions", 2 },
				{ BkResGetBridgeSpanMarks, BkResSetBridgeSpanMarks, "bridge span marks", 3 },
				{ BkResGetMissionObjectives, BkResSetMissionObjectives, "mission objectives", 0 },
				{ BkResGetChapterCrosses, BkResSetChapterCrosses, "chapter crosses", 0 },
				{ BkResGetCampaignCrosses, BkResSetCampaignCrosses, "campaign crosses", 0 } };
			for ( const auto &l : lists )
			{
				int n = 0;
				if ( l.pGet( pSession, nId, 0, 0, &n ) != BK_EDITOR_OK )
					continue;
				std::vector<BkResPoint2> v( size_t( n ) + 1 );
				l.pGet( pSession, nId, v.data(), int( v.size() ), &n );
				v.resize( size_t( n ) );
				if ( n == 0 )
					v.assign( size_t( l.nFresh ), BkResPoint2{ 64, 32 } );
				Count( l.pSet( pSession, nId, v.data(), int( v.size() ) ), l.pszName );
			}
			typedef BkEditorStatus ( *AimedGet )( BkResSession *, int, BkResAimedPoint *, int, int * );
			typedef BkEditorStatus ( *AimedSet )( BkResSession *, int, const BkResAimedPoint *, int );
			const struct { AimedGet pGet; AimedSet pSet; const char *pszName; } aimed[] = {
				{ BkResGetShootPoints, BkResSetShootPoints, "shoot points" }, { BkResGetFirePoints, BkResSetFirePoints, "fire points" },
				{ BkResGetSmokePoints, BkResSetSmokePoints, "smoke points" },
				{ BkResGetDirectedExplosionPoints, BkResSetDirectedExplosionPoints, "directed explosions" } };
			for ( const auto &a : aimed )
			{
				int n = 0;
				if ( a.pGet( pSession, nId, 0, 0, &n ) != BK_EDITOR_OK )
					continue;
				std::vector<BkResAimedPoint> v( size_t( n ) + 1 );
				a.pGet( pSession, nId, v.data(), int( v.size() ), &n );
				v.resize( size_t( n ) );
				if ( n == 0 )
					v.push_back( { { 4, 8 }, 90, 30 } );
				Count( a.pSet( pSession, nId, v.data(), int( v.size() ) ), a.pszName );
			}
			typedef BkEditorStatus ( *Vec3Get )( BkResSession *, int, BkResVec3 *, int, int * );
			typedef BkEditorStatus ( *Vec3Set )( BkResSession *, int, const BkResVec3 *, int );
			const struct { Vec3Get pGet; Vec3Set pSet; const char *pszName; int nFresh; } vec3s[] = {
				{ BkResGetParticleKeyframes, BkResSetParticleKeyframes, "particle keyframes", 2 },
				{ BkResGetEffectKeyframes, BkResSetEffectKeyframes, "effect keyframes", 0 } };
			for ( const auto &l : vec3s )
			{
				int n = 0;
				if ( l.pGet( pSession, nId, 0, 0, &n ) != BK_EDITOR_OK )
					continue;
				std::vector<BkResVec3> v( size_t( n ) + 1 );
				l.pGet( pSession, nId, v.data(), int( v.size() ), &n );
				v.resize( size_t( n ) );
				if ( n == 0 )
					v.assign( size_t( l.nFresh ), BkResVec3{ 0.5f, 2, 0 } );
				Count( l.pSet( pSession, nId, v.data(), int( v.size() ) ), l.pszName );
			}
		}
		nTotalSets += nSets;
		if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, ( szTag + "save" ).c_str() ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		Check( HasNoPrivateGeometry( szOut ), ( szTag + "the written XML has no private geometry element" ).c_str() );
		Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, ( szTag + "reopen" ).c_str() );
		BkResClose( pSession );
	}
	std::printf( "every-channel: %d channel sets over %d fixtures\n", nTotalSets, kFixtureCount );
	Check( nTotalSets > 0, "every-channel: some fixture supports a channel" );
}

// T10: References, MOD settings + PAK, Export (+ batch), Import.
//
// The exporters themselves are ported by each sub-editor's slice; until then
// a kind answers REFUSED, which this tier pins for every fixture whose kind
// is not ported (S06 ported wpn, mcp, trc and scp: those export), and the
// golden comparison is reported as pending, never as a pass. The export
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

// The kinds S06 ported: their fixtures export for real.
static bool IsPortedExport( const std::string &szExt )
{
	return szExt == "wpn" || szExt == "mcp" || szExt == "trc" || szExt == "scp";
}

// The stats file a fixture's export moved into data/, read back with the
// engine's own reader (the struct's operator&, as GameDB does). Field-equal
// proof against hand-derived structs is test-resource-model-comparator's;
// here: the file is where MFC's folders put it, and what only the running
// engine supplies - the squad member resolved through IObjectsDB - is in it.
static void ExportedStatsRead( const std::string &szExt, const std::filesystem::path &modData, const BkResExportReport &report )
{
	if ( szExt == "wpn" )
	{
		SWeaponRPGStats stats;
		const std::string szFile = ( modData / "weapons" / "wpn.xml" ).string();
		Check( ReadChunkAsMfc( szFile, "base", "RPG", stats ) && stats.szKeyName == "Unknown Weapon" && stats.wDeltaAngle == 10 &&
		       stats.shells.size() == 1 && stats.shells[0].fDamagePower == 5.0f && stats.shells[0].fTraceProbability == 0.1f &&
		       stats.shells[0].flashFire.nPower == 100 && stats.shells[0].flashExplosion.nDuration == 1000,
		       "export: wpn lands in weapons/<project folder>.xml and the engine reads the fixture's weapon" );
	}
	else if ( szExt == "mcp" )
	{
		SMineRPGStats stats;
		const std::string szFile = ( modData / "objects/simpleobjects/common/summer/mine/mcp/1.xml" ).string();
		Check( ReadChunkAsMfc( szFile, "base", "RPG", stats ) && stats.fWeight == 10.0f && stats.szFlagModel == "1" && stats.szWeapon.empty(),
		       "export: mcp lands in objects/simpleobjects/common/summer/mine/<folder>/1.xml with weight 10 and flag model 1" );
	}
	else if ( szExt == "trc" )
	{
		SEntrenchmentRPGStats stats;
		const std::string szFile = ( modData / "units/technics/common/entrenchment/trc/1.xml" ).string();
		Check( ReadChunkAsMfc( szFile, "base", "RPG", stats ) && stats.fMaxHP == 100.0f && stats.szKeyName == "Unknown Trench" &&
		       stats.segments.empty() && stats.defences[RPG_FRONT].nArmorMin == 300 && stats.defences[RPG_TOP].fSilhouette == 1.0f,
		       "export: trc lands in units/technics/common/entrenchment/<folder>/1.xml" );
		Check( report.warning_count == 1, "export: trc warns, as MFC's message box did, that its empty segment source cannot be copied" );
	}
	else if ( szExt == "scp" )
	{
		SSquadRPGStats stats;
		const std::string szFile = ( modData / "squads" / "scp" / "1.xml" ).string();
		Check( ReadChunkAsMfc( szFile, "base", "RPG", stats ) && stats.szIcon == "icon.tga" && stats.memberNames.size() == 1 &&
		       stats.memberNames[0] == "USSR_Mosin" && stats.formations.size() == 1 && stats.formations[0].order.size() == 1 &&
		       stats.formations[0].order[0].szSoldier == "USSR_Mosin",
		       "export: scp lands in squads/<folder>/1.xml and USSR\\Mosin resolves to USSR_Mosin through the engine's IObjectsDB" );
	}
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
			// The actions list is CMultySelDialog's, checked on its own below.
			if ( t == int( NResourceModel::EReferenceType::E_ACTIONS_REF ) )
				continue;
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

		// E_ACTIONS_REF: the action types of Data/Editor/actions.ini, in file
		// order, each token the action's id (the bit of MultySelDialog's mask),
		// the list BkEditorActionCommands reads for the map editor too.
		const int nActions = int( NResourceModel::EReferenceType::E_ACTIONS_REF );
		nCount = -1;
		Check( BkResRefList( pSession, nActions, 0, 0, &nCount ) == BK_EDITOR_OK && nCount > 0, "refs: the actions list is actions.ini's" );
		std::vector<BkResReferenceEntry> actions( nCount > 0 ? nCount : 1 );
		Check( BkResRefList( pSession, nActions, actions.data(), nCount, &nCount ) == BK_EDITOR_OK, "refs: the actions list reads" );
		int nMapCount = 0, nDefault = 0;
		BkEditorActionCommands( pSession, 0, 0, &nMapCount, &nDefault );
		std::vector<BkEditorActionCommand> mapActions( nMapCount > 0 ? nMapCount : 1 );
		BkEditorActionCommands( pSession, mapActions.data(), nMapCount, &nMapCount, &nDefault );
		bool bSameActions = nMapCount == nCount;
		for ( int i = 0; bSameActions && i < nCount; ++i )
			bSameActions = actions[i].token == mapActions[i].id && std::strcmp( actions[i].name, mapActions[i].name ) == 0;
		Check( bSameActions, "refs: each action's token is its actions.ini id" );
		Check( nCount > 0 && std::strcmp( actions[0].name, "MOVE_TO" ) == 0 && actions[0].token == 0, "refs: MOVE_TO is action 0" );
		std::printf( "REF %s count=%d\n", NResourceModel::ReferenceTypeName( NResourceModel::EReferenceType::E_ACTIONS_REF ), nCount );
	}

	// The tree panel's entries (T10): rename, expand and a property's strings,
	// each written where MFC keeps it and read back by the MFC-layout reader.
	{
		const fs::path project = scratch / "tree" / "project.wpn";
		fs::create_directories( project.parent_path(), ec );
		fs::copy_file( fs::path( szFixtureRoot ) / "wpn" / "project.wpn", project, fs::copy_options::overwrite_existing, ec );
		if ( Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "tree: open the wpn copy" ) )
		{
			std::vector<BkResNodeRecord> nodes = AllNodes( pSession );
			const int nNode = nodes.size() > 1 ? nodes[1].id : nodes[0].id;
			Check( BkResSetNodeName( pSession, nNode, "Renamed node" ) == BK_EDITOR_OK, "tree: BkResSetNodeName" );
			Check( BkResSetNodeName( pSession, nNode, "" ) == BK_EDITOR_BAD_ARGUMENT, "tree: an empty name is a bad argument" );
			const std::string szLong( 64, 'n' );
			Check( BkResSetNodeName( pSession, nNode, szLong.c_str() ) == BK_EDITOR_BAD_ARGUMENT, "tree: a name the record cannot hold is a bad argument" );
			Check( BkResSetNodeName( pSession, 999999, "x" ) == BK_EDITOR_REFUSED, "tree: renaming an unknown node is refused" );
			Check( BkResSetNodeExpand( pSession, nNode, 1 ) == BK_EDITOR_OK, "tree: BkResSetNodeExpand" );
			Check( BkResSetNodeExpand( pSession, 999999, 1 ) == BK_EDITOR_REFUSED, "tree: expanding an unknown node is refused" );

			// The first property with strings anywhere in the project.
			int nStringsNode = -1, nStringsProp = -1, nStringsCount = 0;
			for ( const BkResNodeRecord &node : AllNodes( pSession ) )
			{
				int nProps = 0;
				BkResProps( pSession, node.id, 0, 0, &nProps );
				std::vector<BkResPropRecord> props( nProps > 0 ? nProps : 1 );
				BkResProps( pSession, node.id, props.data(), nProps, &nProps );
				for ( int i = 0; i < nProps && nStringsNode < 0; ++i )
					if ( props[i].combo_count > 0 )
					{
						nStringsNode = node.id;
						nStringsProp = props[i].id;
						nStringsCount = props[i].combo_count;
					}
			}
			if ( Check( nStringsNode >= 0, "tree: the wpn project has a property with strings" ) )
			{
				int nGot = -1;
				Check( BkResPropStrings( pSession, nStringsNode, nStringsProp, 0, 0, &nGot ) == BK_EDITOR_OK && nGot == nStringsCount,
					"tree: BkResPropStrings counts combo_count strings" );
				std::vector<BkResReferenceEntry> strings( nGot > 0 ? nGot : 1 );
				Check( BkResPropStrings( pSession, nStringsNode, nStringsProp, strings.data(), nGot, &nGot ) == BK_EDITOR_OK && strings[0].name[0] != 0 && strings[0].token == 0,
					"tree: BkResPropStrings reads the strings" );
				if ( nGot > 1 )
				{
					BkResReferenceEntry one;
					Check( BkResPropStrings( pSession, nStringsNode, nStringsProp, &one, 1, &nGot ) == BK_EDITOR_REFUSED, "tree: a short strings buffer is refused" );
				}
				Check( BkResPropStrings( pSession, nStringsNode, -12345, 0, 0, &nGot ) == BK_EDITOR_REFUSED, "tree: strings of an unknown prop are refused" );
			}

			Check( BkResSave( pSession, project.string().c_str() ) == BK_EDITOR_OK, "tree: save the renamed project" );
			BkResClose( pSession );
			Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "tree: reopen" );
			bool bName = false, bExpand = false;
			for ( const BkResNodeRecord &node : AllNodes( pSession ) )
				if ( node.id == nNode )
				{
					bName = std::strcmp( node.display_name, "Renamed node" ) == 0;
					bExpand = node.expand == 1;
				}
			Check( bName, "tree: the new name survives save and reopen" );
			Check( bExpand, "tree: the expand state survives save and reopen" );
			BkResClose( pSession );
			std::string szSaved;
			ReadBytes( project.string(), szSaved );
			Check( szSaved.find( "Renamed node" ) != std::string::npos, "tree: the project file holds the new display_name" );
		}
		Check( BkResSetNodeName( pSession, 1, "x" ) == BK_EDITOR_REFUSED, "tree: renaming with no project open is refused" );
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
			if ( szExt == "spt" )
			{
				// The fixture's directory is MFC's default "_.", which MFC joins to the
				// frame name as "<project folder>\_.sprite-1frame.tga": no such file, so
				// nothing is composed (S07Sprite proves the export with real frames).
				Check( status == BK_EDITOR_OK && report.written == 0, "export: .spt exports through its S07 exporter; the fixture's default directory finds no frame, so nothing is composed" );
			}
			else if ( szExt == "unt" )
			{
				// The fixture has no frame files: the compose finds no valid animation, which is a warning
				// as in MFC, and the stats are written (S07Infantry proves the frames).
				Check( status == BK_EDITOR_OK, "export: .unt exports through its S07 exporter" );
			}
			else if ( IsPortedExport( szExt ) )
			{
				if ( !Check( status == BK_EDITOR_OK && report.written >= 1, ( "export: ." + szExt + " exports through its S06 exporter" ).c_str() ) )
					std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
				ExportedStatsRead( szExt, modData, report );
			}
			else
				Check( status == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), "not ported yet" ) != 0 && report.written == 0,
				       ( "export: ." + szExt + " says its exporter is not ported yet" ).c_str() );
			// The comparison itself is the comparator tier's Goldens(); this tier
			// only says what it sees, so the two reports agree.
			const int nGolden = GoldenFiles( fs::path( szFixtureRoot ) / szExt / "golden" );
			if ( nGolden == 0 )
				std::printf( "GOLDEN %s pending: golden missing (run tools/zig/win-home/export-goldens.ps1 on win-home)\n", szExt.c_str() );
			else
				std::printf( "GOLDEN %s pending: %d golden files, compared by test-resource-model-comparator\n", szExt.c_str(), nGolden );
			++nPending;
		}
		std::printf( "GOLDEN_SUMMARY extensions=%d pass=0 fail=0 pending=%d (this tier does not compare; see test-resource-model-comparator)\n", kFixtureCount, nPending );

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

		// Batch: an mdc and a wpn (exported), a msh (not ported: skipped with
		// a warning), then -os re-saving a wpn unchanged.
		NResourceModel::RegisterExporter( "mdc", &GoodExporter );
		const fs::path src = scratch / "batch-src";
		fs::create_directories( src / "nested", ec );
		fs::copy_file( szFixtureRoot + "/mdc/project.mdc", src / "nested" / "medal.mdc", fs::copy_options::overwrite_existing, ec );
		fs::copy_file( szFixtureRoot + "/wpn/project.wpn", src / "weapon.wpn", fs::copy_options::overwrite_existing, ec );
		fs::copy_file( szFixtureRoot + "/msh/project.msh", src / "mesh.msh", fs::copy_options::overwrite_existing, ec );
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
		Check( report.written == 3 && report.skipped == 1 && nNotPorted == 1, "batch: mdc and wpn exported, msh skipped as not ported" );
		Check( fs::is_regular_file( dst / "data" / "weapons" / "batch-src.xml", ec ), "batch: the weapon lands in dst/data/weapons/<project folder>.xml" );
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
		Check( BkResImportFromGame( pSession, 6, szGunner.c_str() ) == BK_EDITOR_REFUSED && std::strstr( BkEditorLastMessage( pSession ), "not ported yet" ) != 0,
		       "import: msh is refused as not ported yet" );
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

// A particle source is one key-based xml (its texture stays in the shipped
// data); the bridge wraps it in a one-particle effect, as the MFC frame did.
static bool ParticleSourceExporter( const NResourceModel::Project &, const NResourceModel::SExportContext &context, NResourceModel::SExportOutcome &outcome )
{
	std::error_code ec;
	const std::filesystem::path target = std::filesystem::path( context.szStagingRoot ) / "editor/preview/particle.xml";
	std::filesystem::create_directories( target.parent_path(), ec );
	std::filesystem::copy_file( FoldedPath( g_dataRoot, "Effects/Particles/flame.xml" ), target, std::filesystem::copy_options::overwrite_existing, ec );
	outcome.nWritten = ec ? 0 : 1;
	outcome.szObjectName = "editor\\preview\\particle";
	if ( ec )
		outcome.szError = "cannot copy flame.xml: " + ec.message();
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
		{ "particle", "pcp", 11, &ParticleSourceExporter },
		// The effect project, an extra beside the one particle source.
		{ "effect",   "eff", 12, &EffectExporter },
	};
	for ( const Capture &capture : kCaptures )
	{
		const std::string szLabel = capture.pszLabel;
		const NResourceModel::FExporter pfnRegistered = NResourceModel::FindExporter( capture.pszExt );
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
		// What the table held before this stand-in came and went (a kind ported
		// since has its real exporter back for the cases after this one).
		NResourceModel::RegisterExporter( capture.pszExt, pfnRegistered );
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

// S06 T04: the squad, weapon and trench sub-editors' tools on the real
// bridge, the same sequences test-resource-core runs on the fake through
// resource_core's sub_editor_tools: get-before, apply, get-after, undo ==
// before, redo == after, save and reopen == after. Undo and redo replay the
// bridge calls resource_core's Document makes for each command (a geometry
// write of before / after, an insert undone by a delete and redone by a
// restore of its blob, a delete undone by a restore, a property write of the
// old / new text). The state is the whole tree (classes, names, props) plus
// every formation's slots, zero point and direction, with floats at the six
// significant digits a project file keeps (MFC's %g).
namespace S06Tools
{

static const int kWeaponShootTypes  = 0x11000000 + 83;
static const int kWeaponDamageProps = 0x11000000 + 84;
static const int kWeaponEffects     = 0x11000000 + 86;
static const int kWeaponFlashProps  = 0x11000000 + 88;
static const int kWeaponCraters     = 0x11000000 + 281;
static const int kWeaponCraterProps = 0x11000000 + 282;
static const int kTrenchSources     = 0x11000000 + 153;
static const int kTrenchSourceProps = 0x11000000 + 154;
static const int kUnitSeasonProps   = 0x11000000 + 8;
static const int kUnitAnimationsItem= 0x11000000 + 10;
static const int kUnitAnimationProps= 0x11000000 + 11;
static const int kUnitFrameProps    = 0x11000000 + 12;
static const int kSpritesItem       = 0x11000000 + 22;
static const int kSpriteProps       = 0x11000000 + 23;

struct SAction
{
	std::function<bool()> apply, undo, redo;
};

static std::vector<BkResPropRecord> PropsOf( BkResSession *pSession, int nNode )
{
	int nCount = 0;
	BkResProps( pSession, nNode, 0, 0, &nCount );
	std::vector<BkResPropRecord> props( nCount > 0 ? nCount : 0 );
	if ( nCount > 0 )
		BkResProps( pSession, nNode, props.data(), nCount, &nCount );
	return props;
}

static std::vector<BkResPoint2> SlotsOf( BkResSession *pSession, int nNode )
{
	int nCount = 0;
	BkResGetFormationPositions( pSession, nNode, 0, 0, &nCount );
	std::vector<BkResPoint2> slots( nCount > 0 ? nCount : 0 );
	if ( nCount > 0 )
		BkResGetFormationPositions( pSession, nNode, slots.data(), nCount, &nCount );
	return slots;
}

static std::string State( BkResSession *pSession )
{
	std::ostringstream out;
	out.precision( 6 );
	std::map<int, int> depth;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
	{
		const int nDepth = depth.count( n.parent ) != 0 ? depth[n.parent] + 1 : 0;
		depth[n.id] = nDepth;
		const std::string szIndent( size_t( nDepth * 2 ), ' ' );
		out << szIndent << n.class_type << " \"" << n.display_name << "\"\n";
		for ( const BkResPropRecord &p : PropsOf( pSession, n.id ) )
			out << szIndent << "  " << p.default_name << " = " << p.value_text << "\n";
		if ( n.class_type != kSquadFormationProps )
			continue;
		out << szIndent << "  channel formation_positions:";
		for ( const BkResPoint2 &slot : SlotsOf( pSession, n.id ) )
			out << " (" << slot.x << "," << slot.y << ")";
		BkResPoint2 zero = { 0, 0 }, direction = { 0, 0 };
		BkResGetZeroPoint( pSession, n.id, &zero );
		BkResGetFormationDirection( pSession, n.id, &direction );
		out << "\n" << szIndent << "  channel zero_point: (" << zero.x << "," << zero.y << ")\n";
		out << szIndent << "  channel formation_direction: " << direction.x << "\n";
	}
	return out.str();
}

static void SameState( const std::string &szWhat, const std::string &szWant, const std::string &szGot, bool bEqual )
{
	if ( !Check( ( szWant == szGot ) == bEqual, szWhat.c_str() ) )
		std::printf( "--- expected (%s) ---\n%s--- got ---\n%s", bEqual ? "equal" : "a change", szWant.c_str(), szGot.c_str() );
}

static int NthChildOfType( BkResSession *pSession, int nParent, int nType, int nNth )
{
	int nSeen = 0;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.parent == nParent && n.class_type == nType && nSeen++ == nNth )
			return n.id;
	return 0;
}

static int RootOf( BkResSession *pSession )
{
	const std::vector<BkResNodeRecord> nodes = AllNodes( pSession );
	return nodes.empty() ? 0 : nodes[0].id;
}

static int ChildCount( BkResSession *pSession, int nParent )
{
	int nCount = 0;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.parent == nParent )
			++nCount;
	return nCount;
}

static bool DeleteInto( BkResSession *pSession, int nNode, std::vector<unsigned char> &blob )
{
	int nSize = 0;
	BkResDeleteNode( pSession, nNode, 0, 0, &nSize );
	blob.assign( size_t( nSize > 0 ? nSize : 1 ), 0 );
	if ( BkResDeleteNode( pSession, nNode, blob.data(), nSize, &nSize ) != BK_EDITOR_OK )
		return false;
	blob.resize( size_t( nSize ) );
	return true;
}

// insert_node: undo deletes the new node, redo restores it from that blob.
static SAction Insert( BkResSession *pSession, int nParent, int nClass, int nIndex, std::shared_ptr<int> pId )
{
	auto pBlob = std::make_shared<std::vector<unsigned char>>();
	SAction action;
	action.apply = [=]() { return BkResInsertNode( pSession, nParent, nClass, nIndex, pId.get() ) == BK_EDITOR_OK; };
	action.undo = [=]() { return DeleteInto( pSession, *pId, *pBlob ); };
	action.redo = [=]() { return BkResRestoreNode( pSession, pBlob->data(), int( pBlob->size() ), nParent, nIndex, pId.get() ) == BK_EDITOR_OK; };
	return action;
}

// delete_node: the parent and index come from the tree, as the core's
// deleteNode builder takes them from the mirror.
static SAction Delete( BkResSession *pSession, int nNode )
{
	int nParent = 0, nIndex = 0;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.id == nNode )
			nParent = n.parent;
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
	{
		if ( n.id == nNode )
			break;
		if ( n.parent == nParent )
			++nIndex;
	}
	auto pId = std::make_shared<int>( nNode );
	auto pBlob = std::make_shared<std::vector<unsigned char>>();
	SAction action;
	action.apply = [=]() { return DeleteInto( pSession, *pId, *pBlob ); };
	action.undo = [=]() { return BkResRestoreNode( pSession, pBlob->data(), int( pBlob->size() ), nParent, nIndex, pId.get() ) == BK_EDITOR_OK; };
	action.redo = action.apply;
	return action;
}

// set_prop by MFC's default name, from the value the tree holds now.
static SAction SetProp( BkResSession *pSession, int nNode, const char *pszName, const std::string &szAfter )
{
	int nProp = -1;
	std::string szBefore;
	for ( const BkResPropRecord &p : PropsOf( pSession, nNode ) )
		if ( std::strcmp( p.default_name, pszName ) == 0 )
		{
			nProp = p.id;
			szBefore = p.value_text;
		}
	Check( nProp != -1, ( std::string( "s06-tool: the node has a \"" ) + pszName + "\" property" ).c_str() );
	SAction action;
	action.apply = [=]() { return BkResSetProp( pSession, nNode, nProp, szAfter.c_str() ) == BK_EDITOR_OK; };
	action.undo = [=]() { return BkResSetProp( pSession, nNode, nProp, szBefore.c_str() ) == BK_EDITOR_OK; };
	action.redo = action.apply;
	return action;
}

static bool SetSlots( BkResSession *pSession, int nNode, const std::vector<BkResPoint2> &slots )
{
	return BkResSetFormationPositions( pSession, nNode, slots.empty() ? 0 : slots.data(), int( slots.size() ) ) == BK_EDITOR_OK;
}

// The sequence: get-before, apply, get-after, undo, redo, save, reopen.
// Leaves the reopened copy open.
static void RunTool( BkResSession *pSession, const std::string &szScratchRoot, const char *pszExt, const std::string &szTool, const SAction &action )
{
	const std::string szTag = "s06-tool " + szTool + ": ";
	const std::string szBefore = State( pSession );
	if ( !Check( action.apply(), ( szTag + "apply" ).c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	const std::string szAfter = State( pSession );
	SameState( szTag + "apply changes the project", szBefore, szAfter, false );
	if ( Check( action.undo(), ( szTag + "undo" ).c_str() ) )
		SameState( szTag + "undo == before", szBefore, State( pSession ), true );
	else
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	if ( Check( action.redo(), ( szTag + "redo" ).c_str() ) )
		SameState( szTag + "redo == after", szAfter, State( pSession ), true );
	else
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

	std::string szSlug = szTool;
	std::replace( szSlug.begin(), szSlug.end(), ' ', '-' );
	const std::string szDir = szScratchRoot + "/s06-tools";
	const std::string szSaved = szDir + "/" + szSlug + "." + pszExt;
	std::error_code ec;
	std::filesystem::create_directories( szDir, ec );
	std::filesystem::remove( szSaved, ec );
	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, ( szTag + "save" ).c_str() ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	BkResClose( pSession );
	if ( Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, ( szTag + "reopen" ).c_str() ) )
		SameState( szTag + "save and reopen == after", szAfter, State( pSession ), true );
}

static bool Open( BkResSession *pSession, const std::string &szFixtureRoot, const char *pszExt )
{
	const std::string szIn = szFixtureRoot + "/" + pszExt + "/project." + pszExt;
	const bool bOpen = BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK;
	if ( !Check( bOpen, ( std::string( "s06-tool: opens the " ) + pszExt + " fixture" ).c_str() ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	return bOpen;
}

static void Squad( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	// Formation drag: three live writes while the mouse moves, one step
	// from the press to the release.
	if ( Open( pSession, szFixtureRoot, "scp" ) )
	{
		const int nFormation = FirstNodeOfType( pSession, kSquadFormationProps );
		const std::vector<BkResPoint2> pressed = SlotsOf( pSession, nFormation );
		Check( pressed.size() == 1, "s06-tool squad formation drag: the fixture has one slot" );
		std::vector<BkResPoint2> released = pressed;
		released.push_back( { 760.5f, 380.25f } );
		SAction action;
		action.apply = [=]()
		{
			std::vector<BkResPoint2> moving = released;
			moving[1] = { 740.0f, 370.0f };
			bool bOk = SetSlots( pSession, nFormation, moving );
			moving[1] = { 750.0f, 375.5f };
			bOk = SetSlots( pSession, nFormation, moving ) && bOk;
			return SetSlots( pSession, nFormation, released ) && bOk;
		};
		action.undo = [=]() { return SetSlots( pSession, nFormation, pressed ); };
		action.redo = [=]() { return SetSlots( pSession, nFormation, released ); };
		RunTool( pSession, szScratchRoot, "scp", "squad formation drag", action );
		BkResClose( pSession );
	}
	// Zero point.
	if ( Open( pSession, szFixtureRoot, "scp" ) )
	{
		const int nFormation = FirstNodeOfType( pSession, kSquadFormationProps );
		BkResPoint2 before = { 0, 0 };
		Check( BkResGetZeroPoint( pSession, nFormation, &before ) == BK_EDITOR_OK, "s06-tool squad zero point: get" );
		const BkResPoint2 after = { 600.5f, 300.25f };
		SAction action;
		action.apply = [=]() { return BkResSetZeroPoint( pSession, nFormation, &after ) == BK_EDITOR_OK; };
		action.undo = [=]() { return BkResSetZeroPoint( pSession, nFormation, &before ) == BK_EDITOR_OK; };
		action.redo = action.apply;
		RunTool( pSession, szScratchRoot, "scp", "squad zero point", action );
		Check( SameZero( pSession, FirstNodeOfType( pSession, kSquadFormationProps ), after ), "s06-tool squad zero point: reads the new point" );
		BkResClose( pSession );
	}
	// Direction arrow: FormationDir and the slots turned about the zero
	// point, one composite step (undo in reverse order).
	if ( Open( pSession, szFixtureRoot, "scp" ) )
	{
		const int nFormation = FirstNodeOfType( pSession, kSquadFormationProps );
		BkResPoint2 direction = { -1, -1 }, zero = { 0, 0 };
		Check( BkResGetFormationDirection( pSession, nFormation, &direction ) == BK_EDITOR_OK && direction.x == 0.25f && direction.y == 0,
			"s06-tool squad direction arrow: reads the fixture's FormationDir" );
		BkResGetZeroPoint( pSession, nFormation, &zero );
		const std::vector<BkResPoint2> before = SlotsOf( pSession, nFormation );
		const float fAngle = 0.25f + 3.14159265f / 2.0f;
		const float fDelta = fAngle - direction.x;
		std::vector<BkResPoint2> after;
		for ( const BkResPoint2 &slot : before )
		{
			const float dx = slot.x - zero.x, dy = slot.y - zero.y;
			after.push_back( { zero.x + dx * std::cos( fDelta ) - dy * std::sin( fDelta ), zero.y + dx * std::sin( fDelta ) + dy * std::cos( fDelta ) } );
		}
		const BkResPoint2 dirBefore = direction, dirAfter = { fAngle, 0 };
		SAction action;
		action.apply = [=]()
		{
			return BkResSetFormationDirection( pSession, nFormation, &dirAfter ) == BK_EDITOR_OK && SetSlots( pSession, nFormation, after );
		};
		action.undo = [=]()
		{
			return SetSlots( pSession, nFormation, before ) && BkResSetFormationDirection( pSession, nFormation, &dirBefore ) == BK_EDITOR_OK;
		};
		action.redo = action.apply;
		RunTool( pSession, szScratchRoot, "scp", "squad direction arrow", action );
		const std::string szSaved = szScratchRoot + "/s06-tools/squad-direction-arrow.scp";
		NResourceModel::Project project;
		const auto *pItem = dynamic_cast<const NResourceModel::CSquadFormationPropsItem *>( LoadItemOfType( szSaved, project, kSquadFormationProps ) );
		Check( pItem != 0 && std::fabs( pItem->fFormationDir - fAngle ) < 1e-5f,
			"s06-tool squad direction arrow: the S03 formation item reads the angle as its FormationDir" );
		BkResClose( pSession );
	}
	// No MFC home: the squad root takes none of the three channels, and a
	// refused set changes nothing.
	if ( Open( pSession, szFixtureRoot, "scp" ) )
	{
		const std::string szBefore = State( pSession );
		const int nRoot = RootOf( pSession );
		const BkResPoint2 point = { 1, 2 };
		Check( BkResSetFormationDirection( pSession, nRoot, &point ) == BK_EDITOR_REFUSED, "s06-tool no home: the squad root has no formation direction" );
		Check( BkResSetZeroPoint( pSession, nRoot, &point ) == BK_EDITOR_REFUSED, "s06-tool no home: the squad root has no zero point" );
		Check( BkResSetFormationPositions( pSession, nRoot, &point, 1 ) == BK_EDITOR_REFUSED, "s06-tool no home: the squad root has no slots" );
		BkResPoint2 read = { 0, 0 };
		Check( BkResGetFormationDirection( pSession, nRoot, &read ) == BK_EDITOR_REFUSED, "s06-tool no home: reading the root's direction is refused" );
		SameState( "s06-tool no home: a refused set leaves the project as it was", szBefore, State( pSession ), true );
		BkResClose( pSession );
	}
}

static void Weapon( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	if ( Open( pSession, szFixtureRoot, "wpn" ) )
	{
		const int nShootTypes = FirstNodeOfType( pSession, kWeaponShootTypes );
		RunTool( pSession, szScratchRoot, "wpn", "weapon shoot type insert",
			Insert( pSession, nShootTypes, kWeaponDamageProps, ChildCount( pSession, nShootTypes ), std::make_shared<int>( 0 ) ) );
		Check( ChildCount( pSession, FirstNodeOfType( pSession, kWeaponShootTypes ) ) == 2, "s06-tool weapon shoot type insert: two shoot types" );
		BkResClose( pSession );
	}
	if ( Open( pSession, szFixtureRoot, "wpn" ) )
	{
		RunTool( pSession, szScratchRoot, "wpn", "weapon shoot type delete", Delete( pSession, FirstNodeOfType( pSession, kWeaponDamageProps ) ) );
		Check( FirstNodeOfType( pSession, kWeaponDamageProps ) == 0, "s06-tool weapon shoot type delete: no shoot type left" );
		BkResClose( pSession );
	}
	struct SEdit { const char *pszTool; int nPartType; int nNth; const char *pszProp; const char *pszValue; };
	const SEdit kEdits[] = {
		{ "weapon damage edit", kWeaponDamageProps, -1, "Damage power", "40" },
		{ "weapon sound edit", kWeaponEffects, 0, "Human fire sound", "rifle_shot" },
		{ "weapon effect edit", kWeaponEffects, 0, "Gun fire effect", "gun_smoke" },
		{ "weapon flash edit", kWeaponFlashProps, 1, "Flash power", "250" },
	};
	for ( const SEdit &edit : kEdits )
	{
		if ( !Open( pSession, szFixtureRoot, "wpn" ) )
			continue;
		const int nShell = FirstNodeOfType( pSession, kWeaponDamageProps );
		const int nNode = edit.nNth < 0 ? nShell : NthChildOfType( pSession, nShell, edit.nPartType, edit.nNth );
		Check( nNode != 0, ( std::string( "s06-tool " ) + edit.pszTool + ": finds the part" ).c_str() );
		RunTool( pSession, szScratchRoot, "wpn", edit.pszTool, SetProp( pSession, nNode, edit.pszProp, edit.pszValue ) );
		BkResClose( pSession );
	}
	// Craters: insert one, then edit and delete it in the reopened copy.
	if ( Open( pSession, szFixtureRoot, "wpn" ) )
	{
		const int nCraters = FirstNodeOfType( pSession, kWeaponCraters );
		RunTool( pSession, szScratchRoot, "wpn", "weapon crater insert",
			Insert( pSession, nCraters, kWeaponCraterProps, ChildCount( pSession, nCraters ), std::make_shared<int>( 0 ) ) );
		const int nCrater = FirstNodeOfType( pSession, kWeaponCraterProps );
		if ( Check( nCrater != 0, "s06-tool weapon crater insert: the crater is there after reopen" ) )
		{
			RunTool( pSession, szScratchRoot, "wpn", "weapon crater edit", SetProp( pSession, nCrater, "Crater file", "craters\\big" ) );
			RunTool( pSession, szScratchRoot, "wpn", "weapon crater delete", Delete( pSession, FirstNodeOfType( pSession, kWeaponCraterProps ) ) );
			Check( FirstNodeOfType( pSession, kWeaponCraterProps ) == 0, "s06-tool weapon crater delete: no crater left" );
		}
		BkResClose( pSession );
	}
}

static void Trench( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	// The fixture's source lists: with embrasure, line (one source), ends, arcs.
	if ( Open( pSession, szFixtureRoot, "trc" ) )
	{
		const int nEnds = NthChildOfType( pSession, RootOf( pSession ), kTrenchSources, 2 );
		RunTool( pSession, szScratchRoot, "trc", "trench source add",
			Insert( pSession, nEnds, kTrenchSourceProps, ChildCount( pSession, nEnds ), std::make_shared<int>( 0 ) ) );
		Check( ChildCount( pSession, NthChildOfType( pSession, RootOf( pSession ), kTrenchSources, 2 ) ) == 1, "s06-tool trench source add: Trench ends has a source" );
		BkResClose( pSession );
	}
	if ( Open( pSession, szFixtureRoot, "trc" ) )
	{
		RunTool( pSession, szScratchRoot, "trc", "trench source remove", Delete( pSession, FirstNodeOfType( pSession, kTrenchSourceProps ) ) );
		Check( FirstNodeOfType( pSession, kTrenchSourceProps ) == 0, "s06-tool trench source remove: no source left" );
		BkResClose( pSession );
	}
}

// S07 T09: the frame tools. A thumbnail double-click inserts a frame item and
// names it after the picture (one undo step); redo restores it from the blob,
// name included.
static SAction InsertNamed( BkResSession *pSession, int nParent, int nClass, int nIndex, const std::string &szName )
{
	auto pId = std::make_shared<int>( 0 );
	SAction action = Insert( pSession, nParent, nClass, nIndex, pId );
	const std::function<bool()> insert = action.apply;
	action.apply = [=]() { return insert() && BkResSetNodeName( pSession, *pId, szName.c_str() ) == BK_EDITOR_OK; };
	return action;
}

static std::string NameOf( BkResSession *pSession, int nNode )
{
	for ( const BkResNodeRecord &n : AllNodes( pSession ) )
		if ( n.id == nNode )
			return n.display_name;
	return "";
}

static void Frames( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	if ( Open( pSession, szFixtureRoot, "spt" ) )
	{
		const int nSprites = FirstNodeOfType( pSession, kSpritesItem );
		const int nBefore = ChildCount( pSession, nSprites );
		RunTool( pSession, szScratchRoot, "spt", "sprite add frame", InsertNamed( pSession, nSprites, kSpriteProps, nBefore, "walk_07" ) );
		Check( ChildCount( pSession, FirstNodeOfType( pSession, kSpritesItem ) ) == nBefore + 1, "s07-tool sprite add frame: one more frame" );
		Check( NameOf( pSession, NthChildOfType( pSession, FirstNodeOfType( pSession, kSpritesItem ), kSpriteProps, nBefore ) ) == "walk_07", "s07-tool sprite add frame: the frame is named after its picture, also after save and reopen" );
		BkResClose( pSession );
	}
	if ( Open( pSession, szFixtureRoot, "spt" ) )
	{
		RunTool( pSession, szScratchRoot, "spt", "sprite delete frame", Delete( pSession, FirstNodeOfType( pSession, kSpriteProps ) ) );
		Check( FirstNodeOfType( pSession, kSpriteProps ) == 0, "s07-tool sprite delete frame: no frame left" );
		BkResClose( pSession );
	}
	const struct { const char *pszTool; const char *pszProp; const char *pszValue; } kSpriteEdits[] = {
		{ "sprite directory", "Directory", "units\\walk\\" }, { "sprite frame time", "Frame time", "80" },
		{ "sprite x position", "X position", "16" }, { "sprite y position", "Y position", "48" },
	};
	for ( const auto &edit : kSpriteEdits )
		if ( Open( pSession, szFixtureRoot, "spt" ) )
		{
			RunTool( pSession, szScratchRoot, "spt", edit.pszTool, SetProp( pSession, FirstNodeOfType( pSession, kSpritesItem ), edit.pszProp, edit.pszValue ) );
			BkResClose( pSession );
		}

	if ( Open( pSession, szFixtureRoot, "unt" ) )
	{
		const int nAnimation = FirstNodeOfType( pSession, kUnitAnimationProps );
		const int nBefore = ChildCount( pSession, nAnimation );
		RunTool( pSession, szScratchRoot, "unt", "infantry add frame", InsertNamed( pSession, nAnimation, kUnitFrameProps, nBefore, "run_09" ) );
		Check( NameOf( pSession, NthChildOfType( pSession, FirstNodeOfType( pSession, kUnitAnimationProps ), kUnitFrameProps, nBefore ) ) == "run_09", "s07-tool infantry add frame: the frame is named after its picture" );
		BkResClose( pSession );
	}
	if ( Open( pSession, szFixtureRoot, "unt" ) )
	{
		const int nFrame = FirstNodeOfType( pSession, kUnitFrameProps );
		if ( nFrame != 0 )
		{
			RunTool( pSession, szScratchRoot, "unt", "infantry delete frame", Delete( pSession, nFrame ) );
			Check( ChildCount( pSession, FirstNodeOfType( pSession, kUnitAnimationProps ) ) >= 0, "s07-tool infantry delete frame: the animation stays" );
		}
		BkResClose( pSession );
	}
	const struct { const char *pszTool; const char *pszProp; const char *pszValue; } kAnimationEdits[] = {
		{ "infantry frame time", "Frame time", "60" }, { "infantry action frame", "Action frame", "2" },
		{ "infantry animation speed", "Animation speed", "2" }, { "infantry is cycled", "Is cycled?", "true" },
	};
	for ( const auto &edit : kAnimationEdits )
		if ( Open( pSession, szFixtureRoot, "unt" ) )
		{
			RunTool( pSession, szScratchRoot, "unt", edit.pszTool, SetProp( pSession, FirstNodeOfType( pSession, kUnitAnimationProps ), edit.pszProp, edit.pszValue ) );
			BkResClose( pSession );
		}
	if ( Open( pSession, szFixtureRoot, "unt" ) )
	{
		const int nSeason = FirstNodeOfType( pSession, kUnitSeasonProps );
		RunTool( pSession, szScratchRoot, "unt", "infantry season dir", SetProp( pSession, NthChildOfType( pSession, nSeason, 0x11000000 + 7, 1 ), "Directory", "units\\up\\" ) );
		BkResClose( pSession );
	}
}

static void Run( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	std::error_code ec;
	std::filesystem::remove_all( szScratchRoot + "/s06-tools", ec );
	Squad( pSession, szFixtureRoot, szScratchRoot );
	Weapon( pSession, szFixtureRoot, szScratchRoot );
	Trench( pSession, szFixtureRoot, szScratchRoot );
	Frames( pSession, szFixtureRoot, szScratchRoot );
}

}

// S06 T01: the trench exporter against the shipped entrenchment. A trench
// project naming the eight shipped segment models in the folders and order
// of Data/Units/Technics/Common/Entrenchment/1.xml is exported through
// BkResExport, whose mesh reader builds each model with the engine's
// IVisObjBuilder as CTrenchFrame::SaveRPGStats did. The engine reads the
// export and the shipped file into SEntrenchmentRPGStats and every segment
// must match: model, type, coverage, bounding box and fire places. The
// shipped file holds MFC's six-digit "%g" floats, so values agree to that.

namespace S06Export
{

static bool Near( float a, float b )
{
	return std::fabs( a - b ) <= 1e-5f * std::max( 1.0f, std::fabs( b ) ) * 10.0f;
}

// One <item> source block per model, in the folder order MFC numbers them.
static std::string TrenchProject( const std::string &szFixture )
{
	std::string szXml = szFixture;
	const std::string::size_type nStart = szXml.find( "<item ClassTypeID=\"285212826\"" );
	const std::string::size_type nEnd = szXml.find( "</item>", szXml.find( "<childs/>", nStart ) ) + std::string( "</item>" ).size();
	std::string szTemplate = szXml.substr( nStart, nEnd - nStart );
	szXml.erase( nStart, nEnd - nStart );
	const std::string szIndex = " TrenchIndex=\"0\"";
	szTemplate.erase( szTemplate.find( szIndex ), szIndex.size() );
	szTemplate.replace( szTemplate.find( "float_value=\"0.2\"" ), std::string( "float_value=\"0.2\"" ).size(), "float_value=\"1\"" );
	auto Segment = [&]( const char *pszModel )
	{
		std::string szItem = szTemplate;
		szItem.replace( szItem.find( "<string_value/>" ), std::string( "<string_value/>" ).size(), std::string( "<string_value>" ) + pszModel + ".mod</string_value>" );
		return szItem;
	};
	const char *const kFolders[] = { "Trenches with embrasure", "Trenches line", "Trench ends", "Trench arcs" };
	const std::vector<std::vector<const char *>> models = { { "2", "7", "8" }, { "1", "5", "6" }, { "3" }, { "4" } };
	for ( int i = 0; i < 4; ++i )
	{
		std::string szChilds = "<childs>";
		for ( const char *pszModel : models[i] )
			szChilds += Segment( pszModel );
		szChilds += "</childs>";
		const std::string::size_type nName = szXml.find( std::string( "<default_name>" ) + kFolders[i] + "</default_name>" );
		const std::string::size_type nChilds = szXml.find( "<childs", nName );
		const std::string::size_type nChildsEnd = szXml.compare( nChilds, 9, "<childs/>" ) == 0 ? nChilds + 9
			: szXml.find( "</childs>", nChilds ) + std::string( "</childs>" ).size();
		szXml.replace( nChilds, nChildsEnd - nChilds, szChilds );
	}
	auto ReplaceAfter = [&]( const std::string &szAnchor, const std::string &szOld, const std::string &szNew )
	{
		const std::string::size_type nAt = szXml.find( szOld, szXml.find( szAnchor ) );
		szXml.replace( nAt, szOld.size(), szNew );
	};
	ReplaceAfter( "<default_name>Name</default_name>", "<string_value>Unknown Trench</string_value>", "<string_value/>" );
	ReplaceAfter( "<default_name>Health</default_name>", "int_value=\"100\"", "int_value=\"1000\"" );
	ReplaceAfter( "<default_name>Top</default_name>", "int_value=\"300\"", "int_value=\"0\"" );
	ReplaceAfter( "<default_name>Top</default_name>", "int_value=\"300\"", "int_value=\"0\"" );
	return szXml;
}

static void ShippedTrench( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s06-export";
	fs::remove_all( scratch, ec );
	const fs::path projectDir = scratch / "entrenchment";
	fs::create_directories( projectDir, ec );
	const fs::path shipped = T11::FoldedPath( fs::path( szRoot ) / "Data", "Units/Technics/Common/Entrenchment" );
	for ( int i = 1; i <= 8; ++i )
		fs::copy_file( shipped / ( std::to_string( i ) + ".mod" ), projectDir / ( std::to_string( i ) + ".mod" ), fs::copy_options::overwrite_existing, ec );
	// The pictures the exporter converts from the first model's folder.
	for ( const char *pszName : { "1.tga", "1w.tga", "1a.tga" } )
		fs::copy_file( fs::path( szFixtureRoot ) / "trc" / pszName, projectDir / pszName, fs::copy_options::overwrite_existing, ec );
	std::string szFixture;
	ReadBytes( szFixtureRoot + "/trc/project.trc", szFixture );
	const fs::path project = projectDir / "project.trc";
	T10::WriteText( project, TrenchProject( szFixture ) );

	const fs::path modDir = scratch / "mod";
	BkResModSettings mod = {};
	std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
	std::snprintf( mod.name, sizeof( mod.name ), "S06 export" );
	Check( BkResModSettingsSet( pSession, &mod ) == BK_EDITOR_OK, "s06-export: the mod folder is set" );
	if ( !Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "s06-export: the trench built from the shipped models opens" ) )
		return;
	BkResExportReport report = {};
	if ( !Check( BkResExport( pSession, BK_RES_EXPORT_FORCE, &report ) == BK_EDITOR_OK && report.warning_count == 0,
	             "s06-export: the trench exports with every segment model built by the engine, no warning" ) )
		std::printf( "   detail: %s (warnings %d)\n", BkEditorLastMessage( pSession ), report.warning_count );
	BkResClose( pSession );

	SEntrenchmentRPGStats port, game;
	const bool bPort = ReadChunkAsMfc( ( modDir / "data/units/technics/common/entrenchment/entrenchment/1.xml" ).string(), "base", "RPG", port );
	const bool bGame = ReadChunkAsMfc( ( shipped / "1.xml" ).string(), "base", "RPG", game );
	if ( !Check( bPort && bGame && port.segments.size() == game.segments.size() && port.segments.size() == 8,
	             "s06-export: the engine reads 8 segments from the export and the shipped 1.xml" ) )
		return;
	Check( port.lines == game.lines && port.fireplaces == game.fireplaces && port.terminators == game.terminators && port.arcs == game.arcs,
	       "s06-export: the line, fire place, terminator and arc lists are the shipped ones" );
	Check( port.fMaxHP == game.fMaxHP && port.szKeyName == game.szKeyName && port.defences[RPG_TOP].nArmorMax == game.defences[RPG_TOP].nArmorMax &&
	       port.defences[RPG_FRONT].nArmorMin == game.defences[RPG_FRONT].nArmorMin && port.defences[RPG_BACK].fSilhouette == game.defences[RPG_BACK].fSilhouette,
	       "s06-export: health, name and defences are the shipped ones" );
	for ( std::size_t i = 0; i < port.segments.size(); ++i )
	{
		const SEntrenchmentRPGStats::SSegmentRPGStats &a = port.segments[i], &b = game.segments[i];
		bool bFire = a.fireplaces.size() == b.fireplaces.size();
		for ( std::size_t f = 0; bFire && f < a.fireplaces.size(); ++f )
			bFire = Near( a.fireplaces[f].x, b.fireplaces[f].x ) && Near( a.fireplaces[f].y, b.fireplaces[f].y );
		const bool bBox = Near( a.vAABBCenter.x, b.vAABBCenter.x ) && Near( a.vAABBCenter.y, b.vAABBCenter.y ) && Near( a.vAABBHalfSize.x, b.vAABBHalfSize.x ) &&
		                  Near( a.vAABBHalfSize.y, b.vAABBHalfSize.y ) && Near( a.vAABBHalfSize.z, b.vAABBHalfSize.z );
		if ( !Check( a.szModel == b.szModel && a.eType == b.eType && a.fCoverage == b.fCoverage && bBox && bFire,
		             ( "s06-export: segment " + std::to_string( i ) + " (model " + b.szModel + ") has the shipped type, coverage, box and " +
		               std::to_string( b.fireplaces.size() ) + " fire places" ).c_str() ) )
			std::printf( "   detail: port model %s type %d box %g %g %g fire places %d%s\n", a.szModel.c_str(), int( a.eType ), a.vAABBHalfSize.x, a.vAABBHalfSize.y,
			             a.vAABBHalfSize.z, int( a.fireplaces.size() ), a.fireplaces.empty() ? "" : ( " first " + std::to_string( a.fireplaces[0].x ) + "," +
			             std::to_string( a.fireplaces[0].y ) ).c_str() );
	}
}

// B-03.6, B-04.3, B-05.3, B-13.4: a shipped runtime stats file imported into
// a new project of its kind, the project saved, exported stats-only, and the
// exported file compared with the shipped one through the comparator, both
// read by the engine's own readers. A field MFC's frame could not round-trip
// is in the comparator's kRoundTripLosses with its reason and printed as
// EXCUSED; anything else fails the case and prints its field path.
struct SRoundTrip
{
	const char *pszCase;            // named in the tier output
	BkResKind kind;
	const char *pszExtension;
	NResourceModel::EExportKind exportKind;
	const char *pszShipped;         // below Data/, matched case-insensitively
	bool bFlatFile;                 // a weapon is one file, not a folder holding 1.xml
	const char *pszExported;        // below the mod's data/, with <folder> for the project's folder
};

static const SRoundTrip kRoundTrips[] = {
	{ "wpn weapons/mg_37t.xml", 0, "wpn", NResourceModel::EExportKind::WEAPON, "Weapons/mg_37t.xml", true, "weapons/<folder>.xml" },
	{ "mcp mine/mine_at", 1, "mcp", NResourceModel::EExportKind::MINE, "Objects/SimpleObjects/common/summer/mine/mine_at", false,
	  "objects/simpleobjects/common/summer/mine/<folder>/1.xml" },
	{ "trc entrenchment", 2, "trc", NResourceModel::EExportKind::ENTRENCHMENT, "Units/Technics/Common/Entrenchment", false,
	  "units/technics/common/entrenchment/<folder>/1.xml" },
	{ "scp squads/german_rifle_45", 3, "scp", NResourceModel::EExportKind::SQUAD, "Squads/german_rifle_45", false, "squads/<folder>/1.xml" },
	{ "unt humans/german/gunner", 5, "unt", NResourceModel::EExportKind::INFANTRY, "Units/Humans/German/Gunner", false, "units/humans/<folder>/1.xml" },
};

static void RoundTripOne( BkResSession *pSession, const SRoundTrip &trip, const std::string &szRoot, const std::filesystem::path &scratch )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const std::string szCase = std::string( "roundtrip " ) + trip.pszCase;
	const fs::path shipped = T11::FoldedPath( fs::path( szRoot ) / "Data", trip.pszShipped );
	// The sample is copied: nothing below reads or writes Data/ twice, and
	// the import is of the copy.
	const fs::path copyDir = scratch / trip.pszExtension / "shipped";
	fs::create_directories( copyDir, ec );
	fs::path imported = copyDir;
	if ( trip.bFlatFile )
	{
		imported = copyDir / shipped.filename();
		fs::copy_file( shipped, imported, fs::copy_options::overwrite_existing, ec );
	}
	else
		fs::copy_file( T11::FoldedPath( shipped, "1.xml" ), copyDir / "1.xml", fs::copy_options::overwrite_existing, ec );
	const fs::path shippedCopy = trip.bFlatFile ? imported : copyDir / "1.xml";
	if ( !Check( !ec && fs::is_regular_file( shippedCopy, ec ), ( szCase + ": the shipped sample " + shipped.string() + " is copied" ).c_str() ) )
		return;

	if ( !Check( BkResImportFromGame( pSession, trip.kind, imported.string().c_str() ) == BK_EDITOR_OK, ( szCase + ": imported into a new project" ).c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	const fs::path projectDir = scratch / trip.pszExtension / "project";
	fs::create_directories( projectDir, ec );
	const fs::path project = projectDir / ( std::string( "project." ) + trip.pszExtension );
	if ( !Check( BkResSave( pSession, project.string().c_str() ) == BK_EDITOR_OK, ( szCase + ": the imported project saves" ).c_str() ) )
		return;
	const fs::path modDir = scratch / trip.pszExtension / "mod";
	BkResModSettings mod = {};
	std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
	std::snprintf( mod.name, sizeof( mod.name ), "S06 round trip" );
	Check( BkResModSettingsSet( pSession, &mod ) == BK_EDITOR_OK, ( szCase + ": the mod folder is set" ).c_str() );
	BkResExportReport report = {};
	if ( !Check( BkResExportStatsOnly( pSession, BK_RES_EXPORT_FORCE, &report ) == BK_EDITOR_OK && report.written >= 1, ( szCase + ": exported stats-only" ).c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		return;
	}
	BkResClose( pSession );

	std::string szExported = trip.pszExported;
	szExported.replace( szExported.find( "<folder>" ), 8, "project" );
	const fs::path exported = modDir / "data" / szExported;
	const NResourceModel::SCompareResult result = NResourceModel::CompareRoundTrip( trip.exportKind, exported.string(), shippedCopy.string() );
	std::printf( "ROUNDTRIP %s: shipped %s, %s, %d fields compared\n", trip.pszCase, trip.pszShipped, NResourceModel::CompareStatusName( result.status ), result.nFieldsCompared );
	for ( const std::string &szExcused : result.excused )
		std::printf( "   EXCUSED %s\n", szExcused.c_str() );
	for ( const std::string &szMessage : result.messages )
		std::printf( "   DIFFERENT %s\n", szMessage.c_str() );
	Check( result.status == NResourceModel::ECompareStatus::EQUAL,
	       ( szCase + ": the exported stats equal the shipped file" + ( result.messages.empty() ? "" : ", first difference " + result.messages[0] ) ).c_str() );
}

static void RoundTrips( BkResSession *pSession, const std::string &szRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s06-roundtrip";
	fs::remove_all( scratch, ec );
	std::string szError;
	if ( !Check( NResourceModel::StartEngineReaders( &szError ), ( "roundtrip: the comparator's engine readers start " + szError ).c_str() ) )
		return;
	for ( const SRoundTrip &trip : kRoundTrips )
		RoundTripOne( pSession, trip, szRoot, scratch );
}

// Every .unt that ships (Data/Old and the WinSniper test project), copied,
// opened and exported: no source TGAs ship, so no .san is composed and the
// compose result is a warning, as in MFC. The stats are still written and the
// engine's own reader must read them.
static void UntRoundTrips( BkResSession *pSession, const std::string &szRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s07-unt-roundtrip";
	fs::remove_all( scratch, ec );
	std::vector<fs::path> sources;
	for ( fs::recursive_directory_iterator it( fs::path( szRoot ) / "Data" / "Old", ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->is_regular_file( ec ) && it->path().extension() == ".unt" )
			sources.push_back( it->path() );
	std::sort( sources.begin(), sources.end() );
	Check( sources.size() == 14, ( "unt round trips: Data/Old holds 14 .unt (" + std::to_string( sources.size() ) + ")" ).c_str() );
	sources.push_back( T11::FoldedPath( fs::path( szRoot ) / "Data", "Editor/TestProjects/02_InfantryAnimation/WinSniper.unt" ) );
	int nIndex = 0;
	for ( const fs::path &source : sources )
	{
		const std::string szCase = "unt round trip " + source.filename().string();
		const fs::path dir = scratch / std::to_string( nIndex++ );
		const fs::path project = dir / "project" / source.filename();
		fs::create_directories( project.parent_path(), ec );
		fs::copy_file( source, project, fs::copy_options::overwrite_existing, ec );
		if ( !Check( !ec && BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, ( szCase + ": opens" ).c_str() ) )
			continue;
		const fs::path modDir = dir / "mod";
		BkResModSettings mod = {};
		std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
		std::snprintf( mod.name, sizeof( mod.name ), "S07 unt round trip" );
		BkResModSettingsSet( pSession, &mod );
		BkResWarning warnings[16] = {};
		BkResExportReport report = {};
		report.warnings = warnings;
		report.warnings_capacity = 16;
		const BkEditorStatus status = BkResExport( pSession, BK_RES_EXPORT_FORCE, &report );
		BkResClose( pSession );
		if ( !Check( status == BK_EDITOR_OK, ( szCase + ": exports " + BkEditorLastMessage( pSession ) ).c_str() ) )
			continue;
		fs::path xml;
		int nSan = 0;
		for ( fs::recursive_directory_iterator it( modDir / "data", ec ), end; !ec && it != end; it.increment( ec ) )
		{
			if ( !it->is_regular_file( ec ) )
				continue;
			if ( it->path().filename() == "1.xml" )
				xml = it->path();
			if ( it->path().extension() == ".san" )
				++nSan;
		}
		SInfantryRPGStats stats;
		Check( !xml.empty() && ReadChunkAsMfc( xml.string(), "base", "RPG", stats ), ( szCase + ": 1.xml is written and the engine reads it as SInfantryRPGStats" ).c_str() );
		Check( nSan == 0 && report.warning_count >= 1, ( szCase + ": no .san is composed (no source TGAs ship) and the compose result is a warning" ).c_str() );
	}
}

// A shipped human imported and exported stats-only must equal the shipped
// 1.xml. MFC's FillRPGStats writes some fields as constants, so a unit that
// MFC did not make with those constants differs there; they are listed in the
// comparator's kRoundTripLosses (fRotateSpeed, nPriority, nUninstallRotate,
// nUninstallTransport, animdescs nAABB_A / nAABB_D) and printed as EXCUSED.
// The round trip of Gunner is the kRoundTrips entry "unt humans/german/gunner".

static void Run( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	ShippedTrench( pSession, szRoot, szFixtureRoot, szScratchRoot );
	RoundTrips( pSession, szRoot, szScratchRoot );
	UntRoundTrips( pSession, szRoot, szScratchRoot );
}

}

// S06 T05: the preview captures of the mine, trench and squad (D015: MFC's
// weapon frame draws nothing, so a weapon has none). Each runs the real
// exporter into the preview folder and draws on the empty scene, measured by
// code like T11's captures: neither black nor magenta, and different from the
// empty frame. The mine is the composed 16x16 sprite of its fixture, the
// trench the shipped entrenchment models, the squad the first formation of the
// shipped german_rifle_45 imported into a project.

namespace S06Preview
{

struct SCase
{
	const char *pszLabel;
	int nKind;
	double fMinChanged;     // the share of the frame the object must change
};

static void Capture( BkResSession *pSession, const SCase &c, const std::filesystem::path &project, const std::filesystem::path &scratch, const std::string &szFixtureRoot )
{
	namespace fs = std::filesystem;
	const std::string szLabel = std::string( "preview " ) + c.pszLabel;
	if ( !Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, ( szLabel + ": the project opens" ).c_str() ) )
		return;
	Check( BkResPreviewBegin( pSession, c.nKind ) == BK_EDITOR_OK, ( szLabel + ": Begin" ).c_str() );
	const fs::path empty = scratch / ( std::string( c.pszLabel ) + "-empty.tga" );
	Check( BkEditorCaptureFrame( pSession, empty.string().c_str() ) == BK_EDITOR_OK, ( szLabel + ": the empty frame captures" ).c_str() );
	const BkEditorStatus nShow = BkResPreviewShow( pSession );
	Check( nShow == BK_EDITOR_OK, ( szLabel + ": Show: " + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkResPreviewPlayback( pSession, 1 ) == BK_EDITOR_OK, ( szLabel + ": Run" ).c_str() );
	const auto start = std::chrono::steady_clock::now();
	while ( std::chrono::steady_clock::now() - start < std::chrono::milliseconds( 500 ) )
		BkEditorFrame( pSession );
	const fs::path tga = scratch / ( std::string( c.pszLabel ) + ".tga" );
	const BkEditorStatus nCapture = BkEditorCaptureFrame( pSession, tga.string().c_str() );
	Check( BkResPreviewPlayback( pSession, 0 ) == BK_EDITOR_OK, ( szLabel + ": Stop playback" ).c_str() );
	std::vector<unsigned char> emptyRgb, rgb;
	int nW = 0, nH = 0;
	const bool bRead = nCapture == BK_EDITOR_OK && T11::ReadCapture( tga.string(), rgb, nW, nH ) && T11::ReadCapture( empty.string(), emptyRgb, nW, nH );
	Check( bRead, ( szLabel + ": the capture reads back" ).c_str() );
	const double fShare = bRead ? T11::NonBlackNonMagentaShare( rgb ) : -1.0;
	const double fChanged = bRead ? T11::ChangedShare( rgb, emptyRgb ) : -1.0;
	std::printf( "preview-scene: %s show_status=%d capture_status=%d non-black-non-magenta=%f changed-vs-empty=%f path=%s\n",
	             c.pszLabel, int( nShow ), int( nCapture ), fShare, fChanged, tga.string().c_str() );
	Check( fShare >= 0.01, ( szLabel + ": the capture is >= 1% non-black-non-magenta" ).c_str() );
	Check( fChanged >= c.fMinChanged, ( szLabel + ": the object drew (the frame changed by at least " + std::to_string( c.fMinChanged ) + ")" ).c_str() );
	Check( BkResPreviewStop( pSession ) == BK_EDITOR_OK, ( szLabel + ": Stop" ).c_str() );
	BkResClose( pSession );
	(void)szFixtureRoot;
}

static void Run( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s06-preview";
	fs::remove_all( scratch, ec );
	fs::create_directories( scratch, ec );
	const fs::path dataRoot = fs::path( szRoot ) / "Data";

	Check( BkResPreviewBegin( pSession, 0 ) == BK_EDITOR_REFUSED, "preview: a weapon still has no preview" );

	// The mine: the fixture's pictures beside the project, composed on export.
	{
		const fs::path dir = scratch / "mcp";
		fs::create_directories( dir, ec );
		for ( const char *pszName : { "project.mcp", "1.tga", "1s.tga" } )
			fs::copy_file( fs::path( szFixtureRoot ) / "mcp" / pszName, dir / pszName, fs::copy_options::overwrite_existing, ec );
		Capture( pSession, { "mine", 1, 0.0001 }, dir / "project.mcp", scratch, szFixtureRoot );
	}

	// The trench: the eight shipped models, as S06Export builds it.
	{
		const fs::path dir = scratch / "trc";
		fs::create_directories( dir, ec );
		const fs::path shipped = T11::FoldedPath( dataRoot, "Units/Technics/Common/Entrenchment" );
		for ( int i = 1; i <= 8; ++i )
			fs::copy_file( shipped / ( std::to_string( i ) + ".mod" ), dir / ( std::to_string( i ) + ".mod" ), fs::copy_options::overwrite_existing, ec );
		for ( const char *pszName : { "1.tga", "1w.tga", "1a.tga" } )
			fs::copy_file( fs::path( szFixtureRoot ) / "trc" / pszName, dir / pszName, fs::copy_options::overwrite_existing, ec );
		std::string szFixture;
		ReadBytes( szFixtureRoot + "/trc/project.trc", szFixture );
		T10::WriteText( dir / "project.trc", S06Export::TrenchProject( szFixture ) );
		Capture( pSession, { "trench", 2, 0.0001 }, dir / "project.trc", scratch, szFixtureRoot );
	}

	// The squad: the shipped german_rifle_45 imported into a project.
	{
		const fs::path dir = scratch / "scp";
		fs::create_directories( dir / "shipped", ec );
		fs::copy_file( T11::FoldedPath( dataRoot, "Squads/german_rifle_45/1.xml" ), dir / "shipped" / "1.xml", fs::copy_options::overwrite_existing, ec );
		if ( Check( BkResImportFromGame( pSession, 3, ( dir / "shipped" ).string().c_str() ) == BK_EDITOR_OK, "preview squad: the shipped squad imports" ) )
		{
			const fs::path project = dir / "project.scp";
			Check( BkResSave( pSession, project.string().c_str() ) == BK_EDITOR_OK, "preview squad: the imported project saves" );
			BkResClose( pSession );
			Capture( pSession, { "squad", 3, 0.0001 }, project, scratch, szFixtureRoot );
		}
	}
}

}

// S07 T05: the portable BuildAnimations (ResourceModel/compose.cpp) on the
// hosted engine. Frames are small generated TGAs written below the scratch
// root; the .san is read back with the engine's structure loader, the DDS
// through NDxt, and the writer is proved against a shipped human's 1.san.

namespace S07Compose
{

static void WriteTga( const std::filesystem::path &file, int nSize, int nIndex )
{
	std::string szBytes( 18, '\0' );
	szBytes[2] = 2;
	szBytes[12] = char( nSize ); szBytes[14] = char( nSize );
	szBytes[16] = 32;
	szBytes[17] = 0x28;  // 8 alpha bits, origin top-left
	for ( int y = 0; y < nSize; ++y )
		for ( int x = 0; x < nSize; ++x )
		{
			const bool bInside = x >= 2 + nIndex && x < nSize - 3 && y >= 3 && y < nSize - 2 - nIndex;
			const unsigned char b = 100, g = bInside ? (unsigned char)120 : 0, r = bInside ? (unsigned char)200 : 0, a = bInside ? 255 : 0;
			szBytes += char( b ); szBytes += char( g ); szBytes += char( r ); szBytes += char( a );
		}
	std::ofstream( file, std::ios::binary ) << szBytes;
}

static bool LoadSan( const std::filesystem::path &file, SSpriteAnimationFormat &fmt )
{
	const std::string szDir = file.parent_path().string() + "/";
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( file.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &fmt );
	return true;
}

static NResourceModel::NCompose::SAnimationDesc Animation( const char *pszName, std::vector<std::vector<short>> dirs )
{
	NResourceModel::NCompose::SAnimationDesc desc;
	desc.szName = pszName;
	desc.nFrameTime = 100;
	desc.fSpeed = 1.5f;
	desc.bCycled = true;
	desc.frames[0] = CVec2( 0, 0 );
	for ( const std::vector<short> &frames : dirs )
	{
		desc.dirs.emplace_back();
		desc.dirs.back().frames = frames;
	}
	return desc;
}

// One compose into <root>/<szSub>: the .san and the three DDS, as the sprite
// exporter will write them. False with the outcome's error printed.
static bool ComposeInto( const std::filesystem::path &root, const char *pszSub, std::vector<NResourceModel::NCompose::SAnimationDesc> descs,
                         const std::vector<std::string> &files, DWORD dwMinAlpha, SSpriteAnimationFormat &fmt, CPtr<IImage> *ppPacked = 0 )
{
	NResourceModel::SExportContext context;
	context.szStagingRoot = ( root / pszSub ).string();
	NResourceModel::SExportOutcome outcome;
	CPtr<IImage> pPacked = NResourceModel::NCompose::BuildAnimations( &descs, &fmt, files, true, dwMinAlpha, outcome );
	bool bOk = pPacked != 0 &&
	           NResourceModel::NImageExport::SaveCompressedTexture( context, pPacked, "1", NResourceModel::NImageExport::SGamma(), GFXPF_ARGB4444, outcome ) &&
	           NResourceModel::NImageExport::SaveAnimation( context, fmt, "1.san", outcome );
	if ( !bOk )
		std::printf( "   detail: %s\n", outcome.szError.c_str() );
	if ( ppPacked != 0 )
		*ppPacked = pPacked;
	return bOk;
}

static void Run( const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s07-compose";
	fs::remove_all( scratch, ec );
	fs::create_directories( scratch / "frames", ec );
	std::vector<std::string> files;
	for ( int i = 0; i < 3; ++i )
	{
		const fs::path frame = scratch / "frames" / ( std::to_string( i ) + ".tga" );
		WriteTga( frame, 16, i );
		files.push_back( frame.string() );
	}

	// Check the action table against the engine's ANIMATION_* slots.
	bool bKnown = false;
	Check( NResourceModel::NCompose::GetActionFromName( "Idle", &bKnown ) == ANIMATION_IDLE && bKnown, "compose: Idle is ANIMATION_IDLE" );
	Check( NResourceModel::NCompose::GetActionFromName( "Death", &bKnown ) == ANIMATION_DEATH && bKnown, "compose: Death is ANIMATION_DEATH" );
	Check( NResourceModel::NCompose::GetActionFromName( "RUN", &bKnown ) == ANIMATION_MOVE && bKnown, "compose: names ignore case, Run is ANIMATION_MOVE" );
	Check( NResourceModel::NCompose::GetActionFromName( "default" ) == ANIMATION_IDLE && NResourceModel::NCompose::GetActionFromName( "prisoning" ) == ANIMATION_PRISONING,
	       "compose: default maps to idle, prisoning to ANIMATION_PRISONING" );
	Check( NResourceModel::NCompose::GetActionFromName( "no such animation", &bKnown ) == 0 && !bKnown, "compose: an unknown name is slot 0 and reported unknown" );

	// (a) three frames, one animation, one direction.
	{
		SSpriteAnimationFormat written;
		CPtr<IImage> pPacked;
		if ( !Check( ComposeInto( scratch, "a", { Animation( "Idle", { { 0, 1, 2 } } ) }, files, 0, written, &pPacked ), "compose (a): three frames compose and write" ) )
			return;
		for ( const char *pszName : { "1.san", "1_c.dds", "1_l.dds", "1_h.dds" } )
			Check( fs::is_regular_file( scratch / "a" / pszName, ec ), ( std::string( "compose (a): " ) + pszName + " is written" ).c_str() );
		SSpriteAnimationFormat fmt;
		if ( !Check( LoadSan( scratch / "a" / "1.san", fmt ), "compose (a): the engine reads the .san back" ) )
			return;
		bool bShape = fmt.animations.size() == 1 && fmt.animations[0].dirs.size() == 1 && fmt.animations[0].rects.size() == 3;
		if ( !Check( bShape, "compose (a): one animation at ANIMATION_IDLE, one direction, three rects" ) )
			return;
		const SSpriteAnimationFormat::SSpriteAnimation &anim = fmt.animations[ANIMATION_IDLE];
		Check( anim.dirs[0].frames == std::vector<short>( { 0, 1, 2 } ), "compose (a): the frame list is 0 1 2" );
		Check( anim.nFrameTime == 100 && anim.fSpeed == 1.5f && anim.bCycled, "compose (a): frame time, speed and cycled are kept" );
		bool bRects = true, bDepth = true;
		for ( int i = 0; i < 3; ++i )
		{
			const SSpriteRect &rect = anim.rects[i];
			const float fWidth = ( rect.maps.x2 - rect.maps.x1 ) * pPacked->GetSizeX(), fHeight = ( rect.maps.y2 - rect.maps.y1 ) * pPacked->GetSizeY();
			// Frame i's picture is 10-i wide and 10-i high inside the transparent border.
			bRects = bRects && rect.rect.maxx - rect.rect.minx == 10 - i && rect.rect.maxy - rect.rect.miny == 10 - i &&
			         std::fabs( fWidth - ( 10 - i ) ) < 0.01f && std::fabs( fHeight - ( 10 - i ) ) < 0.01f;
			bDepth = bDepth && rect.fDepthLeft == 0.0f && rect.fDepthRight == 0.0f;
		}
		Check( bRects, "compose (a): each rect is the cropped picture, and agrees with its texture mapping" );
		Check( bDepth, "compose (a): fDepth is 0 without a minimum alpha" );
		std::printf( "COMPOSE (a): %zu animation, rect0 %d,%d-%d,%d depth %g/%g\n", fmt.animations.size(), anim.rects[0].rect.minx, anim.rects[0].rect.miny,
		             anim.rects[0].rect.maxx, anim.rects[0].rect.maxy, anim.rects[0].fDepthLeft, anim.rects[0].fDepthRight );

		// The depth pass with a minimum alpha reads the picture and keeps the rest.
		SSpriteAnimationFormat deep;
		if ( Check( ComposeInto( scratch, "a-depth", { Animation( "Idle", { { 0, 1, 2 } } ) }, files, 128, deep ), "compose (a): the depth pass composes" ) )
		{
			SSpriteAnimationFormat deepFmt;
			Check( LoadSan( scratch / "a-depth" / "1.san", deepFmt ) && deepFmt.animations.size() == 1 && deepFmt.animations[0].rects.size() == 3 &&
			       deepFmt.animations[0].rects[1].rect.maxx == anim.rects[1].rect.maxx && deepFmt.animations[0].dirs[0].frames == anim.dirs[0].frames,
			       "compose (a): the depth pass leaves rects and frames unchanged" );
		}

		// (d) _c.dds decoded through NDxt, held to the DXT5 gate.
		NResourceModel::SDxtTolerance tolerance;
		std::string szError, szDds;
		const bool bGate = NResourceModel::LoadDxtTolerance( ( fs::path( szFixtureRoot ) / "dxt-tolerance.json" ).string(), &tolerance, &szError ) && tolerance.Find( "DXT5" ) != 0;
		NResourceModel::SDdsImage decoded;
		if ( Check( bGate && ReadBytes( ( scratch / "a" / "1_c.dds" ).string(), szDds ) && NResourceModel::DecodeDds( szDds, &decoded, &szError ) && decoded.szFourCC == "DXT5" &&
		            !decoded.mips.empty(), ( "compose (d): 1_c.dds is a DXT5 NDxt decodes " + szError ).c_str() ) )
		{
			NResourceModel::SDdsMip source;
			source.nWidth = pPacked->GetSizeX();
			source.nHeight = pPacked->GetSizeY();
			const SColor *pColors = pPacked->GetLFB();
			// A fully transparent pixel has no visible colour, and the packer
			// leaves it unspecified, so it is taken from the decoded picture.
			for ( int i = 0; i < source.nWidth * source.nHeight && i < int( decoded.mips[0].pixels.size() ); ++i )
				source.pixels.push_back( pColors[i].a == 0 ? decoded.mips[0].pixels[i] :
				                         ( unsigned( pColors[i].a ) << 24 ) | ( unsigned( pColors[i].r ) << 16 ) | ( unsigned( pColors[i].g ) << 8 ) | unsigned( pColors[i].b ) );
			NResourceModel::SDxtDelta delta;
			delta.Add( 0, decoded.mips[0], source );
			const NResourceModel::SDxtStats stats = delta.Stats(), *pGate = tolerance.Find( "DXT5" );
			std::printf( "COMPOSE (d): DXT5 colour max %d p99 %d alpha max %d p99 %d, gate colour max %d p99 %d alpha max %d p99 %d\n", stats.nColourMax, stats.nColourP99,
			             stats.nAlphaMax, stats.nAlphaP99, pGate->nColourMax, pGate->nColourP99, pGate->nAlphaMax, pGate->nAlphaP99 );
			Check( decoded.mips[0].nWidth == source.nWidth && stats.nColourMax <= pGate->nColourMax && stats.nColourP99 <= pGate->nColourP99 &&
			       stats.nAlphaMax <= pGate->nAlphaMax && stats.nAlphaP99 <= pGate->nAlphaP99, "compose (d): the _c.dds decodes within the DXT5 gate of the packed picture" );
		}
	}

	// (b) two animations, two directions each, a file used twice.
	{
		std::vector<std::string> shared = files;
		shared.push_back( files[1] );
		SSpriteAnimationFormat written;
		if ( !Check( ComposeInto( scratch, "b", { Animation( "Idle", { { 0, 1 }, { 1, 2 } } ), Animation( "Death", { { 2, 3 }, { 3, 2 } } ) }, shared, 0, written ),
		             "compose (b): two animations with two directions compose and write" ) )
			return;
		SSpriteAnimationFormat fmt;
		if ( !Check( LoadSan( scratch / "b" / "1.san", fmt ) && fmt.animations.size() == ANIMATION_DEATH + 1, "compose (b): the engine reads ANIMATION_DEATH + 1 slots" ) )
			return;
		const SSpriteAnimationFormat::SSpriteAnimation &idle = fmt.animations[ANIMATION_IDLE], &death = fmt.animations[ANIMATION_DEATH];
		Check( idle.dirs.size() == 2 && idle.dirs[0].frames == std::vector<short>( { 0, 1 } ) && idle.dirs[1].frames == std::vector<short>( { 1, 2 } ) && idle.rects.size() == 3,
		       "compose (b): Idle keeps its two directions over three used frames" );
		// The fourth name is the second file: its index is 1, so Death uses
		// files 2 and 1, the used frames {1, 2}, which index as 1 and 0.
		Check( death.dirs.size() == 2 && death.dirs[0].frames == std::vector<short>( { 1, 0 } ) && death.dirs[1].frames == std::vector<short>( { 0, 1 } ) && death.rects.size() == 2,
		       "compose (b): Death indexes its used frames, and a repeated file name shares its frame" );
		Check( death.rects.size() == 2 && idle.rects.size() == 3 && death.rects[0].maps.x1 == idle.rects[1].maps.x1 && death.rects[1].maps.x1 == idle.rects[2].maps.x1,
		       "compose (b): Death's rects are Idle's rects of the same files" );
		bool bEmpty = true;
		for ( int i = 1; i < ANIMATION_DEATH; ++i )
			bEmpty = bEmpty && fmt.animations[i].dirs.empty() && fmt.animations[i].rects.empty();
		Check( bEmpty, "compose (b): the slots between Idle and Death are empty" );
	}

	// (c) the same compose twice writes the same bytes.
	{
		SSpriteAnimationFormat first, second;
		const bool bFirst = ComposeInto( scratch, "c1", { Animation( "Idle", { { 0, 1, 2 } } ), Animation( "Run", { { 2, 1 } } ) }, files, 0, first );
		const bool bSecond = ComposeInto( scratch, "c2", { Animation( "Idle", { { 0, 1, 2 } } ), Animation( "Run", { { 2, 1 } } ) }, files, 0, second );
		if ( Check( bFirst && bSecond, "compose (c): two composes of the same project write" ) )
			for ( const char *pszName : { "1.san", "1_c.dds", "1_l.dds", "1_h.dds" } )
			{
				std::string a, b;
				Check( ReadBytes( ( scratch / "c1" / pszName ).string(), a ) && ReadBytes( ( scratch / "c2" / pszName ).string(), b ) && !a.empty() && a == b,
				       ( std::string( "compose (c): " ) + pszName + " is byte-identical across composes" ).c_str() );
			}
	}

	// (e) the writer on a shipped human's 1.san.
	{
		const fs::path shipped = T11::FoldedPath( fs::path( szRoot ) / "Data", "Units/Humans/German/Mp43/1.san" );
		std::printf( "COMPOSE (e): writer identity on %s\n", shipped.string().c_str() );
		SSpriteAnimationFormat fmt;
		std::string szShipped, szResaved;
		if ( Check( LoadSan( shipped, fmt ) && !fmt.animations.empty() && ReadBytes( shipped.string(), szShipped ), "compose (e): the shipped 1.san loads" ) )
		{
			NResourceModel::SExportContext context;
			context.szStagingRoot = ( scratch / "e" ).string();
			NResourceModel::SExportOutcome outcome;
			Check( NResourceModel::NImageExport::SaveAnimation( context, fmt, "1.san", outcome ) && ReadBytes( ( scratch / "e" / "1.san" ).string(), szResaved ),
			       ( "compose (e): the shipped format re-saves " + outcome.szError ).c_str() );
			Check( szShipped == szResaved, ( "compose (e): the re-saved .san is byte-identical to the shipped one (" + std::to_string( szShipped.size() ) + " vs " +
			                                 std::to_string( szResaved.size() ) + " bytes)" ).c_str() );
		}
	}
}

}

// S07 T06: the sprite exporter and its preview on the real bridge. The
// fixture project is copied beside a frames folder of generated TGAs; the
// .san is read back with the engine's structure loader. The preview Run and
// Stop captures are measured by code.

namespace S07Sprite
{

// A square of one colour on a transparent border, so every frame is told
// apart by its colour and cropped to the same rect.
static void WriteFrame( const std::filesystem::path &file, int nSize, int nBorder, unsigned char r, unsigned char g, unsigned char b )
{
	std::string szBytes( 18, '\0' );
	szBytes[2] = 2;
	szBytes[12] = char( nSize & 0xff ); szBytes[13] = char( nSize >> 8 );
	szBytes[14] = char( nSize & 0xff ); szBytes[15] = char( nSize >> 8 );
	szBytes[16] = 32;
	szBytes[17] = 0x28;
	for ( int y = 0; y < nSize; ++y )
		for ( int x = 0; x < nSize; ++x )
		{
			const bool bInside = x >= nBorder && x < nSize - nBorder && y >= nBorder && y < nSize - nBorder;
			szBytes += char( bInside ? b : 0 ); szBytes += char( bInside ? g : 0 ); szBytes += char( bInside ? r : 0 ); szBytes += char( bInside ? 255 : 0 );
		}
	std::ofstream( file, std::ios::binary ) << szBytes;
}

// The fixture project with its directory set to frames\ and its one frame
// item replaced by the named ones.
static std::string Project( const std::string &szFixture, const std::vector<std::string> &names )
{
	std::string szXml = szFixture;
	const std::string szOld = "<string_value>_.</string_value>";
	szXml.replace( szXml.find( szOld ), szOld.size(), "<string_value>frames\\</string_value>" );
	const std::string::size_type nStart = szXml.find( "<item ClassTypeID=\"285212695\"" );
	const std::string::size_type nEnd = szXml.find( "</item>", nStart ) + 7;
	std::string szItems;
	for ( const std::string &szName : names )
		szItems += "<item ClassTypeID=\"285212695\" expand=\"0\"><default_name>" + szName + "</default_name><display_name>" + szName +
		           "</display_name><values/><childs/></item>";
	szXml.replace( nStart, nEnd - nStart, szItems );
	return szXml;
}

static std::vector<std::string> Warnings( BkResSession *pSession, int nFlags, bool bStatsOnly, BkResExportReport &report, BkEditorStatus &status )
{
	std::vector<BkResWarning> warnings( 16 );
	report = {};
	report.warnings = warnings.data();
	report.warnings_capacity = int( warnings.size() );
	status = bStatsOnly ? BkResExportStatsOnly( pSession, nFlags, &report ) : BkResExport( pSession, nFlags, &report );
	std::vector<std::string> texts;
	for ( int i = 0; i < report.warning_count && i < int( warnings.size() ); ++i )
		texts.push_back( warnings[i].text );
	return texts;
}

static bool HasWarning( const std::vector<std::string> &warnings, const char *pszPart )
{
	for ( const std::string &sz : warnings )
		if ( sz.find( pszPart ) != std::string::npos )
			return true;
	return false;
}

static void Run( BkResSession *pSession, const std::string &szRoot, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s07-sprite";
	fs::remove_all( scratch, ec );
	const fs::path projectDir = scratch / "sprite";
	fs::create_directories( projectDir / "frames", ec );
	std::string szFixture;
	ReadBytes( szFixtureRoot + "/spt/project.spt", szFixture );
	const fs::path project = projectDir / "project.spt";
	const fs::path modDir = scratch / "mod";
	const fs::path outDir = modDir / "data" / "effects" / "sprites" / "sprite";
	BkResModSettings mod = {};
	std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
	std::snprintf( mod.name, sizeof( mod.name ), "S07 sprite" );
	Check( BkResModSettingsSet( pSession, &mod ) == BK_EDITOR_OK, "sprite: the mod folder is set" );
	const unsigned char kColours[3][3] = { { 220, 40, 40 }, { 40, 220, 40 }, { 40, 40, 220 } };
	for ( int i = 0; i < 3; ++i )
		WriteFrame( projectDir / "frames" / ( "f" + std::to_string( i ) + ".tga" ), 16, 2, kColours[i][0], kColours[i][1], kColours[i][2] );
	T10::WriteText( project, Project( szFixture, { "f0", "f1", "f2" } ) );

	BkResExportReport report;
	BkEditorStatus status;
	std::vector<std::string> warnings;
	auto Files = [&]( const fs::path &dir )
	{
		std::string szList;
		for ( const char *pszName : { "1.san", "1_c.dds", "1_l.dds", "1_h.dds" } )
			szList += fs::is_regular_file( dir / pszName, ec ) ? '1' : '0';
		return szList;
	};

	// (a) the export file set and the .san read back.
	if ( !Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "sprite: the three-frame project opens" ) )
		return;
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
	Check( status == BK_EDITOR_OK && report.written == 4 && warnings.empty(),
	       ( "sprite export: four files, no warning " + std::string( BkEditorLastMessage( pSession ) ) + " written " + std::to_string( report.written ) +
	         " warnings " + std::to_string( report.warning_count ) ).c_str() );
	Check( Files( outDir ) == "1111", ( "sprite export: 1.san, 1_c, 1_l and 1_h exist (" + Files( outDir ) + ")" ).c_str() );
	SSpriteAnimationFormat fmt;
	if ( Check( S07Compose::LoadSan( outDir / "1.san", fmt ), "sprite export: the engine reads the .san" ) )
	{
		const bool bShape = fmt.animations.size() == 1 && fmt.animations[0].dirs.size() == 1 && fmt.animations[0].rects.size() == 3;
		if ( Check( bShape, "sprite export: one animation, one direction, three frames" ) )
		{
			const SSpriteAnimationFormat::SSpriteAnimation &anim = fmt.animations[0];
			Check( anim.dirs[0].frames == std::vector<short>( { 0, 1, 2 } ) && anim.nFrameTime == 125 && !anim.bCycled && anim.fSpeed == 0.0f,
			       "sprite export: frames 0 1 2, frame time 125, not cycled" );
			bool bRects = true;
			for ( const SSpriteRect &rect : anim.rects )
				bRects = bRects && rect.rect.maxx - rect.rect.minx == 11 && rect.rect.maxy - rect.rect.miny == 11;
			Check( bRects, "sprite export: each frame is cropped to its picture" );
			std::printf( "SPRITE export: %zu animation, %zu rects, rect0 %d,%d-%d,%d frame time %d\n", fmt.animations.size(), anim.rects.size(), anim.rects[0].rect.minx,
			             anim.rects[0].rect.miny, anim.rects[0].rect.maxx, anim.rects[0].rect.maxy, anim.nFrameTime );
		}
	}
	const fs::file_time_type firstExport = fs::last_write_time( outDir / "1.san", ec );

	// (b) stats-only writes nothing.
	fs::remove_all( modDir / "data", ec );
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, true, report, status );
	Check( status == BK_EDITOR_OK && report.written == 0 && HasWarning( warnings, "stats-only" ), "sprite stats-only: nothing written, said so" );
	Check( Files( outDir ) == "0000", "sprite stats-only: no game file appears" );

	// (c) the up-to-date skip: an older export (with the 1.tga MFC compared) is left alone.
	Check( Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status ).empty() && status == BK_EDITOR_OK, "sprite skip: the forced export is made again" );
	T10::WriteText( outDir / "1.tga", "x" );
	fs::last_write_time( projectDir / "project.spt", firstExport - std::chrono::hours( 2 ), ec );
	for ( int i = 0; i < 3; ++i )
		fs::last_write_time( projectDir / "frames" / ( "f" + std::to_string( i ) + ".tga" ), firstExport - std::chrono::hours( 2 ), ec );
	warnings = Warnings( pSession, 0, false, report, status );
	Check( status == BK_EDITOR_OK && report.written == 0 && report.skipped >= 1, ( "sprite skip: up to date is skipped (written " + std::to_string( report.written ) +
	       " skipped " + std::to_string( report.skipped ) + ")" ).c_str() );
	fs::last_write_time( projectDir / "frames" / "f1.tga", fs::file_time_type::clock::now() + std::chrono::hours( 1 ), ec );
	warnings = Warnings( pSession, 0, false, report, status );
	Check( status == BK_EDITOR_OK && report.written == 4 && report.skipped == 0, "sprite skip: a newer frame exports again" );
	fs::last_write_time( outDir / "1.tga", fs::file_time_type::clock::now() + std::chrono::hours( 2 ), ec );
	fs::last_write_time( outDir / "1.san", fs::file_time_type::clock::now() + std::chrono::hours( 2 ), ec );
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
	Check( status == BK_EDITOR_OK && report.written == 4 && report.skipped == 0, "sprite skip: a forced export ignores the up-to-date files" );

	// (d) a missing frame: left out without the stand-in picture, stood in for with it.
	BkResClose( pSession );
	fs::remove( projectDir / "frames" / "f1.tga", ec );
	fs::remove_all( modDir / "data", ec );
	if ( Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "sprite missing frame: the project reopens" ) )
	{
		warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
		Check( status == BK_EDITOR_OK && HasWarning( warnings, "f1.tga" ) && HasWarning( warnings, "left out" ), "sprite missing frame: warns, naming the file, without invalid.tga" );
		SSpriteAnimationFormat two;
		Check( S07Compose::LoadSan( outDir / "1.san", two ) && two.animations.size() == 1 && two.animations[0].rects.size() == 2 && two.animations[0].dirs[0].frames == std::vector<short>( { 0, 1 } ),
		       "sprite missing frame: the other two frames are exported" );
		fs::create_directories( modDir / "data" / "editor", ec );
		fs::copy_file( T11::FoldedPath( fs::path( szRoot ) / "Data", "Editor/invalid.tga" ), modDir / "data" / "editor" / "invalid.tga", fs::copy_options::overwrite_existing, ec );
		warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
		Check( status == BK_EDITOR_OK && HasWarning( warnings, "f1.tga" ) && HasWarning( warnings, "stands in" ), "sprite missing frame: warns, naming the file, with invalid.tga" );
		SSpriteAnimationFormat three;
		Check( S07Compose::LoadSan( outDir / "1.san", three ) && three.animations.size() == 1 && three.animations[0].rects.size() == 3 && three.animations[0].dirs[0].frames == std::vector<short>( { 0, 1, 2 } ),
		       "sprite missing frame: invalid.tga fills the slot, three frames" );
		BkResClose( pSession );
	}

	// (e) no frames: a warning, nothing written, not a failure.
	{
		const fs::path emptyDir = scratch / "empty";
		fs::create_directories( emptyDir, ec );
		T10::WriteText( emptyDir / "project.spt", Project( szFixture, {} ) );
		fs::remove_all( modDir / "data", ec );
		if ( Check( BkResOpen( pSession, ( emptyDir / "project.spt" ).string().c_str() ) == BK_EDITOR_OK, "sprite no frames: the project opens" ) )
		{
			warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
			Check( status == BK_EDITOR_OK && report.written == 0 && HasWarning( warnings, "no valid animations" ),
			       ( "sprite no frames: 'no valid animations' with nothing written (status " + std::to_string( int( status ) ) + ")" ).c_str() );
			BkResClose( pSession );
		}
	}

	// (f) the preview: Run plays the frames, Stop holds one.
	{
		const fs::path previewDir = scratch / "preview";
		fs::create_directories( previewDir / "frames", ec );
		for ( int i = 0; i < 3; ++i )
			WriteFrame( previewDir / "frames" / ( "f" + std::to_string( i ) + ".tga" ), 64, 8, kColours[i][0], kColours[i][1], kColours[i][2] );
		T10::WriteText( previewDir / "project.spt", Project( szFixture, { "f0", "f1", "f2" } ) );
		if ( !Check( BkResOpen( pSession, ( previewDir / "project.spt" ).string().c_str() ) == BK_EDITOR_OK, "sprite preview: the project opens" ) )
			return;
		Check( BkResPreviewBegin( pSession, 4 ) == BK_EDITOR_OK, "sprite preview: Begin" );
		const fs::path empty = scratch / "empty.tga";
		Check( BkEditorCaptureFrame( pSession, empty.string().c_str() ) == BK_EDITOR_OK, "sprite preview: the empty frame captures" );
		const BkEditorStatus nShow = BkResPreviewShow( pSession );
		Check( nShow == BK_EDITOR_OK, ( std::string( "sprite preview: Show " ) + BkEditorLastMessage( pSession ) ).c_str() );
		auto Pump = [&]( int nMs )
		{
			const auto start = std::chrono::steady_clock::now();
			while ( std::chrono::steady_clock::now() - start < std::chrono::milliseconds( nMs ) )
				BkEditorFrame( pSession );
		};
		auto Shot = [&]( const char *pszName, std::vector<unsigned char> &rgb )
		{
			int nW = 0, nH = 0;
			const fs::path tga = scratch / pszName;
			return BkEditorCaptureFrame( pSession, tga.string().c_str() ) == BK_EDITOR_OK && T11::ReadCapture( tga.string(), rgb, nW, nH );
		};
		std::vector<unsigned char> emptyRgb, runA, runB, stopA, stopB;
		int nW = 0, nH = 0;
		const bool bEmpty = T11::ReadCapture( empty.string(), emptyRgb, nW, nH );
		Check( BkResPreviewPlayback( pSession, 1 ) == BK_EDITOR_OK, "sprite preview: Run" );
		Pump( 20 );
		const bool bA = Shot( "run-a.tga", runA );
		Pump( 140 );
		const bool bB = Shot( "run-b.tga", runB );
		Check( BkResPreviewPlayback( pSession, 0 ) == BK_EDITOR_OK, "sprite preview: Stop" );
		Pump( 60 );
		const bool bC = Shot( "stop-a.tga", stopA );
		Pump( 200 );
		const bool bD = Shot( "stop-b.tga", stopB );
		const bool bRead = bEmpty && bA && bB && bC && bD;
		Check( bRead, "sprite preview: the captures read back" );
		const double fDrew = bRead ? T11::ChangedShare( runA, emptyRgb ) : -1.0;
		const double fRun = bRead ? T11::ChangedShare( runA, runB ) : -1.0;
		const double fStop = bRead ? T11::ChangedShare( stopA, stopB ) : -1.0;
		std::printf( "SPRITE preview: drew-vs-empty=%f run-a-vs-b=%f stop-a-vs-b=%f\n", fDrew, fRun, fStop );
		Check( fDrew >= 0.0005, "sprite preview: the sprite drew (>= 0.05% of the frame changed)" );
		Check( fRun >= 0.0005, "sprite preview: Run plays, two frames 140 ms apart differ" );
		Check( fStop == 0.0, "sprite preview: Stop holds, two frames 200 ms apart are equal" );
		Check( BkResPreviewStop( pSession ) == BK_EDITOR_OK, "sprite preview: Stop preview" );
		BkResClose( pSession );
	}
}

}

// S07 T07: the infantry exporter on the real bridge. The fixture project is
// given three season folders of generated frames: Run has two frames in each
// of 8 directions, Death two frames in 4 directions of which the second
// frame file is missing, to show MFC's frame numbering quirk. A blood\ folder
// holds a bigger Death frame, so the blood pass is told apart by the rect.
namespace S07Infantry
{

// The fixture with each season's directory set to s<N>\, the Run item given
// frames f0 f1 and the Death item d0 d1 in 4 directions.
static std::string Project( const std::string &szFixture )
{
	std::string szXml = szFixture;
	const std::string szSeason = "<item ClassTypeID=\"285212680\"";
	std::vector<std::string::size_type> starts;
	for ( std::string::size_type n = szXml.find( szSeason ); n != std::string::npos; n = szXml.find( szSeason, n + 1 ) )
		starts.push_back( n );
	starts.push_back( szXml.find( "<item ClassTypeID=\"285212682\"" ) );
	for ( int i = int( starts.size() ) - 2; i >= 0; --i )
	{
		std::string szPart = szXml.substr( starts[i], starts[i + 1] - starts[i] );
		const std::string szOld = "<string_value>_.</string_value>";
		for ( std::string::size_type n = szPart.find( szOld ); n != std::string::npos; n = szPart.find( szOld, n + 1 ) )
			szPart.replace( n, szOld.size(), "<string_value>s" + std::to_string( i ) + "\\</string_value>" );
		szXml.replace( starts[i], starts[i + 1] - starts[i], szPart );
	}
	auto Frames = [&]( const char *pszAnim, const std::vector<std::string> &names, const char *pszDirs )
	{
		std::string::size_type nStart = szXml.find( std::string( "<display_name>" ) + pszAnim + "</display_name>" );
		const std::string::size_type nChilds = szXml.find( "<childs/>", nStart );
		std::string szItems = "<childs>";
		for ( const std::string &szName : names )
			szItems += "<item ClassTypeID=\"285212684\" expand=\"0\"><default_name>" + szName + "</default_name><display_name>" + szName +
			           "</display_name><values/><childs/></item>";
		szItems += "</childs>";
		szXml.replace( nChilds, 9, szItems );
		const std::string szOld = "<string_value>8</string_value>";
		const std::string::size_type nDirs = szXml.find( szOld, nStart );
		if ( nDirs < nChilds )
			szXml.replace( nDirs, szOld.size(), std::string( "<string_value>" ) + pszDirs + "</string_value>" );
	};
	Frames( "Run", { "f0", "f1" }, "8" );
	Frames( "Death", { "d0", "d1" }, "4" );
	return szXml;
}

static std::vector<std::string> Warnings( BkResSession *pSession, int nFlags, bool bStatsOnly, BkResExportReport &report, BkEditorStatus &status )
{
	return S07Sprite::Warnings( pSession, nFlags, bStatsOnly, report, status );
}

static void Run( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path scratch = fs::path( szScratchRoot ) / "s07-infantry";
	fs::remove_all( scratch, ec );
	const fs::path projectDir = scratch / "inf";
	std::string szFixture;
	ReadBytes( szFixtureRoot + "/unt/project.unt", szFixture );
	const fs::path project = projectDir / "project.unt";
	const fs::path modDir = scratch / "mod";
	const fs::path outDir = modDir / "data" / "units" / "humans" / "inf";
	BkResModSettings mod = {};
	std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
	std::snprintf( mod.name, sizeof( mod.name ), "S07 infantry" );
	Check( BkResModSettingsSet( pSession, &mod ) == BK_EDITOR_OK, "infantry: the mod folder is set" );

	// Season s<N>: the colours differ, the frame d1 of Death is not there.
	for ( int nSeason = 0; nSeason < 3; ++nSeason )
	{
		fs::create_directories( projectDir / ( "s" + std::to_string( nSeason ) ), ec );
		for ( const char *pszName : { "f0", "f1", "d0" } )
			S07Sprite::WriteFrame( projectDir / ( "s" + std::to_string( nSeason ) ) / ( std::string( pszName ) + ".tga" ), 16, 2, 60 * ( nSeason + 1 ), 120, 200 );
	}
	fs::create_directories( projectDir / "blood" / "s0", ec );
	S07Sprite::WriteFrame( projectDir / "blood" / "s0" / "d0.tga", 24, 4, 200, 20, 20 );
	T10::WriteText( projectDir / "name.txt", "Infantry name" );
	T10::WriteText( projectDir / "desc.txt", "Infantry description" );
	T10::WriteText( project, Project( szFixture ) );

	BkResExportReport report;
	BkEditorStatus status;
	std::vector<std::string> warnings;
	auto Files = [&]( const fs::path &dir )
	{
		std::string szList;
		for ( const char *pszSuffix : { "", "b" } )
			for ( const char *pszSeason : { "", "w", "a" } )
			{
				const std::string szStem = std::string( "1" ) + pszSuffix + pszSeason;
				szList += fs::is_regular_file( dir / ( szStem + ".san" ), ec ) ? '1' : '0';
				for ( const char *pszTexture : { "_c.dds", "_l.dds", "_h.dds" } )
					szList += fs::is_regular_file( dir / ( szStem + pszTexture ), ec ) ? '1' : '0';
			}
		for ( const char *pszName : { "1.xml", "name.txt", "desc.txt", "stats.txt" } )
			szList += fs::is_regular_file( dir / pszName, ec ) ? '1' : '0';
		return szList;
	};
	auto RectWidth = [&]( const fs::path &san, int nAnim, int nRect )
	{
		SSpriteAnimationFormat fmt;
		if ( !S07Compose::LoadSan( san, fmt ) || nAnim >= int( fmt.animations.size() ) || nRect >= int( fmt.animations[nAnim].rects.size() ) )
			return -1;
		const SSpriteRect &rect = fmt.animations[nAnim].rects[nRect];
		return rect.rect.maxx - rect.rect.minx;
	};

	if ( !Check( BkResOpen( pSession, project.string().c_str() ) == BK_EDITOR_OK, "infantry: the project opens" ) )
		return;
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
	Check( status == BK_EDITOR_OK, ( "infantry export: OK " + std::string( BkEditorLastMessage( pSession ) ) ).c_str() );
	const std::string szAll = "111111" "111111" "111111" "111111" "111111" "111111";
	const std::string szExpected = std::string( 24, '1' ) + "1110";
	Check( Files( outDir ) == szExpected, ( "infantry export: 6 .san with 3 .dds each, 1.xml, name.txt, desc.txt and no stats.txt (" + Files( outDir ) + ")" ).c_str() );
	(void) szAll;
	Check( S07Sprite::HasWarning( warnings, "stats.txt" ), "infantry export: the missing localisation source is a warning" );
	Check( S07Sprite::HasWarning( warnings, "d1.tga" ), "infantry export: the missing frame is named in a warning" );
	for ( const std::string &sz : warnings )
		std::printf( "INFANTRY warning: %.*s\n", 1500, sz.c_str() );

	SSpriteAnimationFormat fmt;
	if ( Check( S07Compose::LoadSan( outDir / "1.san", fmt ), "infantry export: the engine reads 1.san" ) )
	{
		const bool bRun = ANIMATION_MOVE < int( fmt.animations.size() ) && fmt.animations[ANIMATION_MOVE].dirs.size() == 8;
		const bool bDeath = ANIMATION_DEATH < int( fmt.animations.size() ) && fmt.animations[ANIMATION_DEATH].dirs.size() == 4;
		Check( bRun && bDeath, "infantry export: Run has 8 directions, Death 4" );
		if ( bRun && bDeath )
		{
			const auto &run = fmt.animations[ANIMATION_MOVE];
			const auto &death = fmt.animations[ANIMATION_DEATH];
			// BuildAnimations numbers the frames of an animation from its own first one.
			// Run: both frames in every direction. Death: d1 is missing, so the counter
			// moved only for d0 and the four directions share the frames written; the
			// animation holds 2 pictures, not the 8 of its frame items.
			bool bRunFrames = run.rects.size() == 2;
			for ( int d = 0; d < 8; ++d )
				bRunFrames = bRunFrames && run.dirs[d].frames == std::vector<short>( { 0, 1 } );
			Check( bRunFrames, "infantry export: Run has 2 pictures, both in every direction" );
			bool bDeathFrames = death.rects.size() == 2;
			for ( int d = 0; d < 4; ++d )
				for ( short nFrame : death.dirs[d].frames )
					bDeathFrames = bDeathFrames && nFrame >= 0 && nFrame < 2;
			Check( bDeathFrames, "infantry export: Death holds 2 pictures for its 8 frame slots, the missing-file quirk" );
			std::printf( "INFANTRY san: Run %zu rects, Death %zu rects\n", run.rects.size(), death.rects.size() );
		}
	}
	Check( RectWidth( outDir / "1.san", ANIMATION_DEATH, 0 ) == 11 && RectWidth( outDir / "1.san", ANIMATION_DEATH, 1 ) == 11, "infantry blood: pass 0 Death has only plain frames" );
	Check( RectWidth( outDir / "1b.san", ANIMATION_DEATH, 0 ) == 15 || RectWidth( outDir / "1b.san", ANIMATION_DEATH, 1 ) == 15, "infantry blood: pass 1 Death takes the blood\\ frame" );
	Check( RectWidth( outDir / "1b.san", ANIMATION_MOVE, 0 ) == 11, "infantry blood: pass 1 Run is the plain frame" );

	// Stats-only: no game file, the stats are written and said so.
	fs::remove_all( modDir / "data", ec );
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, true, report, status );
	Check( status == BK_EDITOR_OK && Files( outDir ).substr( 0, 24 ) == std::string( 24, '0' ), "infantry stats-only: no .san and no .dds" );

	// Up to date: the second export skips; a newer frame exports again.
	warnings = Warnings( pSession, BK_RES_EXPORT_FORCE, false, report, status );
	warnings = Warnings( pSession, 0, false, report, status );
	Check( status == BK_EDITOR_OK && report.written == 0 && report.skipped >= 1, "infantry skip: up to date is skipped" );
	fs::last_write_time( projectDir / "s0" / "f0.tga", fs::file_time_type::clock::now() + std::chrono::seconds( 5 ), ec );
	warnings = Warnings( pSession, 0, false, report, status );
	Check( status == BK_EDITOR_OK && report.written >= 1 && report.skipped == 0, "infantry skip: a newer frame exports again" );
	BkResClose( pSession );
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

	// The cells, point and aimed channels where MFC keeps them (D014 item 2).
	BuildingGeometryInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	ObjectGeometryInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	ItemGeometryInMfcLayout( pSession, szFixtureRoot, szScratchRoot );

	// The list channels where MFC keeps them (D014 item 2, part 2), and every
	// channel on every fixture with no private geometry element written.
	ListGeometryInMfcLayout( pSession, szFixtureRoot, szScratchRoot );
	EveryChannelOnEveryFixture( pSession, szFixtureRoot, szScratchRoot );

	T10::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );

	ExportRollback::Run( pSession, szFixtureRoot, szScratchRoot );

	T11::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );
	// S06 T01: the trench exporter against the shipped entrenchment.
	S06Export::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );
	// S06 T05: the mine, trench and squad previews.
	S06Preview::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );
	// S06 T04: the sub-editors' tools, undo, redo and save on the real bridge.
	S06Tools::Run( pSession, szFixtureRoot, szScratchRoot );

	// S07 T05: the portable BuildAnimations and the .san writer.
	S07Compose::Run( pszRoot, szFixtureRoot, szScratchRoot );
	// S07 T06: the sprite exporter and its preview.
	S07Sprite::Run( pSession, pszRoot, szFixtureRoot, szScratchRoot );
	S07Infantry::Run( pSession, szFixtureRoot, szScratchRoot );

	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "the bridge stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();

	if ( g_nFailures == 0 )
		std::printf( "resource-bridge: Project+Tree OK (%d fixtures)\n", kFixtureCount );
	else
		std::printf( "resource-bridge: %d failures\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
