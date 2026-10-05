// The resource editor's C ABI implementation. Every entry point is wrapped in
// the shared Guarded template (guarded.h) so no exception crosses into Zig.
// T01 stubbed every call to BK_EDITOR_OK. T02 fills in the Project+Tree group:
// New/Open/Save (safe-save read-back), Close, KindOf, Lock/LockOwner, Nodes,
// Props, SetProp, InsertNode, MoveNode, DeleteNode, RestoreNode. T05 fills in
// the cells family of geometry (passability, locked tiles, transparency
// lines): the C ABI entries now serve against an in-session geometry map that
// hangs off ResourceState, and BkResOpen / BkResSave persist / restore entries
// as `_bk_geometry` elements inside the owning item's own element (any node,
// not only the root; see WriteGeometry) so a save then reopen keeps the
// written cells byte-identically. T06 wires the second
// geometry family on top of that: the two point2 channels (zero point,
// entrance) and the four aimed-point channels (shoot/fire/smoke/
// directed-explosion). Angles cross the ABI as MFC-era degrees - a typed
// building/squad item class is free to store engine turns internally; the ABI
// boundary is the one place the unit is pinned. The remaining geometry
// channels (formation, bridge-spans, keyframes, crosses) and the other groups
// (references, export, mod, preview, import) stay stubbed and are replaced
// in T08-T11. T07 replaced the `<path>.lock` sentinel with MFC's per-folder
// `locked_<user>` (CParentFrame::LockFile, D-08) and made a delete blob carry
// its subtree's geometry, so BkResRestoreNode brings it back.
//
// Per-session state (open project, path, node id maps, lock) lives in a module-
// private map keyed by the BkEditorSession pointer the lifecycle layer owns.
// BkEditorStop drops that pointer; this file catches that via a lazy sweep: the
// bridge is single-threaded, each entry point touches at most one session, and
// Guarded already serialises exceptions. ResourceStateOf() does the lookup and
// EnsureClosedFor() clears it on Close. A session that is Stop'd without a Close
// leaks its ResourceState until this file is unloaded - acceptable for the
// headless tests here; later tasks can wire the sweep into BkEditorStop if a
// long-running host surfaces the leak.
#include "StdAfx.h"
#include "resource_bridge.h"
#include "session.h"
#include "bridge_session.h"
#include "guarded.h"

#include "../ResourceModel/project.h"
#include "../ResourceModel/factory.h"
#include "../ResourceModel/items/stats_item.h"
#include "../ResourceModel/items/tree_item_types.h"
#include "../ResourceModel/xml.h"
#include "../ResourceModel/future_blob.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// The 21-entry kind table. Order follows EXTENSIONS.md and MEM005. A caller's
// BkResKind is an ordinal into this list, 0..20; BkResKindOf writes the same
// ordinal back out so Save-then-Open round-trips the kind through the ABI.
// The ETIT_*_ROOT_ITEM is what the factory creates on New; the tag is what
// the Project loader matches on Open.
struct KindEntry
{
	const char *pszTag;
	int nRootType;
};
const KindEntry kKindTable[] =
{
	{ "Weapon_Composer_Project",   NResourceModel::ETIT_WEAPON_ROOT_ITEM    },
	{ "Mine_Composer_Project",     NResourceModel::ETIT_MINE_ROOT_ITEM      },
	{ "Trench_Composer_Project",   NResourceModel::ETIT_TRENCH_ROOT_ITEM    },
	{ "Squad_Composer_Project",    NResourceModel::ETIT_SQUAD_ROOT_ITEM     },
	{ "Sprite_Composer_Project",   NResourceModel::ETIT_SPRITE_ROOT_ITEM    },
	{ "Unit_Composer_Project",     NResourceModel::ETIT_ANIMATION_ROOT_ITEM },
	{ "Mesh_Composer_Project",     NResourceModel::ETIT_MESH_ROOT_ITEM      },
	{ "Object_Composer_Project",   NResourceModel::ETIT_OBJECT_ROOT_ITEM    },
	{ "Fence_Composer_Project",    NResourceModel::ETIT_FENCE_ROOT_ITEM     },
	{ "Building_Composer_Project", NResourceModel::ETIT_BUILDING_ROOT_ITEM  },
	{ "Bridge_Composer_Project",   NResourceModel::ETIT_BRIDGE_ROOT_ITEM    },
	{ "Particle_Composer_Project", NResourceModel::ETIT_PARTICLE_ROOT_ITEM  },
	{ "Effect_Composer_Project",   NResourceModel::ETIT_EFFECT_ROOT_ITEM    },
	{ "TileSet_Composer_Project",  NResourceModel::ETIT_TILESET_ROOT_ITEM   },
	{ "Road3D_Composer_Project",   NResourceModel::ETIT_3DROAD_ROOT_ITEM    },
	{ "River3D_Composer_Project",  NResourceModel::ETIT_3DRIVER_ROOT_ITEM   },
	{ "Mission_Composer_Project",  NResourceModel::ETIT_MISSION_ROOT_ITEM   },
	{ "Chapter_Composer_Project",  NResourceModel::ETIT_CHAPTER_ROOT_ITEM   },
	{ "Campaign_Composer_Project", NResourceModel::ETIT_CAMPAIGN_ROOT_ITEM  },
	{ "Medal_Composer_Project",    NResourceModel::ETIT_MEDAL_ROOT_ITEM     },
	{ "GUI_Composer_Project",      NResourceModel::ETIT_GUI_ROOT_ITEM       }
};
const int kKindCount = int( sizeof(kKindTable) / sizeof(kKindTable[0]) );

int KindOrdinalFromRootType( int nRootType )
{
	for ( int i = 0; i < kKindCount; ++i )
		if ( kKindTable[i].nRootType == nRootType )
			return i;
	return -1;
}

// One geometry entry's shape: the bytes_grid family (passability, locked
// tiles) stores a width/height plus the row-major grid; the points family
// (transparency lines, formation positions, ...) stores a flat f32 pair list;
// the point2 family (zero point, entrance) stores exactly one Point2 as a
// 2-element points vector; the aimed family (shoot/fire/smoke/
// directed-explosion) stores one AimedPoint per entry in the parallel `aimed`
// vector. The four payload families are mutually exclusive on a given (node,
// channel) pair because the channel enum fixes the family; the struct
// carries all of them so a later channel can reuse this blob.
struct AimedPoint
{
	float x = 0;
	float y = 0;
	int nAngle = 0;
	int nCone = 0;
};

struct GeometryBlob
{
	// bytes_grid family: non-empty bytes + w > 0 && h > 0 && w*h == bytes.size.
	std::vector<unsigned char> bytes;
	int nWidth = 0;
	int nHeight = 0;
	// points family: 2*n floats in order (x0, y0, x1, y1, ...).
	std::vector<float> points;
	// aimed family: one AimedPoint per entry.
	std::vector<AimedPoint> aimed;
};

// Per-session state the resource bridge needs on top of BkEditorSession. The
// shared lifecycle layer (bridge.cpp) owns the session; this map hangs the
// project-shaped state off it. See the file header for the lifetime contract.
struct ResourceState
{
	bool bOpen = false;
	int nKindOrdinal = -1;
	std::string szPath;                                    // "" until first Save
	std::unique_ptr<NResourceModel::Project> pProject;
	int nNextNodeId = 0;
	std::unordered_map<int, NResourceModel::CTreeItem *> idToItem;
	std::unordered_map<const NResourceModel::CTreeItem *, int> itemToId;
	// A node has a parent (the item that owns the unique_ptr pointing at it) or
	// is the root (parent == 0). Kept so BkResNodes can emit the parent id in
	// one pass without a tree walk.
	std::unordered_map<int, int> parentOf;
	// Every live id in tree pre-order, root first. Ids are stable rather than
	// dense, so BkResNodes walks this instead of 1..nNextNodeId.
	std::vector<int> preorder;
	// The `locked_<user>` file, if this session holds it; BkResClose removes
	// it, as MFC's UnLockFile does when the frame closes the project.
	bool bHoldsLock = false;
	std::string szLockPath;
	// Geometry map keyed by (node_id, channel). The channel is the C ABI
	// integer the Zig GeometryChannel enum uses (BkResPassabilityCells = 0,
	// BkResLockedTiles = 1, BkResTransparencyLines = 2, etc.). An entry
	// exists only after a successful BkResSet*; a read of an un-set channel
	// returns an empty payload (w = h = 0, count = 0). BkResSave writes the
	// entries as `_bk_geometry` elements in the owning item's element so a
	// BkResOpen on the saved file restores the map.
	std::map<std::pair<int, int>, GeometryBlob> geometry;
};

