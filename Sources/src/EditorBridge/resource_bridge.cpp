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
// in T08-T11 (T11: the preview group). T07 replaced the `<path>.lock` sentinel with MFC's per-folder
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
#include "../ResourceModel/references.h"
#include "../ResourceModel/exporter.h"
#include "../Main/RPGStats.h"
#include "../Main/iMain.h"
#include "../Main/GameTimer.h"
#include "../Misc/HPTimer.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneScreenScale.h"
#include "../Scene/PFX.h"
#include "../Anim/Animation.h"
#include "../zlib/zlib.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
#include <sys/stat.h>
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
	// points family: 2*n floats in order (x0, y0, x1, y1, ...); the vec3
	// family keeps 3*n (x0, y0, z0, ...) in the same vector.
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
	// Session-level, not project-level: ResetState leaves these alone so a
	// new project keeps the mod settings and the reference lists. The export
	// dir is BkResModSettingsSet's ("" until then: the default). The lists
	// are built on the first BkResRefList and again when the active mod
	// changes.
	std::string szExportDir;
	bool bRefsBuilt = false;
	std::string szRefsModFolder;
	std::vector<std::string> refLists[NResourceModel::kReferenceTypeCount];
	// The preview (D-16), session-level like the above: it outlives a
	// project close and is rebuilt by the next BkResPreviewShow. The object
	// is held by a raw pointer with its own reference, not a CPtr: this map
	// is destroyed at exit, after the engine modules are gone, so a
	// forgotten BkResPreviewStop must leak the reference, not release it
	// into an unloaded module. The export folder is mounted over the data as
	// the storage layer kPreviewLayer.
	bool bPreview = false;
	int nPreviewKind = -1;
	std::filesystem::path previewRoot;
	IVisObj *pPreviewObj = nullptr;
	bool bPreviewEffect = false;
	bool bPreviewRunning = false;
};

// Channel ids: the C ABI's geometry channel integers (shared with Zig's
// bridge.GeometryChannel enum). T05 wired channels 0..2 (cells family); T06
// adds 3..8 (point2 family + aimed-points family); 9..13 join channel 2 in
// the points family; 14 and 15 are the vec3 family. Kept as a plain enum so a
// test can hand-assert against an integer.
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
	CHANNEL_BRIDGE_SPAN_MARKS = 10,
	CHANNEL_MISSION_OBJECTIVES = 11,
	CHANNEL_CHAPTER_CROSSES = 12,
	CHANNEL_CAMPAIGN_CROSSES = 13,
	CHANNEL_PARTICLE_KEYFRAMES = 14,
	CHANNEL_EFFECT_KEYFRAMES = 15
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

static bool IsVec3Channel( int nChannel )
{
	return nChannel == CHANNEL_PARTICLE_KEYFRAMES || nChannel == CHANNEL_EFFECT_KEYFRAMES;
}

// Floats per entry in a list channel's `points` vector: three for the vec3
// family, two for the points family.
static int ListStride( int nChannel )
{
	return IsVec3Channel( nChannel ) ? 3 : 2;
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
//   vec3      : <_bk_geometry channel="N" count="K">x0,y0,z0;x1,y1,z1;...</_bk_geometry>
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
		const int nStride = ListStride( nChannel );
		const int nCount = static_cast<int>( blob.points.size() / nStride );
		out.attrs.push_back( { "count", std::to_string( nCount ) } );
		text.reserve( blob.points.size() * 10 );
		for ( int i = 0; i < nCount; ++i )
		{
			for ( int k = 0; k < nStride; ++k )
			{
				if ( i != 0 || k != 0 ) text.push_back( k == 0 ? ';' : ',' );
				char buf[32];
				std::snprintf( buf, sizeof( buf ), "%.9g", blob.points[nStride*i + k] );
				text += buf;
			}
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
	// points and vec3 families: `count` tuples of ListStride floats.
	const int nStride = ListStride( nChannel );
	const std::string szCount = FindAttr( node, "count" );
	const int nCount = std::atoi( szCount.c_str() );
	if ( nCount < 0 ) return false;
	out.points.clear();
	if ( nCount == 0 ) return true;
	out.points.reserve( static_cast<std::size_t>( nCount ) * nStride );
	const char *p = body.c_str();
	const char *pEnd = p + body.size();
	for ( int i = 0; i < nCount; ++i )
	{
		for ( int k = 0; k < nStride; ++k )
		{
			if ( i != 0 || k != 0 )
			{
				if ( p >= pEnd || *p != ( k == 0 ? ';' : ',' ) ) return false;
				++p;
			}
			char *q = nullptr;
			const float v = std::strtof( p, &q );
			if ( q == p ) return false;
			p = q;
			out.points.push_back( v );
		}
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

// The bytes BkResSave writes for the open project: the tree, plus every
// geometry entry as a `_bk_geometry` element in its owner's element. Export
// hands its exporter a project parsed from the same bytes, so what is
// exported is what a save would write. Re-parsing Serialise's output and
// writing it again gives the same bytes (xml.h), so a project without
// geometry is not touched.
bool RenderForSave( const ResourceState &state, std::string &szOut, std::string &szError )
{
	szOut = NResourceModel::Save( *state.pProject );
	if ( state.pProject->root && ( !state.geometry.empty() || szOut.find( kGeometryTag ) != std::string::npos ) )
	{
		NResourceXml::Document doc;
		std::string szParseError;
		if ( !NResourceXml::Parse( szOut, doc, szParseError ) )
		{
			szError = "cannot re-read the rendered project: " + szParseError;
			return false;
		}
		WriteGeometry( state, *state.pProject->root, doc.root );
		szOut = NResourceXml::Serialise( doc );
	}
	return true;
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
		std::string szIntended;
		if ( !RenderForSave( state, szIntended, pSession->szMessage ) )
			return BK_EDITOR_FAILED;

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
			rec.value_kind = static_cast<int>( p.value.GetKind() );
			rec.combo_count = static_cast<int>( p.szStrings.size() );
			CopyFixed( rec.default_name, sizeof( rec.default_name ), p.szDefaultName );
			CopyFixed( rec.display_name, sizeof( rec.display_name ), p.szDisplayName );
			// The typed value's text form (CVariant::ToString), the same text
			// BkResSetProp parses back into the prop's kind.
			CopyFixed( rec.value_text, sizeof( rec.value_text ), p.value.ToString() );
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
				// Parsed into the kind the prop already holds, so a float stays
				// a float and the project writes the same element type back.
				p.value = NResourceModel::CVariant::FromString( p.value.GetKind(), pszText );
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

/* ---- References ------------------------------------------------------- */

extern "C++" {
namespace {

// A folder's child by name, case-insensitively: an exact match first, then
// any entry that folds equal (the staged install has both "mods" and "Mods";
// a mod may say "Data" or "data"). The name as given when nothing matches,
// so a folder that does not exist yet is created with MFC's spelling.
std::filesystem::path ChildFolder( const std::filesystem::path &dir, const std::string &szName )
{
	std::error_code ec;
	const std::filesystem::path exact = dir / szName;
	if ( std::filesystem::is_directory( exact, ec ) )
		return exact;
	auto fold = []( std::string s ) { for ( char &c : s ) c = char( std::tolower( (unsigned char)c ) ); return s; };
	const std::string szWant = fold( szName );
	for ( std::filesystem::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->is_directory( ec ) && fold( it->path().filename().string() ) == szWant )
			return it->path();
	return exact;
}

std::filesystem::path ShippedDataFolder( const BkEditorSession *pSession )
{
	return ChildFolder( pSession->szDataRoot.empty() ? std::filesystem::path( "." ) : std::filesystem::path( pSession->szDataRoot ), "Data" );
}

// The active mod's data folder, or an empty path with none active.
std::filesystem::path ModDataFolder( const BkEditorSession *pSession )
{
	if ( pSession->szModFolder.empty() || pSession->szModFolder == "none" )
		return std::filesystem::path();
	const std::filesystem::path root = pSession->szDataRoot.empty() ? std::filesystem::path( "." ) : std::filesystem::path( pSession->szDataRoot );
	return ChildFolder( ChildFolder( ChildFolder( root, "mods" ), pSession->szModFolder ), "data" );
}

void BuildReferenceLists( BkEditorSession *pSession, ResourceState &state )
{
	if ( state.bRefsBuilt && state.szRefsModFolder == pSession->szModFolder )
		return;
	NResourceModel::References base, mod;
	base.rebuild( ShippedDataFolder( pSession ) );
	const std::filesystem::path modData = ModDataFolder( pSession );
	if ( !modData.empty() )
		mod.rebuild( modData );
	for ( int i = 0; i < NResourceModel::kReferenceTypeCount; ++i )
	{
		const auto eType = static_cast<NResourceModel::EReferenceType>( i );
		std::vector<std::string> list = base.enumerate( eType );
		for ( const std::string &szEntry : mod.enumerate( eType ) )
			if ( std::find( list.begin(), list.end(), szEntry ) == list.end() )
				list.push_back( szEntry );
		state.refLists[i] = std::move( list );
	}
	state.bRefsBuilt = true;
	state.szRefsModFolder = pSession->szModFolder;
}

// Copies text into a fixed C field, truncated, always NUL-terminated.
void CopyField( char *pField, std::size_t nSize, const std::string &szText )
{
	const std::size_t n = std::min( nSize - 1, szText.size() );
	std::memcpy( pField, szText.data(), n );
	pField[n] = 0;
}

} // namespace
} // extern "C++"

BkEditorStatus BkResRefList( BkResSession *pSession, int nType, BkResReferenceEntry *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nType < 0 || nType >= NResourceModel::kReferenceTypeCount || pnCount == nullptr || nCapacity < 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnCount = 0;
		if ( pSession->szDataRoot.empty() )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		ResourceState &state = StateOf( pSession );
		BuildReferenceLists( pSession, state );
		const std::vector<std::string> &list = state.refLists[nType];
		*pnCount = int( list.size() );
		if ( pOut == nullptr && nCapacity == 0 )
			return BK_EDITOR_OK;
		if ( pOut == nullptr || nCapacity < int( list.size() ) )
		{
			pSession->szMessage = "the buffer holds " + std::to_string( nCapacity ) + " of " + std::to_string( list.size() ) + " entries";
			return BK_EDITOR_REFUSED;
		}
		for ( std::size_t i = 0; i < list.size(); ++i )
		{
			pOut[i].token = int( i );
			CopyField( pOut[i].name, sizeof( pOut[i].name ), list[i] );
		}
		return BK_EDITOR_OK;
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

/* Points-family helpers: transparency lines, formation positions, bridge
   span marks, mission objectives and chapter/campaign crosses all carry a
   flat Point2 list and differ only in channel id, so one Get/Set pair serves
   them. */

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

/* Vec3-family helpers: particle and effect keyframes carry a flat 3D vector
   list. MFC's framesList entries are (time, value) pairs; z travels through
   unchanged so a caller can keep a third component without a new channel. */

namespace {

BkEditorStatus GetVec3List( BkResSession *pSession, int nChannel, int nNodeId,
                            BkResVec3 *pOut, int nCapacity, int *pnCount )
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
	const int nCount = static_cast<int>( it->second.points.size() / 3 );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		pOut[i].x = it->second.points[3*i];
		pOut[i].y = it->second.points[3*i + 1];
		pOut[i].z = it->second.points[3*i + 2];
	}
	return BK_EDITOR_OK;
}

BkEditorStatus SetVec3List( BkResSession *pSession, int nChannel, int nNodeId,
                            const BkResVec3 *pIn, int nCount )
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
		pSession->szMessage = "negative keyframe count";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( nCount != 0 && pIn == nullptr )
	{
		pSession->szMessage = "null buffer for non-empty list";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	GeometryBlob blob;
	blob.points.reserve( static_cast<std::size_t>( nCount ) * 3 );
	for ( int i = 0; i < nCount; ++i )
	{
		blob.points.push_back( pIn[i].x );
		blob.points.push_back( pIn[i].y );
		blob.points.push_back( pIn[i].z );
	}
	state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
	return BK_EDITOR_OK;
}

} // namespace

#define BKRES_POINTS2_PAIR( name, channel ) \
BkEditorStatus BkResGet##name( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut, int nCapacity, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		return GetPoints2( pSession, channel, nNodeId, pOut, nCapacity, pnCount ); \
	} ); \
} \
BkEditorStatus BkResSet##name( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn, int nCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		return SetPoints2( pSession, channel, nNodeId, pIn, nCount ); \
	} ); \
}
BKRES_POINTS2_PAIR( MissionObjectives, CHANNEL_MISSION_OBJECTIVES )
BKRES_POINTS2_PAIR( ChapterCrosses, CHANNEL_CHAPTER_CROSSES )
BKRES_POINTS2_PAIR( CampaignCrosses, CHANNEL_CAMPAIGN_CROSSES )
#undef BKRES_POINTS2_PAIR

