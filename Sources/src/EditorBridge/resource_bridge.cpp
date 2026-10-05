// The resource editor's C ABI implementation. Every entry point is wrapped in
// the shared Guarded template (guarded.h) so no exception crosses into Zig.
// T01 stubbed every call to BK_EDITOR_OK. T02 fills in the Project+Tree group:
// New/Open/Save (safe-save read-back), Close, KindOf, Lock/LockOwner, Nodes,
// Props, SetProp, InsertNode, MoveNode, DeleteNode, RestoreNode. T05 fills in
// the cells family of geometry (passability, locked tiles, transparency
// lines): the C ABI entries now serve against an in-session geometry map that
// hangs off ResourceState, and BkResOpen / BkResSave persist / restore entries
// as auxiliary `_bk_geometry` XML children under the owning node so a save
// then reopen keeps the written cells byte-identically. T06 wires the second
// geometry family on top of that: the two point2 channels (zero point,
// entrance) and the four aimed-point channels (shoot/fire/smoke/
// directed-explosion). Angles cross the ABI as MFC-era degrees - a typed
// building/squad item class is free to store engine turns internally; the ABI
// boundary is the one place the unit is pinned. The remaining geometry
// channels (formation, bridge-spans, keyframes, crosses) and the other groups
// (references, export, mod, preview, import) stay stubbed and are replaced
// in T07-T10.
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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#include <windows.h>
#else
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
	// The lock, if this session holds one. Tracked here rather than on disk so
	// a lost process cannot leave a stale sentinel pinned to a path; the file
	// is removed on BkResClose.
	bool bHoldsLock = false;
	std::string szLockPath;
	// Geometry map keyed by (node_id, channel). The channel is the C ABI
	// integer the Zig GeometryChannel enum uses (BkResPassabilityCells = 0,
	// BkResLockedTiles = 1, BkResTransparencyLines = 2, etc.). An entry
	// exists only after a successful BkResSet*; a read of an un-set channel
	// returns an empty payload (w = h = 0, count = 0). On BkResSave the
	// entries are injected as `_bk_geometry` child elements under the owning
	// node so a BkResOpen on the saved file restores the map.
	std::map<std::pair<int, int>, GeometryBlob> geometry;
};

// Channel ids: the C ABI's geometry channel integers (shared with Zig's
// bridge.GeometryChannel enum). T05 wired channels 0..2 (cells family); T06
// adds 3..8 (point2 family + aimed-points family). The rest are reserved for
// T07+. Kept as a plain enum so a test can hand-assert against an integer.
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
	CHANNEL_DIRECTED_EXPLOSION_POINTS = 8
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