// Channel ids: the C ABI's geometry channel integers (shared with Zig's
// bridge.GeometryChannel enum). T05 wired channels 0..2 (cells family); T06
// adds 3..8 (point2 family + aimed-points family); 9 and 10 join channel 2 in
// the points family. The rest are reserved for later tasks. Kept as a plain
// enum so a test can hand-assert against an integer.
enum GeometryChannel
{
	CHANNEL_PASSABILITY_CELLS = 0,
	CHANNEL_LOCKED_TILES = 1,
	CHANNEL_TRANSPARENCY_LINES = 2,
	CHANNEL_ZERO_POINT = 3,
	CHANNEL_ENTRANCE = 4,
	CHANNEL_SHOOT_POINTS = 5,
	CHANNEL_FIRE_POINTS = 6,
	CHANNEL_SMOKE_POINTS = 7,
	CHANNEL_DIRECTED_EXPLOSION_POINTS = 8,
	CHANNEL_FORMATION_POSITIONS = 9,
	CHANNEL_BRIDGE_SPAN_MARKS = 10
};

static bool IsBytesGridChannel( int nChannel )
{
	return nChannel == CHANNEL_PASSABILITY_CELLS || nChannel == CHANNEL_LOCKED_TILES;
}

static bool IsPoint2Channel( int nChannel )
{
	return nChannel == CHANNEL_ZERO_POINT || nChannel == CHANNEL_ENTRANCE;
}

static bool IsAimedChannel( int nChannel )
{
	return nChannel == CHANNEL_SHOOT_POINTS || nChannel == CHANNEL_FIRE_POINTS
		|| nChannel == CHANNEL_SMOKE_POINTS || nChannel == CHANNEL_DIRECTED_EXPLOSION_POINTS;
}

// Reserved element name for persisted geometry. An item keeps the element in
// its layout as an unknown field, so it never shows up as a tree node; every
// write strips the copies and writes the session's map instead.
const char *kGeometryTag = "_bk_geometry";

std::map<BkEditorSession *, ResourceState> &States()
{
	static std::map<BkEditorSession *, ResourceState> m;
	return m;
}

ResourceState &StateOf( BkEditorSession *pSession )
{
	return States()[pSession];
}

void ReleaseLockFile( ResourceState &state )
{
	if ( !state.bHoldsLock || state.szLockPath.empty() )
		return;
	std::error_code ec;
	std::filesystem::remove( state.szLockPath, ec );
	state.bHoldsLock = false;
	state.szLockPath.clear();
}

void ResetState( ResourceState &state )
{
	ReleaseLockFile( state );
	state.bOpen = false;
	state.nKindOrdinal = -1;
	state.szPath.clear();
	state.pProject.reset();
	state.nNextNodeId = 0;
	state.idToItem.clear();
	state.itemToId.clear();
	state.parentOf.clear();
	state.preorder.clear();
	state.geometry.clear();
}

// Lower-case hex of a byte buffer; two chars per byte, no separators. Used
// for serialising the cells grids inside `_bk_geometry` XML children.
std::string HexEncode( const unsigned char *p, std::size_t n )
{
	static const char kDigits[] = "0123456789abcdef";
	std::string out;
	out.resize( n * 2 );
	for ( std::size_t i = 0; i < n; ++i )
	{
		out[2*i]     = kDigits[( p[i] >> 4 ) & 0xF];
		out[2*i + 1] = kDigits[p[i] & 0xF];
	}
	return out;
}

bool HexDecode( const std::string &szHex, std::vector<unsigned char> &out )
{
	if ( szHex.size() % 2 != 0 ) return false;
	out.resize( szHex.size() / 2 );
	auto Nibble = []( char c, int &v ) -> bool
	{
		if ( c >= '0' && c <= '9' ) { v = c - '0'; return true; }
		if ( c >= 'a' && c <= 'f' ) { v = 10 + ( c - 'a' ); return true; }
		if ( c >= 'A' && c <= 'F' ) { v = 10 + ( c - 'A' ); return true; }
		return false;
	};
	for ( std::size_t i = 0; i < out.size(); ++i )
	{
		int hi = 0, lo = 0;
		if ( !Nibble( szHex[2*i], hi ) || !Nibble( szHex[2*i + 1], lo ) ) return false;
		out[i] = static_cast<unsigned char>( ( hi << 4 ) | lo );
	}
	return true;
}

// Finds an attribute by name; returns an empty string when absent.
std::string FindAttr( const NResourceXml::Node &node, const std::string &szName )
{
	for ( const auto &kv : node.attrs )
		if ( kv.first == szName )
			return kv.second;
	return std::string();
}

// Builds a `_bk_geometry` child element from a stored blob. Shape:
//   bytes_grid: <_bk_geometry channel="N" w="W" h="H">HEX...</_bk_geometry>
//   points    : <_bk_geometry channel="N" count="K">x0,y0;x1,y1;...</_bk_geometry>
//   point2    : <_bk_geometry channel="N">x,y</_bk_geometry>
//   aimed     : <_bk_geometry channel="N" count="K">x0,y0,a0,c0;x1,y1,a1,c1;...</_bk_geometry>
// Element content lives as a single Text child node, which is how xml.cpp
// writes/reads inline text.
NResourceXml::Node EmitGeometryChild( int nChannel, const GeometryBlob &blob )
{
	NResourceXml::Node out;
	out.kind = NResourceXml::Node::Element;
	out.name = kGeometryTag;
	out.attrs.push_back( { "channel", std::to_string( nChannel ) } );
	std::string text;
	if ( IsBytesGridChannel( nChannel ) )
	{
		out.attrs.push_back( { "w", std::to_string( blob.nWidth ) } );
		out.attrs.push_back( { "h", std::to_string( blob.nHeight ) } );
		text = HexEncode( blob.bytes.data(), blob.bytes.size() );
	}
	else if ( IsPoint2Channel( nChannel ) )
	{
		const int nHas = ( blob.points.size() >= 2 ) ? 1 : 0;
		out.attrs.push_back( { "count", std::to_string( nHas ) } );
		if ( nHas != 0 )
		{
			char buf[64];
			std::snprintf( buf, sizeof( buf ), "%.9g,%.9g", blob.points[0], blob.points[1] );
			text = buf;
		}
	}
	else if ( IsAimedChannel( nChannel ) )
	{
		const int nCount = static_cast<int>( blob.aimed.size() );
		out.attrs.push_back( { "count", std::to_string( nCount ) } );
		text.reserve( static_cast<std::size_t>( nCount ) * 24 );
		for ( int i = 0; i < nCount; ++i )
		{
			if ( i != 0 ) text.push_back( ';' );
			char buf[96];
			std::snprintf( buf, sizeof( buf ), "%.9g,%.9g,%d,%d",
				blob.aimed[i].x, blob.aimed[i].y, blob.aimed[i].nAngle, blob.aimed[i].nCone );
			text += buf;
		}
	}
	else
	{
		const int nCount = static_cast<int>( blob.points.size() / 2 );
		out.attrs.push_back( { "count", std::to_string( nCount ) } );
		text.reserve( blob.points.size() * 10 );
		for ( int i = 0; i < nCount; ++i )
		{
			if ( i != 0 ) text.push_back( ';' );
			char buf[64];
			std::snprintf( buf, sizeof( buf ), "%.9g,%.9g", blob.points[2*i], blob.points[2*i + 1] );
			text += buf;
		}
	}
	if ( !text.empty() )
	{
		NResourceXml::Node body;
		body.kind = NResourceXml::Node::Text;
		body.text = std::move( text );
		out.children.push_back( std::move( body ) );
	}
	return out;
}

// Reads a `_bk_geometry` element's single inline Text child; returns the
// child's text (same shape xml.cpp emits via the single-text-child branch of
// WriteNode). Empty when the element has no body.
std::string FindBodyText( const NResourceXml::Node &node )
{
	for ( const auto &c : node.children )
		if ( c.kind == NResourceXml::Node::Text || c.kind == NResourceXml::Node::CData )
			return c.text;
	return std::string();
}