#define BKRES_VEC3_PAIR( name, channel ) \
BkEditorStatus BkResGet##name( BkResSession *pSession, int nNodeId, BkResVec3 *pOut, int nCapacity, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		return GetVec3List( pSession, channel, nNodeId, pOut, nCapacity, pnCount ); \
	} ); \
} \
BkEditorStatus BkResSet##name( BkResSession *pSession, int nNodeId, const BkResVec3 *pIn, int nCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		return SetVec3List( pSession, channel, nNodeId, pIn, nCount ); \
	} ); \
}
BKRES_VEC3_PAIR( ParticleKeyframes, CHANNEL_PARTICLE_KEYFRAMES )
BKRES_VEC3_PAIR( EffectKeyframes, CHANNEL_EFFECT_KEYFRAMES )
#undef BKRES_VEC3_PAIR

/* ---- Export ----------------------------------------------------------- */

extern "C++" {
namespace {

// The project extension of each BkResKind ordinal, kKindTable's order.
const char *const kKindExtensions[] =
{
	"wpn", "mcp", "trc", "scp", "spt", "unt", "msh", "obt", "fnc", "bld", "bdg",
	"pcp", "eff", "til", "3rd", "3rv", "mip", "chc", "cgc", "mdc", "gui"
};
static_assert( sizeof( kKindExtensions ) / sizeof( kKindExtensions[0] ) == sizeof( kKindTable ) / sizeof( kKindTable[0] ),
               "one extension per kind" );

std::string Fold( std::string s )
{
	for ( char &c : s )
		c = char( std::tolower( (unsigned char)c ) );
	return s;
}

// The export dir (MFC's clean destination dir, the mod's own folder).
std::filesystem::path ExportDirOf( BkEditorSession *pSession )
{
	const ResourceState &state = StateOf( pSession );
	if ( !state.szExportDir.empty() )
		return std::filesystem::path( state.szExportDir );
	const std::filesystem::path root = pSession->szDataRoot.empty() ? std::filesystem::path( "." ) : std::filesystem::path( pSession->szDataRoot );
	const bool bMod = !pSession->szModFolder.empty() && pSession->szModFolder != "none";
	return ChildFolder( ChildFolder( root, "mods" ), bMod ? pSession->szModFolder : std::string( "mymod" ) );
}

// True when dir is the shipped Data/ folder: no export, mod.xml or batch
// ever writes there (spec "Saving and exporting").
bool IsShippedData( const BkEditorSession *pSession, const std::filesystem::path &dir )
{
	std::error_code ec;
	const std::filesystem::path shipped = ShippedDataFolder( pSession );
	if ( std::filesystem::exists( dir, ec ) && std::filesystem::exists( shipped, ec ) )
		return std::filesystem::equivalent( dir, shipped, ec );
	return Fold( std::filesystem::weakly_canonical( dir, ec ).generic_string() ) == Fold( std::filesystem::weakly_canonical( shipped, ec ).generic_string() );
}

// The regular files below dir, as generic relative paths, sorted.
std::vector<std::string> FilesBelow( const std::filesystem::path &dir )
{
	std::vector<std::string> files;
	std::error_code ec;
	for ( std::filesystem::recursive_directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->is_regular_file( ec ) )
			files.push_back( std::filesystem::relative( it->path(), dir, ec ).generic_string() );
	std::sort( files.begin(), files.end() );
	return files;
}

// One project through its kind's exporter: the exporter writes into a
// staging folder beside data/, and only when it succeeds are its files
// moved into data/, so a failed export leaves no half-written resource.
bool ExportOne( const NResourceModel::Project &project, const std::string &szProjectPath, const std::string &szExtension,
                const std::filesystem::path &dataDir, int nFlags, bool bStatsOnly,
                NResourceModel::SExportOutcome &outcome, std::string &szError )
{
	const NResourceModel::FExporter pfnExporter = NResourceModel::FindExporter( szExtension );
	if ( pfnExporter == nullptr )
	{
		szError = "exporting ." + szExtension + " projects is not ported yet; the exporter comes with its sub-editor";
		return false;
	}
	std::error_code ec;
	const std::filesystem::path staging = dataDir.parent_path() / ".bk-export-staging";
	std::filesystem::remove_all( staging, ec );
	std::filesystem::create_directories( staging, ec );
	if ( ec )
	{
		szError = "cannot create the staging folder " + staging.string() + ": " + ec.message();
		return false;
	}
	NResourceModel::SExportContext context;
	context.szProjectPath = szProjectPath;
	context.szStagingRoot = staging.string();
	context.bForce = ( nFlags & BK_RES_EXPORT_FORCE ) != 0;
	context.bStatsOnly = bStatsOnly;
	if ( !pfnExporter( project, context, outcome ) )
	{
		std::filesystem::remove_all( staging, ec );
		szError = outcome.szError.empty() ? std::string( "the exporter failed" ) : outcome.szError;
		return false;
	}
	for ( const std::string &szRelative : FilesBelow( staging ) )
	{
		const std::filesystem::path target = dataDir / szRelative;
		std::filesystem::create_directories( target.parent_path(), ec );
		std::filesystem::rename( staging / szRelative, target, ec );
		if ( ec )
		{
			szError = "cannot move " + szRelative + " into " + dataDir.string() + ": " + ec.message();
			std::filesystem::remove_all( staging, ec );
			return false;
		}
	}
	std::filesystem::remove_all( staging, ec );
	return true;
}

// Fills the caller's report. The export has already happened, so a short
// warnings buffer gets as many as fit; warning_count is always the total.
void FillReport( BkResExportReport *pReport, int nWritten, int nSkipped, const std::vector<std::string> &warnings )
{
	if ( pReport == nullptr )
		return;
	pReport->written = nWritten;
	pReport->skipped = nSkipped;
	pReport->warning_count = int( warnings.size() );
	if ( pReport->warnings == nullptr )
		return;
	for ( int i = 0; i < pReport->warnings_capacity && i < int( warnings.size() ); ++i )
		CopyField( pReport->warnings[i].text, sizeof( pReport->warnings[i].text ), warnings[i] );
}

BkEditorStatus ExportOpenProject( BkResSession *pSession, int nFlags, bool bStatsOnly, BkResExportReport *pReport )
{
	FillReport( pReport, 0, 0, std::vector<std::string>() );
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen || !state.pProject )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.szPath.empty() )
	{
		pSession->szMessage = "save the project first: an export reads its sources beside the project file";
		return BK_EDITOR_REFUSED;
	}
	if ( state.nKindOrdinal < 0 || state.nKindOrdinal >= kKindCount )
	{
		pSession->szMessage = "the open project is not of a registered kind";
		return BK_EDITOR_REFUSED;
	}
	const std::string szExtension = kKindExtensions[state.nKindOrdinal];
	if ( NResourceModel::FindExporter( szExtension ) == nullptr )
	{
		pSession->szMessage = "exporting ." + szExtension + " projects is not ported yet; the exporter comes with its sub-editor";
		return BK_EDITOR_REFUSED;
	}
	const std::filesystem::path dataDir = ChildFolder( ExportDirOf( pSession ), "data" );
	if ( IsShippedData( pSession, dataDir ) )
	{
		pSession->szMessage = "the export root is the shipped Data folder; set a mod folder in MOD settings";
		return BK_EDITOR_REFUSED;
	}
	std::string szBytes;
	if ( !RenderForSave( state, szBytes, pSession->szMessage ) )
		return BK_EDITOR_FAILED;
	NResourceModel::Project project;
	std::string szError;
	if ( !NResourceModel::Load( szBytes, project, szError ) )
	{
		pSession->szMessage = "cannot re-read the project for export: " + szError;
		return BK_EDITOR_FAILED;
	}
	NResourceModel::SExportOutcome outcome;
	if ( !ExportOne( project, state.szPath, szExtension, dataDir, nFlags, bStatsOnly, outcome, szError ) )
	{
		FillReport( pReport, 0, 0, outcome.warnings );
		pSession->szMessage = szError;
		return BK_EDITOR_FAILED;
	}
	FillReport( pReport, outcome.nWritten, outcome.nSkipped, outcome.warnings );
	pSession->szMessage = "exported " + std::to_string( outcome.nWritten ) + " files into " + dataDir.string();
	return BK_EDITOR_OK;
}