// Reserved child-element name for persisted geometry. The Project loader
// (adopts children as FutureBlobs under typed roots) hands us the raw XML
// bytes untouched; we scan for this tag on open and strip the matching
// children before the caller sees the tree. On save we re-inject them under
// the owning node's children just before Serialise, then remove them so the
// in-memory tree stays unchanged across the call.
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
			std::snprintf( buf, sizeof( buf ), "%g,%g", blob.points[0], blob.points[1] );
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
			std::snprintf( buf, sizeof( buf ), "%g,%g,%d,%d",
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
			std::snprintf( buf, sizeof( buf ), "%g,%g", blob.points[2*i], blob.points[2*i + 1] );
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

// Walks the tree, hands each item (root first, then descendants in storage
// order) an id. Called once on Open/New and after any Insert/Delete/Restore
// that invalidates existing ids - the simplest invariant.
void RebuildIds( ResourceState &state )
{
	state.nNextNodeId = 0;
	state.idToItem.clear();
	state.itemToId.clear();
	state.parentOf.clear();
	if ( !state.pProject || !state.pProject->root )
		return;
	struct Frame { NResourceModel::CTreeItem *pItem; int nParentId; };
	std::vector<Frame> stack;
	stack.push_back( { state.pProject->root.get(), 0 } );
	while ( !stack.empty() )
	{
		Frame f = stack.back();
		stack.pop_back();
		const int nId = ++state.nNextNodeId;
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

std::string FormatLockOwner()
{
#if defined(_WIN32) || defined(_WIN64)
	char host[256] = {};
	DWORD n = sizeof(host);
	GetComputerNameA( host, &n );
	const int pid = static_cast<int>( _getpid() );
#else
	char host[256] = {};
	gethostname( host, sizeof(host) - 1 );
	const int pid = static_cast<int>( getpid() );
#endif
	std::string out;
	out.reserve( 320 );
	out.append( host );
	out.append( ":" );
	out.append( std::to_string( pid ) );
	return out;
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
	out.kind = NResourceXml::Node::Element;
	const auto *stats = dynamic_cast<const NResourceModel::CStatsItem *>( &item );
	if ( stats != nullptr && !stats->GetXmlTag().empty() )
		out.name = stats->GetXmlTag();
	else if ( !item.GetDefaultName().empty() )
		out.name = item.GetDefaultName();
	else
		out.name = "node";
	// SProp walk first (empty in T02 for every typed shell).
	item.serialise( out );
	// Then every child, in storage order, so the subtree round-trips its shape.
	for ( const auto &pChild : item.GetChildren() )
	{
		NResourceXml::Node emitted;
		EmitNodeFor( *pChild, emitted );
		out.children.push_back( std::move( emitted ) );
	}
}

std::string SerialiseSubtree( const NResourceModel::CTreeItem &item )
{
	NResourceXml::Document doc;
	doc.hasDeclaration = false;
	EmitNodeFor( item, doc.root );
	return NResourceXml::Serialise( doc );
}

std::unique_ptr<NResourceModel::CTreeItem> ParseSubtree( const std::string &szBlob, std::string &szError )
{
	NResourceXml::Document doc;
	if ( !NResourceXml::Parse( szBlob, doc, szError ) )
		return nullptr;
	// Prefer the factory for a known tag; otherwise wrap as FutureBlob.
	auto &factory = NResourceModel::CTreeItemFactory::Instance();
	const int nType = NResourceModel::LookupRootTag( doc.root.name );
	std::unique_ptr<NResourceModel::CTreeItem> p;
	if ( nType != 0 )
		p = factory.Create( nType );
	if ( !p )
		p = std::make_unique<NResourceModel::FutureBlob>( doc.root );
	else
	{
		// Adopt children as FutureBlobs so unknown nodes under a typed root round-trip too.
		for ( const auto &child : doc.root.children )
			p->AddChild( std::make_unique<NResourceModel::FutureBlob>( child ) );
	}
	return p;
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

// Scans every node in the tree for `_bk_geometry` FutureBlob children,
// consumes them into state.geometry (keyed by (node_id, channel)), and
// removes them from the tree so later walks see the authored shape. Called
// once after RebuildIds on BkResOpen.
void ExtractGeometryFromTree( ResourceState &state )
{
	if ( !state.pProject || !state.pProject->root )
		return;
	for ( auto &kv : state.idToItem )
	{
		const int nNodeId = kv.first;
		auto &children = kv.second->MutableChildren();
		for ( std::size_t i = 0; i < children.size(); )
		{
			if ( !NResourceModel::FutureBlob::IsFutureBlob( *children[i] ) )
			{
				++i;
				continue;
			}
			const auto &node = static_cast<const NResourceModel::FutureBlob &>( *children[i] ).GetNode();
			if ( node.kind != NResourceXml::Node::Element || node.name != kGeometryTag )
			{
				++i;
				continue;
			}
			int nChannel = -1;
			GeometryBlob blob;
			if ( ParseGeometryChild( node, nChannel, blob ) )
				state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
			// Whether parsing succeeded or not, drop the magic child so a
			// garbled one does not leak into save output.
			children.erase( children.begin() + i );
		}
	}
	// The erase above invalidated node parent/child relations the id tables
	// cached; recompute them so a BkResNodes immediately after Open does not
	// surface a `_bk_geometry` node id that no longer exists.
	RebuildIds( state );
}

// Pre-save walker: injects one FutureBlob child per (node_id, channel) in
// state.geometry under the owning typed node. Returns the list of (owner,
// index) pointers so the caller can remove them again after Save.
struct InjectedChild
{
	NResourceModel::CTreeItem *pOwner;
	std::size_t nIndex;
};

std::vector<InjectedChild> InjectGeometryIntoTree( ResourceState &state )
{
	std::vector<InjectedChild> injected;
	injected.reserve( state.geometry.size() );
	for ( const auto &kv : state.geometry )
	{
		const int nNodeId = kv.first.first;
		const int nChannel = kv.first.second;
		auto it = state.idToItem.find( nNodeId );
		if ( it == state.idToItem.end() )
			continue;
		NResourceXml::Node emitted = EmitGeometryChild( nChannel, kv.second );
		auto &children = it->second->MutableChildren();
		children.push_back( std::make_unique<NResourceModel::FutureBlob>( std::move( emitted ) ) );
		injected.push_back( { it->second, children.size() - 1 } );
	}
	return injected;
}

void RemoveInjectedChildren( std::vector<InjectedChild> &injected )
{
	// Remove in reverse so an index remains valid against the owner's list
	// across the loop (an earlier remove under the same owner would otherwise
	// shift the later index down).
	for ( std::size_t i = injected.size(); i-- > 0; )
	{
		InjectedChild &c = injected[i];
		auto &children = c.pOwner->MutableChildren();
		if ( c.nIndex < children.size() )
			children.erase( children.begin() + c.nIndex );
	}
	injected.clear();
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
		ExtractGeometryFromTree( state );
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
		// Inject the in-memory geometry entries as `_bk_geometry` FutureBlob
		// children under their owning typed nodes before Serialise runs; the
		// RAII guard below removes them again so the in-memory tree stays as
		// the caller sees it, whether the Save succeeded or failed.
		std::vector<InjectedChild> injected = InjectGeometryIntoTree( state );
		struct Guard
		{
			std::vector<InjectedChild> *p;
			~Guard() { RemoveInjectedChildren( *p ); }
		} guard { &injected };
		const std::string szIntended = NResourceModel::Save( *state.pProject );

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
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || state.szPath.empty() )
		{
			pSession->szMessage = "no on-disk project to lock";
			return BK_EDITOR_REFUSED;
		}
		const std::string szLock = state.szPath + ".lock";
		std::error_code ec;
		if ( std::filesystem::exists( szLock, ec ) )
		{
			std::string szOwner;
			ReadFileBytes( szLock, szOwner );
			pSession->szMessage = szOwner.empty() ? std::string( "lock already held" )
				: std::string( "lock already held by " ) + szOwner;
			return BK_EDITOR_REFUSED;
		}
		if ( !WriteFileBytes( szLock, FormatLockOwner() ) )
		{
			pSession->szMessage = "cannot write lock file";
			return BK_EDITOR_FAILED;
		}
		state.bHoldsLock = true;
		state.szLockPath = szLock;
		return BK_EDITOR_OK;
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
		const std::string szLock = state.szPath + ".lock";
		std::string szOwner;
		if ( !ReadFileBytes( szLock, szOwner ) )
			return BK_EDITOR_OK; // empty string = no owner
		CopyFixed( pOut, nCapacity, szOwner );
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
			return BK_EDITOR_OK;
		}
		// Preorder walk, root first - RebuildIds already laid them out in id
		// order, so iterating 1..nNextNodeId gives the same shape.
		const int nTotal = state.nNextNodeId;
		if ( pnCount != nullptr )
			*pnCount = nTotal;
		if ( pOut == nullptr || nCapacity <= 0 )
			return BK_EDITOR_OK;
		if ( nCapacity < nTotal )
			return BK_EDITOR_REFUSED;
		int nWritten = 0;
		for ( int nId = 1; nId <= nTotal; ++nId )
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
		const std::string szBlob = SerialiseSubtree( *itItem->second );
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
		auto pItem = ParseSubtree( szBlob, szError );
		if ( !pItem )
		{
			pSession->szMessage = std::string( "parse failed: " ) + szError;
			return BK_EDITOR_FAILED;
		}
		auto &children = itParent->second->MutableChildren();
		std::size_t nAt = static_cast<std::size_t>( nIndex );
		if ( nAt > children.size() ) nAt = children.size();
		NResourceModel::CTreeItem *pInserted = pItem.get();
		children.insert( children.begin() + nAt, std::move( pItem ) );
		RebuildIds( state );
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

BkEditorStatus BkResRefList( BkResSession *pSession, int nType, BkResReferenceEntry *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nType < 0 || nType > 19 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pnCount != nullptr )
			*pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

/* ---- Geometry --------------------------------------------------------- */

/* T05 implements the cells family (passability, locked tiles, transparency
   lines) end-to-end: the entries serve against the in-session geometry map
   on ResourceState, and the Open/Save path persists / restores each entry as
   a `_bk_geometry` FutureBlob child under the owning node. The rest of the
   channels (points, aimed points, keyframes) stay stubbed below until T06+.

   The two bytes_grid channels share one helper (GetBytesGrid / SetBytesGrid)
   because they only differ in channel id; the points channel has its own
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

BkEditorStatus BkResGetTransparencyLines( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
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
		auto it = state.geometry.find( std::make_pair( nNodeId, CHANNEL_TRANSPARENCY_LINES ) );
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
	} );
}

BkEditorStatus BkResSetTransparencyLines( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
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
		state.geometry[ std::make_pair( nNodeId, CHANNEL_TRANSPARENCY_LINES ) ] = std::move( blob );
		return BK_EDITOR_OK;
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
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_POINT2_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResPoint2 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_POINT2_STUB( BkResGetFormationPositions )
BKRES_SET_POINT2_STUB( BkResSetFormationPositions )
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
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_VEC3_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResVec3 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_VEC3_STUB( BkResGetBridgeSpanMarks )
BKRES_SET_VEC3_STUB( BkResSetBridgeSpanMarks )
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
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResExportStatsOnly( BkResSession *pSession, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResBatch( BkResSession *pSession, int, const char *, const char *, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return BK_EDITOR_OK;
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
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResModSettingsSet( BkResSession *pSession, const BkResModSettings * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPackMod( BkResSession *pSession, const char * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

/* ---- Preview --------------------------------------------------------- */

BkEditorStatus BkResPreviewBegin( BkResSession *pSession, BkResKind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewShow( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewStop( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewPlayback( BkResSession *pSession, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewCamera( BkResSession *pSession, float, float, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

/* ---- Import ----------------------------------------------------------- */

BkEditorStatus BkResImportFromGame( BkResSession *pSession, BkResKind, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return BK_EDITOR_OK;
	} );
}

}