// Parses a `_bk_geometry` child back into a (channel, blob). Returns false on
// a malformed element (unknown shape, odd attrs, bad hex, bad float).
bool ParseGeometryChild( const NResourceXml::Node &node, int &nChannel, GeometryBlob &out )
{
	if ( node.kind != NResourceXml::Node::Element || node.name != kGeometryTag )
		return false;
	const std::string szChannel = FindAttr( node, "channel" );
	if ( szChannel.empty() ) return false;
	nChannel = std::atoi( szChannel.c_str() );
	const std::string body = FindBodyText( node );
	if ( IsBytesGridChannel( nChannel ) )
	{
		out.nWidth  = std::atoi( FindAttr( node, "w" ).c_str() );
		out.nHeight = std::atoi( FindAttr( node, "h" ).c_str() );
		if ( out.nWidth < 0 || out.nHeight < 0 ) return false;
		if ( !HexDecode( body, out.bytes ) ) return false;
		const std::size_t nExpect = static_cast<std::size_t>( out.nWidth ) * static_cast<std::size_t>( out.nHeight );
		if ( out.bytes.size() != nExpect ) return false;
		return true;
	}
	if ( IsPoint2Channel( nChannel ) )
	{
		const int nHas = std::atoi( FindAttr( node, "count" ).c_str() );
		if ( nHas < 0 || nHas > 1 ) return false;
		out.points.clear();
		if ( nHas == 0 ) return body.empty();
		const char *p = body.c_str();
		const char *pEnd = p + body.size();
		char *q = nullptr;
		const float x = std::strtof( p, &q );
		if ( q == p || q >= pEnd || *q != ',' ) return false;
		p = q + 1;
		const float y = std::strtof( p, &q );
		if ( q == p ) return false;
		p = q;
		if ( p != pEnd ) return false;
		out.points.push_back( x );
		out.points.push_back( y );
		return true;
	}
	if ( IsAimedChannel( nChannel ) )
	{
		const int nCount = std::atoi( FindAttr( node, "count" ).c_str() );
		if ( nCount < 0 ) return false;
		out.aimed.clear();
		if ( nCount == 0 ) return body.empty();
		out.aimed.reserve( static_cast<std::size_t>( nCount ) );
		const char *p = body.c_str();
		const char *pEnd = p + body.size();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( i != 0 )
			{
				if ( p >= pEnd || *p != ';' ) return false;
				++p;
			}
			char *q = nullptr;
			const float x = std::strtof( p, &q );
			if ( q == p || q >= pEnd || *q != ',' ) return false;
			p = q + 1;
			const float y = std::strtof( p, &q );
			if ( q == p || q >= pEnd || *q != ',' ) return false;
			p = q + 1;
			const long nAngle = std::strtol( p, &q, 10 );
			if ( q == p || q >= pEnd || *q != ',' ) return false;
			p = q + 1;
			const long nCone = std::strtol( p, &q, 10 );
			if ( q == p ) return false;
			p = q;
			AimedPoint ap;
			ap.x = x;
			ap.y = y;
			ap.nAngle = static_cast<int>( nAngle );
			ap.nCone  = static_cast<int>( nCone );
			out.aimed.push_back( ap );
		}
		if ( p != pEnd ) return false;
		return true;
	}
	// points family
	const std::string szCount = FindAttr( node, "count" );
	const int nCount = std::atoi( szCount.c_str() );
	if ( nCount < 0 ) return false;
	out.points.clear();
	if ( nCount == 0 ) return true;
	out.points.reserve( static_cast<std::size_t>( nCount ) * 2 );
	const char *p = body.c_str();
	const char *pEnd = p + body.size();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( i != 0 )
		{
			if ( p >= pEnd || *p != ';' ) return false;
			++p;
		}
		char *q = nullptr;
		const float x = std::strtof( p, &q );
		if ( q == p || q >= pEnd || *q != ',' ) return false;
		p = q + 1;
		const float y = std::strtof( p, &q );
		if ( q == p ) return false;
		p = q;
		out.points.push_back( x );
		out.points.push_back( y );
	}
	if ( p != pEnd ) return false;
	return true;
}

// Walks the tree (root first, then descendants in storage order) and rebuilds
// the id tables. Ids are stable: an item keeps the id it already has, and only
// an item new since the last walk takes the next unused one, so an id the
// undo history or the geometry map holds still names the same node after an
// insert, delete, restore or move elsewhere in the tree. Called on Open/New
// (after ResetState, so numbering starts at 1) and after every structural
// edit. Every edit that frees an item calls this before allocating another,
// so a recycled address never inherits a dead item's id.
void RebuildIds( ResourceState &state )
{
	std::unordered_map<const NResourceModel::CTreeItem *, int> previous;
	previous.swap( state.itemToId );
	state.idToItem.clear();
	state.parentOf.clear();
	state.preorder.clear();
	if ( !state.pProject || !state.pProject->root )
		return;
	struct Frame { NResourceModel::CTreeItem *pItem; int nParentId; };
	std::vector<Frame> stack;
	stack.push_back( { state.pProject->root.get(), 0 } );
	while ( !stack.empty() )
	{
		Frame f = stack.back();
		stack.pop_back();
		auto itPrevious = previous.find( f.pItem );
		const int nId = itPrevious != previous.end() ? itPrevious->second : ++state.nNextNodeId;
		state.preorder.push_back( nId );
		state.idToItem[nId] = f.pItem;
		state.itemToId[f.pItem] = nId;
		state.parentOf[nId] = f.nParentId;
		// Push children in reverse so pre-order traversal emits them in storage order.
		auto &children = f.pItem->MutableChildren();
		for ( auto it = children.rbegin(); it != children.rend(); ++it )
			stack.push_back( { it->get(), nId } );
	}
}