// MFC's -os: a project re-saved through the model, temp + read back +
// rename like BkResSave, without a .bak (the batch never kept one).
bool ResaveProject( const std::string &szPath, const std::string &szBytes, std::string &szError )
{
	const std::string szTmp = szPath + ".tmp";
	std::error_code ec;
	std::string szRead;
	if ( !WriteFileBytes( szTmp, szBytes ) || !ReadFileBytes( szTmp, szRead ) || szRead != szBytes )
	{
		std::filesystem::remove( szTmp, ec );
		szError = "cannot write " + szTmp;
		return false;
	}
	std::filesystem::rename( szTmp, szPath, ec );
	if ( ec )
	{
		std::filesystem::remove( szTmp, ec );
		szError = "cannot rename " + szTmp + ": " + ec.message();
		return false;
	}
	return true;
}

} // namespace
} // extern "C++"

BkEditorStatus BkResExport( BkResSession *pSession, int nFlags, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return ExportOpenProject( pSession, nFlags, false, pReport ); } );
}

BkEditorStatus BkResExportStatsOnly( BkResSession *pSession, int nFlags, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return ExportOpenProject( pSession, nFlags, true, pReport ); } );
}

BkEditorStatus BkResBatch( BkResSession *pSession, int nKind, const char *pszSrc, const char *pszDst, int nFlags, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		FillReport( pReport, 0, 0, std::vector<std::string>() );
		if ( pszSrc == nullptr || pszDst == nullptr || *pszDst == 0 || nKind < -1 || nKind >= kKindCount )
			return BK_EDITOR_BAD_ARGUMENT;
		std::error_code ec;
		if ( !std::filesystem::is_directory( pszSrc, ec ) )
		{
			pSession->szMessage = std::string( pszSrc ) + " is not a folder";
			return BK_EDITOR_DATA_MISSING;
		}
		const bool bOpenSave = ( nFlags & BK_RES_EXPORT_OPEN_SAVE ) != 0;
		const std::filesystem::path dataDir = ChildFolder( pszDst, "data" );
		if ( !bOpenSave && IsShippedData( pSession, dataDir ) )
		{
			pSession->szMessage = "the batch destination is the shipped Data folder";
			return BK_EDITOR_REFUSED;
		}
		// Every project of the chosen kinds, in BkResKind order (MFC's frame
		// order for "all"), then by path.
		std::vector<std::pair<int, std::string>> projects;
		for ( std::filesystem::recursive_directory_iterator it( pszSrc, ec ), end; !ec && it != end; it.increment( ec ) )
		{
			if ( !it->is_regular_file( ec ) )
				continue;
			const std::string szExt = Fold( it->path().extension().string() );
			for ( int k = 0; k < kKindCount; ++k )
				if ( ( nKind == -1 || nKind == k ) && szExt == std::string( "." ) + kKindExtensions[k] )
					projects.push_back( { k, it->path().string() } );
		}
		std::sort( projects.begin(), projects.end() );
		int nWritten = 0, nSkipped = 0;
		std::vector<std::string> warnings;
		for ( const auto &entry : projects )
		{
			const std::string &szPath = entry.second;
			std::string szBytes, szError;
			NResourceModel::Project project;
			if ( !ReadFileBytes( szPath, szBytes ) || !NResourceModel::Load( szBytes, project, szError ) )
			{
				warnings.push_back( szPath + ": cannot read the project " + szError );
				++nSkipped;
				continue;
			}
			if ( bOpenSave )
			{
				if ( ResaveProject( szPath, NResourceModel::Save( project ), szError ) )
					++nWritten;
				else
				{
					warnings.push_back( szPath + ": " + szError );
					++nSkipped;
				}
				continue;
			}
			NResourceModel::SExportOutcome outcome;
			if ( ExportOne( project, szPath, kKindExtensions[entry.first], dataDir, nFlags, false, outcome, szError ) )
			{
				nWritten += outcome.nWritten;
				nSkipped += outcome.nSkipped;
			}
			else
			{
				warnings.push_back( szPath + ": " + szError );
				++nSkipped;
			}
			for ( const std::string &szWarning : outcome.warnings )
				warnings.push_back( szPath + ": " + szWarning );
		}
		FillReport( pReport, nWritten, nSkipped, warnings );
		pSession->szMessage = std::to_string( projects.size() ) + " projects, " + std::to_string( nWritten ) + " written, " +
		                      std::to_string( nSkipped ) + " skipped";
		return BK_EDITOR_OK;
	} );
}

/* ---- MOD -------------------------------------------------------------- */

extern "C++" {
namespace {

// A folder path as the engine's file storage takes it: with a trailing
// separator (comparator.cpp's OpenForRead does the same).
std::string StorageDir( const std::filesystem::path &dir )
{
	std::string s = dir.string();
	if ( s.empty() || ( s.back() != '/' && s.back() != '\\' ) )
		s += '/';
	return s;
}

// mod.xml read by the engine's tree reader, as CEditorApp::ReadMODFile and
// the game's mod list do. Fields stay empty when there is no mod.xml.
void ReadModFile( const std::filesystem::path &dataDir, std::string &szName, std::string &szVersion, std::string &szDesc )
{
	std::error_code ec;
	if ( !std::filesystem::is_regular_file( dataDir / "mod.xml", ec ) )
		return;
	CPtr<IDataStorage> pStorage = OpenStorage( StorageDir( dataDir ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return;
	CPtr<IDataStream> pStream = pStorage->OpenStream( "mod.xml", STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return;
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	saver.Add( "MODName", &szName );
	saver.Add( "MODVersion", &szVersion );
	saver.Add( "MODDesc", &szDesc );
}

// CEditorApp::WriteMODFile: mod.xml through the engine's tree saver, then
// modobjects.xml seeded from the data storage when the mod has none. False
// with a reason when mod.xml cannot be written; a missing seed is reported
// in szNote only (mod.xml is the setting, the seed a convenience).
bool WriteModFile( const std::filesystem::path &dataDir, const std::string &szName, const std::string &szVersion,
                   const std::string &szDesc, std::string &szError, std::string &szNote )
{
	std::error_code ec;
	std::filesystem::remove( dataDir / "mod.xml", ec );
	{
		CPtr<IDataStorage> pStorage = CreateStorage( StorageDir( dataDir ).c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pXMLStream = pStorage != 0 ? pStorage->CreateStream( "mod.xml", STREAM_ACCESS_WRITE ) : 0;
		if ( pXMLStream == 0 )
		{
			szError = "cannot create " + ( dataDir / "mod.xml" ).string();
			return false;
		}
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE );
		if ( pDT == 0 )
		{
			szError = "the engine has no tree saver for mod.xml";
			return false;
		}
		std::string szNameTemp = szName, szVersionTemp = szVersion, szDescTemp = szDesc;
		CTreeAccessor saver = pDT;
		saver.Add( "MODName", &szNameTemp );
		saver.Add( "MODVersion", &szVersionTemp );
		saver.Add( "MODDesc", &szDescTemp );
	}
	if ( !std::filesystem::is_regular_file( dataDir / "mod.xml", ec ) )
	{
		szError = "mod.xml was not written into " + dataDir.string();
		return false;
	}
	if ( std::filesystem::exists( dataDir / "modobjects.xml", ec ) )
		return true;
	CPtr<IDataStorage> pData = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pSeed = pData != 0 ? pData->OpenStream( "editor\\modobjects.xml", STREAM_ACCESS_READ ) : 0;
	if ( pSeed == 0 )
	{
		szNote = "no editor\\modobjects.xml in the data storage to seed the mod's modobjects.xml from";
		return true;
	}
	std::string szBytes( std::size_t( pSeed->GetSize() ), '\0' );
	if ( !szBytes.empty() )
		pSeed->Read( &szBytes[0], int( szBytes.size() ) );
	if ( !WriteFileBytes( ( dataDir / "modobjects.xml" ).string(), szBytes ) )
	{
		szError = "cannot write " + ( dataDir / "modobjects.xml" ).string();
		return false;
	}
	return true;
}

void Put16( std::string &s, unsigned v ) { s.push_back( char( v & 0xff ) ); s.push_back( char( ( v >> 8 ) & 0xff ) ); }
void Put32( std::string &s, unsigned long v ) { Put16( s, unsigned( v & 0xffff ) ); Put16( s, unsigned( ( v >> 16 ) & 0xffff ) ); }

// Raw deflate (no zlib header), level 9 - zip's method 8 as `zip -9` wrote it.
bool Deflate9( const std::string &in, std::string &out )
{
	z_stream z;
	std::memset( &z, 0, sizeof( z ) );
	if ( deflateInit2( &z, 9, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY ) != Z_OK )
		return false;
	// zlib 1.1.3's bound: the input plus 0.1% plus 12 bytes.
	out.assign( in.size() + in.size() / 1000 + 64, '\0' );
	z.next_in = reinterpret_cast<Bytef *>( const_cast<char *>( in.data() ) );
	z.avail_in = uInt( in.size() );
	z.next_out = reinterpret_cast<Bytef *>( &out[0] );
	z.avail_out = uInt( out.size() );
	const int nResult = deflate( &z, Z_FINISH );
	out.resize( z.total_out );
	deflateEnd( &z );
	return nResult == Z_STREAM_END;
}

// The file's modification time as the DOS date (high word) and time (low).
unsigned long DosTime( const std::filesystem::path &file )
{
	std::time_t t = 0;
#if defined(_WIN32)
	struct _stat64 st;
	if ( _wstat64( file.wstring().c_str(), &st ) == 0 )
		t = std::time_t( st.st_mtime );
#else
	struct stat st;
	if ( ::stat( file.string().c_str(), &st ) == 0 )
		t = st.st_mtime;
#endif
	std::tm tm = {};
#if defined(_WIN32)
	localtime_s( &tm, &t );
#else
	localtime_r( &t, &tm );
#endif
	if ( tm.tm_year < 80 )
		return ( 1u << 21 ) | ( 1u << 16 );   // 1980-01-01, the earliest DOS date
	const unsigned long nDate = ( unsigned long )( ( ( tm.tm_year - 80 ) << 9 ) | ( ( tm.tm_mon + 1 ) << 5 ) | tm.tm_mday );
	const unsigned long nTime = ( unsigned long )( ( tm.tm_hour << 11 ) | ( tm.tm_min << 5 ) | ( tm.tm_sec / 2 ) );
	return ( nDate << 16 ) | nTime;
}

// Writes the archive of every file below dataDir into szZip. False with a
// reason on a read or write failure or a size the plain zip format cannot hold.
bool WriteModZip( const std::filesystem::path &dataDir, const std::vector<std::string> &files, const std::string &szZip,
                  std::string &szError )
{
	if ( files.size() > 0xffff )
	{
		szError = "more than 65535 files";
		return false;
	}
	std::ofstream out( szZip, std::ios::binary | std::ios::trunc );
	if ( !out )
	{
		szError = "cannot write " + szZip;
		return false;
	}
	std::string central;
	unsigned long long nOffset = 0;
	for ( const std::string &szName : files )
	{
		std::string szData, szPacked;
		if ( !ReadFileBytes( ( dataDir / szName ).string(), szData ) )
		{
			szError = "cannot read " + szName;
			return false;
		}
		const unsigned long nCrc = crc32( crc32( 0L, Z_NULL, 0 ), reinterpret_cast<const Bytef *>( szData.data() ), uInt( szData.size() ) );
		const bool bDeflated = Deflate9( szData, szPacked ) && szPacked.size() < szData.size();
		const std::string &szStored = bDeflated ? szPacked : szData;
		if ( szData.size() >= 0xffffffffull || nOffset >= 0xffffffffull )
		{
			szError = "the mod is too large for a plain zip";
			return false;
		}
		const unsigned nMethod = bDeflated ? 8 : 0;
		const unsigned nVersion = bDeflated ? 20 : 10;
		const unsigned long nWhen = DosTime( dataDir / szName );
		std::string local;
		Put32( local, 0x04034b50 );
		Put16( local, nVersion );
		Put16( local, 0 );                                // flags
		Put16( local, nMethod );
		Put16( local, unsigned( nWhen & 0xffff ) );
		Put16( local, unsigned( nWhen >> 16 ) );
		Put32( local, nCrc );
		Put32( local, ( unsigned long )szStored.size() );
		Put32( local, ( unsigned long )szData.size() );
		Put16( local, unsigned( szName.size() ) );
		Put16( local, 0 );                                // extra
		local += szName;
		Put32( central, 0x02014b50 );
		Put16( central, 20 );                             // made by: MS-DOS, zip 2.0
		Put16( central, nVersion );
		Put16( central, 0 );
		Put16( central, nMethod );
		Put16( central, unsigned( nWhen & 0xffff ) );
		Put16( central, unsigned( nWhen >> 16 ) );
		Put32( central, nCrc );
		Put32( central, ( unsigned long )szStored.size() );
		Put32( central, ( unsigned long )szData.size() );
		Put16( central, unsigned( szName.size() ) );
		Put16( central, 0 );                              // extra
		Put16( central, 0 );                              // comment
		Put16( central, 0 );                              // disk
		Put16( central, 0 );                              // internal attributes
		Put32( central, 0x20 );                           // external: archive
		Put32( central, ( unsigned long )nOffset );
		central += szName;
		out.write( local.data(), std::streamsize( local.size() ) );
		out.write( szStored.data(), std::streamsize( szStored.size() ) );
		nOffset += local.size() + szStored.size();
	}
	std::string end;
	Put32( end, 0x06054b50 );
	Put16( end, 0 );
	Put16( end, 0 );
	Put16( end, unsigned( files.size() ) );
	Put16( end, unsigned( files.size() ) );
	Put32( end, ( unsigned long )central.size() );
	Put32( end, ( unsigned long )nOffset );
	Put16( end, 0 );
	out.write( central.data(), std::streamsize( central.size() ) );
	out.write( end.data(), std::streamsize( end.size() ) );
	out.close();
	if ( !out )
	{
		szError = "cannot finish writing " + szZip;
		return false;
	}
	return true;
}

// Mounts the archive the way the game mounts a mod's data (a storage over
// the folder, which takes in every *.pak there) and reads every entry back
// against its source. szFolder holds nothing but the archive, so each name
// can only come from it.
bool ReadBackModZip( const std::filesystem::path &dataDir, const std::vector<std::string> &files, const std::filesystem::path &folder,
                     std::string &szError )
{
	CPtr<IDataStorage> pStorage = OpenStorage( ( StorageDir( folder ) + "*.pak" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	if ( pStorage == 0 )
	{
		szError = "the engine's storage does not mount " + folder.string();
		return false;
	}
	for ( const std::string &szName : files )
	{
		std::string szEngineName = szName;
		std::replace( szEngineName.begin(), szEngineName.end(), '/', '\\' );
		CPtr<IDataStream> pStream = pStorage->OpenStream( szEngineName.c_str(), STREAM_ACCESS_READ );
		std::string szWant;
		ReadFileBytes( ( dataDir / szName ).string(), szWant );
		if ( pStream == 0 )
		{
			szError = "the mounted archive has no " + szName;
			return false;
		}
		if ( pStream->GetSize() != int( szWant.size() ) )
		{
			szError = "the mounted archive holds " + szName + " with " + std::to_string( pStream->GetSize() ) + " bytes, not " + std::to_string( szWant.size() );
			return false;
		}
		std::string szGot( szWant.size(), '\0' );
		if ( !szGot.empty() )
			pStream->Read( &szGot[0], int( szGot.size() ) );
		if ( szGot != szWant )
		{
			szError = "the mounted archive reads " + szName + " back different";
			return false;
		}
	}
	return true;
}

} // namespace
} // extern "C++"

BkEditorStatus BkResModSettingsGet( BkResSession *pSession, BkResModSettings *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		std::memset( pOut, 0, sizeof( *pOut ) );
		if ( GetSLS() == 0 || pSession->szDataRoot.empty() )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const std::filesystem::path dir = ExportDirOf( pSession );
		std::string szName, szVersion, szDesc;
		ReadModFile( ChildFolder( dir, "data" ), szName, szVersion, szDesc );
		CopyField( pOut->export_dir, sizeof( pOut->export_dir ), dir.string() );
		CopyField( pOut->name, sizeof( pOut->name ), szName );
		CopyField( pOut->version, sizeof( pOut->version ), szVersion );
		CopyField( pOut->desc, sizeof( pOut->desc ), szDesc );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResModSettingsSet( BkResSession *pSession, const BkResModSettings *pIn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pIn == nullptr || pIn->export_dir[0] == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( GetSLS() == 0 || pSession->szDataRoot.empty() )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		auto field = []( const char *p, std::size_t n ) { return std::string( p, strnlen( p, n ) ); };
		const std::string szDir = field( pIn->export_dir, sizeof( pIn->export_dir ) );
		const std::filesystem::path dataDir = ChildFolder( szDir, "data" );
		if ( IsShippedData( pSession, dataDir ) || IsShippedData( pSession, szDir ) )
		{
			pSession->szMessage = "the shipped Data folder is not a mod folder";
			return BK_EDITOR_REFUSED;
		}
		std::error_code ec;
		std::filesystem::create_directories( dataDir, ec );
		if ( ec )
		{
			pSession->szMessage = "cannot create " + dataDir.string() + ": " + ec.message();
			return BK_EDITOR_REFUSED;
		}
		std::string szError, szNote;
		if ( !WriteModFile( dataDir, field( pIn->name, sizeof( pIn->name ) ), field( pIn->version, sizeof( pIn->version ) ),
		                    field( pIn->desc, sizeof( pIn->desc ) ), szError, szNote ) )
		{
			pSession->szMessage = szError;
			return BK_EDITOR_REFUSED;
		}
		StateOf( pSession ).szExportDir = szDir;
		pSession->szMessage = szNote;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPackMod( BkResSession *pSession, const char *pszZip )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszZip == nullptr || *pszZip == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( GetSLS() == 0 || pSession->szDataRoot.empty() )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const std::filesystem::path dataDir = ChildFolder( ExportDirOf( pSession ), "data" );
		std::error_code ec;
		if ( !std::filesystem::is_directory( dataDir, ec ) )
		{
			pSession->szMessage = "there is no " + dataDir.string() + " to pack";
			return BK_EDITOR_REFUSED;
		}
		const std::string szDataAbs = Fold( std::filesystem::weakly_canonical( dataDir, ec ).generic_string() ) + "/";
		const std::string szZipAbs = Fold( std::filesystem::weakly_canonical( pszZip, ec ).generic_string() );
		if ( szZipAbs.compare( 0, szDataAbs.size(), szDataAbs ) == 0 )
		{
			pSession->szMessage = "the archive would be inside the folder it packs";
			return BK_EDITOR_REFUSED;
		}
		const std::vector<std::string> files = FilesBelow( dataDir );
		if ( files.empty() )
		{
			pSession->szMessage = dataDir.string() + " is empty";
			return BK_EDITOR_REFUSED;
		}
		// Written and checked in a folder of its own beside the destination,
		// then moved into place: a failed pack leaves no archive.
		const std::filesystem::path verify = std::filesystem::absolute( pszZip, ec ).parent_path() / ".bk-pack-verify";
		std::filesystem::remove_all( verify, ec );
		std::filesystem::create_directories( verify, ec );
		const std::filesystem::path staged = verify / "mod.pak";
		std::string szError;
		if ( !WriteModZip( dataDir, files, staged.string(), szError ) || !ReadBackModZip( dataDir, files, verify, szError ) )
		{
			std::filesystem::remove_all( verify, ec );
			pSession->szMessage = szError;
			return BK_EDITOR_FAILED;
		}
		std::filesystem::rename( staged, pszZip, ec );
		const std::string szRename = ec ? ec.message() : std::string();
		std::filesystem::remove_all( verify, ec );
		if ( !szRename.empty() )
		{
			pSession->szMessage = "cannot move the archive into place: " + szRename;
			return BK_EDITOR_FAILED;
		}
		pSession->szMessage = "packed " + std::to_string( files.size() ) + " files";
		return BK_EDITOR_OK;
	} );
}

/* ---- Preview --------------------------------------------------------- */

extern "C++" {
namespace {

// The storage layer the preview's export folder is mounted as, on top of
// the data, the MOD and the user's RMG root. MFC exported its preview into
// the data folder itself (editor\temp); a layer of its own keeps the
// shipped Data/ and the mod untouched.
const char *const kPreviewLayer = "RES_PREVIEW";

// Where an exported visual sits in the empty scene: MFC's mesh preview put
// its unit at the twelfth cell on both axes (CMeshFrame, MeshFrm.cpp), and the
// other frames at the camera's anchor - the camera is placed on this point,
// so both are the same.
const float kPreviewCells = 12.0f;
const float kPreviewCellSize = 32.0f;

// The kinds the preview builds today and what IVisObjBuilder builds them as:
// the four the MFC frames this slice measures (D-17) build, with the frame
// that does it. The other scene kinds (object, fence, building, bridge,
// trench, squad, particle) join with their sub-editor slices; road and river
// load maps\road3d / maps\river3d as their terrain (S13).
struct PreviewKind
{
	int nKind;
	EObjVisType eVisType;
	EObjGameType eGameType;
};
const PreviewKind kPreviewKinds[] =
{
	{ 4,  SGVOT_SPRITE, SGVOGT_UNIT   },  // spt: CSpriteFrame::OnRunButton
	{ 5,  SGVOT_SPRITE, SGVOGT_UNIT   },  // unt: infantry draws as a sprite
	{ 6,  SGVOT_MESH,   SGVOGT_UNIT   },  // msh: CMeshFrame's combat object
	{ 12, SGVOT_EFFECT, SGVOGT_EFFECT },  // eff: CEffectFrame::OnRunButton
};

const PreviewKind *FindPreviewKind( int nKind )
{
	for ( const PreviewKind &entry : kPreviewKinds )
		if ( entry.nKind == nKind )
			return &entry;
	return nullptr;
}

// The game timer at the high-precision clock's now, as the map bridge's
// ghost and world do (session.cpp, world.cpp).
NTimer::STime UpdateGameTimer()
{
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	if ( pTimer == 0 )
		return 0;
	NHPTimer::STime hptime;
	NHPTimer::GetTime( &hptime );
	pTimer->Update( DWORD( NHPTimer::GetSeconds( hptime ) * 1000.0f ) );
	return pTimer->GetGameTime();
}

// DrawSessionFrame's hook while the playback runs: one timer step and one
// update of the preview object per drawn frame.
void PreviewBeforeDraw( SEditorSession *pBase )
{
	ResourceState &state = StateOf( static_cast<BkEditorSession *>( pBase ) );
	const NTimer::STime time = UpdateGameTimer();
	if ( state.pPreviewObj != nullptr )
		state.pPreviewObj->Update( time );
}

// Restarts the object's own animation: an effect from the current game time,
// a sprite or mesh from its first animation.
void RestartPreviewObject( ResourceState &state )
{
	if ( state.pPreviewObj == nullptr )
		return;
	const NTimer::STime time = UpdateGameTimer();
	if ( state.bPreviewEffect )
		static_cast<IEffectVisObj *>( state.pPreviewObj )->SetStartTime( time );
	else if ( IAnimation *pAnimation = static_cast<IObjVisObj *>( state.pPreviewObj )->GetAnimation() )
		pAnimation->SetAnimation( 0 );
	state.pPreviewObj->Update( time );
}

void DropPreviewObject( ResourceState &state )
{
	if ( state.pPreviewObj == nullptr )
		return;
	if ( IScene *pScene = GetSingleton<IScene>() )
		pScene->RemoveObject( state.pPreviewObj );
	state.pPreviewObj->Release();
	state.pPreviewObj = nullptr;
}

// The shared caches that hold what the last export built, by name: the next
// export may write other files under the same names. The same managers
// BkEditorSetMod clears after its storage changes (bridge.cpp
// ReloadAfterModChange), minus sound, fonts, text and the object database,
// which an export into the preview layer does not touch. (CLEAL_ is the
// engine's own spelling.)
void ForgetPreviewCaches()
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>() )
		pVOB->Clear();
	GetSingleton<IParticleManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	GetSingleton<IAnimationManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	GetSingleton<IFilesInspector>()->Clear();
	GetSingleton<IFilesInspector>()->InspectStorage( pStorage );
}

// OpenStorage's own convention for a folder: backslash-separated with a
// trailing "*.pak" mask (bridge.cpp ModEngineDir); the common file system
// takes the loose files below it as well as any packs.
std::string EngineFolderPattern( const std::filesystem::path &dir )
{
	std::string szDir = dir.string();
	for ( char &c : szDir )
		if ( c == '/' ) c = '\\';
	if ( szDir.empty() || szDir.back() != '\\' )
		szDir += '\\';
	return szDir + "*.pak";
}

void StopPreview( BkEditorSession *pSession, ResourceState &state )
{
	if ( !state.bPreview )
		return;
	DropPreviewObject( state );
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
		pStorage->RemoveStorage( kPreviewLayer );
	pSession->pfnBeforeDraw = nullptr;
	std::error_code ec;
	if ( !state.previewRoot.empty() )
		std::filesystem::remove_all( state.previewRoot, ec );
	state.previewRoot.clear();
	state.bPreview = false;
	state.nPreviewKind = -1;
	state.bPreviewEffect = false;
	state.bPreviewRunning = false;
}

// A folder of the system's temp directory for this session's preview export,
// named so two sessions or two processes never share one.
std::filesystem::path NewPreviewRoot( const BkEditorSession *pSession )
{
	static int nCounter = 0;
#if defined(_WIN32) || defined(_WIN64)
	const unsigned long nProcess = GetCurrentProcessId();
#else
	const unsigned long nProcess = (unsigned long)getpid();
#endif
	char szName[96];
	std::snprintf( szName, sizeof szName, "bk-resource-preview-%lu-%p-%d", nProcess, (const void *)pSession, ++nCounter );
	std::error_code ec;
	return std::filesystem::temp_directory_path( ec ) / szName;
}

BkEditorStatus BeginPreview( BkEditorSession *pSession, int nKind )
{
	if ( nKind < 0 || nKind >= kKindCount )
		return BK_EDITOR_BAD_ARGUMENT;
	ResourceState &state = StateOf( pSession );
	IScene *pScene = pSession->bEngineStarted ? GetSingleton<IScene>() : 0;
	if ( pScene == 0 || GetSingleton<IVisObjBuilder>() == 0 || GetSingleton<ICamera>() == 0 )
	{
		pSession->szMessage = "no GPU device: the engine is not started in this session";
		return BK_EDITOR_NO_DEVICE;
	}
	if ( pSession->bMapOpen )
	{
		pSession->szMessage = "a map is open in this session; the preview draws on an empty scene";
		return BK_EDITOR_REFUSED;
	}
	const PreviewKind *pKind = FindPreviewKind( nKind );
	if ( pKind == nullptr )
	{
		pSession->szMessage = std::string( "the preview of ." ) + kKindExtensions[nKind]
			+ " projects is not ported yet; it comes with its sub-editor";
		return BK_EDITOR_REFUSED;
	}
	StopPreview( pSession, state );
	const std::filesystem::path root = NewPreviewRoot( pSession );
	std::error_code ec;
	std::filesystem::create_directories( root, ec );
	if ( ec )
	{
		pSession->szMessage = "cannot create the preview folder " + root.string() + ": " + ec.message();
		return BK_EDITOR_FAILED;
	}
	// D-16: an empty scene - MFC's frames cleared the scene and the builder
	// before every run - and the game's own camera (no yaw override).
	pScene->Clear();
	GetSingleton<IVisObjBuilder>()->Clear();
	const float fOrigin = kPreviewCells * kPreviewCellSize;
	SetSessionCamera( pSession, fOrigin, fOrigin );
	state.bPreview = true;
	state.nPreviewKind = nKind;
	state.previewRoot = root;
	state.bPreviewEffect = pKind->eVisType == SGVOT_EFFECT;
	pSession->szMessage = std::string( "preview of ." ) + kKindExtensions[nKind] + " begun";
	return BK_EDITOR_OK;
}

BkEditorStatus ShowPreview( BkEditorSession *pSession )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bPreview )
	{
		pSession->szMessage = "no preview: call BkResPreviewBegin first";
		return BK_EDITOR_REFUSED;
	}
	if ( !state.bOpen || !state.pProject )
	{
		pSession->szMessage = "no project is open";
		return BK_EDITOR_REFUSED;
	}
	if ( state.nKindOrdinal != state.nPreviewKind )
	{
		pSession->szMessage = std::string( "the preview was begun for ." ) + kKindExtensions[state.nPreviewKind]
			+ " and the open project is not one";
		return BK_EDITOR_REFUSED;
	}
	if ( state.szPath.empty() )
	{
		pSession->szMessage = "save the project first: an export reads its sources beside the project file";
		return BK_EDITOR_REFUSED;
	}
	const std::string szExtension = kKindExtensions[state.nKindOrdinal];
	const NResourceModel::FExporter pfnExporter = NResourceModel::FindExporter( szExtension );
	if ( pfnExporter == nullptr )
	{
		pSession->szMessage = "exporting ." + szExtension + " projects is not ported yet; the exporter comes with its sub-editor";
		return BK_EDITOR_REFUSED;
	}
	// The edited tree, as BkResExport hands it to the exporter.
	std::string szBytes;
	if ( !RenderForSave( state, szBytes, pSession->szMessage ) )
		return BK_EDITOR_FAILED;
	NResourceModel::Project project;
	std::string szError;
	if ( !NResourceModel::Load( szBytes, project, szError ) )
	{
		pSession->szMessage = "cannot re-read the project for the preview: " + szError;
		return BK_EDITOR_FAILED;
	}
	// The old object goes first: its files are about to be replaced.
	DropPreviewObject( state );
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	pStorage->RemoveStorage( kPreviewLayer );
	const std::filesystem::path dataDir = state.previewRoot / "data";
	std::error_code ec;
	std::filesystem::remove_all( dataDir, ec );
	std::filesystem::create_directories( dataDir, ec );
	NResourceModel::SExportContext context;
	context.szProjectPath = state.szPath;
	context.szStagingRoot = dataDir.string();
	context.bForce = true;
	NResourceModel::SExportOutcome outcome;
	if ( !pfnExporter( project, context, outcome ) )
	{
		pSession->szMessage = "the preview export failed: " + ( outcome.szError.empty() ? std::string( "the exporter failed" ) : outcome.szError );
		return BK_EDITOR_FAILED;
	}
	if ( outcome.szObjectName.empty() )
	{
		pSession->szMessage = "the ." + szExtension + " export named no visual to build";
		return BK_EDITOR_FAILED;
	}
	// The folder is enumerated when it is opened, so it is mounted after the
	// export wrote it, and again after every export.
	CPtr<IDataStorage> pPreview = OpenStorage( EngineFolderPattern( dataDir ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	if ( pPreview == 0 )
	{
		pSession->szMessage = "cannot mount the preview folder " + dataDir.string();
		return BK_EDITOR_FAILED;
	}
	pStorage->AddStorage( pPreview, kPreviewLayer );
	ForgetPreviewCaches();
	const PreviewKind *pKind = FindPreviewKind( state.nPreviewKind );
	IVisObj *pObj = GetSingleton<IVisObjBuilder>()->BuildObject( outcome.szObjectName.c_str(), 0, pKind->eVisType );
	if ( pObj == nullptr )
	{
		pSession->szMessage = "IVisObjBuilder would not build \"" + outcome.szObjectName + "\" from the export";
		return BK_EDITOR_FAILED;
	}
	pObj->AddRef();
	state.pPreviewObj = pObj;
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->Update();
	const CVec3 vAnchor = pCamera->GetAnchor();
	pObj->SetPlacement( CVec3( vAnchor.x, vAnchor.y, 0.0f ), 0 );
	RestartPreviewObject( state );
	GetSingleton<IScene>()->AddObject( pObj, pKind->eGameType );
	pSession->szMessage = "built \"" + outcome.szObjectName + "\" from " + std::to_string( outcome.nWritten ) + " exported files";
	return BK_EDITOR_OK;
}

} // namespace
} // extern "C++"

BkEditorStatus BkResPreviewBegin( BkResSession *pSession, BkResKind kind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BeginPreview( pSession, kind ); } );
}