bool ReadFileBytes( const std::string &szPath, std::string &out )
{
	std::ifstream f( szPath, std::ios::binary );
	if ( !f )
		return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

bool WriteFileBytes( const std::string &szPath, const std::string &bytes )
{
	// Binary mode: the bytes are already CRLF-encoded by NResourceXml::Serialise,
	// and the test tier compares them byte-for-byte against the file on disk.
	std::ofstream f( szPath, std::ios::binary | std::ios::trunc );
	if ( !f )
		return false;
	f.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
	return f.good();
}

// The login name MFC's LockFile puts in `locked_<user>` (GetUserName). The
// test seam BK_RESOURCE_EDITOR_USER stands in for it so a test can play two
// users without touching the real account. Path separators and the other
// characters a file name cannot hold become '_'.
std::string LockUserName()
{
	std::string szUser;
	if ( const char *pszSeam = std::getenv( "BK_RESOURCE_EDITOR_USER" ) )
		szUser = pszSeam;
#if defined(_WIN32) || defined(_WIN64)
	if ( szUser.empty() )
	{
		char buf[256] = {};
		DWORD n = sizeof( buf );
		if ( GetUserNameA( buf, &n ) )
			szUser = buf;
	}
#else
	if ( szUser.empty() )
		if ( const passwd *pw = getpwuid( geteuid() ) )
			if ( pw->pw_name != nullptr )
				szUser = pw->pw_name;
	if ( szUser.empty() )
		if ( const char *pszEnv = std::getenv( "USER" ) )
			szUser = pszEnv;
#endif
	if ( szUser.empty() )
		szUser = "unknown";
	for ( char &c : szUser )
		if ( c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' )
			c = '_';
	return szUser;
}

const char *kLockPrefix = "locked_";

// Every `locked_*` file in the project's folder, as (path, user) pairs - what
// MFC's NFile::EnumerateFiles( szDir, "locked_*" ) finds. The lock is per
// folder, as in MFC: two projects in one folder share it.
std::vector<std::pair<std::string, std::string>> LockFilesIn( const std::filesystem::path &dir )
{
	std::vector<std::pair<std::string, std::string>> out;
	std::error_code ec;
	for ( std::filesystem::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		const std::string szName = it->path().filename().string();
		if ( szName.compare( 0, std::strlen( kLockPrefix ), kLockPrefix ) != 0 )
			continue;
		out.emplace_back( it->path().string(), szName.substr( std::strlen( kLockPrefix ) ) );
	}
	return out;
}

std::filesystem::path ProjectFolder( const std::string &szPath )
{
	std::filesystem::path dir = std::filesystem::path( szPath ).parent_path();
	return dir.empty() ? std::filesystem::path( "." ) : dir;
}

std::string JoinOwners( const std::vector<std::pair<std::string, std::string>> &locks, const std::string &szExcept )
{
	std::string out;
	for ( const auto &l : locks )
	{
		if ( l.second == szExcept )
			continue;
		if ( !out.empty() )
			out += ",";
		out += l.second;
	}
	return out;
}

// Takes `locked_<user>` for the session. MFC's LockFile checks for other
// locks, then creates its own; two editors can both pass the check. The
// exclusive create plus a second look afterwards closes that window: an
// editor that finds another user's lock next to its own backs off, so two
// racing users may both be refused but never both hold the lock.
BkEditorStatus AcquireLock( BkEditorSession *pSession, ResourceState &state, bool bTakeOver )
{
	if ( !state.bOpen || state.szPath.empty() )
	{
		pSession->szMessage = "no on-disk project to lock";
		return BK_EDITOR_REFUSED;
	}
	const std::string szUser = LockUserName();
	const std::filesystem::path dir = ProjectFolder( state.szPath );
	const std::string szMine = ( dir / ( std::string( kLockPrefix ) + szUser ) ).string();
	std::error_code ec;
	auto locks = LockFilesIn( dir );
	if ( bTakeOver )
	{
		for ( const auto &l : locks )
			if ( l.second != szUser )
				std::filesystem::remove( l.first, ec );
		locks = LockFilesIn( dir );
	}
	std::string szOthers = JoinOwners( locks, szUser );
	if ( !szOthers.empty() )
	{
		pSession->szMessage = "the project is locked by " + szOthers;
		return BK_EDITOR_REFUSED;
	}
	bool bCreated = false;
	if ( std::FILE *pLock = std::fopen( szMine.c_str(), "wbx" ) )
	{
		bCreated = true;
		if ( std::fclose( pLock ) != 0 )
		{
			std::filesystem::remove( szMine, ec );
			pSession->szMessage = "cannot write lock file";
			return BK_EDITOR_FAILED;
		}
	}
	else if ( !std::filesystem::exists( szMine, ec ) )
	{
		pSession->szMessage = "cannot write lock file";
		return BK_EDITOR_FAILED;
	}
	szOthers = JoinOwners( LockFilesIn( dir ), szUser );
	if ( !szOthers.empty() )
	{
		if ( bCreated )
			std::filesystem::remove( szMine, ec );
		pSession->szMessage = "the project is locked by " + szOthers;
		return BK_EDITOR_REFUSED;
	}
	// A `locked_<user>` that was already there is this user's own, as MFC
	// treats it; the session takes charge of removing it on close.
	state.bHoldsLock = true;
	state.szLockPath = szMine;
	return BK_EDITOR_OK;
}

// A removed subtree is written as its own one-node Document so Serialise can
// emit it; restore parses the same shape back out. Keeping the shape minimal
// (no declaration) keeps the blob small for undo stacks.
void EmitNodeFor( const NResourceModel::CTreeItem &item, NResourceXml::Node &out );

void EmitNodeFor( const NResourceModel::CTreeItem &item, NResourceXml::Node &out )
{
	if ( NResourceModel::FutureBlob::IsFutureBlob( item ) )
	{
		out = static_cast<const NResourceModel::FutureBlob &>( item ).GetNode();
		return;
	}
	// A typed item is written as the <item> MFC's childs list holds: its
	// ClassTypeID and its operator&, children included.
	out.kind = NResourceXml::Node::Element;
	out.name = "item";
	item.serialise( out );
}

std::string SerialiseSubtree( const ResourceState &state, NResourceModel::CTreeItem &item );

std::unique_ptr<NResourceModel::CTreeItem> ParseSubtree( const std::string &szBlob, NResourceXml::Document &doc, std::string &szError )

{
	if ( !NResourceXml::Parse( szBlob, doc, szError ) )
		return nullptr;
	// An <item> with a ClassTypeID the factory knows comes back typed, read
	// by its own operator&; anything else is kept whole as a FutureBlob.
	auto &factory = NResourceModel::CTreeItemFactory::Instance();
	std::unique_ptr<NResourceModel::CTreeItem> p;
	if ( doc.root.name == "item" )
		for ( const auto &attr : doc.root.attrs )
			if ( attr.first == "ClassTypeID" || attr.first == "type" )
			{
				p = factory.Create( (int)std::strtol( attr.second.c_str(), nullptr, 0 ) );
				if ( attr.first == "ClassTypeID" )
					break;
			}
	if ( !p )
		return std::make_unique<NResourceModel::FutureBlob>( doc.root );
	p->parse( doc.root );
	return p;
}

// The subtree's items in the pre-order RebuildIds numbers them in.
void CollectPreorder( NResourceModel::CTreeItem *pItem, std::vector<NResourceModel::CTreeItem *> &out )
{
	out.push_back( pItem );
	for ( auto &pChild : pItem->MutableChildren() )
		CollectPreorder( pChild.get(), out );
}

// A delete blob leads with the subtree's ids, so BkResRestoreNode can give
// every node its old id back and the undo history's ids stay valid (the
// fake bridge does the same). The XML parser skips a leading comment.
const char *kIdsPrefix = "<!--bk_ids:";

std::string IdsHeader( ResourceState &state, NResourceModel::CTreeItem *pItem )
{
	std::vector<NResourceModel::CTreeItem *> items;
	CollectPreorder( pItem, items );
	std::string out = kIdsPrefix;
	for ( std::size_t i = 0; i < items.size(); ++i )
		out += ( i ? "," : "" ) + std::to_string( state.itemToId[items[i]] );
	return out + "-->";
}

std::vector<int> ReadIdsHeader( const std::string &szBlob )
{
	std::vector<int> ids;
	if ( szBlob.compare( 0, std::strlen( kIdsPrefix ), kIdsPrefix ) != 0 )
		return ids;
	const std::size_t nEnd = szBlob.find( "-->" );
	std::size_t i = std::strlen( kIdsPrefix );
	while ( i < nEnd )
	{
		ids.push_back( std::atoi( szBlob.c_str() + i ) );
		i = szBlob.find( ',', i );
		if ( i == std::string::npos || i > nEnd ) break;
		++i;
	}
	return ids;
}

// Finds the container (parent's children vector) that owns the node with id
// nNodeId, plus its index in it. Returns false if nNodeId is the root or
// unknown.
bool LocateInParent( ResourceState &state, int nNodeId,
                     NResourceModel::CTreeItem::CTreeItemList **ppContainer,
                     std::size_t *pIndex )
{
	auto itItem = state.idToItem.find( nNodeId );
	if ( itItem == state.idToItem.end() )
		return false;
	auto itParent = state.parentOf.find( nNodeId );
	if ( itParent == state.parentOf.end() || itParent->second == 0 )
		return false;
	auto itParentItem = state.idToItem.find( itParent->second );
	if ( itParentItem == state.idToItem.end() )
		return false;
	auto &children = itParentItem->second->MutableChildren();
	for ( std::size_t i = 0; i < children.size(); ++i )
	{
		if ( children[i].get() == itItem->second )
		{
			*ppContainer = &children;
			*pIndex = i;
			return true;
		}
	}
	return false;
}

void CopyFixed( char *pOut, int nCapacity, const std::string &szSrc )
{
	if ( pOut == nullptr || nCapacity <= 0 )
		return;
	const std::size_t nMax = static_cast<std::size_t>( nCapacity - 1 );
	const std::size_t n = szSrc.size() < nMax ? szSrc.size() : nMax;
	std::memcpy( pOut, szSrc.data(), n );
	pOut[n] = 0;
}

bool IsDescendant( const NResourceModel::CTreeItem *pAncestor, const NResourceModel::CTreeItem *pTarget )
{
	if ( pAncestor == pTarget )
		return true;
	for ( const auto &pChild : pAncestor->GetChildren() )
		if ( IsDescendant( pChild.get(), pTarget ) )
			return true;
	return false;
}

// Geometry lives in the project as `_bk_geometry` elements inside the
// element of the item that owns it, beside its default_name/values/childs.
// It is never part of the item tree: an item keeps an unknown element in its
// layout, so what Load read comes back out of Save, and every write strips
// the elements and puts the session's current geometry back. An item whose
// parent does not write its children (bSerializeChilds off, or a FutureBlob)
// has no element of its own; its geometry goes on the nearest ancestor that
// has one, with `path` naming the child indices down to the owner.
bool IsLayoutSpace( const NResourceXml::Node &node )
{
	if ( node.kind != NResourceXml::Node::Text )
		return false;
	for ( char c : node.text )
		if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' )
			return false;
	return true;
}

void StripGeometry( NResourceXml::Node &node )
{
	auto &children = node.children;
	for ( std::size_t i = 0; i < children.size(); )
	{
		if ( children[i].kind == NResourceXml::Node::Element && children[i].name == kGeometryTag )
			children.erase( children.begin() + i );
		else
			StripGeometry( children[i++] );
	}
}

// The elements of an item's children, in treeItemList order: the entries of
// its `childs` list, which CTreeItem::WriteData writes one per child. Empty
// when the item writes no list or the list does not line up with the tree.
std::vector<NResourceXml::Node *> ChildElements( NResourceModel::CTreeItem &item, NResourceXml::Node &elem )
{
	std::vector<NResourceXml::Node *> out;
	if ( NResourceModel::FutureBlob::IsFutureBlob( item ) || item.MutableChildren().empty() )
		return out;
	for ( auto &c : elem.children )
	{
		if ( c.kind != NResourceXml::Node::Element || c.name != "childs" )
			continue;
		for ( auto &entry : c.children )
			if ( !IsLayoutSpace( entry ) )
				out.push_back( &entry );
		break;
	}
	if ( out.size() != item.MutableChildren().size() )
		out.clear();
	return out;
}

void InjectGeometry( const ResourceState &state, NResourceModel::CTreeItem &item, NResourceXml::Node *pElem,
                     NResourceXml::Node &anchor, const std::string &szPath )
{
	auto itId = state.itemToId.find( &item );
	if ( itId != state.itemToId.end() )
	{
		for ( auto it = state.geometry.lower_bound( std::make_pair( itId->second, std::numeric_limits<int>::min() ) );
		      it != state.geometry.end() && it->first.first == itId->second; ++it )
		{
			NResourceXml::Node emitted = EmitGeometryChild( it->first.second, it->second );
			if ( pElem == nullptr )
				emitted.attrs.insert( emitted.attrs.begin(), { "path", szPath } );
			( pElem != nullptr ? *pElem : anchor ).children.push_back( std::move( emitted ) );
		}
	}
	const bool bOwnElement = pElem != nullptr && pElem->kind == NResourceXml::Node::Element;
	std::vector<NResourceXml::Node *> elems;
	if ( bOwnElement )
		elems = ChildElements( item, *pElem );
	NResourceXml::Node &childAnchor = bOwnElement ? *pElem : anchor;
	const std::string szBase = bOwnElement ? std::string() : szPath + ".";
	auto &children = item.MutableChildren();
	for ( std::size_t i = 0; i < children.size(); ++i )
	{
		NResourceXml::Node *pChild = i < elems.size() && elems[i]->kind == NResourceXml::Node::Element ? elems[i] : nullptr;
		InjectGeometry( state, *children[i], pChild, childAnchor, ( bOwnElement ? std::string() : szBase ) + std::to_string( i ) );
	}
}

// Puts the session's geometry for the subtree under pItem into elem, the
// subtree's freshly written XML, after taking out whatever geometry elements
// the items carried over from the file they were read from.
void WriteGeometry( const ResourceState &state, NResourceModel::CTreeItem &item, NResourceXml::Node &elem )
{
	StripGeometry( elem );
	InjectGeometry( state, item, &elem, elem, std::string() );
}

void ReadGeometry( ResourceState &state, NResourceModel::CTreeItem &item, NResourceXml::Node &elem )
{
	for ( const auto &c : elem.children )
	{
		if ( c.kind != NResourceXml::Node::Element || c.name != kGeometryTag )
			continue;
		int nChannel = -1;
		GeometryBlob blob;
		// A garbled element is dropped: the next write strips it.
		if ( !ParseGeometryChild( c, nChannel, blob ) )
			continue;
		NResourceModel::CTreeItem *pOwner = &item;
		const std::string szPath = FindAttr( c, "path" );
		for ( std::size_t i = 0; pOwner != nullptr && i < szPath.size(); )
		{
			const std::size_t nIndex = static_cast<std::size_t>( std::atoi( szPath.c_str() + i ) );
			auto &children = pOwner->MutableChildren();
			pOwner = nIndex < children.size() ? children[nIndex].get() : nullptr;
			i = szPath.find( '.', i );
			i = i == std::string::npos ? szPath.size() : i + 1;
		}
		if ( pOwner == nullptr )
			continue;
		auto itId = state.itemToId.find( pOwner );
		if ( itId != state.itemToId.end() )
			state.geometry[ std::make_pair( itId->second, nChannel ) ] = std::move( blob );
	}
	std::vector<NResourceXml::Node *> elems = ChildElements( item, elem );
	auto &children = item.MutableChildren();
	for ( std::size_t i = 0; i < elems.size(); ++i )
		if ( elems[i]->kind == NResourceXml::Node::Element )
			ReadGeometry( state, *children[i], *elems[i] );
}

// A delete blob holds the subtree with its geometry, so a restore brings
// both back and a save after delete -> restore matches the one before.
std::string SerialiseSubtree( const ResourceState &state, NResourceModel::CTreeItem &item )
{
	NResourceXml::Document doc;
	doc.hasDeclaration = false;
	EmitNodeFor( item, doc.root );
	WriteGeometry( state, item, doc.root );
	return NResourceXml::Serialise( doc );
}

} // namespace

extern "C" {

/* ---- Projects --------------------------------------------------------- */

BkEditorStatus BkResNew( BkResSession *pSession, BkResKind kind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( kind < 0 || kind >= kKindCount )
		{
			pSession->szMessage = "unknown resource kind";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		ResourceState &state = StateOf( pSession );
		ResetState( state );
		auto &factory = NResourceModel::CTreeItemFactory::Instance();
		auto pRoot = factory.Create( kKindTable[kind].nRootType );
		if ( !pRoot )
		{
			pSession->szMessage = "factory refused the kind";
			return BK_EDITOR_FAILED;
		}
		auto pProject = std::make_unique<NResourceModel::Project>();
		pProject->document.hasDeclaration = true;
		pProject->document.declaration = " version=\"1.0\"";
		pProject->document.root.kind = NResourceXml::Node::Element;
		pProject->document.root.name = kKindTable[kind].pszTag;
		pProject->root = std::move( pRoot );
		state.pProject = std::move( pProject );
		state.bOpen = true;
		state.nKindOrdinal = kind;
		RebuildIds( state );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResOpen( BkResSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == nullptr || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		std::string bytes;
		if ( !ReadFileBytes( pszPath, bytes ) )
		{
			pSession->szMessage = std::string( "cannot read " ) + pszPath;
			return BK_EDITOR_DATA_MISSING;
		}
		auto pProject = std::make_unique<NResourceModel::Project>();
		std::string szError;
		if ( !NResourceModel::Load( bytes, *pProject, szError ) )
		{
			pSession->szMessage = std::string( "parse failed: " ) + szError;
			return BK_EDITOR_DATA_MISSING;
		}
		const int nOrdinal = KindOrdinalFromRootType( pProject->root ? pProject->root->GetItemType() : 0 );
		ResourceState &state = StateOf( pSession );
		ResetState( state );
		state.pProject = std::move( pProject );
		state.bOpen = true;
		state.nKindOrdinal = nOrdinal;
		state.szPath = pszPath;
		RebuildIds( state );
		if ( state.pProject->root )
			ReadGeometry( state, *state.pProject->root, state.pProject->document.root );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSave( BkResSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == nullptr || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || !state.pProject )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		std::string szIntended = NResourceModel::Save( *state.pProject );
		// Re-parsing Serialise's output and writing it again gives the same
		// bytes (xml.h), so a project without geometry is not touched.
		if ( state.pProject->root && ( !state.geometry.empty() || szIntended.find( kGeometryTag ) != std::string::npos ) )
		{
			NResourceXml::Document doc;
			std::string szError;
			if ( !NResourceXml::Parse( szIntended, doc, szError ) )
			{
				pSession->szMessage = "cannot re-read the rendered project: " + szError;
				return BK_EDITOR_FAILED;
			}
			WriteGeometry( state, *state.pProject->root, doc.root );
			szIntended = NResourceXml::Serialise( doc );
		}

		// Safe-save pattern (mirrors SaveSessionMap in session.cpp):
		//  1. Back up any pre-existing destination to <path>.bak.
		//  2. Write bytes to <path>.tmp.
		//  3. Read .tmp back; compare against the just-rendered output.
		//  4. Rename .tmp -> final. Any failure clears .tmp and surfaces .bak.
		const std::string szFinal = pszPath;
		const std::string szTmp = szFinal + ".tmp";
		const std::string szBak = szFinal + ".bak";
		std::error_code ec;
		if ( std::filesystem::exists( szFinal, ec ) )
		{
			std::filesystem::remove( szBak, ec );
			std::filesystem::copy_file( szFinal, szBak, ec );
			if ( ec )
			{
				pSession->szMessage = "cannot back up existing file: " + ec.message();
				return BK_EDITOR_FAILED;
			}
		}
		if ( !WriteFileBytes( szTmp, szIntended ) )
		{
			std::filesystem::remove( szTmp, ec );
			pSession->szMessage = "cannot write " + szTmp;
			return BK_EDITOR_FAILED;
		}
		std::string szRead;
		if ( !ReadFileBytes( szTmp, szRead ) )
		{
			std::filesystem::remove( szTmp, ec );
			pSession->szMessage = "cannot read back " + szTmp;
			return BK_EDITOR_FAILED;
		}
		if ( szRead != szIntended )
		{
			std::filesystem::remove( szTmp, ec );
			pSession->szMessage = "the written file reads back different";
			return BK_EDITOR_FAILED;
		}
		std::filesystem::rename( szTmp, szFinal, ec );
		if ( ec )
		{
			std::filesystem::remove( szTmp, ec );
			pSession->szMessage = "cannot rename tmp into place: " + ec.message();
			return BK_EDITOR_FAILED;
		}
		state.szPath = szFinal;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResClose( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		ResetState( state );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResKindOf( BkResSession *pSession, BkResKind *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			*pOut = -1;
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		*pOut = state.nKindOrdinal;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResLock( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return AcquireLock( pSession, StateOf( pSession ), false );
	} );
}

BkEditorStatus BkResLockTakeOver( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return AcquireLock( pSession, StateOf( pSession ), true );
	} );
}

BkEditorStatus BkResLockOwner( BkResSession *pSession, char *pOut, int nCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == nullptr || nCapacity <= 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		pOut[0] = 0;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || state.szPath.empty() )
		{
			pSession->szMessage = "no on-disk project";
			return BK_EDITOR_REFUSED;
		}
		// Empty string = no owner.
		CopyFixed( pOut, nCapacity, JoinOwners( LockFilesIn( ProjectFolder( state.szPath ) ), std::string() ) );
		return BK_EDITOR_OK;
	} );
}