BkEditorStatus BkResPreviewShow( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return ShowPreview( pSession ); } );
}

BkEditorStatus BkResPreviewStop( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		StopPreview( pSession, StateOf( pSession ) );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPreviewPlayback( BkResSession *pSession, int nRun )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bPreview || state.pPreviewObj == nullptr )
		{
			pSession->szMessage = "no preview object: call BkResPreviewBegin and BkResPreviewShow first";
			return BK_EDITOR_REFUSED;
		}
		// MFC's Run restarted the object; Stop left it where it was.
		if ( nRun != 0 && !state.bPreviewRunning )
			RestartPreviewObject( state );
		state.bPreviewRunning = nRun != 0;
		pSession->pfnBeforeDraw = state.bPreviewRunning ? &PreviewBeforeDraw : nullptr;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPreviewCamera( BkResSession *pSession, float fX, float fY, int nZoom )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bPreview )
		{
			pSession->szMessage = "no preview: call BkResPreviewBegin first";
			return BK_EDITOR_REFUSED;
		}
		// The zoom step the game's own wheel sets (GFX.World.ZoomSteps),
		// clamped to the window's range as ZoomAtScreenPoint does.
		const CTRect<float> rcScreen = GetSingleton<IGFX>()->GetScreenRect();
		SetGlobalVar( "GFX.World.ZoomSteps", Clamp( nZoom, 0, NSceneScreenScale::GetMaxZoomSteps( rcScreen ) ) );
		if ( !SetSessionCamera( pSession, fX, fY ) )
			return BK_EDITOR_FAILED;
		return BK_EDITOR_OK;
	} );
}

/* ---- Import ----------------------------------------------------------- */

extern "C++" {
namespace {

const int kInfantryKind = 5; // "unt", kKindTable's Unit_Composer_Project

NResourceModel::CTreeItem *ChildOfType( NResourceModel::CTreeItem &item, int nType )
{
	for ( const auto &pChild : item.GetChildren() )
		if ( pChild->GetItemType() == nType )
			return pChild.get();
	return nullptr;
}

// MFC's `values[n].value = v` setters; a slot the item does not have is
// left alone (MFC asserted on it).
template <class T>
void SetSlot( NResourceModel::CTreeItem *pItem, std::size_t nSlot, const T &value )
{
	if ( pItem != nullptr && nSlot < pItem->MutableValues().size() )
		pItem->MutableValues()[nSlot].value = value;
}

// CAnimationFrame::GetRPGStats (AnimationFrm.cpp), line for line, onto the
// port's items: the slots are the indices MFC's setters in AnimTreeItem.h use.
void InfantryStatsToTree( const SInfantryRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_UNIT_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	const char *pszType = "soldier";
	switch ( rpgStats.type )
	{
		case RPG_TYPE_ENGINEER: pszType = "engineer"; break;
		case RPG_TYPE_SNIPER:   pszType = "sniper";   break;
		case RPG_TYPE_OFFICER:  pszType = "officer";  break;
		default: break;
	}
	SetSlot( pCommonProps, 1, std::string( pszType ) );
	SetSlot( pCommonProps, 3, rpgStats.fMaxHP );
	SetSlot( pCommonProps, 4, rpgStats.nMinArmor );
	SetSlot( pCommonProps, 5, rpgStats.fCamouflage );
	SetSlot( pCommonProps, 6, rpgStats.fSpeed );
	SetSlot( pCommonProps, 7, rpgStats.fPassability );
	SetSlot( pCommonProps, 8, rpgStats.bCanAttackUp );
	SetSlot( pCommonProps, 9, rpgStats.bCanAttackDown );
	SetSlot( pCommonProps, 10, rpgStats.fPrice );
	SetSlot( pCommonProps, 11, rpgStats.fSight );
	SetSlot( pCommonProps, 12, rpgStats.fSightPower );

	// CUnitAnimationPropsItem::SetAnimationSpeed: the prop with id 2.
	if ( NResourceModel::CTreeItem *pAnims = ChildOfType( root, NResourceModel::ETIT_UNIT_ANIMATIONS_ITEM ) )
		for ( const auto &pAnim : pAnims->GetChildren() )
		{
			const std::string &szAnimName = pAnim->GetDefaultName();
			if ( szAnimName != "Run" && szAnimName != "Crawl" )
				continue;
			for ( NResourceModel::SProp &prop : pAnim->MutableValues() )
				if ( prop.nId == 2 )
					prop.value = szAnimName == "Run" ? rpgStats.fRunSpeed : rpgStats.fCrawlSpeed;
		}

	NResourceModel::CTreeItem *pAcks = ChildOfType( root, NResourceModel::ETIT_UNIT_ACKS_ITEM );
	if ( rpgStats.szAcksNames.size() >= 1 )
		SetSlot( pAcks, 0, rpgStats.szAcksNames[0] );
	if ( rpgStats.szAcksNames.size() >= 2 )
		SetSlot( pAcks, 1, rpgStats.szAcksNames[1] );

	// CUnitActionsItem::SetActions / CUnitExposuresItem::SetExposures.
	std::int64_t nActions = 0, nExposures = 0;
	for ( int i = 0; i < 64; ++i )
	{
		if ( rpgStats.HasCommand( i ) )
			nActions |= std::int64_t( 1 ) << i;
		if ( i < rpgStats.availExposures.GetSize() && rpgStats.availExposures.GetData( i ) )
			nExposures |= std::int64_t( 1 ) << i;
	}
	SetSlot( ChildOfType( root, NResourceModel::ETIT_UNIT_ACTIONS_ITEM ), 0, nActions );
	SetSlot( ChildOfType( root, NResourceModel::ETIT_UNIT_EXPOSURES_ITEM ), 0, nExposures );

	if ( rpgStats.guns.size() > 0 )
	{
		NResourceModel::CTreeItem *pWeaponProps = ChildOfType( root, NResourceModel::ETIT_UNIT_WEAPON_PROPS_ITEM );
		SetSlot( pWeaponProps, 0, rpgStats.guns[0].szWeapon );
		SetSlot( pWeaponProps, 1, rpgStats.guns[0].nAmmo );
		SetSlot( pWeaponProps, 2, rpgStats.guns[0].fReloadCost );
	}
	if ( rpgStats.guns.size() > 1 )
	{
		NResourceModel::CTreeItem *pGrenadeProps = ChildOfType( root, NResourceModel::ETIT_UNIT_GRENADE_PROPS_ITEM );
		SetSlot( pGrenadeProps, 0, rpgStats.guns[1].szWeapon );
		SetSlot( pGrenadeProps, 1, rpgStats.guns[1].nAmmo );
		SetSlot( pGrenadeProps, 2, rpgStats.guns[1].fReloadCost );
	}
}

// The runtime folder's 1.xml, its name matched case-insensitively.
std::filesystem::path StatsFileIn( const std::filesystem::path &dir )
{
	std::error_code ec;
	for ( std::filesystem::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->is_regular_file( ec ) && Fold( it->path().filename().string() ) == "1.xml" )
			return it->path();
	return std::filesystem::path();
}

} // namespace
} // extern "C++"