/* ---- Tree ------------------------------------------------------------- */

BkEditorStatus BkResNodes( BkResSession *pSession, BkResNodeRecord *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || !state.pProject )
		{
			if ( pnCount != nullptr )
				*pnCount = 0;
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		// Preorder walk, root first, as RebuildIds recorded it.
		const int nTotal = static_cast<int>( state.preorder.size() );
		if ( pnCount != nullptr )
			*pnCount = nTotal;
		if ( pOut == nullptr || nCapacity <= 0 )
			return BK_EDITOR_OK;
		if ( nCapacity < nTotal )
			return BK_EDITOR_REFUSED;
		int nWritten = 0;
		for ( const int nId : state.preorder )
		{
			auto itItem = state.idToItem.find( nId );
			if ( itItem == state.idToItem.end() )
				continue;
			NResourceModel::CTreeItem *pItem = itItem->second;
			BkResNodeRecord &rec = pOut[nWritten++];
			rec.id = nId;
			auto itParent = state.parentOf.find( nId );
			rec.parent = ( itParent == state.parentOf.end() ) ? 0 : itParent->second;
			rec.class_type = pItem->GetItemType();
			rec.expand = 0;
			rec.child_count = static_cast<int>( pItem->GetChildren().size() );
			const std::string &szName = pItem->GetDisplayName().empty()
				? pItem->GetDefaultName()
				: pItem->GetDisplayName();
			CopyFixed( rec.display_name, sizeof( rec.display_name ), szName );
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResProps( BkResSession *pSession, int nNodeId, BkResPropRecord *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itItem = state.idToItem.find( nNodeId );
		if ( itItem == state.idToItem.end() )
		{
			pSession->szMessage = "unknown node id";
			return BK_EDITOR_REFUSED;
		}
		const auto &values = itItem->second->GetValues();
		const int nTotal = static_cast<int>( values.size() );
		if ( pnCount != nullptr )
			*pnCount = nTotal;
		if ( pOut == nullptr || nCapacity <= 0 )
			return BK_EDITOR_OK;
		if ( nCapacity < nTotal )
			return BK_EDITOR_REFUSED;
		for ( int i = 0; i < nTotal; ++i )
		{
			const NResourceModel::SProp &p = values[i];
			BkResPropRecord &rec = pOut[i];
			rec.id = p.nId;
			rec.domain_type = static_cast<int>( p.nDomenType );
			rec.value_kind = 0;
			rec.combo_count = static_cast<int>( p.szStrings.size() );
			CopyFixed( rec.default_name, sizeof( rec.default_name ), p.szDefaultName );
			CopyFixed( rec.display_name, sizeof( rec.display_name ), p.szDisplayName );
			// Text form: T04 fills this against the typed variant. For T02 the
			// values vectors are empty on every typed shell, so this loop body
			// does not actually run for the current fixtures.
			rec.value_text[0] = 0;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetProp( BkResSession *pSession, int nNodeId, int nPropId, const char *pszText )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszText == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itItem = state.idToItem.find( nNodeId );
		if ( itItem == state.idToItem.end() )
		{
			pSession->szMessage = "unknown node id";
			return BK_EDITOR_REFUSED;
		}
		auto &values = itItem->second->MutableValues();
		for ( auto &p : values )
		{
			if ( p.nId == nPropId )
			{
				// T04 wires the typed CVariant parser against DomenID; here we
				// stash the text to prove the write path reaches the item.
				p.szDisplayName = pszText;
				return BK_EDITOR_OK;
			}
		}
		pSession->szMessage = "unknown prop id";
		return BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkResInsertNode( BkResSession *pSession, int nParentId, int nClassType, int nIndex, int *pnOutId )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnOutId != nullptr )
			*pnOutId = 0;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itParent = state.idToItem.find( nParentId );
		if ( itParent == state.idToItem.end() )
		{
			pSession->szMessage = "unknown parent id";
			return BK_EDITOR_REFUSED;
		}
		auto &factory = NResourceModel::CTreeItemFactory::Instance();
		auto pNew = factory.Create( nClassType );
		if ( !pNew )
		{
			pSession->szMessage = "unknown class type";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		auto &children = itParent->second->MutableChildren();
		std::size_t nAt = static_cast<std::size_t>( nIndex );
		if ( nAt > children.size() ) nAt = children.size();
		children.insert( children.begin() + nAt, std::move( pNew ) );
		RebuildIds( state );
		if ( pnOutId != nullptr )
		{
			auto itId = state.itemToId.find( children[nAt].get() );
			*pnOutId = ( itId == state.itemToId.end() ) ? 0 : itId->second;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResDeleteNode( BkResSession *pSession, int nNodeId, unsigned char *pOutBlob, int nCapacity, int *pnSize )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itItem = state.idToItem.find( nNodeId );
		if ( itItem == state.idToItem.end() )
		{
			pSession->szMessage = "unknown node id";
			return BK_EDITOR_REFUSED;
		}
		auto itParent = state.parentOf.find( nNodeId );
		if ( itParent == state.parentOf.end() || itParent->second == 0 )
		{
			pSession->szMessage = "cannot delete the root";
			return BK_EDITOR_REFUSED;
		}
		// Two-pass size: a null buffer gets just the byte count.
		const std::string szBlob = IdsHeader( state, itItem->second ) + SerialiseSubtree( state, *itItem->second );
		const int nTotal = static_cast<int>( szBlob.size() );
		if ( pnSize != nullptr )
			*pnSize = nTotal;
		if ( pOutBlob == nullptr || nCapacity <= 0 )
			return BK_EDITOR_OK;
		if ( nCapacity < nTotal )
			return BK_EDITOR_REFUSED;
		std::memcpy( pOutBlob, szBlob.data(), nTotal );

		// Second phase: actually remove from the tree.
		NResourceModel::CTreeItem::CTreeItemList *pContainer = nullptr;
		std::size_t nAt = 0;
		if ( !LocateInParent( state, nNodeId, &pContainer, &nAt ) )
		{
			pSession->szMessage = "cannot locate node in its parent";
			return BK_EDITOR_FAILED;
		}
		pContainer->erase( pContainer->begin() + nAt );
		RebuildIds( state );
		// The blob carries the removed subtree's geometry; drop it here so it is
		// not written for a node that is gone. A restore reads it back.
		for ( auto it = state.geometry.begin(); it != state.geometry.end(); )
			it = state.idToItem.count( it->first.first ) ? std::next( it ) : state.geometry.erase( it );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResRestoreNode( BkResSession *pSession, const unsigned char *pBlob, int nSize, int nParentId, int nIndex, int *pnOutId )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnOutId != nullptr )
			*pnOutId = 0;
		if ( pBlob == nullptr || nSize <= 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itParent = state.idToItem.find( nParentId );
		if ( itParent == state.idToItem.end() )
		{
			pSession->szMessage = "unknown parent id";
			return BK_EDITOR_REFUSED;
		}
		const std::string szBlob( reinterpret_cast<const char *>( pBlob ), static_cast<std::size_t>( nSize ) );
		std::string szError;
		NResourceXml::Document doc;
		auto pItem = ParseSubtree( szBlob, doc, szError );
		if ( !pItem )
		{
			pSession->szMessage = std::string( "parse failed: " ) + szError;
			return BK_EDITOR_FAILED;
		}
		auto &children = itParent->second->MutableChildren();
		std::size_t nAt = static_cast<std::size_t>( nIndex );
		if ( nAt > children.size() ) nAt = children.size();
		NResourceModel::CTreeItem *pInserted = pItem.get();
		// Seed the old ids where they are still free; RebuildIds keeps a
		// seeded id and numbers anything else afresh.
		const std::vector<int> ids = ReadIdsHeader( szBlob );
		std::vector<NResourceModel::CTreeItem *> items;
		CollectPreorder( pInserted, items );
		if ( ids.size() == items.size() )
			for ( std::size_t i = 0; i < items.size(); ++i )
				if ( ids[i] > 0 && ids[i] <= state.nNextNodeId && state.idToItem.count( ids[i] ) == 0 )
					state.itemToId[items[i]] = ids[i];
		children.insert( children.begin() + nAt, std::move( pItem ) );
		RebuildIds( state );
		ReadGeometry( state, *pInserted, doc.root );
		if ( pnOutId != nullptr )
		{
			auto itId = state.itemToId.find( pInserted );
			*pnOutId = ( itId == state.itemToId.end() ) ? 0 : itId->second;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResMoveNode( BkResSession *pSession, int nNodeId, int nNewParentId, int nNewIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		auto itItem = state.idToItem.find( nNodeId );
		auto itNewParent = state.idToItem.find( nNewParentId );
		if ( itItem == state.idToItem.end() || itNewParent == state.idToItem.end() )
		{
			pSession->szMessage = "unknown node id";
			return BK_EDITOR_REFUSED;
		}
		if ( IsDescendant( itItem->second, itNewParent->second ) )
		{
			pSession->szMessage = "cannot move a node under itself";
			return BK_EDITOR_REFUSED;
		}
		NResourceModel::CTreeItem::CTreeItemList *pContainer = nullptr;
		std::size_t nAt = 0;
		if ( !LocateInParent( state, nNodeId, &pContainer, &nAt ) )
		{
			pSession->szMessage = "cannot move the root";
			return BK_EDITOR_REFUSED;
		}
		std::unique_ptr<NResourceModel::CTreeItem> pMoved = std::move( ( *pContainer )[nAt] );
		pContainer->erase( pContainer->begin() + nAt );
		auto &newChildren = itNewParent->second->MutableChildren();
		std::size_t nTarget = static_cast<std::size_t>( nNewIndex );
		if ( nTarget > newChildren.size() ) nTarget = newChildren.size();
		newChildren.insert( newChildren.begin() + nTarget, std::move( pMoved ) );
		RebuildIds( state );
		return BK_EDITOR_OK;
	} );
}

/* Entry points the header declares but this slice has not built yet answer
   BK_EDITOR_FAILED with a message, never a silent OK: a caller must not take
   an export, a preview or a geometry write that did nothing for success. */
static BkEditorStatus NotImplemented( BkResSession *pSession, const char *pszWhat )
{
	pSession->szMessage = std::string( pszWhat ) + " is not implemented yet";
	return BK_EDITOR_FAILED;
}

/* ---- References ------------------------------------------------------- */

BkEditorStatus BkResRefList( BkResSession *pSession, int nType, BkResReferenceEntry *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nType < 0 || nType > 19 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pnCount != nullptr )
			*pnCount = 0;
		return NotImplemented( pSession, "BkResRefList" );
	} );
}

/* ---- Geometry --------------------------------------------------------- */

/* T05 implements the cells family (passability, locked tiles, transparency
   lines) end-to-end: the entries serve against the in-session geometry map
   on ResourceState, and the Open/Save path persists / restores each entry as
   a `_bk_geometry` element in the owning item's element. The rest of the
   channels (points, aimed points, keyframes) stay stubbed below until T06+.

   The two bytes_grid channels share one helper (GetBytesGrid / SetBytesGrid)
   because they only differ in channel id; the points channels have their own
   pair because the payload is Point2 structs, not bytes. */

namespace {

BkEditorStatus GetBytesGrid( BkResSession *pSession, int nChannel, int nNodeId,
                             unsigned char *pOut, int nCapacity, int *pnW, int *pnH )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	auto it = state.geometry.find( std::make_pair( nNodeId, nChannel ) );
	if ( it == state.geometry.end() )
	{
		if ( pnW != nullptr ) *pnW = 0;
		if ( pnH != nullptr ) *pnH = 0;
		return BK_EDITOR_OK;
	}
	const GeometryBlob &blob = it->second;
	if ( pnW != nullptr ) *pnW = blob.nWidth;
	if ( pnH != nullptr ) *pnH = blob.nHeight;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	const int nNeeded = static_cast<int>( blob.bytes.size() );
	if ( nCapacity < nNeeded )
		return BK_EDITOR_REFUSED;
	if ( nNeeded > 0 )
		std::memcpy( pOut, blob.bytes.data(), static_cast<std::size_t>( nNeeded ) );
	return BK_EDITOR_OK;
}

BkEditorStatus SetBytesGrid( BkResSession *pSession, int nChannel, int nNodeId,
                             const unsigned char *pIn, int nW, int nH )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	if ( nW < 0 || nH < 0 )
	{
		pSession->szMessage = "negative grid dimensions";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	const std::size_t nTotal = static_cast<std::size_t>( nW ) * static_cast<std::size_t>( nH );
	if ( nTotal != 0 && pIn == nullptr )
	{
		pSession->szMessage = "null buffer for non-empty grid";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	GeometryBlob blob;
	blob.nWidth = nW;
	blob.nHeight = nH;
	blob.bytes.assign( pIn, pIn + nTotal );
	state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
	return BK_EDITOR_OK;
}

} // namespace

BkEditorStatus BkResGetPassabilityCells( BkResSession *pSession, int nNodeId, unsigned char *pOut, int nCapacity, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetBytesGrid( pSession, CHANNEL_PASSABILITY_CELLS, nNodeId, pOut, nCapacity, pnW, pnH );
	} );
}