BkEditorStatus BkResImportFromGame( BkResSession *pSession, BkResKind kind, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == nullptr || *pszPath == 0 || kind < 0 || kind >= kKindCount )
			return BK_EDITOR_BAD_ARGUMENT;
		const std::string szExtension = kKindExtensions[kind];
		if ( kind != kInfantryKind )
		{
			pSession->szMessage = kind == 4
				? std::string( "importing .spt is refused: MFC's sprite export only composes .san packs and has no reverse path" )
				: "importing ." + szExtension + " is not ported yet; it comes with its sub-editor";
			return BK_EDITOR_REFUSED;
		}
		if ( GetSLS() == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const std::filesystem::path statsFile = StatsFileIn( pszPath );
		if ( statsFile.empty() )
		{
			pSession->szMessage = std::string( "no 1.xml in " ) + pszPath;
			return BK_EDITOR_DATA_MISSING;
		}
		// CAnimationFrame::LoadRPGStats: the engine's own operator& reads it.
		SInfantryRPGStats rpgStats;
		{
			CPtr<IDataStorage> pStorage = OpenStorage( StorageDir( statsFile.parent_path() ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
			CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( statsFile.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
			CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ ) : 0;
			if ( pDT == 0 )
			{
				pSession->szMessage = "the engine cannot read " + statsFile.string();
				return BK_EDITOR_DATA_MISSING;
			}
			CTreeAccessor tree = pDT;
			tree.Add( "RPG", &rpgStats );
		}
		if ( rpgStats.szKeyName.empty() )
		{
			pSession->szMessage = statsFile.string() + " holds no infantry RPG stats";
			return BK_EDITOR_DATA_MISSING;
		}
		auto pRoot = NResourceModel::CTreeItemFactory::Instance().Create( kKindTable[kind].nRootType );
		if ( !pRoot )
		{
			pSession->szMessage = "factory refused the kind";
			return BK_EDITOR_FAILED;
		}
		// MFC's CreateTrees: the default tree the frame then fills.
		pRoot->CreateDefaultChilds();
		InfantryStatsToTree( rpgStats, *pRoot );
		auto pProject = std::make_unique<NResourceModel::Project>();
		pProject->document.hasDeclaration = true;
		pProject->document.declaration = " version=\"1.0\"";
		pProject->document.root.kind = NResourceXml::Node::Element;
		pProject->document.root.name = kKindTable[kind].pszTag;
		pProject->root = std::move( pRoot );
		ResourceState &state = StateOf( pSession );
		ResetState( state );
		state.pProject = std::move( pProject );
		state.bOpen = true;
		state.nKindOrdinal = kind;
		RebuildIds( state );
		pSession->szMessage = "imported " + rpgStats.szKeyName + " from " + statsFile.string();
		return BK_EDITOR_OK;
	} );
}

}