BkEditorStatus BkResSetPassabilityCells( BkResSession *pSession, int nNodeId, const unsigned char *pIn, int nW, int nH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetBytesGrid( pSession, CHANNEL_PASSABILITY_CELLS, nNodeId, pIn, nW, nH );
	} );
}

BkEditorStatus BkResGetLockedTiles( BkResSession *pSession, int nNodeId, unsigned char *pOut, int nCapacity, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetBytesGrid( pSession, CHANNEL_LOCKED_TILES, nNodeId, pOut, nCapacity, pnW, pnH );
	} );
}

BkEditorStatus BkResSetLockedTiles( BkResSession *pSession, int nNodeId, const unsigned char *pIn, int nW, int nH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetBytesGrid( pSession, CHANNEL_LOCKED_TILES, nNodeId, pIn, nW, nH );
	} );
}

/* Points-family helpers: transparency lines, formation positions and
   bridge span marks all carry a flat Point2 list and differ only in channel
   id, so one Get/Set pair serves them. */

namespace {

BkEditorStatus GetPoints2( BkResSession *pSession, int nChannel, int nNodeId,
                           BkResPoint2 *pOut, int nCapacity, int *pnCount )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	auto it = state.geometry.find( std::make_pair( nNodeId, nChannel ) );
	if ( it == state.geometry.end() )
	{
		if ( pnCount != nullptr ) *pnCount = 0;
		return BK_EDITOR_OK;
	}
	const int nCount = static_cast<int>( it->second.points.size() / 2 );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		pOut[i].x = it->second.points[2*i];
		pOut[i].y = it->second.points[2*i + 1];
	}
	return BK_EDITOR_OK;
}

BkEditorStatus SetPoints2( BkResSession *pSession, int nChannel, int nNodeId,
                           const BkResPoint2 *pIn, int nCount )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	if ( nCount < 0 )
	{
		pSession->szMessage = "negative point count";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( nCount != 0 && pIn == nullptr )
	{
		pSession->szMessage = "null buffer for non-empty list";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	GeometryBlob blob;
	blob.points.reserve( static_cast<std::size_t>( nCount ) * 2 );
	for ( int i = 0; i < nCount; ++i )
	{
		blob.points.push_back( pIn[i].x );
		blob.points.push_back( pIn[i].y );
	}
	state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
	return BK_EDITOR_OK;
}

} // namespace

BkEditorStatus BkResGetTransparencyLines( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoints2( pSession, CHANNEL_TRANSPARENCY_LINES, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetTransparencyLines( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoints2( pSession, CHANNEL_TRANSPARENCY_LINES, nNodeId, pIn, nCount );
	} );
}

BkEditorStatus BkResGetFormationPositions( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoints2( pSession, CHANNEL_FORMATION_POSITIONS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetFormationPositions( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoints2( pSession, CHANNEL_FORMATION_POSITIONS, nNodeId, pIn, nCount );
	} );
}

BkEditorStatus BkResGetBridgeSpanMarks( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoints2( pSession, CHANNEL_BRIDGE_SPAN_MARKS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetBridgeSpanMarks( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoints2( pSession, CHANNEL_BRIDGE_SPAN_MARKS, nNodeId, pIn, nCount );
	} );
}

/* Point2 and aimed-point helpers.
   T06 adds the second geometry family (zero point, entrance + the four aimed-
   point channels). The payload crosses the ABI as MFC-era degrees for the
   angle; a typed building/squad item class is free to store engine turns
   internally and convert here, but the ABI boundary stays degrees so a
   caller does not have to know the item class's internal convention. */

namespace {

BkEditorStatus GetPoint2( BkResSession *pSession, int nChannel, int nNodeId, BkResPoint2 *pOut )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	if ( pOut == nullptr )
		return BK_EDITOR_BAD_ARGUMENT;
	pOut->x = 0;
	pOut->y = 0;
	auto it = state.geometry.find( std::make_pair( nNodeId, nChannel ) );
	if ( it == state.geometry.end() )
		return BK_EDITOR_OK;
	const GeometryBlob &blob = it->second;
	if ( blob.points.size() >= 2 )
	{
		pOut->x = blob.points[0];
		pOut->y = blob.points[1];
	}
	return BK_EDITOR_OK;
}

BkEditorStatus SetPoint2( BkResSession *pSession, int nChannel, int nNodeId, const BkResPoint2 *pIn )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	if ( pIn == nullptr )
		return BK_EDITOR_BAD_ARGUMENT;
	GeometryBlob blob;
	blob.points.push_back( pIn->x );
	blob.points.push_back( pIn->y );
	state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
	return BK_EDITOR_OK;
}

BkEditorStatus GetAimed( BkResSession *pSession, int nChannel, int nNodeId,
                         BkResAimedPoint *pOut, int nCapacity, int *pnCount )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	auto it = state.geometry.find( std::make_pair( nNodeId, nChannel ) );
	if ( it == state.geometry.end() )
	{
		if ( pnCount != nullptr ) *pnCount = 0;
		return BK_EDITOR_OK;
	}
	const int nCount = static_cast<int>( it->second.aimed.size() );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		const AimedPoint &src = it->second.aimed[i];
		pOut[i].at.x  = src.x;
		pOut[i].at.y  = src.y;
		pOut[i].angle = src.nAngle;
		pOut[i].cone  = src.nCone;
	}
	return BK_EDITOR_OK;
}

BkEditorStatus SetAimed( BkResSession *pSession, int nChannel, int nNodeId,
                         const BkResAimedPoint *pIn, int nCount )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.idToItem.find( nNodeId ) == state.idToItem.end() )
	{
		pSession->szMessage = "unknown node id";
		return BK_EDITOR_REFUSED;
	}
	if ( nCount < 0 )
	{
		pSession->szMessage = "negative aimed-point count";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( nCount != 0 && pIn == nullptr )
	{
		pSession->szMessage = "null buffer for non-empty aimed list";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	GeometryBlob blob;
	blob.aimed.reserve( static_cast<std::size_t>( nCount ) );
	for ( int i = 0; i < nCount; ++i )
	{
		AimedPoint ap;
		ap.x = pIn[i].at.x;
		ap.y = pIn[i].at.y;
		ap.nAngle = pIn[i].angle;
		ap.nCone  = pIn[i].cone;
		blob.aimed.push_back( ap );
	}
	state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
	return BK_EDITOR_OK;
}

} // namespace

BkEditorStatus BkResGetZeroPoint( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoint2( pSession, CHANNEL_ZERO_POINT, nNodeId, pOut );
	} );
}

BkEditorStatus BkResSetZeroPoint( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoint2( pSession, CHANNEL_ZERO_POINT, nNodeId, pIn );
	} );
}

BkEditorStatus BkResGetEntrance( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoint2( pSession, CHANNEL_ENTRANCE, nNodeId, pOut );
	} );
}

BkEditorStatus BkResSetEntrance( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoint2( pSession, CHANNEL_ENTRANCE, nNodeId, pIn );
	} );
}

BkEditorStatus BkResGetShootPoints( BkResSession *pSession, int nNodeId, BkResAimedPoint *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetAimed( pSession, CHANNEL_SHOOT_POINTS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetShootPoints( BkResSession *pSession, int nNodeId, const BkResAimedPoint *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetAimed( pSession, CHANNEL_SHOOT_POINTS, nNodeId, pIn, nCount );
	} );
}

BkEditorStatus BkResGetFirePoints( BkResSession *pSession, int nNodeId, BkResAimedPoint *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetAimed( pSession, CHANNEL_FIRE_POINTS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetFirePoints( BkResSession *pSession, int nNodeId, const BkResAimedPoint *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetAimed( pSession, CHANNEL_FIRE_POINTS, nNodeId, pIn, nCount );
	} );
}

BkEditorStatus BkResGetSmokePoints( BkResSession *pSession, int nNodeId, BkResAimedPoint *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetAimed( pSession, CHANNEL_SMOKE_POINTS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetSmokePoints( BkResSession *pSession, int nNodeId, const BkResAimedPoint *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetAimed( pSession, CHANNEL_SMOKE_POINTS, nNodeId, pIn, nCount );
	} );
}

BkEditorStatus BkResGetDirectedExplosionPoints( BkResSession *pSession, int nNodeId, BkResAimedPoint *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetAimed( pSession, CHANNEL_DIRECTED_EXPLOSION_POINTS, nNodeId, pOut, nCapacity, pnCount );
	} );
}

BkEditorStatus BkResSetDirectedExplosionPoints( BkResSession *pSession, int nNodeId, const BkResAimedPoint *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetAimed( pSession, CHANNEL_DIRECTED_EXPLOSION_POINTS, nNodeId, pIn, nCount );
	} );
}

#define BKRES_GET_POINT2_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResPoint2 *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return NotImplemented( pSession, #fname ); \
	} ); \
}
#define BKRES_SET_POINT2_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResPoint2 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, #fname ); } ); \
}
BKRES_GET_POINT2_STUB( BkResGetMissionObjectives )
BKRES_SET_POINT2_STUB( BkResSetMissionObjectives )
BKRES_GET_POINT2_STUB( BkResGetChapterCrosses )
BKRES_SET_POINT2_STUB( BkResSetChapterCrosses )
BKRES_GET_POINT2_STUB( BkResGetCampaignCrosses )
BKRES_SET_POINT2_STUB( BkResSetCampaignCrosses )
#undef BKRES_GET_POINT2_STUB
#undef BKRES_SET_POINT2_STUB

#define BKRES_GET_VEC3_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResVec3 *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return NotImplemented( pSession, #fname ); \
	} ); \
}
#define BKRES_SET_VEC3_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResVec3 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, #fname ); } ); \
}
BKRES_GET_VEC3_STUB( BkResGetParticleKeyframes )
BKRES_SET_VEC3_STUB( BkResSetParticleKeyframes )
BKRES_GET_VEC3_STUB( BkResGetEffectKeyframes )
BKRES_SET_VEC3_STUB( BkResSetEffectKeyframes )
#undef BKRES_GET_VEC3_STUB
#undef BKRES_SET_VEC3_STUB

/* ---- Export ----------------------------------------------------------- */

static void ClearReport( BkResExportReport *pReport )
{
	if ( pReport == 0 )
		return;
	pReport->written = 0;
	pReport->skipped = 0;
	pReport->warning_count = 0;
}

BkEditorStatus BkResExport( BkResSession *pSession, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return NotImplemented( pSession, "BkResExport" );
	} );
}

BkEditorStatus BkResExportStatsOnly( BkResSession *pSession, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return NotImplemented( pSession, "BkResExportStatsOnly" );
	} );
}

BkEditorStatus BkResBatch( BkResSession *pSession, int, const char *, const char *, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return NotImplemented( pSession, "BkResBatch" );
	} );
}

/* ---- MOD -------------------------------------------------------------- */

BkEditorStatus BkResModSettingsGet( BkResSession *pSession, BkResModSettings *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 )
		{
			pOut->name[0] = 0;
			pOut->version[0] = 0;
			pOut->bake_compressed = 0;
			pOut->bake_packed = 0;
		}
		return NotImplemented( pSession, "BkResModSettingsGet" );
	} );
}

BkEditorStatus BkResModSettingsSet( BkResSession *pSession, const BkResModSettings * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResModSettingsSet" ); } );
}

BkEditorStatus BkResPackMod( BkResSession *pSession, const char * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPackMod" ); } );
}

/* ---- Preview --------------------------------------------------------- */

BkEditorStatus BkResPreviewBegin( BkResSession *pSession, BkResKind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPreviewBegin" ); } );
}

BkEditorStatus BkResPreviewShow( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPreviewShow" ); } );
}

BkEditorStatus BkResPreviewStop( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPreviewStop" ); } );
}

BkEditorStatus BkResPreviewPlayback( BkResSession *pSession, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPreviewPlayback" ); } );
}

BkEditorStatus BkResPreviewCamera( BkResSession *pSession, float, float, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return NotImplemented( pSession, "BkResPreviewCamera" ); } );
}

/* ---- Import ----------------------------------------------------------- */

BkEditorStatus BkResImportFromGame( BkResSession *pSession, BkResKind, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return NotImplemented( pSession, "BkResImportFromGame" );
	} );
}

}
