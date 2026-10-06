// The resource editor's C ABI implementation. Every entry point is wrapped in
// the shared Guarded template (guarded.h) so no exception crosses into Zig.
// T01 stubbed every call to BK_EDITOR_OK. T02 fills in the Project+Tree group:
// New/Open/Save (safe-save read-back), Close, KindOf, Lock/LockOwner, Nodes,
// Props, SetProp, InsertNode, MoveNode, DeleteNode, RestoreNode. T05 and T06
// add the geometry channels (cells, points, aimed points); angles cross the
// ABI as MFC-era degrees. T08-T11 fill in the other groups (references,
// export, mod, preview, import). T07 replaced the `<path>.lock` sentinel with
// MFC's per-folder `locked_<user>` (CParentFrame::LockFile, D-08). S05 T03
// and T04 (D014 item 2) store every geometry channel where MFC keeps it: the
// frame chunks beside the tree (own_data, desc, RPG) or fields of the S03
// items (HomeOf, and the table in the phase 6 spec); a channel with no MFC
// home on a node is refused.
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
#include "../ResourceModel/items/squad/squad.h"
#include "../ResourceModel/items/fence/fence.h"
#include "../ResourceModel/items/bridge/bridge.h"
#include "../ResourceModel/key_frame_tree_item.h"
#include "../ResourceModel/mfc_value.h"
#include "../ResourceModel/xml.h"
#include "../ResourceModel/future_blob.h"
#include "../ResourceModel/references.h"
#include "../ResourceModel/exporter.h"
#include "../ResourceModel/items/stats_export.h"
#include "../ResourceModel/items/mesh/mesh.h"
#include "../ResourceModel/items/object/object_export.h"
#include "../ResourceModel/items/building/building_export.h"
#include "../ResourceModel/items/bridge/bridge_export.h"
#include "../ResourceModel/items/particle/particle_export.h"
#include "../ResourceModel/items/effect/effect_export.h"
#include "../ResourceModel/items/tileset/tileset.h"
#include "../ResourceModel/items/tileset/tileset_export.h"
#include "../ResourceModel/image_export.h"
#include "../Formats/fmtTerrain.h"
#include "../ResourceModel/items/road3d/road3d_export.h"
#include "../ResourceModel/items/river3d/river3d_export.h"
#include "../Formats/fmtVSO.h"
#include "../ResourceModel/combos.h"
#include "../Main/RPGStats.h"
#include "../Main/GameStats.h"
#include "../Main/iMain.h"
#include "../Main/GameTimer.h"
#include "../Misc/HPTimer.h"
#include "../Scene/Scene.h"
#include "../Scene/Terrain.h"
#include "../MapFile/MapFile.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../Scene/SceneScreenScale.h"
#include "../Scene/PFX.h"
#include "../Scene/ParticleSourceData.h"
#include "../Scene/SmokinParticleSourceData.h"
#include "../Anim/Animation.h"
#include "../zlib/zlib.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
#include <set>
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
	// returns an empty payload (w = h = 0, count = 0). Only the root's frame
	// homes enter the map, and only once edited (an unedited one is read from
	// the document): BkResSave writes them into own_data / desc / RPG. The
	// item-field homes never enter the map.
	std::map<std::pair<int, int>, GeometryBlob> geometry;
	// The cross containers (by item type) set through a cross channel since
	// the open: their crosses are read from the tree, no longer from the RPG
	// copy, and BkResSave writes them into RPG (WriteCrossesToRpg).
	std::set<int> editedCrossLists;
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
	// A squad previews as its members: one sprite each, held like pPreviewObj.
	std::vector<IVisObj *> previewMembers;
	bool bPreviewEffect = false;
	bool bPreviewRunning = false;
	// A road or river preview (kinds 14 and 15) draws the terrain of
	// maps\road3d / maps\river3d, not an object: the scene's terrain is the
	// preview and Stop takes it out again.
	bool bPreviewTerrain = false;
	// The Effect editor's direction dock angle, radians (CDirectionButtonDockBar
	// held it in MFC): view state, neither saved nor undone, back at 45 degrees
	// whenever a project is opened or begun (SpecificClearBeforeBatchMode).
	float fEffectAngle = 0.78539816f;
	// The unit (msh) preview: the export's folder as the engine names it (with
	// a trailing backslash), the three model names of Graphics Info without
	// folder and extension, the variant drawn, its direction in degrees and
	// the locator display. The sprites are held like pPreviewObj.
	std::string szMeshDir;
	std::string meshModels[3];
	int nMeshVariant = 0;
	int nMeshDirection = 0;
	bool bMeshLocators = false;
	bool bMeshBoxes = false;
	std::vector<IVisObj *> locatorSprites;
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
	CHANNEL_EFFECT_KEYFRAMES = 15,
	// S06: a squad formation's FormationDir, carried as a point2 (x the
	// angle, y unused) so it shares the zero point's entry-point shape.
	CHANNEL_FORMATION_DIRECTION = 16,
	// S09: the Object and Fence sub-editors. The object's transparency
	// cells (its desc visibility grid without the one-way tiles), a fence
	// segment's Transparences list and the sprite's place (an object's
	// own_data sprite_pos, a fence segment's SpritePos).
	CHANNEL_TRANSPARENCY_CELLS = 17,
	CHANNEL_FENCE_TRANSPARENCES = 18,
	CHANNEL_SPRITE_POS = 19
};

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
	state.editedCrossLists.clear();
	state.fEffectAngle = ToRadian( 45.0f );
}

// Upper-case hex of a byte buffer, two chars per byte and no separators, as
// NStr::BinToString writes CDataTreeXML raw data (a passability grid row).
std::string HexEncode( const unsigned char *p, std::size_t n )
{
	static const char kDigits[] = "0123456789ABCDEF";
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

// The text of an element's single inline Text child (how xml.cpp writes
// inline text, e.g. a passability row's hex). Empty when the element has no
// body.
std::string FindBodyText( const NResourceXml::Node &node )
{
	for ( const auto &c : node.children )
		if ( c.kind == NResourceXml::Node::Text || c.kind == NResourceXml::Node::CData )
			return c.text;
	return std::string();
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

// CMeshFrame::SetCombatMesh rebuilt the Locators children from the combat
// model on every load and every change of its name. The rebuild is derived
// data, not an edit: it does not touch the dirty state and the undo history
// never sees it. A missing model leaves the item empty and is reported in
// szMessage; the open or the set that triggered it still succeeds, as MFC's
// SetCombatMesh returned quietly.
void RebuildMeshLocators( ResourceState &state, std::string &szMessage )
{
	if ( !state.pProject || !state.pProject->root || state.pProject->root->GetItemType() != NResourceModel::ETIT_MESH_ROOT_ITEM )
		return;
	NResourceModel::SExportContext context;
	context.szProjectPath = state.szPath;
	int nNodes = 0;
	std::string szModFile, szWhy;
	if ( !NResourceModel::RebuildMeshLocators( *state.pProject->root, context, nNodes, szModFile, szWhy ) )
		szMessage = szWhy;
	RebuildIds( state );
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

std::string SerialiseSubtree( NResourceModel::CTreeItem &item );

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

// Every geometry channel lives where MFC keeps it (D014 item 2; the channel
// -> home table is in the phase 6 spec's geometry section). Two kinds of home:
//   * Frame chunks. CBuildingFrame, CObjectFrame and CBridgeFrame keep their
//     geometry in the frame, not in a tree item, and OnFileSave writes it
//     beside the tree: own_data (SaveFrameOwnData: krest_pos, the object's
//     TransLines, the bridge's Begin / End / Front / Back) and desc
//     (SaveRPGStats: the SBuildingRPGStats / SObjectRPGStats copy with
//     passability, Entrances, FireSlots, FirePoints, SmokePoints and
//     DirExplosions). No item owns these elements, so the session plays the
//     frame: an edit is held in the geometry map, a read of an unedited
//     channel comes from the document as it was read, and a save writes the
//     edited channels into the chunks, leaving every other field as it was.
//   * Item fields. A squad formation's zero point and slots are the S03
//     item's ZeroPos and units, a fence segment's or bridge part's locked
//     tiles its LockedTiles list, a particle track's keys its Key_frames,
//     and the map crosses of a mission, chapter or campaign and the places
//     of an effect's parts are the position values of the container's
//     children. Get and set go straight to the items, so a save, a delete
//     blob and a restore carry them like any other field. The crosses also
//     have a copy in the RPG chunk, which MFC's LoadRPGStats copies over the
//     tree; see CrossListOf.
// A channel on a node with no MFC home is refused.
enum EGeometryHome
{
	HOME_NONE,           // no MFC home on this node: refused
	HOME_FRAME,          // own_data / desc beside the root's tree
	HOME_SQUAD_ZERO,     // CSquadFormationPropsItem::vZeroPos
	HOME_SQUAD_UNITS,    // CSquadFormationPropsItem::units, SUnit::vPos
	HOME_SQUAD_DIR,      // CSquadFormationPropsItem::fFormationDir
	HOME_TILE_LIST,      // CFencePropsItem / CBridgePartsItem::lockedTiles (and a fence's transeparences)
	HOME_FENCE_SPRITE,   // CFencePropsItem::vSpritePos
	HOME_CROSSES,        // the children's map position values (+ the RPG copy)
	HOME_EFFECT_PLACES,  // the children's X / Y / Z position values
	HOME_KEY_FRAMES      // CKeyFrameTreeItem::framesList
};

// A container whose children carry a map cross: the two values of each
// child that hold it (MFC's Get*Position read them by index; these are the
// names at those indices) and the list and field of the frame's RPG copy
// (SMissionStats / SChapterStats / SCampaignStats::operator&).
struct SCrossList
{
	int nChannel;
	int nContainerType;
	const char *pszX;
	const char *pszY;
	const char *pszRpgList;
	const char *pszRpgField;
};
const SCrossList kCrossLists[] =
{
	{ CHANNEL_MISSION_OBJECTIVES, NResourceModel::ETIT_MISSION_OBJECTIVES_ITEM, "Objective position X", "Objective position Y", "Objectives", "PosOnMap" },
	{ CHANNEL_CHAPTER_CROSSES, NResourceModel::ETIT_CHAPTER_MISSIONS_ITEM, "Mission position X", "Mission position Y", "Missions", "PosOnMap" },
	{ CHANNEL_CHAPTER_CROSSES, NResourceModel::ETIT_CHAPTER_PLACES_ITEM, "Place holder position X", "Place holder position Y", "PlaceHolders", "Position" },
	{ CHANNEL_CAMPAIGN_CROSSES, NResourceModel::ETIT_CAMPAIGN_CHAPTERS_ITEM, "Chapter position X", "Chapter position Y", "AllChapters", "PosOnMap" }
};

const SCrossList *CrossListOf( int nChannel, int nType )
{
	for ( const SCrossList &list : kCrossLists )
		if ( list.nChannel == nChannel && list.nContainerType == nType )
			return &list;
	return nullptr;
}

bool IsCrossChannel( int nChannel )
{
	return nChannel == CHANNEL_MISSION_OBJECTIVES || nChannel == CHANNEL_CHAPTER_CROSSES || nChannel == CHANNEL_CAMPAIGN_CROSSES;
}

// The effect parts CEffectFrame::SaveRPGStats places by their X / Y / Z
// position values: sprite animations, meshes, function and Maya particles.
bool IsEffectPartList( int nType )
{
	return nType == NResourceModel::ETIT_EFFECT_ANIMATIONS_ITEM || nType == NResourceModel::ETIT_EFFECT_MESHES_ITEM
		|| nType == NResourceModel::ETIT_EFFECT_FUNC_PARTICLES_ITEM || nType == NResourceModel::ETIT_EFFECT_MAYA_PARTICLES_ITEM;
}

EGeometryHome HomeOf( const ResourceState &state, int nNodeId, int nChannel )
{
	auto itItem = state.idToItem.find( nNodeId );
	auto itParent = state.parentOf.find( nNodeId );
	if ( itItem == state.idToItem.end() || itParent == state.parentOf.end() )
		return HOME_NONE;
	const int nType = itItem->second->GetItemType();
	const bool bRoot = itParent->second == 0;
	const bool bBuilding = bRoot && nType == NResourceModel::ETIT_BUILDING_ROOT_ITEM;
	const bool bObject = bRoot && nType == NResourceModel::ETIT_OBJECT_ROOT_ITEM;
	const bool bBridge = bRoot && nType == NResourceModel::ETIT_BRIDGE_ROOT_ITEM;
	switch ( nChannel )
	{
	case CHANNEL_PASSABILITY_CELLS:
		// A fence segment's passability is its locked tiles: MFC keeps no
		// grid for it, the exporter builds one from them.
		if ( nType == NResourceModel::ETIT_FENCE_PROPS_ITEM )
			return HOME_TILE_LIST;
		return bBuilding || bObject ? HOME_FRAME : HOME_NONE;
	case CHANNEL_LOCKED_TILES:
		return nType == NResourceModel::ETIT_FENCE_PROPS_ITEM || nType == NResourceModel::ETIT_BRIDGE_PARTS_ITEM ? HOME_TILE_LIST : HOME_NONE;
	case CHANNEL_TRANSPARENCY_LINES:
		return bObject ? HOME_FRAME : HOME_NONE;
	case CHANNEL_TRANSPARENCY_CELLS:
		return bObject || bBuilding ? HOME_FRAME : HOME_NONE;
	case CHANNEL_FENCE_TRANSPARENCES:
		return nType == NResourceModel::ETIT_FENCE_PROPS_ITEM ? HOME_TILE_LIST : HOME_NONE;
	case CHANNEL_SPRITE_POS:
		// CBuildingFrame keeps m_SpriteLoadPos in own_data like the object; its
		// Move object drag changes it together with the zero point.
		if ( bObject || bBuilding )
			return HOME_FRAME;
		return nType == NResourceModel::ETIT_FENCE_PROPS_ITEM ? HOME_FENCE_SPRITE : HOME_NONE;
	case CHANNEL_ZERO_POINT:
		if ( bBuilding || bObject || bBridge )
			return HOME_FRAME;
		return nType == NResourceModel::ETIT_SQUAD_FORMATION_PROPS_ITEM ? HOME_SQUAD_ZERO : HOME_NONE;
	case CHANNEL_FORMATION_POSITIONS:
		return nType == NResourceModel::ETIT_SQUAD_FORMATION_PROPS_ITEM ? HOME_SQUAD_UNITS : HOME_NONE;
	case CHANNEL_FORMATION_DIRECTION:
		return nType == NResourceModel::ETIT_SQUAD_FORMATION_PROPS_ITEM ? HOME_SQUAD_DIR : HOME_NONE;
	case CHANNEL_BRIDGE_SPAN_MARKS:
		return bRoot && nType == NResourceModel::ETIT_BRIDGE_ROOT_ITEM ? HOME_FRAME : HOME_NONE;
	case CHANNEL_MISSION_OBJECTIVES:
	case CHANNEL_CHAPTER_CROSSES:
	case CHANNEL_CAMPAIGN_CROSSES:
		return CrossListOf( nChannel, nType ) != nullptr ? HOME_CROSSES : HOME_NONE;
	case CHANNEL_PARTICLE_KEYFRAMES:
		return dynamic_cast<const NResourceModel::CKeyFrameTreeItem *>( itItem->second ) != nullptr ? HOME_KEY_FRAMES : HOME_NONE;
	case CHANNEL_EFFECT_KEYFRAMES:
		return IsEffectPartList( nType ) ? HOME_EFFECT_PLACES : HOME_NONE;
	case CHANNEL_ENTRANCE:
	case CHANNEL_SHOOT_POINTS:
		return bBuilding ? HOME_FRAME : HOME_NONE;
	case CHANNEL_FIRE_POINTS:
	case CHANNEL_SMOKE_POINTS:
	case CHANNEL_DIRECTED_EXPLOSION_POINTS:
		// A bridge keeps these too, in the RPG chunk (CBridgeFrame::SaveRPGStats).
		return bBuilding || bBridge ? HOME_FRAME : HOME_NONE;
	default:	// a channel integer the bridge does not define
		return HOME_NONE;
	}
}

const char *ChannelName( int nChannel )
{
	static const char *const kNames[] = { "passability_cells", "locked_tiles", "transparency_lines", "zero_point", "entrance",
		"shoot_points", "fire_points", "smoke_points", "directed_explosion_points", "formation_positions", "bridge_span_marks",
		"mission_objectives", "chapter_crosses", "campaign_crosses", "particle_keyframes", "effect_keyframes", "formation_direction",
		"transparency_cells", "fence_transparences", "sprite_pos" };
	return nChannel >= 0 && nChannel < int( sizeof( kNames ) / sizeof( kNames[0] ) ) ? kNames[nChannel] : "unknown";
}

// The refusal a channel with no MFC home gets: it names the channel and the
// kind of project, so a caller sees which pair was wrong.
std::string NoHomeMessage( const ResourceState &state, int nChannel )
{
	const char *pszKind = state.nKindOrdinal >= 0 && state.nKindOrdinal < kKindCount ? kKindTable[state.nKindOrdinal].pszTag : "unknown";
	return std::string( "geometry channel " ) + ChannelName( nChannel ) + " (" + std::to_string( nChannel ) + ") has no MFC home on this node of a " + pszKind + " project";
}

NResourceXml::Node NewElement( const std::string &szName )
{
	NResourceXml::Node n;
	n.kind = NResourceXml::Node::Element;
	n.name = szName;
	return n;
}

NResourceXml::Node *MutableChild( NResourceXml::Node &parent, const std::string &szName )
{
	for ( auto &c : parent.children )
		if ( c.kind == NResourceXml::Node::Element && c.name == szName )
			return &c;
	return nullptr;
}

NResourceXml::Node &ChildOrNew( NResourceXml::Node &parent, const std::string &szName )
{
	if ( NResourceXml::Node *p = MutableChild( parent, szName ) )
		return *p;
	parent.children.push_back( NewElement( szName ) );
	return parent.children.back();
}

// The chunk a frame's stats copy lives in: CBuildingFrame and CObjectFrame
// write "desc", CBridgeFrame "RPG" (the bridge exporter's WriteBridgeFrameData
// builds the same one).
const char *StatsChunkName( const NResourceXml::Node &root )
{
	return root.name == "Bridge_Composer_Project" ? "RPG" : "desc";
}

// own_data, desc or RPG under the project element. A missing one goes where
// CParentFrame::OnFileSave writes it: own_data first, then desc, then the tree;
// the bridge's RPG chunk follows the tree.
NResourceXml::Node &FrameChunk( NResourceXml::Node &root, const std::string &szName )
{
	if ( NResourceXml::Node *p = MutableChild( root, szName ) )
		return *p;
	if ( szName == "RPG" )
	{
		root.children.push_back( NewElement( szName ) );
		return root.children.back();
	}
	std::size_t nAt = 0;
	if ( szName == "desc" )
		for ( std::size_t i = 0; i < root.children.size(); ++i )
			if ( root.children[i].kind == NResourceXml::Node::Element && root.children[i].name == "own_data" )
				nAt = i + 1;
	root.children.insert( root.children.begin() + nAt, NewElement( szName ) );
	return root.children[nAt];
}

std::vector<const NResourceXml::Node *> Items( const NResourceXml::Node &list )
{
	std::vector<const NResourceXml::Node *> out;
	for ( const auto &c : list.children )
		if ( c.kind == NResourceXml::Node::Element && c.name == "item" )
			out.push_back( &c );
	return out;
}

float NumberAttr( const NResourceXml::Node &node, const char *pszName )
{
	return static_cast<float>( std::strtod( FindAttr( node, pszName ).c_str(), nullptr ) );
}

// The x and y of a CVec2 / CVec3 chunk (DTHelper: x, y, z attributes).
void ReadXY( const NResourceXml::Node *pVec, float &x, float &y )
{
	x = pVec != nullptr ? NumberAttr( *pVec, "x" ) : 0;
	y = pVec != nullptr ? NumberAttr( *pVec, "y" ) : 0;
}

// Sets x and y and keeps z; a new CVec3 gets MFC's z = 0 (the frames zero it).
void WriteXY( NResourceXml::Node &vec, float x, float y, bool bVec3 )
{
	NResourceModel::SetAttr( vec, "x", NResourceModel::MfcFloat( x ) );
	NResourceModel::SetAttr( vec, "y", NResourceModel::MfcFloat( y ) );
	if ( bVec3 && FindAttr( vec, "z" ).empty() )
		NResourceModel::SetAttr( vec, "z", "0" );
}

NResourceXml::Node NewVec2( const std::string &szName, float x, float y )
{
	NResourceXml::Node n = NewElement( szName );
	WriteXY( n, x, y, false );
	return n;
}

NResourceXml::Node NewVec3( const std::string &szName, float x, float y )
{
	NResourceXml::Node n = NewElement( szName );
	WriteXY( n, x, y, true );
	return n;
}

// The desc list an aimed channel lives in, and the attribute its cone goes
// to: SBuildingRPGStats::SSlot's Angle (the fire sector), the vertical angle
// of an SFirePoint or SDirectionExplosion. Direction is the angle for all.
const char *AimedListName( int nChannel )
{
	switch ( nChannel )
	{
	case CHANNEL_SHOOT_POINTS: return "FireSlots";
	case CHANNEL_FIRE_POINTS: return "FirePoints";
	case CHANNEL_SMOKE_POINTS: return "SmokePoints";
	default: return "DirExplosions";
	}
}

const char *ConeAttr( int nChannel )
{
	return nChannel == CHANNEL_SHOOT_POINTS ? "Angle" : "VerticalAngle";
}

// A new desc list entry: the fields of the struct's operator& with the
// values its constructor sets (RPGStats.cpp), numbers as attributes.
NResourceXml::Node NewAimedEntry( int nChannel )
{
	NResourceXml::Node item = NewElement( "item" );
	item.attrs.push_back( { "Direction", "0" } );
	item.children.push_back( NewVec3( "Position", 0, 0 ) );
	if ( nChannel == CHANNEL_SHOOT_POINTS )
	{
		item.attrs.push_back( { "Angle", "30" } );
		item.attrs.push_back( { "SightMultiplier", "1" } );
		item.attrs.push_back( { "Coverage", "1" } );
		item.attrs.push_back( { "Ammo", "0" } );
		item.attrs.push_back( { "GunPriority", "1" } );
		item.attrs.push_back( { "RotationSpeed", "0" } );
		item.attrs.push_back( { "BeforeSprite", "1" } );
		item.attrs.push_back( { "ShowFlashes", "1" } );
		item.children.push_back( NResourceModel::StringElement( "Weapon", std::string() ) );
	}
	else
	{
		item.attrs.push_back( { "VerticalAngle", "78" } );
		if ( nChannel != CHANNEL_DIRECTED_EXPLOSION_POINTS )
			item.children.push_back( NResourceModel::StringElement( "FireEffect", std::string() ) );
	}
	item.children.push_back( NewVec2( "PicturePosition", 0, 0 ) );
	NResourceXml::Node world = NewElement( "WorldPosition" );
	world.attrs = { { "x", "0" }, { "y", "0" }, { "z", "0" } };
	item.children.push_back( std::move( world ) );
	return item;
}

// Reads a frame-home channel from the project element as MFC's
// LoadFrameOwnData / LoadRPGStats would. A missing chunk reads as un-set;
// false with szError on a passability grid CDataTreeXML would reject.
bool ReadFrameGeometry( const NResourceXml::Node &root, int nChannel, GeometryBlob &out, std::string &szError )
{
	out = GeometryBlob();
	const bool bOwnData = nChannel == CHANNEL_ZERO_POINT || nChannel == CHANNEL_TRANSPARENCY_LINES
		|| nChannel == CHANNEL_BRIDGE_SPAN_MARKS || nChannel == CHANNEL_SPRITE_POS;
	const NResourceXml::Node *pChunk = NResourceXml::FindChild( root, bOwnData ? "own_data" : StatsChunkName( root ) );
	if ( pChunk == nullptr )
		return true;
	switch ( nChannel )
	{
	case CHANNEL_PASSABILITY_CELLS:
	{
		// CTreeAccessor::Do2DArrayData: <item size_x size_y/>, then one
		// <item> of hex bytes per row; one item or none is an empty grid.
		const NResourceXml::Node *pGrid = NResourceXml::FindChild( *pChunk, "passability" );
		const std::vector<const NResourceXml::Node *> rows = pGrid != nullptr ? Items( *pGrid ) : std::vector<const NResourceXml::Node *>();
		if ( rows.size() <= 1 )
			return true;
		const int nW = std::atoi( FindAttr( *rows[0], "size_x" ).c_str() );
		const int nH = std::atoi( FindAttr( *rows[0], "size_y" ).c_str() );
		if ( nW < 0 || nH < 0 || rows.size() != static_cast<std::size_t>( nH ) + 1 )
		{
			szError = "desc passability: the row count does not match size_y";
			return false;
		}
		for ( int y = 0; y < nH; ++y )
		{
			std::vector<unsigned char> row;
			if ( !HexDecode( FindBodyText( *rows[y + 1] ), row ) || row.size() != static_cast<std::size_t>( nW ) )
			{
				szError = "desc passability: a row is not size_x hex bytes";
				return false;
			}
			out.bytes.insert( out.bytes.end(), row.begin(), row.end() );
		}
		out.nWidth = nW;
		out.nHeight = nH;
		return true;
	}
	case CHANNEL_ZERO_POINT:
		if ( const NResourceXml::Node *pKrest = NResourceXml::FindChild( *pChunk, "krest_pos" ) )
		{
			float x = 0, y = 0;
			ReadXY( pKrest, x, y );
			out.points = { x, y };
		}
		return true;
	case CHANNEL_SPRITE_POS:
		if ( const NResourceXml::Node *pSprite = NResourceXml::FindChild( *pChunk, "sprite_pos" ) )
		{
			float x = 0, y = 0;
			ReadXY( pSprite, x, y );
			out.points = { x, y };
		}
		return true;
	case CHANNEL_TRANSPARENCY_LINES:
		if ( const NResourceXml::Node *pLines = NResourceXml::FindChild( *pChunk, "TransLines" ) )
			for ( const NResourceXml::Node *pLine : Items( *pLines ) )
			{
				float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
				ReadXY( NResourceXml::FindChild( *pLine, "Point1" ), x1, y1 );
				ReadXY( NResourceXml::FindChild( *pLine, "Point2" ), x2, y2 );
				out.points.insert( out.points.end(), { x1, y1, x2, y2 } );
			}
		return true;
	case CHANNEL_BRIDGE_SPAN_MARKS:
	{
		// CBridgeFrame::LoadFrameOwnData: Begin, End (CVec3) and the Front /
		// Back offsets. A field that is missing keeps the frame's
		// constructor value; with none of them the list is un-set.
		const NResourceXml::Node *pBegin = NResourceXml::FindChild( *pChunk, "Begin" );
		const NResourceXml::Node *pEnd = NResourceXml::FindChild( *pChunk, "End" );
		const std::string szFront = FindAttr( *pChunk, "Front" );
		const std::string szBack = FindAttr( *pChunk, "Back" );
		if ( pBegin == nullptr && pEnd == nullptr && szFront.empty() && szBack.empty() )
			return true;
		const float fDefaultX = 16 * NResourceModel::fWorldCellSize - 300;
		const float fDefaultY = 16 * NResourceModel::fWorldCellSize;
		float bx = fDefaultX, by = fDefaultY, ex = fDefaultX, ey = fDefaultY;
		if ( pBegin != nullptr )
			ReadXY( pBegin, bx, by );
		if ( pEnd != nullptr )
			ReadXY( pEnd, ex, ey );
		out.points = { bx, by, ex, ey, NumberAttr( *pChunk, "Front" ), NumberAttr( *pChunk, "Back" ) };
		return true;
	}
	case CHANNEL_ENTRANCE:
		if ( const NResourceXml::Node *pList = NResourceXml::FindChild( *pChunk, "Entrances" ) )
		{
			const std::vector<const NResourceXml::Node *> items = Items( *pList );
			if ( !items.empty() )
			{
				float x = 0, y = 0;
				ReadXY( NResourceXml::FindChild( *items[0], "Position" ), x, y );
				out.points = { x, y };
			}
		}
		return true;
	default:
		if ( const NResourceXml::Node *pList = NResourceXml::FindChild( *pChunk, AimedListName( nChannel ) ) )
			for ( const NResourceXml::Node *pItem : Items( *pList ) )
			{
				AimedPoint ap;
				ReadXY( NResourceXml::FindChild( *pItem, "Position" ), ap.x, ap.y );
				ap.nAngle = static_cast<int>( std::lround( NumberAttr( *pItem, "Direction" ) ) );
				ap.nCone = static_cast<int>( std::lround( NumberAttr( *pItem, ConeAttr( nChannel ) ) ) );
				out.aimed.push_back( ap );
			}
		return true;
	}
}

// Writes an edited frame-home channel into the project element in the
// shape MFC's SaveFrameOwnData / SaveRPGStats give it. Fields the channel
// does not carry (an entry's z, weapon, picture position, ...) stay as read.
void WriteFrameGeometry( NResourceXml::Node &root, int nChannel, const GeometryBlob &blob )
{
	switch ( nChannel )
	{
	case CHANNEL_PASSABILITY_CELLS:
	{
		NResourceXml::Node &grid = ChildOrNew( FrameChunk( root, "desc" ), "passability" );
		grid.children.clear();
		const int nW = blob.bytes.empty() ? 0 : blob.nWidth;
		const int nH = blob.bytes.empty() ? 0 : blob.nHeight;
		NResourceXml::Node size = NewElement( "item" );
		size.attrs = { { "size_x", NResourceModel::MfcInt( nW ) }, { "size_y", NResourceModel::MfcInt( nH ) } };
		grid.children.push_back( std::move( size ) );
		for ( int y = 0; y < nH; ++y )
			grid.children.push_back( NResourceModel::StringElement( "item",
				HexEncode( blob.bytes.data() + static_cast<std::size_t>( y ) * nW, static_cast<std::size_t>( nW ) ) ) );
		return;
	}
	case CHANNEL_ZERO_POINT:
		if ( blob.points.size() >= 2 )
			WriteXY( ChildOrNew( FrameChunk( root, "own_data" ), "krest_pos" ), blob.points[0], blob.points[1], true );
		return;
	case CHANNEL_SPRITE_POS:
		if ( blob.points.size() >= 2 )
			WriteXY( ChildOrNew( FrameChunk( root, "own_data" ), "sprite_pos" ), blob.points[0], blob.points[1], true );
		return;
	case CHANNEL_TRANSPARENCY_CELLS:
		// Part of the frame's grids: RewriteFrameGrids writes them.
		return;
	case CHANNEL_TRANSPARENCY_LINES:
	{
		NResourceXml::Node &lines = ChildOrNew( FrameChunk( root, "own_data" ), "TransLines" );
		lines.children.clear();
		for ( std::size_t i = 0; i + 3 < blob.points.size(); i += 4 )
		{
			NResourceXml::Node line = NewElement( "item" );
			line.children.push_back( NewVec2( "Point1", blob.points[i], blob.points[i + 1] ) );
			line.children.push_back( NewVec2( "Point2", blob.points[i + 2], blob.points[i + 3] ) );
			lines.children.push_back( std::move( line ) );
		}
		return;
	}
	case CHANNEL_BRIDGE_SPAN_MARKS:
	{
		// CBridgeFrame::SaveFrameOwnData's Begin, End, Front and Back; z and
		// the export file name stay as read.
		if ( blob.points.size() < 6 )
			return;
		NResourceXml::Node &ownData = FrameChunk( root, "own_data" );
		WriteXY( ChildOrNew( ownData, "Begin" ), blob.points[0], blob.points[1], true );
		WriteXY( ChildOrNew( ownData, "End" ), blob.points[2], blob.points[3], true );
		NResourceModel::SetAttr( ownData, "Front", NResourceModel::MfcFloat( blob.points[4] ) );
		NResourceModel::SetAttr( ownData, "Back", NResourceModel::MfcFloat( blob.points[5] ) );
		return;
	}
	case CHANNEL_ENTRANCE:
	{
		if ( blob.points.size() < 2 )
			return;
		NResourceXml::Node &list = ChildOrNew( FrameChunk( root, "desc" ), "Entrances" );
		NResourceXml::Node *pItem = MutableChild( list, "item" );
		if ( pItem == nullptr )
		{
			NResourceXml::Node item = NewElement( "item" );
			item.attrs.push_back( { "Stormable", "0" } );
			item.children.push_back( NewVec3( "Position", 0, 0 ) );
			list.children.push_back( std::move( item ) );
			pItem = &list.children.back();
		}
		WriteXY( ChildOrNew( *pItem, "Position" ), blob.points[0], blob.points[1], true );
		return;
	}
	default:
	{
		NResourceXml::Node &list = ChildOrNew( FrameChunk( root, StatsChunkName( root ) ), AimedListName( nChannel ) );
		std::vector<NResourceXml::Node> kept;
		for ( auto &c : list.children )
			if ( c.kind == NResourceXml::Node::Element && c.name == "item" && kept.size() < blob.aimed.size() )
				kept.push_back( std::move( c ) );
		while ( kept.size() < blob.aimed.size() )
		{
			kept.push_back( NewAimedEntry( nChannel ) );
			// A bridge's smoke point has no fire effect (SBridgeRPGStats::SSmokePoint).
			if ( nChannel == CHANNEL_SMOKE_POINTS && root.name == "Bridge_Composer_Project" )
				kept.back().children.erase( std::remove_if( kept.back().children.begin(), kept.back().children.end(),
					[]( const NResourceXml::Node &n ) { return n.kind == NResourceXml::Node::Element && n.name == "FireEffect"; } ), kept.back().children.end() );
		}
		for ( std::size_t i = 0; i < kept.size(); ++i )
		{
			const AimedPoint &ap = blob.aimed[i];
			WriteXY( ChildOrNew( kept[i], "Position" ), ap.x, ap.y, true );
			NResourceModel::SetAttr( kept[i], "Direction", NResourceModel::MfcFloat( ap.nAngle ) );
			NResourceModel::SetAttr( kept[i], ConeAttr( nChannel ), NResourceModel::MfcFloat( ap.nCone ) );
		}
		list.children = std::move( kept );
		return;
	}
	}
}

NResourceModel::CListOfTiles *TileListOf( NResourceModel::CTreeItem *pItem, int nChannel )
{
	if ( auto *pFence = dynamic_cast<NResourceModel::CFencePropsItem *>( pItem ) )
		return nChannel == CHANNEL_FENCE_TRANSPARENCES ? &pFence->transeparences : &pFence->lockedTiles;
	if ( auto *pPart = dynamic_cast<NResourceModel::CBridgePartsItem *>( pItem ) )
		return &pPart->lockedTiles;
	return nullptr;
}

// A tile list as the ABI's grid: cell (x, y) is tile (x, y), so the grid
// runs from tile (0, 0) to the furthest stored tile. MFC stores only set
// tiles (SetTileInListOfTiles drops a zero), so trailing empty rows and
// columns of a set grid do not come back.
bool TilesToGrid( const NResourceModel::CListOfTiles &tiles, GeometryBlob &out, std::string &szError )
{
	out = GeometryBlob();
	for ( const auto &tile : tiles )
	{
		if ( tile.nTileX < 0 || tile.nTileY < 0 )
		{
			szError = "a locked tile lies left of or above tile (0, 0), which the grid cannot carry";
			return false;
		}
		out.nWidth = std::max( out.nWidth, tile.nTileX + 1 );
		out.nHeight = std::max( out.nHeight, tile.nTileY + 1 );
	}
	out.bytes.assign( static_cast<std::size_t>( out.nWidth ) * out.nHeight, 0 );
	for ( const auto &tile : tiles )
		out.bytes[static_cast<std::size_t>( tile.nTileY ) * out.nWidth + tile.nTileX] = static_cast<unsigned char>( tile.nVal );
	return true;
}

void GridToTiles( const GeometryBlob &blob, NResourceModel::CListOfTiles &tiles )
{
	tiles.clear();
	for ( int y = 0; y < blob.nHeight; ++y )
		for ( int x = 0; x < blob.nWidth; ++x )
			if ( const unsigned char c = blob.bytes[static_cast<std::size_t>( y ) * blob.nWidth + x] )
				tiles.push_back( { x, y, c } );
}

// The Object and Building frames' tile grids. MFC's CObjectFrame and
// CBuildingFrame keep the locked and transparency tiles as lists and
// SaveRPGStats crops them to a grid whose origin is the zero point minus the
// world position of the grid's leftmost corner (SObjectRPGStats and
// SBuildingRPGStats share the passability, origin, visibility and VisOrigin
// fields, so one reader and writer serve both). The ABI's passability_cells and transparency_cells channels are the
// tile frame: cell (x, y) is tile (x, y), like a fence's lists. An unedited
// channel is read from desc through LoadRPGStats' inverse; an edit is held in
// the tile frame and RewriteFrameGrids crops it with the origin of the zero
// point the save ends with, so a zero point set after the grid cannot leave
// the origin stale. The camera is the grid constants' own one: the engine
// camera is not reachable from the bridge, and the exporter uses the same.
bool IsTileFrameChannel( const ResourceState &state, int nNodeId, int nChannel )
{
	if ( nChannel != CHANNEL_PASSABILITY_CELLS && nChannel != CHANNEL_TRANSPARENCY_CELLS )
		return false;
	auto it = state.idToItem.find( nNodeId );
	if ( it == state.idToItem.end() )
		return false;
	const int nType = it->second->GetItemType();
	return nType == NResourceModel::ETIT_OBJECT_ROOT_ITEM || nType == NResourceModel::ETIT_BUILDING_ROOT_ITEM;
}

// The frame's passability and transparency tiles and its one-way tiles, from
// the grids and origins the document holds (LoadRPGStats).
void ObjectTiles( const NResourceModel::SObjectFrameData &data, NResourceModel::CListOfTiles &passTiles,
                  NResourceModel::CListOfTiles &transTiles, NResourceModel::CListOfNormalTiles &dirTiles )
{
	const NResourceModel::GridProjection projection( NResourceModel::DefaultEditorCamera() );
	int nTileX = 0, nTileY = 0;
	if ( !data.passability.empty() )
	{
		projection.FirstTileOfGrid( data.vZeroPos, NResourceModel::SVec3{ data.vOrigin.x, data.vOrigin.y, 0 }, nTileX, nTileY );
		NResourceModel::GridToTiles( data.passability, nTileX, nTileY, passTiles );
	}
	if ( !data.visibility.empty() )
	{
		projection.FirstTileOfGrid( data.vZeroPos, NResourceModel::SVec3{ data.vVisOrigin.x, data.vVisOrigin.y, 0 }, nTileX, nTileY );
		NResourceModel::VisGridToTiles( data.visibility, nTileX, nTileY, transTiles, dirTiles );
	}
}

bool ReadFrameTileGrid( const NResourceXml::Node &root, int nChannel, GeometryBlob &out, std::string &szError )
{
	NResourceModel::SObjectFrameData data;
	if ( !NResourceModel::ReadObjectFrameData( root, data, szError ) )
		return false;
	NResourceModel::CListOfTiles passTiles, transTiles;
	NResourceModel::CListOfNormalTiles dirTiles;
	ObjectTiles( data, passTiles, transTiles, dirTiles );
	return TilesToGrid( nChannel == CHANNEL_PASSABILITY_CELLS ? passTiles : transTiles, out, szError );
}

// Writes the frame's own_data and desc grids for a save that edited any of
// the channels they depend on (passability, transparency cells, trans-lines,
// zero point). The tiles an edit did not touch are read from the document
// before the new zero point applies, so they stay on their tiles and only
// their origin follows the zero point. Edited trans-lines give the one-way
// tiles again, as the editor's mouse-up handler does. A building has no
// trans-lines and no TransLines element: only its desc grids are written.
bool RewriteFrameGrids( const ResourceState &state, NResourceXml::Node &root, std::string &szError )
{
	if ( !state.pProject->root )
		return true;
	const int nRootType = state.pProject->root->GetItemType();
	const bool bBuilding = nRootType == NResourceModel::ETIT_BUILDING_ROOT_ITEM;
	if ( nRootType != NResourceModel::ETIT_OBJECT_ROOT_ITEM && !bBuilding )
		return true;
	auto itRoot = state.itemToId.find( state.pProject->root.get() );
	if ( itRoot == state.itemToId.end() )
		return true;
	auto Edit = [&]( int nChannel ) -> const GeometryBlob *
	{
		auto it = state.geometry.find( std::make_pair( itRoot->second, nChannel ) );
		return it == state.geometry.end() ? nullptr : &it->second;
	};
	const GeometryBlob *pPass = Edit( CHANNEL_PASSABILITY_CELLS ), *pTrans = Edit( CHANNEL_TRANSPARENCY_CELLS );
	const GeometryBlob *pLines = Edit( CHANNEL_TRANSPARENCY_LINES ), *pZero = Edit( CHANNEL_ZERO_POINT );
	if ( pPass == nullptr && pTrans == nullptr && pLines == nullptr && pZero == nullptr )
		return true;

	NResourceModel::SObjectFrameData data;
	if ( !NResourceModel::ReadObjectFrameData( root, data, szError ) )
		return false;
	NResourceModel::CListOfTiles passTiles, transTiles;
	NResourceModel::CListOfNormalTiles dirTiles;
	ObjectTiles( data, passTiles, transTiles, dirTiles );
	const NResourceModel::GridProjection projection( NResourceModel::DefaultEditorCamera() );
	if ( pPass != nullptr )
		GridToTiles( *pPass, passTiles );
	if ( pTrans != nullptr )
		GridToTiles( *pTrans, transTiles );
	if ( pLines != nullptr )
	{
		data.transLines.clear();
		for ( std::size_t i = 0; i + 3 < pLines->points.size(); i += 4 )
			data.transLines.push_back( { { pLines->points[i], pLines->points[i + 1] }, { pLines->points[i + 2], pLines->points[i + 3] } } );
		dirTiles.clear();
		projection.DirTilesFromTransLines( data.transLines, dirTiles );
	}
	if ( pZero != nullptr && pZero->points.size() >= 2 )
	{
		data.vZeroPos.x = pZero->points[0];
		data.vZeroPos.y = pZero->points[1];
	}
	data.passability = NResourceModel::TilesToGrid( passTiles );
	data.vOrigin = NResourceModel::SVec2();
	if ( !data.passability.empty() )
	{
		const NResourceModel::SVec3 origin = projection.OriginOfGrid( data.vZeroPos, data.passability.minTileX, data.passability.minTileY );
		data.vOrigin = { origin.x, origin.y };
	}
	data.visibility = NResourceModel::TilesToVisGrid( transTiles, dirTiles );
	data.vVisOrigin = NResourceModel::SVec2();
	if ( !data.visibility.empty() )
	{
		const NResourceModel::SVec3 origin = projection.OriginOfGrid( data.vZeroPos, data.visibility.minTileX, data.visibility.minTileY );
		data.vVisOrigin = { origin.x, origin.y };
	}
	if ( bBuilding )
	{
		FrameChunk( root, "desc" );
		NResourceModel::WriteObjectGrids( root, data );
		return true;
	}
	FrameChunk( root, "own_data" );
	FrameChunk( root, "desc" );
	NResourceModel::WriteObjectFrameData( root, data );
	return true;
}

NResourceModel::SProp *FindValue( NResourceModel::CTreeItem &item, const char *pszName )
{
	for ( NResourceModel::SProp &prop : item.MutableValues() )
		if ( prop.szDefaultName == pszName )
			return &prop;
	return nullptr;
}

// A position value as MFC's Get*Position reads it: CVariant's conversion of
// whichever type the value holds to a float.
float NumberOf( const NResourceModel::SProp *pProp )
{
	if ( pProp == nullptr )
		return 0;
	switch ( pProp->value.GetKind() )
	{
	case NResourceModel::CVariant::VK_INT: return static_cast<float>( pProp->value.AsInt() );
	case NResourceModel::CVariant::VK_FLOAT: return pProp->value.AsFloat();
	case NResourceModel::CVariant::VK_BOOL: return pProp->value.AsBool() ? 1.0f : 0.0f;
	default: return 0;
	}
}

// The entries of the RPG copy of a cross list, as read. MFC's LoadRPGStats
// (GetRPGStats) copies entry i over child i, so while a list is unedited
// its crosses are read from here; a child past the end of the list keeps
// its own values.
std::vector<const NResourceXml::Node *> RpgCrossEntries( const ResourceState &state, const SCrossList &list )
{
	const NResourceXml::Node *pRpg = NResourceXml::FindChild( state.pProject->document.root, "RPG" );
	const NResourceXml::Node *pList = pRpg != nullptr ? NResourceXml::FindChild( *pRpg, list.pszRpgList ) : nullptr;
	return pList != nullptr ? Items( *pList ) : std::vector<const NResourceXml::Node *>();
}

void LoadCrosses( const ResourceState &state, NResourceModel::CTreeItem &container, const SCrossList &list, GeometryBlob &out )
{
	std::vector<const NResourceXml::Node *> rpg;
	if ( state.editedCrossLists.count( list.nContainerType ) == 0 )
		rpg = RpgCrossEntries( state, list );
	std::size_t i = 0;
	for ( const auto &pChild : container.MutableChildren() )
	{
		float x = 0, y = 0;
		if ( i < rpg.size() )
			ReadXY( NResourceXml::FindChild( *rpg[i], list.pszRpgField ), x, y );
		else
		{
			x = NumberOf( FindValue( *pChild, list.pszX ) );
			y = NumberOf( FindValue( *pChild, list.pszY ) );
		}
		out.points.insert( out.points.end(), { x, y } );
		++i;
	}
}

// One cross per child, written the way CChapterFrame's drag does it
// (Set*Position: the values become floats). The RPG copy follows on save.
BkEditorStatus StoreCrosses( BkResSession *pSession, ResourceState &state, NResourceModel::CTreeItem &container,
                             const SCrossList &list, const GeometryBlob &blob )
{
	auto &children = container.MutableChildren();
	if ( blob.points.size() != children.size() * 2 )
	{
		pSession->szMessage = "a cross list carries one point per child of the node (" + std::to_string( children.size() ) + ")";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	for ( const auto &pChild : children )
		if ( FindValue( *pChild, list.pszX ) == nullptr || FindValue( *pChild, list.pszY ) == nullptr )
		{
			pSession->szMessage = std::string( "a child has no " ) + list.pszX + " value";
			return BK_EDITOR_FAILED;
		}
	for ( std::size_t i = 0; i < children.size(); ++i )
	{
		FindValue( *children[i], list.pszX )->value = blob.points[2*i];
		FindValue( *children[i], list.pszY )->value = blob.points[2*i + 1];
	}
	state.editedCrossLists.insert( list.nContainerType );
	return BK_EDITOR_OK;
}

// Writes the edited cross lists into the RPG copy, as CMissionFrame /
// CChapterFrame / CCampaignFrame::FillRPGStats would: entry i's position is
// child i's. Every other RPG field, and an entry with no child, stay as read.
// A project without an RPG chunk gets none (see the spec's limits).
void WriteCrossesToRpg( const ResourceState &state, NResourceXml::Node &root )
{
	NResourceXml::Node *pRpg = MutableChild( root, "RPG" );
	if ( pRpg == nullptr )
		return;
	for ( int nType : state.editedCrossLists )
	{
		const SCrossList *pList = nullptr;
		for ( const SCrossList &list : kCrossLists )
			if ( list.nContainerType == nType )
				pList = &list;
		NResourceModel::CTreeItem *pContainer = nullptr;
		for ( int nId : state.preorder )
			if ( state.idToItem.at( nId )->GetItemType() == nType )
			{
				pContainer = state.idToItem.at( nId );
				break;
			}
		NResourceXml::Node *pEntries = pList != nullptr ? MutableChild( *pRpg, pList->pszRpgList ) : nullptr;
		if ( pContainer == nullptr || pEntries == nullptr )
			continue;
		std::size_t i = 0;
		for ( NResourceXml::Node &entry : pEntries->children )
		{
			if ( entry.kind != NResourceXml::Node::Element || entry.name != "item" )
				continue;
			if ( i >= pContainer->MutableChildren().size() )
				break;
			NResourceModel::CTreeItem &child = *pContainer->MutableChildren()[i++];
			WriteXY( ChildOrNew( entry, pList->pszRpgField ), NumberOf( FindValue( child, pList->pszX ) ),
				NumberOf( FindValue( child, pList->pszY ) ), false );
		}
	}
}

// The current value of a channel on an open project's node, from wherever it
// lives. An un-set channel reads as an empty blob.
BkEditorStatus LoadGeometry( BkResSession *pSession, ResourceState &state, int nNodeId, int nChannel, GeometryBlob &out )
{
	out = GeometryBlob();
	std::string szError;
	NResourceModel::CTreeItem *pItem = state.idToItem[nNodeId];
	switch ( HomeOf( state, nNodeId, nChannel ) )
	{
	case HOME_NONE:
		pSession->szMessage = NoHomeMessage( state, nChannel );
		return BK_EDITOR_REFUSED;
	case HOME_FRAME:
	{
		auto it = state.geometry.find( std::make_pair( nNodeId, nChannel ) );
		if ( it != state.geometry.end() )
			out = it->second;
		else if ( IsTileFrameChannel( state, nNodeId, nChannel ) )
		{
			if ( !ReadFrameTileGrid( state.pProject->document.root, nChannel, out, szError ) )
			{
				pSession->szMessage = szError;
				return BK_EDITOR_FAILED;
			}
		}
		else if ( !ReadFrameGeometry( state.pProject->document.root, nChannel, out, szError ) )
		{
			pSession->szMessage = szError;
			return BK_EDITOR_FAILED;
		}
		return BK_EDITOR_OK;
	}
	case HOME_FENCE_SPRITE:
	{
		const auto *pFence = static_cast<const NResourceModel::CFencePropsItem *>( pItem );
		out.points = { pFence->vSpritePos.x, pFence->vSpritePos.y };
		return BK_EDITOR_OK;
	}
	case HOME_SQUAD_ZERO:
	{
		const auto *pFormation = static_cast<const NResourceModel::CSquadFormationPropsItem *>( pItem );
		out.points = { pFormation->vZeroPos.x, pFormation->vZeroPos.y };
		return BK_EDITOR_OK;
	}
	case HOME_SQUAD_UNITS:
		for ( const auto &unit : static_cast<const NResourceModel::CSquadFormationPropsItem *>( pItem )->units )
			out.points.insert( out.points.end(), { unit.vPos.x, unit.vPos.y } );
		return BK_EDITOR_OK;
	case HOME_SQUAD_DIR:
		out.points = { static_cast<const NResourceModel::CSquadFormationPropsItem *>( pItem )->fFormationDir, 0.0f };
		return BK_EDITOR_OK;
	case HOME_TILE_LIST:
		if ( !TilesToGrid( *TileListOf( pItem, nChannel ), out, szError ) )
		{
			pSession->szMessage = szError;
			return BK_EDITOR_FAILED;
		}
		return BK_EDITOR_OK;
	case HOME_CROSSES:
		LoadCrosses( state, *pItem, *CrossListOf( nChannel, pItem->GetItemType() ), out );
		return BK_EDITOR_OK;
	case HOME_EFFECT_PLACES:
		for ( const auto &pChild : pItem->MutableChildren() )
			out.points.insert( out.points.end(), { NumberOf( FindValue( *pChild, "X position" ) ),
				NumberOf( FindValue( *pChild, "Y position" ) ), NumberOf( FindValue( *pChild, "Z position" ) ) } );
		return BK_EDITOR_OK;
	case HOME_KEY_FRAMES:
		for ( const auto &frame : static_cast<const NResourceModel::CKeyFrameTreeItem *>( pItem )->framesList )
			out.points.insert( out.points.end(), { frame.first, frame.second, 0.0f } );
		return BK_EDITOR_OK;
	}
	return BK_EDITOR_FAILED;
}

// A building's aimed points are kept twice, as in MFC: the position in the
// frame (desc) and the direction and cone on a tree child per point
// (CBuildingSlotPropsItem and its kin, which the editor sets whenever it moves
// a point's handles and SaveRPGStats reads back). Writing the channel keeps
// the children's values equal to the list's, point i to child i; a list longer
// or shorter than the children (a composite that has inserted or removed the
// child yet, or not) syncs the children both have. Inserting and removing the
// child is the caller's own step (point_tools.zig), so an undo restores its
// other properties.
struct SAimedChildren
{
	int nChannel;
	int nContainerType;
	const char *pszConeValue;
};
const SAimedChildren kAimedChildren[] =
{
	{ CHANNEL_SHOOT_POINTS, NResourceModel::ETIT_BUILDING_SLOTS_ITEM, "Angle" },
	{ CHANNEL_FIRE_POINTS, NResourceModel::ETIT_BUILDING_FIRE_POINTS_ITEM, "Vertical angle" },
	{ CHANNEL_SMOKE_POINTS, NResourceModel::ETIT_BUILDING_SMOKES_ITEM, "Vertical angle" },
	{ CHANNEL_DIRECTED_EXPLOSION_POINTS, NResourceModel::ETIT_BUILDING_DIR_EXPLOSIONS_ITEM, "Vertical angle" },
	// The bridge's children carry a direction only, so the cone name finds nothing.
	{ CHANNEL_FIRE_POINTS, NResourceModel::ETIT_BRIDGE_FIRE_POINTS_ITEM, "Vertical angle" },
	{ CHANNEL_SMOKE_POINTS, NResourceModel::ETIT_BRIDGE_SMOKES_ITEM, "Vertical angle" },
	{ CHANNEL_DIRECTED_EXPLOSION_POINTS, NResourceModel::ETIT_BRIDGE_DIR_EXPLOSIONS_ITEM, "Vertical angle" }
};

void SyncAimedChildren( const ResourceState &state, int nChannel, const GeometryBlob &blob )
{
	for ( const SAimedChildren &entry : kAimedChildren )
	{
		if ( entry.nChannel != nChannel )
			continue;
		for ( int nId : state.preorder )
		{
			NResourceModel::CTreeItem *pContainer = state.idToItem.at( nId );
			if ( pContainer->GetItemType() != entry.nContainerType )
				continue;
			std::size_t i = 0;
			for ( const auto &pChild : pContainer->MutableChildren() )
			{
				if ( i >= blob.aimed.size() )
					break;
				if ( NResourceModel::SProp *pDirection = FindValue( *pChild, "Direction" ) )
					pDirection->value = static_cast<float>( blob.aimed[i].nAngle );
				if ( NResourceModel::SProp *pCone = FindValue( *pChild, entry.pszConeValue ) )
					pCone->value = static_cast<float>( blob.aimed[i].nCone );
				++i;
			}
			return;
		}
	}
}

BkEditorStatus StoreGeometry( BkResSession *pSession, ResourceState &state, int nNodeId, int nChannel, GeometryBlob blob )
{
	NResourceModel::CTreeItem *pItem = state.idToItem[nNodeId];
	if ( ( nChannel == CHANNEL_TRANSPARENCY_CELLS || nChannel == CHANNEL_FENCE_TRANSPARENCES ) && HomeOf( state, nNodeId, nChannel ) != HOME_NONE )
		for ( unsigned char c : blob.bytes )
			if ( c > 7 )
			{
				pSession->szMessage = std::string( "geometry channel " ) + ChannelName( nChannel ) + " carries transparency values 0..7, not " + std::to_string( c );
				return BK_EDITOR_BAD_ARGUMENT;
			}
	switch ( HomeOf( state, nNodeId, nChannel ) )
	{
	case HOME_NONE:
		pSession->szMessage = NoHomeMessage( state, nChannel );
		return BK_EDITOR_REFUSED;
	case HOME_FRAME:
		if ( !blob.aimed.empty() && ( pItem->GetItemType() == NResourceModel::ETIT_BUILDING_ROOT_ITEM || pItem->GetItemType() == NResourceModel::ETIT_BRIDGE_ROOT_ITEM ) )
			SyncAimedChildren( state, nChannel, blob );
		state.geometry[ std::make_pair( nNodeId, nChannel ) ] = std::move( blob );
		return BK_EDITOR_OK;
	case HOME_FENCE_SPRITE:
	{
		auto *pFence = static_cast<NResourceModel::CFencePropsItem *>( pItem );
		pFence->vSpritePos.x = blob.points[0];
		pFence->vSpritePos.y = blob.points[1];
		return BK_EDITOR_OK;
	}
	case HOME_SQUAD_ZERO:
	{
		auto *pFormation = static_cast<NResourceModel::CSquadFormationPropsItem *>( pItem );
		pFormation->vZeroPos.x = blob.points[0];
		pFormation->vZeroPos.y = blob.points[1];
		return BK_EDITOR_OK;
	}
	case HOME_SQUAD_UNITS:
	{
		// Slot i keeps its z and Dir; a new slot gets AddUnit's z = 0 and
		// Dir = 0, and a shorter list drops the tail.
		auto &units = static_cast<NResourceModel::CSquadFormationPropsItem *>( pItem )->units;
		const std::size_t nCount = blob.points.size() / 2;
		while ( units.size() > nCount )
			units.pop_back();
		while ( units.size() < nCount )
			units.push_back( NResourceModel::CSquadFormationPropsItem::SUnit() );
		std::size_t i = 0;
		for ( auto &unit : units )
		{
			unit.vPos.x = blob.points[2*i];
			unit.vPos.y = blob.points[2*i + 1];
			++i;
		}
		return BK_EDITOR_OK;
	}
	case HOME_SQUAD_DIR:
		// Only the angle: MFC's WM_ANGLE_CHANGED also turns the slots about
		// the zero point, which the editor writes as its own channel.
		static_cast<NResourceModel::CSquadFormationPropsItem *>( pItem )->fFormationDir = blob.points[0];
		return BK_EDITOR_OK;
	case HOME_TILE_LIST:
		GridToTiles( blob, *TileListOf( pItem, nChannel ) );
		return BK_EDITOR_OK;
	case HOME_CROSSES:
		return StoreCrosses( pSession, state, *pItem, *CrossListOf( nChannel, pItem->GetItemType() ), blob );
	case HOME_EFFECT_PLACES:
	{
		// The X / Y / Z position values are DT_DEC: whole numbers, as the
		// property inspector writes them.
		auto &children = pItem->MutableChildren();
		if ( blob.points.size() != children.size() * 3 )
		{
			pSession->szMessage = "an effect part list carries one place per child of the node (" + std::to_string( children.size() ) + ")";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		static const char *const kAxes[3] = { "X position", "Y position", "Z position" };
		for ( std::size_t i = 0; i < blob.points.size(); ++i )
		{
			const float f = blob.points[i];
			// 2147483647 is not a float; the bound is 2^31, exclusive, so the cast below stays in int's range.
			if ( !( f >= -2147483648.0f && f < 2147483648.0f ) || std::floor( f ) != f )
			{
				pSession->szMessage = "an effect part's place is in whole units";
				return BK_EDITOR_BAD_ARGUMENT;
			}
			if ( FindValue( *children[i / 3], kAxes[i % 3] ) == nullptr )
			{
				pSession->szMessage = std::string( "a child has no " ) + kAxes[i % 3] + " value";
				return BK_EDITOR_FAILED;
			}
		}
		for ( std::size_t i = 0; i < blob.points.size(); ++i )
			FindValue( *children[i / 3], kAxes[i % 3] )->value = static_cast<int>( blob.points[i] );
		return BK_EDITOR_OK;
	}
	case HOME_KEY_FRAMES:
	{
		// A key is a (time, value) pair; MFC has no third component.
		NResourceModel::CFramesList frames;
		for ( std::size_t i = 0; i + 2 < blob.points.size(); i += 3 )
		{
			if ( blob.points[i + 2] != 0 )
			{
				pSession->szMessage = "a particle key frame is a (time, value) pair: z must be 0";
				return BK_EDITOR_BAD_ARGUMENT;
			}
			frames.push_back( { blob.points[i], blob.points[i + 1] } );
		}
		static_cast<NResourceModel::CKeyFrameTreeItem *>( pItem )->framesList = std::move( frames );
		return BK_EDITOR_OK;
	}
	}
	return BK_EDITOR_FAILED;
}

// A delete blob holds the subtree as its items write it. Item-field
// geometry is part of that, and the frame homes belong to the root, which
// cannot be deleted, so a restore brings everything back.
std::string SerialiseSubtree( NResourceModel::CTreeItem &item )
{
	NResourceXml::Document doc;
	doc.hasDeclaration = false;
	EmitNodeFor( item, doc.root );
	return NResourceXml::Serialise( doc );
}

// The bytes BkResSave writes for the open project: the tree, with the
// edited frame homes written into own_data / desc and the edited cross lists
// into RPG. Export hands its exporter a project parsed from the same bytes,
// so what is exported is what a save would write. Re-parsing Serialise's
// output and writing it again gives the same bytes (xml.h), so a project
// without frame edits is not touched.
bool RenderForSave( const ResourceState &state, std::string &szOut, std::string &szError )
{
	szOut = NResourceModel::Save( *state.pProject );
	if ( state.pProject->root && ( !state.geometry.empty() || !state.editedCrossLists.empty() ) )
	{
		NResourceXml::Document doc;
		std::string szParseError;
		if ( !NResourceXml::Parse( szOut, doc, szParseError ) )
		{
			szError = "cannot re-read the rendered project: " + szParseError;
			return false;
		}
		if ( !RewriteFrameGrids( state, doc.root, szError ) )
			return false;
		for ( const auto &entry : state.geometry )
			if ( HomeOf( state, entry.first.first, entry.first.second ) == HOME_FRAME && !IsTileFrameChannel( state, entry.first.first, entry.first.second ) )
				WriteFrameGeometry( doc.root, entry.first.second, entry.second );
		WriteCrossesToRpg( state, doc.root );
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
		// CTileSetFrame::InitFreeTerrainIndexes / InitFreeCrossetIndexes ran on every open.
		if ( state.pProject->root && state.pProject->root->GetItemType() == NResourceModel::ETIT_TILESET_ROOT_ITEM )
			NResourceModel::AssignMissingTileIndexes( *state.pProject->root );
		RebuildIds( state );
		RebuildMeshLocators( state, pSession->szMessage );
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
			rec.expand = pItem->GetExpand() ? 1 : 0;
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
				if ( itItem->second->GetItemType() == NResourceModel::ETIT_MESH_GRAPHICS_ITEM && nPropId == 1 )
					RebuildMeshLocators( state, pSession->szMessage );
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
		// CTileSetFrame::GetFreeTerrainIndex / GetFreeCrossetIndex: a new tile
		// takes the lowest index no tile of its list has.
		if ( nClassType == NResourceModel::ETIT_TILESET_TILE_PROPS_ITEM )
			static_cast<NResourceModel::CTileSetTilePropsItem *>( pNew.get() )->nTileIndex = NResourceModel::GetFreeTerrainIndex( *state.pProject->root );
		else if ( nClassType == NResourceModel::ETIT_CROSSET_TILE_PROPS_ITEM )
			static_cast<NResourceModel::CCrossetTilePropsItem *>( pNew.get() )->nCrossIndex = NResourceModel::GetFreeCrossetIndex( *state.pProject->root );
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
		const std::string szBlob = IdsHeader( state, itItem->second ) + SerialiseSubtree( *itItem->second );
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
		// Geometry map entries belong to the root's frame homes; drop any that
		// name a node that is gone.
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

BkEditorStatus BkResSetNodeName( BkResSession *pSession, int nNodeId, const char *pszName )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		// The record's display_name is the limit, so a name the tree could not
		// show back whole is refused rather than cut.
		if ( pszName == nullptr || pszName[0] == 0 || std::strlen( pszName ) >= sizeof( BkResNodeRecord().display_name ) )
		{
			pSession->szMessage = "a node name must be 1 to 63 characters";
			return BK_EDITOR_BAD_ARGUMENT;
		}
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
		itItem->second->SetDisplayName( pszName );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetNodeExpand( BkResSession *pSession, int nNodeId, int nExpand )
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
		itItem->second->SetExpand( nExpand != 0 );
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
		// E_ACTIONS_REF is CMultySelDialog's list, not InitLists': the action
		// types of actions.ini, each keyed by its id (the mask's bit). A data
		// folder without the file has an empty list, as InitLists' was.
		std::vector<std::string> actionNames;
		std::vector<int> actionIds;
		const bool bActions = nType == int( NResourceModel::EReferenceType::E_ACTIONS_REF );
		if ( bActions )
		{
			std::vector<SActionCommandEntry> actions;
			std::string szWhy;
			if ( LoadActionCommands( &actions, &szWhy ) )
			{
				for ( const SActionCommandEntry &entry : actions )
				{
					actionNames.push_back( entry.szName );
					actionIds.push_back( entry.nID );
				}
			}
		}
		const std::vector<std::string> &list = bActions ? actionNames : state.refLists[nType];
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
			pOut[i].token = bActions ? actionIds[i] : int( i );
			CopyField( pOut[i].name, sizeof( pOut[i].name ), list[i] );
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPropStrings( BkResSession *pSession, int nNodeId, int nPropId, BkResReferenceEntry *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == nullptr || nCapacity < 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnCount = 0;
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
		for ( const NResourceModel::SProp &p : itItem->second->GetValues() )
		{
			if ( p.nId != nPropId )
				continue;
			// The unit editor's locator combos are computed from the Locators
			// children as MFC did on a click, so they follow a model switch.
			std::vector<std::string> locatorStrings;
			const bool bLocatorCombo = state.pProject && state.pProject->root &&
				NResourceModel::MeshLocatorStrings( *state.pProject->root, itItem->second->GetItemType(), nPropId, locatorStrings );
			const std::vector<std::string> &strings = bLocatorCombo ? locatorStrings : p.szStrings;
			*pnCount = int( strings.size() );
			if ( pOut == nullptr && nCapacity == 0 )
				return BK_EDITOR_OK;
			if ( pOut == nullptr || nCapacity < int( strings.size() ) )
			{
				pSession->szMessage = "the buffer holds " + std::to_string( nCapacity ) + " of " + std::to_string( strings.size() ) + " strings";
				return BK_EDITOR_REFUSED;
			}
			for ( std::size_t i = 0; i < strings.size(); ++i )
			{
				pOut[i].token = int( i );
				CopyField( pOut[i].name, sizeof( pOut[i].name ), strings[i] );
			}
			return BK_EDITOR_OK;
		}
		pSession->szMessage = "unknown prop id";
		return BK_EDITOR_REFUSED;
	} );
}

/* ---- Geometry --------------------------------------------------------- */

/* Every helper reads through LoadGeometry and writes through StoreGeometry,
   which go to the channel's MFC home (HomeOf).

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
	GeometryBlob blob;
	const BkEditorStatus nLoaded = LoadGeometry( pSession, state, nNodeId, nChannel, blob );
	if ( nLoaded != BK_EDITOR_OK )
		return nLoaded;
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
	return StoreGeometry( pSession, state, nNodeId, nChannel, std::move( blob ) );
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

BkEditorStatus BkResGetTransparencyCells( BkResSession *pSession, int nNodeId, unsigned char *pOut, int nCapacity, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetBytesGrid( pSession, CHANNEL_TRANSPARENCY_CELLS, nNodeId, pOut, nCapacity, pnW, pnH );
	} );
}

BkEditorStatus BkResSetTransparencyCells( BkResSession *pSession, int nNodeId, const unsigned char *pIn, int nW, int nH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetBytesGrid( pSession, CHANNEL_TRANSPARENCY_CELLS, nNodeId, pIn, nW, nH );
	} );
}

BkEditorStatus BkResGetFenceTransparences( BkResSession *pSession, int nNodeId, unsigned char *pOut, int nCapacity, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetBytesGrid( pSession, CHANNEL_FENCE_TRANSPARENCES, nNodeId, pOut, nCapacity, pnW, pnH );
	} );
}

BkEditorStatus BkResSetFenceTransparences( BkResSession *pSession, int nNodeId, const unsigned char *pIn, int nW, int nH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetBytesGrid( pSession, CHANNEL_FENCE_TRANSPARENCES, nNodeId, pIn, nW, nH );
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
	GeometryBlob blob;
	const BkEditorStatus nLoaded = LoadGeometry( pSession, state, nNodeId, nChannel, blob );
	if ( nLoaded != BK_EDITOR_OK )
		return nLoaded;
	const int nCount = static_cast<int>( blob.points.size() / 2 );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		pOut[i].x = blob.points[2*i];
		pOut[i].y = blob.points[2*i + 1];
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
	// CObjectFrame::STransLine: each line is a Point1 / Point2 pair.
	if ( nChannel == CHANNEL_TRANSPARENCY_LINES && nCount % 2 != 0 )
	{
		pSession->szMessage = "transparency lines come in point pairs";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	// CBridgeFrame's own data: Begin, End and (Front, Back), always all three.
	if ( nChannel == CHANNEL_BRIDGE_SPAN_MARKS && nCount != 3 )
	{
		pSession->szMessage = "bridge span marks are Begin, End and (Front, Back): three points";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	GeometryBlob blob;
	blob.points.reserve( static_cast<std::size_t>( nCount ) * 2 );
	for ( int i = 0; i < nCount; ++i )
	{
		blob.points.push_back( pIn[i].x );
		blob.points.push_back( pIn[i].y );
	}
	return StoreGeometry( pSession, state, nNodeId, nChannel, std::move( blob ) );
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
	GeometryBlob blob;
	const BkEditorStatus nLoaded = LoadGeometry( pSession, state, nNodeId, nChannel, blob );
	if ( nLoaded != BK_EDITOR_OK )
		return nLoaded;
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
	return StoreGeometry( pSession, state, nNodeId, nChannel, std::move( blob ) );
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
	GeometryBlob blob;
	const BkEditorStatus nLoaded = LoadGeometry( pSession, state, nNodeId, nChannel, blob );
	if ( nLoaded != BK_EDITOR_OK )
		return nLoaded;
	const int nCount = static_cast<int>( blob.aimed.size() );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		const AimedPoint &src = blob.aimed[i];
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
	return StoreGeometry( pSession, state, nNodeId, nChannel, std::move( blob ) );
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

BkEditorStatus BkResGetSpritePos( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoint2( pSession, CHANNEL_SPRITE_POS, nNodeId, pOut );
	} );
}

BkEditorStatus BkResSetSpritePos( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoint2( pSession, CHANNEL_SPRITE_POS, nNodeId, pIn );
	} );
}

BkEditorStatus BkResGetFormationDirection( BkResSession *pSession, int nNodeId, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return GetPoint2( pSession, CHANNEL_FORMATION_DIRECTION, nNodeId, pOut );
	} );
}

BkEditorStatus BkResSetFormationDirection( BkResSession *pSession, int nNodeId, const BkResPoint2 *pIn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetPoint2( pSession, CHANNEL_FORMATION_DIRECTION, nNodeId, pIn );
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

/* Vec3-family helpers: particle key frames ((time, value, 0) per key) and
   effect part places (x, y, z per child). */

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
	GeometryBlob blob;
	const BkEditorStatus nLoaded = LoadGeometry( pSession, state, nNodeId, nChannel, blob );
	if ( nLoaded != BK_EDITOR_OK )
		return nLoaded;
	const int nCount = static_cast<int>( blob.points.size() / 3 );
	if ( pnCount != nullptr ) *pnCount = nCount;
	if ( pOut == nullptr || nCapacity <= 0 )
		return BK_EDITOR_OK;
	if ( nCapacity < nCount )
		return BK_EDITOR_REFUSED;
	for ( int i = 0; i < nCount; ++i )
	{
		pOut[i].x = blob.points[3*i];
		pOut[i].y = blob.points[3*i + 1];
		pOut[i].z = blob.points[3*i + 2];
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
	return StoreGeometry( pSession, state, nNodeId, nChannel, std::move( blob ) );
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

// Moves every staged file into data/, all or nothing (D-09). Each live file
// about to be replaced is first copied into backup; if any move then fails,
// the files already moved are taken out again (restored from their backup,
// or removed when they are new, with the folders made for them), so data/ is
// as it was. A failed restore keeps the backup folder and says where it is.
bool PromoteStaged( const std::filesystem::path &staging, const std::filesystem::path &backup,
                    const std::filesystem::path &dataDir, std::string &szError )
{
	std::error_code ec;
	const std::vector<std::string> files = FilesBelow( staging );
	std::filesystem::remove_all( backup, ec );
	std::vector<bool> backedUp( files.size(), false );
	for ( std::size_t i = 0; i < files.size(); ++i )
	{
		const std::filesystem::path target = dataDir / files[i];
		if ( !std::filesystem::is_regular_file( target, ec ) )
			continue;
		const std::filesystem::path copy = backup / files[i];
		std::filesystem::create_directories( copy.parent_path(), ec );
		if ( ec || !std::filesystem::copy_file( target, copy, std::filesystem::copy_options::overwrite_existing, ec ) )
		{
			szError = "cannot back up " + files[i] + " before replacing it: " + ec.message() + "; nothing was exported";
			std::filesystem::remove_all( backup, ec );
			return false;
		}
		backedUp[i] = true;
	}

	std::vector<std::filesystem::path> createdDirs;
	std::size_t nPromoted = 0;
	std::string szFailure;
	for ( ; nPromoted < files.size(); ++nPromoted )
	{
		const std::filesystem::path target = dataDir / files[nPromoted];
		std::vector<std::filesystem::path> missing;
		for ( std::filesystem::path dir = target.parent_path(); !dir.empty() && !std::filesystem::exists( dir, ec ); dir = dir.parent_path() )
		{
			missing.push_back( dir );
			if ( dir == dir.parent_path() )
				break;
		}
		std::filesystem::create_directories( target.parent_path(), ec );
		createdDirs.insert( createdDirs.end(), missing.rbegin(), missing.rend() );
		if ( !ec )
			std::filesystem::rename( staging / files[nPromoted], target, ec );
		if ( ec )
		{
			szFailure = "cannot move " + files[nPromoted] + " into " + dataDir.string() + ": " + ec.message();
			break;
		}
	}
	if ( nPromoted == files.size() )
	{
		std::filesystem::remove_all( backup, ec );
		return true;
	}

	std::vector<std::string> unrestored;
	for ( std::size_t i = nPromoted; i-- > 0; )
	{
		const std::filesystem::path target = dataDir / files[i];
		if ( backedUp[i] )
			std::filesystem::rename( backup / files[i], target, ec );
		else
			std::filesystem::remove( target, ec );
		if ( ec )
			unrestored.push_back( files[i] );
	}
	for ( std::size_t i = createdDirs.size(); i-- > 0; )
		std::filesystem::remove( createdDirs[i], ec );
	if ( unrestored.empty() )
	{
		std::filesystem::remove_all( backup, ec );
		szError = szFailure + "; the export was rolled back, " + dataDir.string() + " is as it was";
	}
	else
		szError = szFailure + "; the export was rolled back except " + unrestored.front() + ( unrestored.size() > 1 ? " and others" : "" ) +
		          ", whose originals are kept in " + backup.string();
	return false;
}

std::string EngineFolderPattern( const std::filesystem::path &dir );
void ForgetPreviewCaches();

// The layer a trench segment model is mounted on while it is built.
const char *const kExportMeshLayer = "RES_EXPORT_MESH";

// The scene's camera as the ground-plane matrix an object, fence or building
// export reads where MFC called IScene::GetPos2. The scene's transform is
// affine on the ground plane: three probes give the matrix.
bool SceneGroundCamera( NResourceModel::SGroundCamera &camera )
{
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
		return false;
	CVec2 origin, unitX, unitY;
	pScene->GetPos2( &origin, CVec3( 0, 0, 0 ) );
	pScene->GetPos2( &unitX, CVec3( 1, 0, 0 ) );
	pScene->GetPos2( &unitY, CVec3( 0, 1, 0 ) );
	camera.m11 = unitX.x - origin.x;
	camera.m21 = unitX.y - origin.y;
	camera.m12 = unitY.x - origin.x;
	camera.m22 = unitY.y - origin.y;
	camera.m13 = camera.m23 = 0;
	camera.m14 = origin.x;
	camera.m24 = origin.y;
	return true;
}

// What MFC's frames took from the running editor and an exporter does not
// own (D015): the objects database CSquadFrame::SaveRPGStats searched for a
// member's key, and the mesh builder CTrenchFrame::SaveRPGStats read a
// segment's locators from. Each is set only when the engine has it running.
// scratch is a folder of the bridge's own beside the export, where a model
// is mounted while it is built.
void FillEngineLookups( NResourceModel::SExportContext &context, const std::filesystem::path &scratch )
{
	if ( GetSingleton<IObjectsDB>() != 0 )
	{
		context.findUnitKey = []( const std::string &szPath, std::string &szKey ) -> bool
		{
			IObjectsDB *pObjDB = GetSingleton<IObjectsDB>();
			const int nNumDescs = pObjDB->GetNumDescs();
			const SGDBObjectDesc *pObjDescs = pObjDB->GetAllDescs();
			for ( int i = 0; i < nNumDescs; i++ )
				if ( pObjDescs[i].eVisType == SGVOT_SPRITE && pObjDescs[i].eGameType == SGVOGT_UNIT && Fold( pObjDescs[i].szPath ) == Fold( szPath ) )
				{
					szKey = pObjDescs[i].szKey;
					return true;
				}
			return false;
		};
	}
	if ( GetSingleton<IObjectsDB>() != 0 )
	{
		context.isBallisticWeapon = []( const std::string &szWeapon, bool &bBallistic ) -> bool
		{
			const SWeaponRPGStats *pWeapon = static_cast<const SWeaponRPGStats *>( GetSingleton<IObjectsDB>()->GetAddStats( szWeapon.c_str(), IObjectsDB::WEAPON ) );
			if ( pWeapon == 0 )
				return false;
			bBallistic = false;
			for ( const auto &shell : pWeapon->shells )
				if ( shell.trajectory == SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER || shell.trajectory == SWeaponRPGStats::SShell::TRAJECTORY_CANNON )
					bBallistic = true;
			return true;
		};
	}
	if ( GetSingleton<IScene>() != 0 )
	{
		// The scene's transform is affine on the ground plane: three probes give the matrix.
		context.groundCamera = &SceneGroundCamera;
	}
	if ( GetSingleton<IVisObjBuilder>() != 0 && GetSingleton<IDataStorage>() != 0 )
	{
		context.meshFirePlaces = [scratch]( const std::string &szModFile, std::vector<std::pair<float, float>> &firePlaces, std::string &szError ) -> bool
		{
			// MFC copied the model to the editor's temp folder as 1.mod and
			// built "<temp>\1". A new name each time keeps the builder's and
			// the mesh manager's caches from answering with an earlier model.
			static int nModel = 0;
			++nModel;
			std::error_code ec;
			std::filesystem::remove_all( scratch, ec );
			const std::filesystem::path folder = scratch / "bkexportmesh" / std::to_string( nModel );
			std::filesystem::create_directories( folder, ec );
			if ( ec || !std::filesystem::copy_file( szModFile, folder / "1.mod", std::filesystem::copy_options::overwrite_existing, ec ) )
			{
				szError = "cannot copy " + szModFile + " to " + folder.string();
				std::filesystem::remove_all( scratch, ec );
				return false;
			}
			IDataStorage *pStorage = GetSingleton<IDataStorage>();
			CPtr<IDataStorage> pLayer = OpenStorage( EngineFolderPattern( scratch ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
			if ( pLayer == 0 )
			{
				szError = "cannot mount " + scratch.string();
				std::filesystem::remove_all( scratch, ec );
				return false;
			}
			pStorage->AddStorage( pLayer, kExportMeshLayer );
			ForgetPreviewCaches();
			bool bBuilt = false;
			{
				const std::string szName = "bkexportmesh\\" + std::to_string( nModel ) + "\\1";
				CPtr<IObjVisObj> pReadyObject = static_cast<IObjVisObj *>( GetSingleton<IVisObjBuilder>()->BuildObject( szName.c_str(), 0, SGVOT_MESH ) );
				IMeshAnimation *pMeshAnim = pReadyObject ? static_cast<IMeshAnimation *>( pReadyObject->GetAnimation() ) : 0;
				IMeshAnimationEdit *pMeshAnimEdit = pMeshAnim ? dynamic_cast<IMeshAnimationEdit *>( pMeshAnim ) : 0;
				if ( pMeshAnimEdit == 0 )
					szError = "IVisObjBuilder would not build the mesh";
				else
				{
					const int nNumLocators = pMeshAnimEdit->GetNumLocators();
					std::vector<const char *> locatorNamesVector( std::max( nNumLocators, 1 ) );
					if ( nNumLocators > 0 )
						pMeshAnimEdit->GetAllLocatorNames( &( locatorNamesVector[0] ), nNumLocators );
					const int nNumNodes = pMeshAnim->GetNumNodes();
					std::vector<const char *> allNamesVector( std::max( nNumNodes, 1 ) );
					if ( nNumNodes > 0 )
						pMeshAnimEdit->GetAllNodeNames( &( allNamesVector[0] ), nNumNodes );
					bBuilt = true;
					for ( int i = 0; i < nNumLocators; i++ )
					{
						// MFC compared the name pointers, which point into the
						// same skeleton; the strings say the same.
						int nFireLocatorIndex = 0;
						for ( ; nFireLocatorIndex < nNumNodes; nFireLocatorIndex++ )
							if ( std::strcmp( allNamesVector[nFireLocatorIndex], locatorNamesVector[i] ) == 0 )
								break;
						if ( nFireLocatorIndex == nNumNodes )
						{
							szError = std::string( "locator " ) + locatorNamesVector[i] + " is no node of the mesh";
							bBuilt = false;
							break;
						}
						pReadyObject->SetPosition( CVec3( 0, 0, 0 ) );
						pReadyObject->SetDirection( 0 );
						IMeshAnimation *pPlaced = static_cast<IMeshAnimation *>( pReadyObject->GetAnimation() );
						const SHMatrix *pMatrix = pPlaced->GetMatrices( MONE );
						const CVec3 vFireTrans = pMatrix[ nFireLocatorIndex ].GetTrans3();
						firePlaces.push_back( std::make_pair( vFireTrans.x, vFireTrans.y ) );
					}
				}
			}
			pStorage->RemoveStorage( kExportMeshLayer );
			pLayer = 0;
			ForgetPreviewCaches();
			std::filesystem::remove_all( scratch, ec );
			return bBuilt;
		};
	}
}

// One project through its kind's exporter: the exporter writes into a
// staging folder beside data/, and only when it succeeds are its files
// promoted into data/, all or nothing, so a failed export leaves no
// half-written resource.
bool ExportOne( const NResourceModel::Project &project, const std::string &szProjectPath, const std::string &szExtension,
                const std::filesystem::path &dataDir, const std::filesystem::path &editorDataDir, int nFlags, bool bStatsOnly,
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
	context.szDataRoot = dataDir.string();
	context.szEditorDataDir = editorDataDir.string();
	FillEngineLookups( context, dataDir.parent_path() / ".bk-export-mesh" );
	if ( !pfnExporter( project, context, outcome ) )
	{
		std::filesystem::remove_all( staging, ec );
		szError = outcome.szError.empty() ? std::string( "the exporter failed" ) : outcome.szError;
		return false;
	}
	const bool bPromoted = PromoteStaged( staging, dataDir.parent_path() / ".bk-export-backup", dataDir, szError );
	std::filesystem::remove_all( staging, ec );
	return bPromoted;
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
	if ( !ExportOne( project, state.szPath, szExtension, dataDir, ShippedDataFolder( pSession ), nFlags, bStatsOnly, outcome, szError ) )
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
			if ( ExportOne( project, szPath, kKindExtensions[entry.first], dataDir, ShippedDataFolder( pSession ), nFlags, false, outcome, szError ) )
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

// The kinds the preview builds today and what IVisObjBuilder builds them as,
// with the MFC frame that does it (D-17). The other scene kind (bridge)
// joins with its sub-editor slice;
// road and river load maps\road3d / maps\river3d as their terrain (S13).
// A particle source is no visual of its own: IVisObjBuilder has no particle
// type, so CParticleFrame::OnRunButton wrapped the exported source in a
// one-particle effect and built that, and so does the preview
// (WrapParticleSource). A squad is no visual either: SquadFrm's view builds
// one sprite per member of the active formation, so the preview does
// (BuildSquadMembers) and its export names no visual. An object and a fence
// build the sprite their export composed, as ObjectFrm and FenceFrm do; a
// building the same, its summer whole picture.
// A weapon has no entry:
// WeaponFrm draws nothing (D015).
struct PreviewKind
{
	int nKind;
	EObjVisType eVisType;
	EObjGameType eGameType;
	bool bParticleSource;
	bool bSquad = false;
};
const int kTilesetKind = 13;  // "til", the Terrain tileset project
const int kRoadKind = 14;     // "3rd", the 3D Road project
const int kRiverKind = 15;    // "3rv", the 3D River project
const int kMedalKind = 19;    // "mdc", the Medal project

const PreviewKind kPreviewKinds[] =
{
	{ 4,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // spt: CSpriteFrame::OnRunButton
	{ 5,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // unt: infantry draws as a sprite
	{ 6,  SGVOT_MESH,   SGVOGT_UNIT,   false },  // msh: CMeshFrame's combat object
	{ 11, SGVOT_EFFECT, SGVOGT_EFFECT, true  },  // pcp: CParticleFrame::OnRunButton
	{ 12, SGVOT_EFFECT, SGVOGT_EFFECT, false },  // eff: CEffectFrame::OnRunButton
	{ 1,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // mcp: the composed sprite "1" the game builds for the mine
	{ 2,  SGVOT_MESH,   SGVOGT_ENTRENCHMENT, false },  // trc: the entrenchment model, TrenchFrm.cpp:238
	{ 3,  SGVOT_SPRITE, SGVOGT_UNIT,   false, true },  // scp: member sprites, SquadFrm.cpp:344-364
	{ 7,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // obt: the composed object sprite, ObjectFrm.cpp:1295
	{ 8,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // fnc: the active segment's sprite, FenceFrm.cpp:665
	{ 9,  SGVOT_SPRITE, SGVOGT_UNIT,   false },  // bld: the composed whole building sprite "1", BuildFrm.cpp:535
	{ 10, SGVOT_SPRITE, SGVOGT_UNIT,   false },  // bdg: the first picture of the whole stage's sprite set "1" (the measured shot comes with the preview task)
};

const PreviewKind *FindPreviewKind( int nKind )
{
	for ( const PreviewKind &entry : kPreviewKinds )
		if ( entry.nKind == nKind )
			return &entry;
	return nullptr;
}

// The value in slot nSlot of an item as a number, whichever of int and float
// the project stored it as; fDefault when the slot is missing or not one.
float NumberSlot( const NResourceModel::CTreeItem *pItem, std::size_t nSlot, float fDefault )
{
	if ( pItem == nullptr || nSlot >= pItem->GetValues().size() )
		return fDefault;
	const NResourceModel::CVariant &value = pItem->GetValues()[nSlot].value;
	if ( value.GetKind() == NResourceModel::CVariant::VK_INT )
		return float( value.AsInt() );
	if ( value.GetKind() == NResourceModel::CVariant::VK_FLOAT )
		return value.AsFloat();
	return fDefault;
}

// CParticleFrame::CreateEffectDescriptionFile (ParticleFrm.cpp) for the
// preview: an effect of one particle item that starts at once, runs for
// twice the source's life time and sits at its position and scale, read
// from the project's common props (CParticleCommonPropsItem's slots 1, 2 and
// 4..6). Written beside the exported source as SEffectDesc's own layout
// (fmtEffect.cpp, the shipped Effects\Effects\*.xml), which the bridge
// writes by hand because it does not link Formats. MFC's complex-source
// switch (a smokin particle item) is a frame toggle, not project data, so
// the preview builds the plain source as the frame did by default.
bool WrapParticleSource( const NResourceModel::Project &project, const std::filesystem::path &dataDir,
                         const std::string &szSourceName, const std::string &szEffectName, std::string &szError )
{
	const NResourceModel::CTreeItem *pProps = nullptr;
	if ( project.root )
		for ( const auto &pChild : project.root->GetChildren() )
			if ( pChild->GetItemType() == NResourceModel::ETIT_PARTICLE_COMMON_PROPS_ITEM )
				pProps = pChild.get();
	if ( pProps == nullptr )
	{
		szError = "the particle project has no common props item";
		return false;
	}
	const int nDuration = int( NumberSlot( pProps, 1, 0.0f ) ) * 2;
	char szItem[512];
	std::snprintf( szItem, sizeof szItem,
		"<item start=\"0\" duration=\"%d\" scale=\"%g\"><path>%s</path><pos x=\"%g\" y=\"%g\" z=\"%g\"/></item>",
		nDuration, NumberSlot( pProps, 2, 1.0f ), szSourceName.c_str(),
		NumberSlot( pProps, 4, 0.0f ), NumberSlot( pProps, 5, 0.0f ), NumberSlot( pProps, 6, 0.0f ) );
	std::string szRelative = szEffectName;
	for ( char &c : szRelative )
		if ( c == '\\' ) c = '/';
	const std::filesystem::path file = dataDir / ( szRelative + ".xml" );
	std::error_code ec;
	std::filesystem::create_directories( file.parent_path(), ec );
	std::ofstream out( file, std::ios::out | std::ios::binary | std::ios::trunc );
	out << "<?xml version=\"1.0\"?>\r\n<effect><effect><sprites/><particles>" << szItem
	    << "</particles><SmokinParticles/></effect></effect>\r\n";
	out.close();
	if ( !out )
	{
		szError = "cannot write the particle preview effect " + file.string();
		return false;
	}
	return true;
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
	for ( IVisObj *pMember : state.previewMembers )
		pMember->Update( time );
}

// CEffectFrame::UpdateEffectAngle's matrix: the quarter turn the dock's 45
// degrees start from is taken out, the rest turns about (-1, 1, 0) / sqrt 2.
void EffectDirectionOf( float fAngle, SHMatrix &matrix )
{
	float fAlpha = -fAngle;
	fAlpha += ToRadian( 45.0f );
	if ( fAlpha >= FP_2PI )
		fAlpha -= FP_2PI;
	const float fTemp = float( 1.0 / sqrt( 2.0 ) );
	CQuat quat( fAlpha, CVec3( -fTemp, fTemp, 0 ) );
	matrix.Set( quat );
}

// UpdateEffectAngle: nothing unless the effect plays.
void ApplyEffectDirection( ResourceState &state )
{
	if ( !state.bPreviewRunning || !state.bPreviewEffect || state.pPreviewObj == nullptr )
		return;
	SHMatrix matrix;
	EffectDirectionOf( state.fEffectAngle, matrix );
	static_cast<IEffectVisObj *>( state.pPreviewObj )->SetEffectDirection( matrix );
}

// Restarts the object's own animation: an effect from the current game time,
// a sprite or mesh from its first animation.
void RestartPreviewObject( ResourceState &state )
{
	const NTimer::STime time = UpdateGameTimer();
	for ( IVisObj *pMember : state.previewMembers )
	{
		if ( IAnimation *pAnimation = static_cast<IObjVisObj *>( pMember )->GetAnimation() )
			pAnimation->SetAnimation( 0 );
		pMember->Update( time );
	}
	if ( state.pPreviewObj == nullptr )
		return;
	if ( state.bPreviewEffect )
		static_cast<IEffectVisObj *>( state.pPreviewObj )->SetStartTime( time );
	else if ( IAnimation *pAnimation = static_cast<IObjVisObj *>( state.pPreviewObj )->GetAnimation() )
		pAnimation->SetAnimation( 0 );
	state.pPreviewObj->Update( time );
}

void DropLocatorSprites( ResourceState &state )
{
	IScene *pScene = GetSingleton<IScene>();
	for ( IVisObj *pSprite : state.locatorSprites )
	{
		if ( pScene )
			pScene->RemoveObject( pSprite );
		pSprite->Release();
	}
	state.locatorSprites.clear();
}

void DropPreviewObject( ResourceState &state )
{
	IScene *pScene = GetSingleton<IScene>();
	DropLocatorSprites( state );
	for ( IVisObj *pMember : state.previewMembers )
	{
		if ( pScene )
			pScene->RemoveObject( pMember );
		pMember->Release();
	}
	state.previewMembers.clear();
	if ( state.pPreviewObj == nullptr )
		return;
	if ( pScene )
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

// SCENE_SHOW_BBS as CMeshFrame::UpdateLocatorVisibility drove it: ToggleShow
// answers the state it switched to, so it is toggled until it is the wanted one.
void SetMeshBoxes( ResourceState &state, bool bShow )
{
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == nullptr )
		return;
	for ( int nTry = 0; nTry < 2; ++nTry )
	{
		const bool bNow = pScene->ToggleShow( SCENE_SHOW_BBS );
		if ( bNow == bShow )
			break;
	}
	state.bMeshBoxes = bShow;
}

// The mesh the unit preview draws and its animation, null when none shows.
IMeshVisObj *PreviewMesh( ResourceState &state )
{
	if ( !state.bPreview || state.szMeshDir.empty() || state.pPreviewObj == nullptr )
		return nullptr;
	return static_cast<IMeshVisObj *>( static_cast<IObjVisObj *>( state.pPreviewObj ) );
}

struct SMeshNode
{
	std::string szName;
	CVec3 vPos;
};

// The skeleton nodes of the drawn model and where they sit now: the object is
// updated (forced, so a new direction takes) and its matrices read, which is
// what CMeshFrame::UpdateLocators did with the animation's matrices.
bool ReadMeshNodes( ResourceState &state, std::vector<SMeshNode> &nodes )
{
	IMeshVisObj *pMesh = PreviewMesh( state );
	if ( pMesh == nullptr )
		return false;
	IMeshAnimation *pAnim = static_cast<IMeshAnimation *>( pMesh->GetAnimation() );
	IMeshAnimationEdit *pAnimEdit = pAnim ? dynamic_cast<IMeshAnimationEdit *>( pAnim ) : nullptr;
	if ( pAnimEdit == nullptr )
		return false;
	pMesh->Update( UpdateGameTimer(), true );
	const int nNodes = pAnim->GetNumNodes();
	nodes.clear();
	if ( nNodes <= 0 )
		return true;
	std::vector<const char *> names( nNodes );
	pAnimEdit->GetAllNodeNames( &names[0], nNodes );
	const SHMatrix *pMatrices = pMesh->GetMatrices();
	if ( pMatrices == nullptr )
		return false;
	for ( int i = 0; i < nNodes; ++i )
		nodes.push_back( { names[i], pMatrices[i].GetTrans3() } );
	return true;
}

// MFC's locator sprites (editor\\locator\\1, SGVOGT_OBJECT) at the nodes of the
// drawn model, half transparent; none when the display is off.
bool UpdateLocatorSprites( ResourceState &state, std::string &szError )
{
	if ( !state.bMeshLocators )
	{
		DropLocatorSprites( state );
		return true;
	}
	std::vector<SMeshNode> nodes;
	if ( !ReadMeshNodes( state, nodes ) )
	{
		DropLocatorSprites( state );
		return true;
	}
	IScene *pScene = GetSingleton<IScene>();
	if ( state.locatorSprites.size() != nodes.size() )
	{
		DropLocatorSprites( state );
		for ( std::size_t i = 0; i < nodes.size(); ++i )
		{
			IVisObj *pSprite = GetSingleton<IVisObjBuilder>()->BuildObject( "editor\\locator\\1", 0, SGVOT_SPRITE );
			if ( pSprite == nullptr )
			{
				szError = "IVisObjBuilder would not build the locator sprite editor\\locator\\1";
				DropLocatorSprites( state );
				return false;
			}
			pSprite->AddRef();
			pScene->AddObject( pSprite, SGVOGT_OBJECT );
			state.locatorSprites.push_back( pSprite );
		}
	}
	const NTimer::STime time = UpdateGameTimer();
	for ( std::size_t i = 0; i < nodes.size(); ++i )
	{
		IVisObj *pSprite = state.locatorSprites[i];
		pSprite->SetPosition( nodes[i].vPos );
		pSprite->SetDirection( 0 );
		pScene->MoveObject( pSprite, nodes[i].vPos );
		pSprite->SetOpacity( 120 );
		pSprite->Update( time, true );
	}
	return true;
}

void StopPreview( BkEditorSession *pSession, ResourceState &state )
{
	if ( !state.bPreview )
		return;
	DropPreviewObject( state );
	if ( state.bPreviewTerrain )
	{
		// The terrain was the preview: the scene must not keep drawing it, and
		// the wire frame is the road and river frames' own switch.
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			pScene->SetTerrain( nullptr );
			pScene->Clear();
		}
		state.bPreviewTerrain = false;
		pSession->nLayerBits &= ~( 1u << BK_EDITOR_LAYER_WIREFRAME );
		if ( IGFX *pGFX = GetSingleton<IGFX>() )
			pGFX->SetWireframe( false );
	}
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
	state.szMeshDir.clear();
	state.nMeshVariant = 0;
	state.nMeshDirection = 0;
	state.bMeshLocators = false;
	state.bMeshBoxes = false;
	// The scene is cleared by the next Begin, but the box display is the
	// scene's own switch and would outlive this preview.
	SetMeshBoxes( state, false );
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

BkEditorStatus BeginTerrainPreview( BkEditorSession *pSession, int nKind );
BkEditorStatus ShowTerrainPreview( BkEditorSession *pSession );

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
	if ( nKind == kTilesetKind )
	{
		pSession->szMessage = "tileset has no scene preview; the thumbnail list is its preview (CTileSetFrame hides its GameWnd)";
		return BK_EDITOR_REFUSED;
	}
	if ( nKind == kRoadKind || nKind == kRiverKind )
		return BeginTerrainPreview( pSession, nKind );
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

// CSquadFrame::OnFormationChanged (SquadFrm.cpp:344-364) for the preview:
// the first formation's soldiers as sprites, each member named by a path under
// units\\humans or by an objects database key, placed about the camera's
// anchor as the formation lies about its zero point and turned by the slot's
// direction plus the formation's. The soldiers are paired with the members in
// order, as CSquadTreeRootItem::CallMeAfterSerialize pairs them. Returns the
// number of sprites built; a member that does not build is reported, not
// skipped silently.
int BuildSquadMembers( ResourceState &state, const NResourceModel::Project &project, const CVec3 &vAnchor, std::string &szError )
{
	const NResourceModel::CTreeItem *pMembers = nullptr, *pFormations = nullptr;
	if ( project.root )
		for ( const auto &pChild : project.root->GetChildren() )
		{
			if ( pChild->GetItemType() == NResourceModel::ETIT_SQUAD_MEMBERS_ITEM )
				pMembers = pChild.get();
			else if ( pChild->GetItemType() == NResourceModel::ETIT_SQUAD_FORMATIONS_ITEM )
				pFormations = pChild.get();
		}
	if ( pMembers == nullptr || pFormations == nullptr )
	{
		szError = "the squad project has no members or formations item";
		return -1;
	}
	const NResourceModel::CSquadFormationPropsItem *pFormation = nullptr;
	for ( const auto &pChild : pFormations->GetChildren() )
		if ( ( pFormation = dynamic_cast<const NResourceModel::CSquadFormationPropsItem *>( pChild.get() ) ) != nullptr )
			break;
	if ( pFormation == nullptr )
	{
		szError = "the squad has no formation to preview";
		return -1;
	}
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pScene = GetSingleton<IScene>();
	const auto &members = pMembers->GetChildren();
	std::size_t nMember = 0;
	for ( const NResourceModel::CSquadFormationPropsItem::SUnit &unit : pFormation->units )
	{
		if ( nMember >= members.size() )
			break;
		const std::string szItemName = members[nMember++]->GetDisplayName();
		std::string szSprite;
		if ( szItemName.find( '\\' ) != std::string::npos )
			szSprite = "units\\humans\\" + szItemName + "\\1";
		else if ( const IObjectsDB *pDB = GetSingleton<IObjectsDB>() )
		{
			const SGDBObjectDesc *pDesc = pDB->GetDesc( szItemName.c_str() );
			if ( pDesc == nullptr )
			{
				szError = "squad member \"" + szItemName + "\" is not a unit of the objects database";
				return -1;
			}
			szSprite = pDesc->szPath + "\\1";
		}
		IVisObj *pSprite = szSprite.empty() ? nullptr : pVOB->BuildObject( szSprite.c_str(), 0, SGVOT_SPRITE );
		if ( pSprite == nullptr )
		{
			szError = "IVisObjBuilder would not build the sprite \"" + szSprite + "\" of squad member \"" + szItemName + "\"";
			return -1;
		}
		pSprite->AddRef();
		float fAngle = unit.fDir + pFormation->fFormationDir;
		fAngle -= 2.0f * FP_PI * std::floor( fAngle / ( 2.0f * FP_PI ) );
		pSprite->SetPlacement( CVec3( vAnchor.x + unit.vPos.x - pFormation->vZeroPos.x,
		                              vAnchor.y + unit.vPos.y - pFormation->vZeroPos.y, 0.0f ),
		                       int( fAngle / ( 2.0f * FP_PI ) * 65536.0f ) & 0xffff );
		pScene->AddObject( pSprite, SGVOGT_UNIT );
		state.previewMembers.push_back( pSprite );
	}
	return int( state.previewMembers.size() );
}

const int kUnitPreviewKind = 6; // "msh": the kind whose preview has model variants and locators

// The engine's 16-bit direction of an angle in degrees, as IVisObj turns it.
int DirectionOfDegrees( int nDegrees )
{
	nDegrees %= 360;
	if ( nDegrees < 0 )
		nDegrees += 360;
	return ( nDegrees * 65536 / 360 ) & 0xffff;
}

// A model name of Graphics Info as the export folder names its file: the
// name without folder and extension, lower case as the engine folds it.
std::string ModelStem( const std::string &szName )
{
	std::string szStem = szName;
	const std::size_t nSlash = szStem.find_last_of( "\\/" );
	if ( nSlash != std::string::npos )
		szStem.erase( 0, nSlash + 1 );
	const std::size_t nDot = szStem.find_last_of( '.' );
	if ( nDot != std::string::npos )
		szStem.erase( nDot );
	for ( char &c : szStem )
		c = char( std::tolower( (unsigned char)c ) );
	return szStem;
}

// Takes the export's folder and the three model names for the variants.
void NoteUnitPreview( ResourceState &state, const NResourceModel::Project &project, const std::string &szObjectName )
{
	const std::size_t nSlash = szObjectName.find_last_of( '\\' );
	state.szMeshDir = nSlash == std::string::npos ? std::string() : szObjectName.substr( 0, nSlash + 1 );
	for ( std::string &szModel : state.meshModels )
		szModel.clear();
	if ( project.root )
		for ( const auto &pChild : project.root->GetChildren() )
			if ( pChild->GetItemType() == NResourceModel::ETIT_MESH_GRAPHICS_ITEM )
				for ( int i = 0; i < 3; ++i )
					state.meshModels[i] = ModelStem( NResourceModel::NStatsExport::ValueStr( *pChild, i ) );
	state.nMeshVariant = 0;
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
	if ( state.bPreviewTerrain )
		return ShowTerrainPreview( pSession );
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
	FillEngineLookups( context, state.previewRoot / "mesh" );
	NResourceModel::SExportOutcome outcome;
	if ( !pfnExporter( project, context, outcome ) )
	{
		pSession->szMessage = "the preview export failed: " + ( outcome.szError.empty() ? std::string( "the exporter failed" ) : outcome.szError );
		return BK_EDITOR_FAILED;
	}
	const PreviewKind *pKind = FindPreviewKind( state.nPreviewKind );
	if ( pKind->bSquad )
	{
		// The export wrote the stats and the icon; the soldiers' sprites are
		// the shipped ones, so there is nothing to mount.
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->Update();
		const int nBuilt = BuildSquadMembers( state, project, pCamera->GetAnchor(), pSession->szMessage );
		if ( nBuilt < 0 )
		{
			DropPreviewObject( state );
			return BK_EDITOR_FAILED;
		}
		RestartPreviewObject( state );
		pSession->szMessage = "built " + std::to_string( nBuilt ) + " member sprites of the first formation";
		return BK_EDITOR_OK;
	}
	if ( outcome.szObjectName.empty() )
	{
		pSession->szMessage = "the ." + szExtension + " export named no visual to build";
		return BK_EDITOR_FAILED;
	}
	if ( state.nPreviewKind == kUnitPreviewKind )
		NoteUnitPreview( state, project, outcome.szObjectName );
	std::string szBuildName = outcome.szObjectName;
	if ( pKind->bParticleSource )
	{
		// The export wrote the source; the preview builds its wrapper effect.
		szBuildName = "editor\\preview\\particle_source_effect";
		if ( !WrapParticleSource( project, dataDir, outcome.szObjectName, szBuildName, pSession->szMessage ) )
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
	IVisObj *pObj = GetSingleton<IVisObjBuilder>()->BuildObject( szBuildName.c_str(), 0, pKind->eVisType );
	if ( pObj == nullptr )
	{
		pSession->szMessage = "IVisObjBuilder would not build \"" + szBuildName + "\" from the export";
		return BK_EDITOR_FAILED;
	}
	pObj->AddRef();
	state.pPreviewObj = pObj;
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->Update();
	const CVec3 vAnchor = pCamera->GetAnchor();
	pObj->SetPlacement( CVec3( vAnchor.x, vAnchor.y, 0.0f ), state.szMeshDir.empty() ? 0 : DirectionOfDegrees( state.nMeshDirection ) );
	RestartPreviewObject( state );
	GetSingleton<IScene>()->AddObject( pObj, pKind->eGameType );
	if ( !state.szMeshDir.empty() )
	{
		std::string szWhy;
		if ( !UpdateLocatorSprites( state, szWhy ) )
		{
			pSession->szMessage = szWhy;
			return BK_EDITOR_FAILED;
		}
	}
	pSession->szMessage = "built \"" + szBuildName + "\" from " + std::to_string( outcome.nWritten ) + " exported files";
	return BK_EDITOR_OK;
}

// The mesh of the unit preview, or a refusal naming what is missing.
IMeshVisObj *RequireUnitPreview( BkEditorSession *pSession, ResourceState &state )
{
	IMeshVisObj *pMesh = PreviewMesh( state );
	if ( pMesh == nullptr )
		pSession->szMessage = "no unit is previewed: call BkResPreviewBegin for a .msh project and BkResPreviewShow first";
	return pMesh;
}

// MFC's SetCombatMesh, SetInstallMesh and SetTransportableMesh: the new model
// is built before the drawn one goes, so a variant that is not there leaves
// the preview as it was.
BkEditorStatus ShowMeshVariant( BkEditorSession *pSession, int nVariant )
{
	ResourceState &state = StateOf( pSession );
	if ( RequireUnitPreview( pSession, state ) == nullptr )
		return BK_EDITOR_REFUSED;
	if ( nVariant < 0 || nVariant > 2 )
	{
		pSession->szMessage = "the model variant is 0 (combat), 1 (install) or 2 (transportable)";
		return BK_EDITOR_REFUSED;
	}
	static const char *const kVariantNames[3] = { "combat", "install", "transportable" };
	const std::string szModel = nVariant == 0 ? std::string( "1" ) : state.meshModels[nVariant];
	if ( szModel.empty() )
	{
		pSession->szMessage = std::string( "the project names no " ) + kVariantNames[nVariant] + " model in Graphics Info";
		return BK_EDITOR_REFUSED;
	}
	const std::string szName = state.szMeshDir + szModel;
	// The install and transportable models wear the combat model's texture.
	const std::string szTexture = state.szMeshDir + "1";
	IVisObj *pObj = GetSingleton<IVisObjBuilder>()->BuildObject( szName.c_str(), nVariant == 0 ? nullptr : szTexture.c_str(), SGVOT_MESH );
	if ( pObj == nullptr )
	{
		pSession->szMessage = std::string( "the " ) + kVariantNames[nVariant] + " model " + szName
			+ ".mod is not in the preview export (a model outside the combat model's folder is not exported)";
		return BK_EDITOR_REFUSED;
	}
	pObj->AddRef();
	DropPreviewObject( state );
	state.pPreviewObj = pObj;
	state.nMeshVariant = nVariant;
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->Update();
	const CVec3 vAnchor = pCamera->GetAnchor();
	pObj->SetPlacement( CVec3( vAnchor.x, vAnchor.y, 0.0f ), DirectionOfDegrees( state.nMeshDirection ) );
	RestartPreviewObject( state );
	GetSingleton<IScene>()->AddObject( pObj, SGVOGT_UNIT );
	std::string szWhy;
	if ( !UpdateLocatorSprites( state, szWhy ) )
	{
		pSession->szMessage = szWhy;
		return BK_EDITOR_FAILED;
	}
	pSession->szMessage = std::string( "showing the " ) + kVariantNames[nVariant] + " model " + szName;
	return BK_EDITOR_OK;
}

} // namespace
} // extern "C++"

BkEditorStatus BkResPreviewMeshVariant( BkResSession *pSession, int nVariant )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return ShowMeshVariant( pSession, nVariant ); } );
}

BkEditorStatus BkResPreviewDirection( BkResSession *pSession, int nAngle )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		IMeshVisObj *pMesh = RequireUnitPreview( pSession, state );
		if ( pMesh == nullptr )
			return BK_EDITOR_REFUSED;
		state.nMeshDirection = ( ( nAngle % 360 ) + 360 ) % 360;
		state.pPreviewObj->SetDirection( DirectionOfDegrees( state.nMeshDirection ) );
		std::string szWhy;
		if ( !UpdateLocatorSprites( state, szWhy ) )
		{
			pSession->szMessage = szWhy;
			return BK_EDITOR_FAILED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPreviewShowLocators( BkResSession *pSession, int nLocators, int nBoundingBoxes )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( RequireUnitPreview( pSession, state ) == nullptr )
			return BK_EDITOR_REFUSED;
		state.bMeshLocators = nLocators != 0;
		SetMeshBoxes( state, nBoundingBoxes != 0 );
		std::string szWhy;
		if ( !UpdateLocatorSprites( state, szWhy ) )
		{
			pSession->szMessage = szWhy;
			return BK_EDITOR_FAILED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResMeshLocators( BkResSession *pSession, BkResLocator *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != nullptr )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == nullptr || ( pOut == nullptr && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		ResourceState &state = StateOf( pSession );
		if ( RequireUnitPreview( pSession, state ) == nullptr )
			return BK_EDITOR_REFUSED;
		std::vector<SMeshNode> nodes;
		if ( !ReadMeshNodes( state, nodes ) )
		{
			pSession->szMessage = "the previewed model has no skeleton to read";
			return BK_EDITOR_FAILED;
		}
		GetSingleton<ICamera>()->Update();
		IScene *pScene = GetSingleton<IScene>();
		for ( int i = 0; i < int( nodes.size() ) && i < nCapacity; ++i )
		{
			BkResLocator &out = pOut[i];
			std::memset( &out, 0, sizeof out );
			out.node_id = i;
			std::snprintf( out.name, sizeof out.name, "%s", nodes[i].szName.c_str() );
			out.wx = nodes[i].vPos.x;
			out.wy = nodes[i].vPos.y;
			out.wz = nodes[i].vPos.z;
			CVec2 vScreen( 0, 0 );
			pScene->GetPos2( &vScreen, nodes[i].vPos );
			out.sx = vScreen.x;
			out.sy = vScreen.y;
		}
		*pnCount = int( nodes.size() );
		return BK_EDITOR_OK;
	} );
}

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
		if ( state.bPreview && state.bPreviewTerrain )
		{
			// Only the river moves: C3DRiverFrame's GFXDraw advanced the game
			// timer on every draw, and the road frame has no Run button.
			if ( state.nPreviewKind == kRoadKind )
			{
				pSession->szMessage = "a 3D road has no Run in MFC (C3DRoadFrame draws a still terrain); only the river plays";
				return BK_EDITOR_REFUSED;
			}
			state.bPreviewRunning = nRun != 0;
			pSession->pfnBeforeDraw = state.bPreviewRunning ? &PreviewBeforeDraw : nullptr;
			return BK_EDITOR_OK;
		}
		if ( !state.bPreview || ( state.pPreviewObj == nullptr && state.previewMembers.empty() ) )
		{
			pSession->szMessage = "no preview object: call BkResPreviewBegin and BkResPreviewShow first";
			return BK_EDITOR_REFUSED;
		}
		// MFC's Run restarted the object; Stop left it where it was.
		if ( nRun != 0 && !state.bPreviewRunning )
			RestartPreviewObject( state );
		state.bPreviewRunning = nRun != 0;
		pSession->pfnBeforeDraw = state.bPreviewRunning ? &PreviewBeforeDraw : nullptr;
		ApplyEffectDirection( state );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResPreviewWireframe( BkResSession *pSession, int nOn )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bPreview || !state.bPreviewTerrain )
		{
			pSession->szMessage = "the wire frame belongs to the road and river previews: call BkResPreviewBegin for a .3rd or .3rv project first";
			return BK_EDITOR_REFUSED;
		}
		// The wire frame is a render state said inside every frame by
		// ApplyWireframeForFrame, which reads the session's layer bit.
		if ( nOn != 0 )
			pSession->nLayerBits |= ( 1u << BK_EDITOR_LAYER_WIREFRAME );
		else
			pSession->nLayerBits &= ~( 1u << BK_EDITOR_LAYER_WIREFRAME );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResEffectSetDirection( BkResSession *pSession, float fAngle )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || state.nKindOrdinal != 12 )
		{
			pSession->szMessage = "the direction dock belongs to an open .eff project";
			return BK_EDITOR_REFUSED;
		}
		if ( !std::isfinite( fAngle ) )
		{
			pSession->szMessage = "the direction angle is not a number";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		state.fEffectAngle = fAngle;
		ApplyEffectDirection( state );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResEffectGetDirection( BkResSession *pSession, float *pAngle )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pAngle == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		*pAngle = StateOf( pSession ).fEffectAngle;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResEffectDirectionMatrix( float fAngle, float *pMatrix )
{
	if ( pMatrix == nullptr )
		return BK_EDITOR_BAD_ARGUMENT;
	SHMatrix matrix;
	EffectDirectionOf( fAngle, matrix );
	std::memcpy( pMatrix, matrix.m, sizeof( float ) * 16 );
	return BK_EDITOR_OK;
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

BkEditorStatus BkResPreviewCameraMode( BkResSession *pSession, int nHorizontal )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ResourceState &state = StateOf( pSession );
		if ( !state.bPreview )
		{
			pSession->szMessage = "no preview: call BkResPreviewBegin first";
			return BK_EDITOR_REFUSED;
		}
		ICamera *pCamera = GetSingleton<ICamera>();
		if ( pCamera == nullptr )
		{
			pSession->szMessage = "there is no camera";
			return BK_EDITOR_FAILED;
		}
		pCamera->Update();
		const CVec3 vAnchor = pCamera->GetAnchor();
		if ( nHorizontal != 0 )
		{
			// SetHorizontalCamera's angles, over the anchor and distance the
			// preview camera has (MFC's placed it at the origin; the preview
			// scene is built about its own origin).
			IGFX *pGFX = GetSingleton<IGFX>();
			const RECT rcScreen = pGFX != nullptr ? pGFX->GetScreenRect() : RECT();
			const float fHeight = float( rcScreen.bottom - rcScreen.top );
			pCamera->SetPlacement( vAnchor, 1024 * 4 + fHeight, -ToRadian( 90.0f ), ToRadian( 45.0f ) );
			pCamera->Update();
		}
		else if ( !SetSessionCamera( pSession, vAnchor.x, vAnchor.y ) )
			return BK_EDITOR_FAILED;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResGetParticleInfo( BkResSession *pSession, BkResParticleInfo *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		std::memset( pOut, 0, sizeof *pOut );
		ResourceState &state = StateOf( pSession );
		const int nParticleKind = 11; // "pcp"
		if ( !state.bOpen || !state.pProject )
		{
			pSession->szMessage = "no project is open";
			return BK_EDITOR_REFUSED;
		}
		if ( state.nKindOrdinal != nParticleKind )
		{
			pSession->szMessage = std::string( "particle info is a .pcp feature; the open project is a ." ) + kKindExtensions[state.nKindOrdinal];
			return BK_EDITOR_REFUSED;
		}
		if ( !state.bPreview || state.nPreviewKind != nParticleKind )
		{
			const BkEditorStatus nBegin = BeginPreview( pSession, nParticleKind );
			if ( nBegin != BK_EDITOR_OK )
				return nBegin;
		}
		const BkEditorStatus nShow = ShowPreview( pSession );
		if ( nShow != BK_EDITOR_OK )
		{
			pSession->szMessage = "particle info needs the built effect: " + pSession->szMessage;
			return nShow;
		}
		IEffectVisObj *pEffect = static_cast<IEffectVisObj *>( state.pPreviewObj );
		IParticleSource **ppSources = nullptr;
		int nSources = 0;
		pEffect->GetParticleEffects( &ppSources, &nSources, true );
		if ( nSources <= 0 || ppSources == nullptr )
		{
			pSession->szMessage = "the built effect has no particle source (the exported source did not load)";
			return BK_EDITOR_FAILED;
		}
		IParticleSourceWithInfo *pWithInfo = dynamic_cast<IParticleSourceWithInfo *>( ppSources[nSources - 1] );
		if ( pWithInfo == nullptr )
		{
			pSession->szMessage = "the built particle source carries no info";
			return BK_EDITOR_FAILED;
		}
		SParticleSourceInfo info;
		std::memset( &info, 0, sizeof info );
		pWithInfo->GetInfo( info );
		pOut->max_count = info.fMaxCount;
		pOut->max_size = info.fMaxSize;
		pOut->average_size = info.fAverageSize;
		pOut->average_count = info.fAverageCount;
		char szText[160];
		std::snprintf( szText, sizeof szText, "Max particles %g, Size %g, Average size %g, Average count %g",
			info.fMaxCount, info.fMaxSize, info.fAverageSize, info.fAverageCount );
		pSession->szMessage = szText;
		return BK_EDITOR_OK;
	} );
}

// The complex source item of the open .pcp, or null with the refusal's reason
// in the session message: the particle source toggle's shared precondition.
static NResourceModel::CTreeItem *ParticleComplexSourceItem( BkResSession *pSession )
{
	ResourceState &state = StateOf( pSession );
	const int nParticleKind = 11; // "pcp"
	if ( !state.bOpen || !state.pProject || !state.pProject->root )
	{
		pSession->szMessage = "no project is open";
		return nullptr;
	}
	if ( state.nKindOrdinal != nParticleKind )
	{
		pSession->szMessage = std::string( "the particle source mode is a .pcp feature; the open project is a ." ) + kKindExtensions[state.nKindOrdinal];
		return nullptr;
	}
	NResourceModel::CTreeItem *pItem = nullptr;
	for ( auto &pChild : state.pProject->root->MutableChildren() )
		if ( pChild->GetItemType() == NResourceModel::ETIT_PARTICLE_COMPLEX_SOURCE_ITEM )
			pItem = pChild.get();
	if ( pItem == nullptr || pItem->MutableValues().empty() )
	{
		pSession->szMessage = "the .pcp project has no complex source item";
		return nullptr;
	}
	return pItem;
}

BkEditorStatus BkResParticleSourceMode( BkResSession *pSession, int *pnComplex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnComplex == nullptr )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnComplex = 0;
		ResourceState &state = StateOf( pSession );
		if ( ParticleComplexSourceItem( pSession ) == nullptr )
			return BK_EDITOR_REFUSED;
		*pnComplex = NResourceModel::IsComplexSource( *state.pProject->root ) ? 1 : 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResParticleSetSourceMode( BkResSession *pSession, int nComplex, const char *pszName )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		NResourceModel::CTreeItem *pItem = ParticleComplexSourceItem( pSession );
		if ( pItem == nullptr )
			return BK_EDITOR_REFUSED;
		std::string szName;
		if ( nComplex != 0 )
		{
			if ( pszName != nullptr )
				szName = pszName;
			if ( szName.empty() )
			{
				pSession->szMessage = "a complex source needs the name of the particle it scatters (the complex source's Particle reference is empty)";
				return BK_EDITOR_REFUSED;
			}
		}
		// The mode is the value, so writing it is the whole switch: the
		// exporter and BkResParticleSourceMode read it back from the same place.
		auto &first = pItem->MutableValues().front();
		first.value = NResourceModel::CVariant::FromString( first.value.GetKind(), szName );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResGetKeyframeKnobs( BkResSession *pSession, int nNodeId, BkResKeyframeKnobs *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == nullptr )
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
		const auto *pKeys = dynamic_cast<const NResourceModel::CKeyFrameTreeItem *>( itItem->second );
		if ( pKeys == nullptr )
		{
			pSession->szMessage = "the node is not a key-frame curve";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		pOut->min_x = pKeys->fMinValX;
		pOut->max_x = pKeys->fMaxValX;
		pOut->min_y = pKeys->fMinValY;
		pOut->max_y = pKeys->fMaxValY;
		pOut->step_x = pKeys->fStepX;
		pOut->step_y = pKeys->fStepY;
		pOut->resize_mode = pKeys->bResizeMode ? 1 : 0;
		return BK_EDITOR_OK;
	} );
}

/* ---- Import ----------------------------------------------------------- */

extern "C++" {
namespace {

const int kInfantryKind = 5; // "unt", kKindTable's Unit_Composer_Project
const int kMeshKind = 6;     // "msh", the Unit (mesh) project
const int kObjectKind = 7;   // "obt", the Object project
const int kFenceKind = 8;    // "fnc", the Fence project
const int kBuildingKind = 9; // "bld", the Building project
const int kBridgeKind = 10;  // "bdg", the Bridge project
const int kParticleKind = 11; // "pcp", the Particle project
const int kTileSetKind = 13;  // "til", the TileSet project

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

// The last component of a runtime stats name: MFC's FillRPGStats prefixed the
// folder the stats are exported to, which the project keeps out of its fields.
std::string LastPathComponent( const std::string &szName )
{
	const std::string::size_type nCut = szName.find_last_of( "\\/" );
	return nCut == std::string::npos ? szName : szName.substr( nCut + 1 );
}

// CMedalFrame::GetRPGStats (MedalFrm.cpp:86). MFC's body is commented out, so
// a medal opened from the game left the default tree. The port fills the three
// fields FillRPGStats wrote: name, description and picture, each without the
// folder prefix the export put in front of it.
void MedalStatsToTree( const SMedalStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_MEDAL_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, LastPathComponent( rpgStats.szHeaderText ) );
	SetSlot( pCommonProps, 1, LastPathComponent( rpgStats.szDescriptionText ) );
	SetSlot( pCommonProps, 2, LastPathComponent( rpgStats.szTexture ) );
}

// CMedalFrame::LoadRPGStats (MedalFrm.cpp:99): the stats file's place below
// the kind's folder (szAddDir "medals\\"), which becomes the project's
// own_data/export_file_name, as MFC kept it in szPrevExportFileName. Empty
// when the file does not lie below a medals folder; the export then names the
// stats after the project's own folder.
std::string MedalExportFileName( const std::filesystem::path &statsFile )
{
	std::vector<std::string> parts;
	for ( const auto &part : statsFile.lexically_normal() )
		parts.push_back( part.string() );
	for ( std::size_t i = 0; i + 1 < parts.size(); ++i )
	{
		std::string szLower = parts[i];
		for ( char &c : szLower )
			c = char( std::tolower( (unsigned char)c ) );
		if ( szLower != "medals" )
			continue;
		std::string szRelative;
		for ( std::size_t j = i + 1; j < parts.size(); ++j )
			szRelative += ( j > i + 1 ? "\\" : "" ) + parts[j];
		return szRelative;
	}
	return std::string();
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

// CWeaponFrame::GetRPGStats (WeaponFrm.cpp:184), onto the port's items. The
// slots are the property order of weapon.cpp, which is the index the
// exporter reads back. MFC asserted that the tree had a shoot type for every
// shell, which a new project does not (its Shoot types is empty): one is
// made per shell, as the frame's insert handler makes them, named "Shell".
void WeaponStatsToTree( const SWeaponRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_WEAPON_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	SetSlot( pCommonProps, 1, int( rpgStats.wDeltaAngle ) );
	SetSlot( pCommonProps, 2, rpgStats.nAmmoPerBurst );
	SetSlot( pCommonProps, 3, rpgStats.fAimingTime );
	SetSlot( pCommonProps, 4, rpgStats.fDispersion );
	SetSlot( pCommonProps, 5, rpgStats.fRangeMin );
	SetSlot( pCommonProps, 6, rpgStats.fRangeMax );
	SetSlot( pCommonProps, 7, rpgStats.nCeiling );
	SetSlot( pCommonProps, 8, rpgStats.fRevealRadius );

	NResourceModel::CTreeItem *pShootTypes = ChildOfType( root, NResourceModel::ETIT_WEAPON_SHOOT_TYPES_ITEM );
	if ( pShootTypes == nullptr )
		return;
	auto &factory = NResourceModel::CTreeItemFactory::Instance();
	for ( const SWeaponRPGStats::SShell &damage : rpgStats.shells )
	{
		auto pDamage = factory.Create( NResourceModel::ETIT_WEAPON_DAMAGE_PROPS_ITEM );
		if ( !pDamage )
			continue;
		pDamage->SetItemName( "Shell" );
		NResourceModel::CTreeItem *pItem = pDamage.get();
		pShootTypes->AddChild( std::move( pDamage ) );

		const char *pszTrajectory = "line";
		switch ( damage.trajectory )
		{
			case SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER: pszTrajectory = "howitzer"; break;
			case SWeaponRPGStats::SShell::TRAJECTORY_BOMB:     pszTrajectory = "bomb";     break;
			case SWeaponRPGStats::SShell::TRAJECTORY_CANNON:   pszTrajectory = "cannon";   break;
			case SWeaponRPGStats::SShell::TRAJECTORY_ROCKET:   pszTrajectory = "rocket";   break;
			case SWeaponRPGStats::SShell::TRAJECTORY_GRENADE:  pszTrajectory = "grenade";  break;
			default: break;
		}
		const char *pszDamageType = "damage";
		if ( damage.eDamageType == SWeaponRPGStats::SShell::DAMAGE_MORALE )
			pszDamageType = "morale";
		else if ( damage.eDamageType == SWeaponRPGStats::SShell::DAMAGE_FOG )
			pszDamageType = "smoke";
		SetSlot( pItem, 0, std::string( pszTrajectory ) );
		SetSlot( pItem, 1, damage.nPiercing );
		SetSlot( pItem, 2, damage.nPiercingRandom );
		SetSlot( pItem, 3, int( damage.fDamagePower ) );
		SetSlot( pItem, 4, damage.nDamageRandom );
		SetSlot( pItem, 5, damage.fArea );
		SetSlot( pItem, 6, damage.fArea2 );
		SetSlot( pItem, 7, damage.fSpeed );
		SetSlot( pItem, 8, bool( damage.specials.GetData( 0 ) ) );
		SetSlot( pItem, 9, damage.fDetonationPower );
		SetSlot( pItem, 10, damage.fFireRate );
		SetSlot( pItem, 11, damage.fRelaxTime );
		SetSlot( pItem, 12, std::string( pszDamageType ) );
		// SetTraceProbability: the percentage the tree shows.
		SetSlot( pItem, 13, damage.fTraceProbability * 100.0f );
		SetSlot( pItem, 14, damage.fTraceSpeedCoeff );
		SetSlot( pItem, 15, damage.fBrokeTrackProbability );

		if ( NResourceModel::CTreeItem *pCraters = ChildOfType( *pItem, NResourceModel::ETIT_WEAPON_CRATERS_ITEM ) )
			for ( const std::string &szCrater : damage.szCraters )
			{
				auto pCrater = factory.Create( NResourceModel::ETIT_WEAPON_CRATER_PROPS_ITEM );
				if ( !pCrater )
					continue;
				pCrater->SetItemName( szCrater );
				NResourceModel::CTreeItem *pCraterItem = pCrater.get();
				pCraters->AddChild( std::move( pCrater ) );
				SetSlot( pCraterItem, 0, szCrater );
			}

		if ( NResourceModel::CTreeItem *pEffects = ChildOfType( *pItem, NResourceModel::ETIT_WEAPON_EFFECTS_ITEM ) )
		{
			SetSlot( pEffects, 0, damage.szFireSound );
			SetSlot( pEffects, 1, damage.szEffectGunFire );
			SetSlot( pEffects, 2, damage.szEffectTrajectory );
			SetSlot( pEffects, 3, damage.szEffectHitDirect );
			SetSlot( pEffects, 4, damage.szEffectHitMiss );
			SetSlot( pEffects, 5, damage.szEffectHitReflect );
			SetSlot( pEffects, 6, damage.szEffectHitGround );
			SetSlot( pEffects, 7, damage.szEffectHitWater );
			SetSlot( pEffects, 8, damage.szEffectHitAir );
		}

		// Flash fire, then flash explosion: the two children of that type.
		int nFlash = 0;
		for ( const auto &pChild : pItem->GetChildren() )
			if ( pChild->GetItemType() == NResourceModel::ETIT_WEAPON_FLASH_PROPS_ITEM && nFlash < 2 )
			{
				const SFlashEffect &effect = nFlash == 0 ? damage.flashFire : damage.flashExplosion;
				SetSlot( pChild.get(), 0, effect.nPower );
				SetSlot( pChild.get(), 1, effect.nDuration );
				++nFlash;
			}
	}
}

// CMineFrame::GetRPGStats (MineFrm.cpp:92): the name and the weight, the only
// two things the mine's tree holds. The weight is a whole number there.
void MineStatsToTree( const SMineRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_MINE_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	SetSlot( pCommonProps, 1, int( rpgStats.fWeight ) );
}

// CTrenchFrame::LoadRPGStats (TrenchFrm.cpp:363): the name, the health and
// each defence's armour. MFC's load did not read the segments back either: a
// segment is a model file the project points at, which a runtime stats file
// does not say, so an imported trench has none.
void TrenchStatsToTree( const SEntrenchmentRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_TRENCH_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	SetSlot( pCommonProps, 1, int( rpgStats.fMaxHP ) );
	NResourceModel::CTreeItem *pDefences = ChildOfType( root, NResourceModel::ETIT_TRENCH_DEFENCES_ITEM );
	if ( pDefences == nullptr )
		return;
	for ( const auto &pDefence : pDefences->GetChildren() )
	{
		const std::string &szName = pDefence->GetDisplayName();
		int nIndex = 0;
		if ( szName == "Left" )        nIndex = RPG_LEFT;
		else if ( szName == "Right" )  nIndex = RPG_RIGHT;
		else if ( szName == "Top" )    nIndex = RPG_TOP;
		else if ( szName == "Bottom" ) nIndex = RPG_BOTTOM;
		else if ( szName == "Front" )  nIndex = RPG_FRONT;
		else if ( szName == "Back" )   nIndex = RPG_BACK;
		SetSlot( pDefence.get(), 0, rpgStats.defences[nIndex].nArmorMin );
		SetSlot( pDefence.get(), 1, rpgStats.defences[nIndex].nArmorMax );
	}
}

// The squad's member as the tree names it: the path under units\humans\ the
// objects database knows the key by (what CSquadFrame::SaveRPGStats's
// MakeName searched in the other direction), or the key itself when the
// database has no such unit, which MakeName also takes as it stands.
std::string SquadMemberName( const std::string &szKey )
{
	if ( IObjectsDB *pObjDB = GetSingleton<IObjectsDB>() )
	{
		const int nNumDescs = pObjDB->GetNumDescs();
		const SGDBObjectDesc *pObjDescs = pObjDB->GetAllDescs();
		const std::string szPrefix = "units\\humans\\";
		for ( int i = 0; i < nNumDescs; i++ )
			if ( pObjDescs[i].eVisType == SGVOT_SPRITE && pObjDescs[i].eGameType == SGVOGT_UNIT && pObjDescs[i].szKey == szKey )
			{
				const std::string szPath = pObjDescs[i].szPath;
				if ( Fold( szPath ).compare( 0, szPrefix.size(), szPrefix ) == 0 )
					return szPath.substr( szPrefix.size() );
			}
	}
	return szKey;
}

// The inverse of CSquadFrame::SaveRPGStats (SquadFrm.cpp:216), which MFC
// never wrote: its LoadRPGStats is commented out. Members and each
// formation's soldiers come back by name, the formation's numbers into its
// slots. The export makes a slot's position relative to the formation's zero
// point plus the cross icon's shift on screen; the zero point is put where
// that sum is the world origin, so the stored relative position is the slot's
// position as it stands, bit for bit. The direction is stored in radians.
void SquadStatsToTree( const SSquadRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	static const char *const kTypes[] = { "riflemans", "infantry", "submachine gunners", "machine gunners", "AT team", "mortar team", "snipers", "gunners", "engineers" };
	static const char *const kFormations[] = { "default", "movement", "defensive", "offensive", "sneak" };
	static const char *const kLie[] = { "standart", "always stand", "always lie" };
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_SQUAD_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 1, rpgStats.szIcon );
	if ( rpgStats.type >= 0 && rpgStats.type < int( sizeof( kTypes ) / sizeof( kTypes[0] ) ) )
		SetSlot( pCommonProps, 2, std::string( kTypes[rpgStats.type] ) );

	auto &factory = NResourceModel::CTreeItemFactory::Instance();
	NResourceModel::CTreeItem *pMembers = ChildOfType( root, NResourceModel::ETIT_SQUAD_MEMBERS_ITEM );
	std::vector<NResourceModel::CTreeItem *> members;
	if ( pMembers != nullptr )
		for ( const std::string &szKey : rpgStats.memberNames )
		{
			auto pMember = factory.Create( NResourceModel::ETIT_SQUAD_MEMBER_PROPS_ITEM );
			if ( !pMember )
				continue;
			const std::string szName = SquadMemberName( szKey );
			pMember->SetItemName( szName );
			NResourceModel::CTreeItem *pItem = pMember.get();
			pMembers->AddChild( std::move( pMember ) );
			SetSlot( pItem, 0, szName );
			members.push_back( pItem );
		}

	NResourceModel::CTreeItem *pFormations = ChildOfType( root, NResourceModel::ETIT_SQUAD_FORMATIONS_ITEM );
	if ( pFormations == nullptr )
		return;
	const float fCos45 = std::cos( ToRadian( 45.0f ) );
	const float fSin30 = std::sin( ToRadian( 30.0f ) );
	// squad_export.cpp's ShiftedZero, as it computes the shift.
	const float fSum = 15.4f / fCos45;
	const float fDiff = 15.4f / ( fCos45 * fSin30 );
	const float fShiftX = ( fSum + fDiff ) / 2, fShiftY = ( fSum - fDiff ) / 2;
	for ( const SSquadRPGStats::SFormation &form : rpgStats.formations )
	{
		auto pFormation = factory.Create( NResourceModel::ETIT_SQUAD_FORMATION_PROPS_ITEM );
		if ( !pFormation )
			continue;
		const int nType = int( form.type );
		pFormation->SetItemName( nType >= 0 && nType < 5 ? kFormations[nType] : "default" );
		auto *pItem = static_cast<NResourceModel::CSquadFormationPropsItem *>( pFormation.get() );
		pFormations->AddChild( std::move( pFormation ) );
		SetSlot( pItem, 0, std::string( nType >= 0 && nType < 5 ? kFormations[nType] : "default" ) );
		SetSlot( pItem, 1, form.changesByEvent.empty() ? -1 : form.changesByEvent[0] );
		SetSlot( pItem, 2, std::string( form.cLieFlag < 3 ? kLie[form.cLieFlag] : kLie[0] ) );
		SetSlot( pItem, 3, form.fSpeedBonus );
		SetSlot( pItem, 4, form.fDispersionBonus );
		SetSlot( pItem, 5, form.fFireRateBonus );
		SetSlot( pItem, 6, form.fRelaxTimeBonus );
		SetSlot( pItem, 7, form.fCoverBonus );
		SetSlot( pItem, 8, form.fVisibleBonus );
		pItem->vZeroPos = NResourceModel::Vec3{ -fShiftX, -fShiftY, 0 };
		pItem->units.clear();
		for ( const SSquadRPGStats::SFormation::SEntry &entry : form.order )
		{
			NResourceModel::CSquadFormationPropsItem::SUnit unit;
			unit.vPos = NResourceModel::Vec3{ entry.vPos.x, entry.vPos.y, 0 };
			unit.fDir = ToRadian( entry.fDir );
			pItem->units.push_back( unit );
		}
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

extern "C++" {
namespace
{

// The stats the engine's own operator& reads from the runtime folder's 1.xml
// (or, for a weapon, from the flat file itself), named by the struct that
// reads them. False with the status and message the import answers.
template <class TStats>
bool ReadRuntimeStats( const std::filesystem::path &statsFile, TStats &stats, BkResSession *pSession, BkEditorStatus &status, const char *pszChunk = "RPG" )
{
	CPtr<IDataStorage> pStorage = OpenStorage( StorageDir( statsFile.parent_path() ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( statsFile.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
	CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ ) : 0;
	if ( pDT == 0 )
	{
		pSession->szMessage = "the engine cannot read " + statsFile.string();
		status = BK_EDITOR_DATA_MISSING;
		return false;
	}
	CTreeAccessor tree = pDT;
	tree.Add( pszChunk, &stats );
	return true;
}

// C3DRoadFrame::LoadRPGStats' chunk: bFound tells a file with a VSODescription
// from any other xml, which reads as an all-default description.
struct SVsoChunk
{
	SVectorStripeObjectDesc &desc;
	bool bFound = false;
	explicit SVsoChunk( SVectorStripeObjectDesc &d ) : desc( d ) {}
	int operator&( IDataTree &ss )
	{
		bFound = true;
		return desc.operator&( ss );
	}
};

// C3DRoadFrame::LoadRoadMap / C3DRiverFrame::LoadRiverMap: the terrain the
// preview draws on is maps\\road3d or maps\\river3d, read through the engine's
// storage like any map and loaded into a fresh terrain that becomes the
// scene's. Not BkEditorOpenMap: no map is open, so the editor's world, layers
// and save machinery stay out of it.
BkEditorStatus BeginTerrainPreview( BkEditorSession *pSession, int nKind )
{
	ResourceState &state = StateOf( pSession );
	const char *pszMap = nKind == kRoadKind ? "maps\\road3d" : "maps\\river3d";
	CMapInfo mapinfo;
	std::string szError;
	if ( !NMapFile::ReadNewest( pszMap, &mapinfo, &szError ) )
	{
		pSession->szMessage = std::string( "cannot load " ) + pszMap + ".xml for the preview: " + szError;
		return BK_EDITOR_DATA_MISSING;
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
	IScene *pScene = GetSingleton<IScene>();
	pScene->Clear();
	GetSingleton<IVisObjBuilder>()->Clear();
	CPtr<ITerrain> pTerrain = CreateTerrain();
	pTerrain->Load( pszMap, mapinfo.terrain );
	pScene->SetTerrain( pTerrain );
	// ShowFrameWindows: the terrain is shown, and the camera looks at 16 cells in.
	for ( int i = 0; i < 2; ++i )
		if ( pScene->ToggleShow( SCENE_SHOW_TERRAIN ) )
			break;
	SetSessionCamera( pSession, 16 * fWorldCellSize, 16 * fWorldCellSize );
	state.bPreview = true;
	state.bPreviewTerrain = true;
	state.nPreviewKind = nKind;
	state.previewRoot = root;
	state.bPreviewEffect = false;
	pSession->szMessage = std::string( "preview of ." ) + kKindExtensions[nKind] + " begun on " + pszMap;
	return BK_EDITOR_OK;
}

// UpdateRoadView / UpdateRiverView: the terrain editor's road entries take the
// project's descriptor and are rebuilt. The descriptor is the real export read
// back through the engine's operator& (D-16: the preview tests the export), so
// what is drawn is what the game would read.
BkEditorStatus ShowTerrainPreview( BkEditorSession *pSession )
{
	ResourceState &state = StateOf( pSession );
	const bool bRoad = state.nPreviewKind == kRoadKind;
	const std::string szExtension = kKindExtensions[state.nPreviewKind];
	const NResourceModel::FExporter pfnExporter = NResourceModel::FindExporter( szExtension );
	if ( pfnExporter == nullptr )
	{
		pSession->szMessage = "exporting ." + szExtension + " projects is not ported yet";
		return BK_EDITOR_REFUSED;
	}
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
	const std::filesystem::path dataDir = state.previewRoot / "data";
	std::error_code ec;
	std::filesystem::remove_all( dataDir, ec );
	std::filesystem::create_directories( dataDir, ec );
	NResourceModel::SExportContext context;
	context.szProjectPath = state.szPath;
	context.szStagingRoot = dataDir.string();
	context.bForce = true;
	FillEngineLookups( context, state.previewRoot / "mesh" );
	NResourceModel::SExportOutcome outcome;
	if ( !pfnExporter( project, context, outcome ) )
	{
		pSession->szMessage = "the preview export failed: " + ( outcome.szError.empty() ? std::string( "the exporter failed" ) : outcome.szError );
		return BK_EDITOR_FAILED;
	}
	std::string szRelative = outcome.szObjectName + ".xml";
	for ( char &c : szRelative )
		if ( c == '\\' ) c = '/';
	SVectorStripeObjectDesc desc;
	SVsoChunk chunk( desc );
	BkEditorStatus status = BK_EDITOR_OK;
	if ( !ReadRuntimeStats( dataDir / szRelative, chunk, pSession, status, "VSODescription" ) )
		return status;
	if ( !chunk.bFound )
	{
		pSession->szMessage = ( dataDir / szRelative ).string() + " has no VSODescription";
		return BK_EDITOR_FAILED;
	}
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene != nullptr ? pScene->GetTerrain() : nullptr;
	ITerrainEditor *pEditor = pTerrain != nullptr ? pTerrain->GetEditor() : nullptr;
	if ( pEditor == nullptr )
	{
		pSession->szMessage = "the preview terrain is gone: call BkResPreviewBegin again";
		return BK_EDITOR_REFUSED;
	}
	STerrainInfo &info = const_cast<STerrainInfo &>( pEditor->GetTerrainInfo() );
	TVSOList &list = bRoad ? info.roads3 : info.rivers;
	// UpdateRoadView fills roads3[0] and [1], UpdateRiverView rivers[0].
	const std::size_t nWanted = bRoad ? 2 : 1;
	if ( list.size() < nWanted )
	{
		pSession->szMessage = std::string( bRoad ? "maps\\road3d" : "maps\\river3d" ) + " has " + std::to_string( list.size() )
			+ ( bRoad ? " roads, the preview needs 2" : " rivers, the preview needs 1" );
		return BK_EDITOR_FAILED;
	}
	for ( std::size_t i = 0; i < nWanted; ++i )
	{
		// The descriptor only: the map's own nID and points stay, as FillRPGStats
		// left them. A river's type is the map's (the river export sets none).
		const int nType = list[i].eType;
		static_cast<SVectorStripeObjectDesc &>( list[i] ) = desc;
		if ( !bRoad )
			list[i].eType = nType;
		const bool bUpdated = bRoad ? pEditor->UpdateRoad( list[i].nID ) : pEditor->UpdateRiver( list[i].nID );
		if ( !bUpdated )
		{
			pSession->szMessage = "the terrain would not rebuild " + std::string( bRoad ? "road " : "river " ) + std::to_string( i );
			return BK_EDITOR_FAILED;
		}
	}
	pSession->szMessage = std::string( "preview shows the exported " ) + ( bRoad ? "road on both map roads" : "river on the map river" );
	return BK_EDITOR_OK;
}

// SSourceType of ParticleFrm.cpp:463: which of the two particle structs a
// KeyData chunk holds. bFound tells a file with no KeyData chunk (any other
// xml) from a simple source, which the flag alone cannot.
struct SParticleSourceType
{
	bool bFound = false;
	bool bComplexParticleSource = false;
	int operator&( IDataTree &ss )
	{
		bFound = true;
		CTreeAccessor saver = &ss;
		saver.Add( "ComplexParticleSource", &bComplexParticleSource );
		return 0;
	}
};

// CObjectFrame::LoadRPGStats' tree half: the common properties and the
// effects, which MFC copied back from the stats. The AI classes the object
// blocks are the port's addition (MFC left the Passes item empty, so a
// re-export lost them): one pass item per class bit.
void ObjectStatsToTree( const SObjectRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_OBJECT_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, int( rpgStats.fMaxHP ) );
	SetSlot( pCommonProps, 1, rpgStats.defences[0].nArmorMax );
	SetSlot( pCommonProps, 2, int( rpgStats.defences[0].fSilhouette ) );
	SetSlot( pCommonProps, 3, rpgStats.szAmbientSound );
	SetSlot( pCommonProps, 4, rpgStats.szCycledSound );

	NResourceModel::CTreeItem *pEffects = ChildOfType( root, NResourceModel::ETIT_OBJECT_EFFECTS_ITEM );
	SetSlot( pEffects, 0, rpgStats.szEffectExplosion );
	SetSlot( pEffects, 1, rpgStats.szEffectDeath );

	static const struct { DWORD dwClass; const char *pszName; } kClasses[] =
	{
		{ AI_CLASS_WHEEL, "wheel" }, { AI_CLASS_HALFTRACK, "halftrack" }, { AI_CLASS_TRACK, "track" }, { AI_CLASS_HUMAN, "human" },
	};
	NResourceModel::CTreeItem *pPasses = ChildOfType( root, NResourceModel::ETIT_OBJECT_PASSES_ITEM );
	if ( pPasses != nullptr )
		for ( const auto &entry : kClasses )
			if ( ( rpgStats.dwAIClasses & entry.dwClass ) != 0 )
			{
				auto pPass = NResourceModel::CTreeItemFactory::Instance().Create( NResourceModel::ETIT_OBJECT_PASS_PROPS_ITEM );
				if ( !pPass )
					continue;
				pPass->SetItemName( entry.pszName );
				NResourceModel::CTreeItem *pItem = pPass.get();
				pPasses->AddChild( std::move( pPass ) );
				SetSlot( pItem, 0, std::string( entry.pszName ) );
			}
}

NResourceModel::STileGrid GridOf( CArray2D<BYTE> &array )
{
	NResourceModel::STileGrid grid;
	grid.sizeX = array.GetSizeX();
	grid.sizeY = array.GetSizeY();
	if ( !grid.empty() )
		grid.data.assign( array.GetBuffer(), array.GetBuffer() + std::size_t( grid.sizeX ) * grid.sizeY );
	else
		grid.sizeX = grid.sizeY = 0;
	return grid;
}

// What the frame holds after CObjectFrame::LoadRPGStats: the grids as stored
// (the tile lists MFC built from them hang off the same origin) and both
// crosses where the frame starts them.
NResourceModel::SObjectFrameData ObjectFrameOf( SObjectRPGStats &rpgStats )
{
	NResourceModel::SObjectFrameData frame;
	frame.passability = GridOf( rpgStats.passability );
	frame.vOrigin = NResourceModel::SVec2{ rpgStats.vOrigin.x, rpgStats.vOrigin.y };
	frame.visibility = GridOf( rpgStats.visibility );
	frame.vVisOrigin = NResourceModel::SVec2{ rpgStats.vVisOrigin.x, rpgStats.vVisOrigin.y };
	return frame;
}

// The n-th child of a type, as CTreeItem::GetChildItem( type, n ).
NResourceModel::CTreeItem *NthChildOfType( NResourceModel::CTreeItem &item, int nType, int nIndex )
{
	for ( const auto &pChild : item.GetChildren() )
		if ( pChild->GetItemType() == nType && nIndex-- == 0 )
			return pChild.get();
	return nullptr;
}

// One segment of the stats as a FENCE_PROPS item: the grids as locked tiles
// and transparences, and the sprite position they hang off. MFC derived the
// origins from the sprite position and the grids' leftmost corner (FillSegmentProps);
// here the grids and origins are given, so the first tile follows from a
// reference position and the sprite position is then the one that reproduces
// the origin exactly. The segment has no picture names in its stats: it is
// called seg<index>, which the Fences directory's pictures must use.
std::unique_ptr<NResourceModel::CTreeItem> FenceSegmentToItem( const SFenceRPGStats::SSegmentRPGStats &segment, int nIndex, const NResourceModel::GridProjection &projection )
{
	auto pItem = NResourceModel::CTreeItemFactory::Instance().Create( NResourceModel::ETIT_FENCE_PROPS_ITEM );
	if ( !pItem )
		return nullptr;
	auto *pProps = static_cast<NResourceModel::CFencePropsItem *>( pItem.get() );
	pProps->SetItemName( "seg" + std::to_string( nIndex ) );
	pProps->nSegmentIndex = nIndex;

	const NResourceModel::STileGrid pass = GridOf( const_cast<CArray2D<BYTE> &>( segment.passability ) );
	const NResourceModel::STileGrid vis = GridOf( const_cast<CArray2D<BYTE> &>( segment.visibility ) );
	const NResourceModel::SVec3 vReference{ pProps->vSpritePos.x, pProps->vSpritePos.y, 0 };
	const bool bPass = !pass.empty();
	const NResourceModel::SVec3 origin{ bPass ? segment.vOrigin.x : segment.vVisOrigin.x, bPass ? segment.vOrigin.y : segment.vVisOrigin.y, 0 };
	if ( bPass || !vis.empty() )
	{
		int nTileX = 0, nTileY = 0;
		projection.FirstTileOfGrid( vReference, origin, nTileX, nTileY );
		const NResourceModel::SVec3 left = projection.OriginOfGrid( NResourceModel::SVec3{ 0, 0, 0 }, nTileX, nTileY );
		// OriginOfGrid( 0 ) is minus the corner's world position.
		pProps->vSpritePos.x = origin.x - left.x;
		pProps->vSpritePos.y = origin.y - left.y;
	}
	const NResourceModel::SVec3 vSprite{ pProps->vSpritePos.x, pProps->vSpritePos.y, 0 };
	if ( bPass )
	{
		int nTileX = 0, nTileY = 0;
		projection.FirstTileOfGrid( vSprite, origin, nTileX, nTileY );
		NResourceModel::GridToTiles( pass, nTileX, nTileY, pProps->lockedTiles );
	}
	if ( !vis.empty() )
	{
		int nTileX = 0, nTileY = 0;
		projection.FirstTileOfGrid( vSprite, NResourceModel::SVec3{ segment.vVisOrigin.x, segment.vVisOrigin.y, 0 }, nTileX, nTileY );
		NResourceModel::GridToTiles( vis, nTileX, nTileY, pProps->transeparences );
	}
	return pItem;
}

// CFenceFrame::LoadRPGStats' tree half (FenceFrm.cpp:346): the segments go
// back under the direction and insert items their lists name, each with the
// SegmentIndex the export keeps. Beside it the common properties, which MFC
// read back from the stats too. A shipped stats file lists each index once;
// one a list names twice stays a single segment per list.
void FenceStatsToTree( const SFenceRPGStats &rpgStats, NResourceModel::CTreeItem &root, const NResourceModel::GridProjection &projection )
{
	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_FENCE_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	SetSlot( pCommonProps, 2, int( rpgStats.fMaxHP ) );
	SetSlot( pCommonProps, 3, rpgStats.defences[0].nArmorMax );
	SetSlot( pCommonProps, 4, ( rpgStats.dwAIClasses & AI_CLASS_HUMAN ) != 0 );
	SetSlot( pCommonProps, 5, ( rpgStats.dwAIClasses & AI_CLASS_WHEEL ) != 0 );
	SetSlot( pCommonProps, 6, ( rpgStats.dwAIClasses & AI_CLASS_HALFTRACK ) != 0 );
	SetSlot( pCommonProps, 7, ( rpgStats.dwAIClasses & AI_CLASS_TRACK ) != 0 );

	NResourceModel::CTreeItem *pEffects = ChildOfType( root, NResourceModel::ETIT_OBJECT_EFFECTS_ITEM );
	SetSlot( pEffects, 0, rpgStats.szEffectExplosion );
	SetSlot( pEffects, 1, rpgStats.szEffectDeath );

	for ( int nDirection = 0; nDirection < 4 && nDirection < int( rpgStats.dirs.size() ); ++nDirection )
	{
		NResourceModel::CTreeItem *pDirection = NthChildOfType( root, NResourceModel::ETIT_FENCE_DIRECTION_ITEM, nDirection );
		if ( pDirection == nullptr )
			continue;
		const SFenceRPGStats::SDir &dir = rpgStats.dirs[nDirection];
		const std::vector<int> *lists[4] = { &dir.centers, &dir.ldamages, &dir.rdamages, &dir.cdamages };
		for ( int nType = 0; nType < 4; ++nType )
		{
			NResourceModel::CTreeItem *pInsert = NthChildOfType( *pDirection, NResourceModel::ETIT_FENCE_INSERT_ITEM, nType );
			if ( pInsert == nullptr )
				continue;
			for ( int nIndex : *lists[nType] )
				if ( nIndex >= 0 && nIndex < int( rpgStats.stats.size() ) )
					if ( auto pSegment = FenceSegmentToItem( rpgStats.stats[nIndex], nIndex, projection ) )
						pInsert->AddChild( std::move( pSegment ) );
		}
	}
}

// CMeshCommonPropsItem::SetMeshType: the combo text of a unit type.
const char *MeshTypeName( EUnitRPGType type )
{
	static const struct { EUnitRPGType type; const char *pszName; } kTypes[] =
	{
		{ RPG_TYPE_TRN_CARRIER, "transport carrier" }, { RPG_TYPE_TRN_SUPPORT, "transport support" },
		{ RPG_TYPE_TRN_MEDICINE, "transport medicine" }, { RPG_TYPE_TRN_TRACTOR, "transport tractor" },
		{ RPG_TYPE_TRN_MILITARY_AUTO, "transport military auto" }, { RPG_TYPE_TRN_CIVILIAN_AUTO, "transport civilian auto" },
		{ RPG_TYPE_ART_GUN, "artillery gun" }, { RPG_TYPE_ART_HOWITZER, "artillery howitzer" },
		{ RPG_TYPE_ART_HEAVY_GUN, "artillery heavy gun" }, { RPG_TYPE_ART_HEAVY_MG, "artillery heavy machine gun" },
		{ RPG_TYPE_ART_AAGUN, "artillery antiair gun" }, { RPG_TYPE_ART_ROCKET, "artillery rocket" },
		{ RPG_TYPE_ART_SUPER, "artillery super" }, { RPG_TYPE_ART_MORTAR, "artillery mortar" },
		{ RPG_TYPE_SPG_ASSAULT, "SPG assault" }, { RPG_TYPE_SPG_ANTITANK, "SPG antitank" },
		{ RPG_TYPE_SPG_SUPER, "SPG super" }, { RPG_TYPE_SPG_AAGUN, "SPG antiair" },
		{ RPG_TYPE_ARM_LIGHT, "armor light" }, { RPG_TYPE_ARM_MEDIUM, "armor medium" },
		{ RPG_TYPE_ARM_SUPER, "armor super" }, { RPG_TYPE_ARM_HEAVY, "armor heavy" },
		{ RPG_TYPE_AVIA_SCOUT, "avia scout" }, { RPG_TYPE_AVIA_BOMBER, "avia bomber" },
		{ RPG_TYPE_AVIA_ATTACK, "avia attack" }, { RPG_TYPE_AVIA_FIGHTER, "avia fighter" },
		{ RPG_TYPE_AVIA_SUPER, "avia super" }, { RPG_TYPE_AVIA_LANDER, "avia lander" },
		{ RPG_TYPE_TRAIN_LOCOMOTIVE, "train locomotive" }, { RPG_TYPE_TRAIN_CARGO, "train cargo" },
		{ RPG_TYPE_TRAIN_CARRIER, "train carrier" }, { RPG_TYPE_TRAIN_SUPER, "train super" },
		{ RPG_TYPE_TRAIN_ARMOR, "train armor" },
	};
	for ( const auto &entry : kTypes )
		if ( entry.type == type )
			return entry.pszName;
	return "transport carrier";
}

// CMeshFrame::GetRPGStats (MeshFrm.cpp:1136), line for line, onto the port's
// items; the slots are the property order of mesh.cpp, the index
// ExportMesh reads back. MFC filled a tree that already had the stats' shape
// (one platform item per platform, one gun item per gun) and asserted on a
// mismatch; an import starts from the default tree, so the platforms and guns
// are made to the stats' counts first, named as MFC's insert handlers name them.
// The locator combos (platform parts, shoot points) are not stats fields:
// GetRPGStats leaves them, and so does this.
void MechStatsToTree( const SMechUnitRPGStats &rpgStats, NResourceModel::CTreeItem &root )
{
	auto &factory = NResourceModel::CTreeItemFactory::Instance();

	NResourceModel::CTreeItem *pPlatforms = ChildOfType( root, NResourceModel::ETIT_MESH_PLATFORMS_ITEM );
	if ( pPlatforms != nullptr )
	{
		auto &platformItems = pPlatforms->MutableChildren();
		if ( platformItems.size() > rpgStats.platforms.size() )
			platformItems.resize( rpgStats.platforms.size() );
		while ( platformItems.size() < rpgStats.platforms.size() )
		{
			auto pPlatform = factory.Create( NResourceModel::ETIT_MESH_PLATFORM_PROPS_ITEM );
			if ( !pPlatform )
				break;
			pPlatform->CreateDefaultChilds();
			pPlatform->SetItemName( "Platform" );
			pPlatforms->AddChild( std::move( pPlatform ) );
		}
		int nGun = 0;
		for ( std::size_t p = 0; p < platformItems.size(); ++p )
		{
			NResourceModel::CTreeItem *pGuns = ChildOfType( *platformItems[p], NResourceModel::ETIT_MESH_GUNS_ITEM );
			if ( pGuns == nullptr )
				continue;
			pGuns->MutableChildren().clear();
			for ( int g = 0; g < rpgStats.platforms[p].nNumGuns; ++g, ++nGun )
			{
				auto pGun = factory.Create( NResourceModel::ETIT_MESH_GUN_PROPS_ITEM );
				if ( !pGun )
					continue;
				pGun->SetItemName( "Gun" );
				pGuns->AddChild( std::move( pGun ) );
			}
		}
	}

	if ( NResourceModel::CTreeItem *pGraphics = ChildOfType( root, NResourceModel::ETIT_MESH_GRAPHICS_ITEM ) )
		if ( NResourceModel::CTreeItem *pDeathCraters = ChildOfType( *pGraphics, NResourceModel::ETIT_MESH_DEATH_CRATERS_ITEM ) )
		{
			pDeathCraters->MutableChildren().clear();
			for ( const std::string &szCrater : rpgStats.deathCraters )
			{
				auto pProps = factory.Create( NResourceModel::ETIT_MESH_DEATH_CRATER_PROPS_ITEM );
				if ( !pProps )
					continue;
				pProps->SetItemName( szCrater );
				SetSlot( pProps.get(), 0, szCrater );
				pDeathCraters->AddChild( std::move( pProps ) );
			}
		}

	NResourceModel::CTreeItem *pCommonProps = ChildOfType( root, NResourceModel::ETIT_MESH_COMMON_PROPS_ITEM );
	SetSlot( pCommonProps, 0, rpgStats.szKeyName );
	SetSlot( pCommonProps, 1, std::string( MeshTypeName( rpgStats.type ) ) );
	const char *pszAIClass = "track";
	switch ( rpgStats.aiClass )
	{
		case AI_CLASS_WHEEL:     pszAIClass = "wheel";     break;
		case AI_CLASS_HALFTRACK: pszAIClass = "halftrack"; break;
		case AI_CLASS_HUMAN:     pszAIClass = "human";     break;
		default: break;
	}
	SetSlot( pCommonProps, 2, std::string( pszAIClass ) );
	SetSlot( pCommonProps, 4, rpgStats.fMaxHP );
	SetSlot( pCommonProps, 5, rpgStats.fRepairCost );
	SetSlot( pCommonProps, 6, rpgStats.fCamouflage );
	SetSlot( pCommonProps, 7, rpgStats.fSpeed );
	SetSlot( pCommonProps, 8, rpgStats.fPassability );
	SetSlot( pCommonProps, 9, rpgStats.fTowingForce );
	SetSlot( pCommonProps, 10, rpgStats.fUninstallRotate );
	SetSlot( pCommonProps, 11, rpgStats.fUninstallTransport );
	SetSlot( pCommonProps, 12, rpgStats.fWeight );
	SetSlot( pCommonProps, 13, rpgStats.nCrew );
	SetSlot( pCommonProps, 14, rpgStats.nPassangers );
	SetSlot( pCommonProps, 15, rpgStats.nPriority );
	SetSlot( pCommonProps, 16, rpgStats.fRotateSpeed );
	SetSlot( pCommonProps, 17, rpgStats.nBoundTileRadius );
	SetSlot( pCommonProps, 18, rpgStats.fTurnRadius );
	SetSlot( pCommonProps, 19, rpgStats.fSmallAABBCoeff );
	SetSlot( pCommonProps, 20, rpgStats.fPrice );
	SetSlot( pCommonProps, 21, rpgStats.fSight );
	SetSlot( pCommonProps, 22, rpgStats.fSightPower );

	if ( rpgStats.szAcksNames.size() > 0 )
		SetSlot( ChildOfType( root, NResourceModel::ETIT_UNIT_ACKS_ITEM ), 0, rpgStats.szAcksNames[0] );

	NResourceModel::CTreeItem *pEffects = ChildOfType( root, NResourceModel::ETIT_MESH_EFFECTS_ITEM );
	SetSlot( pEffects, 0, rpgStats.szEffectDiesel );
	SetSlot( pEffects, 1, rpgStats.szEffectSmoke );
	SetSlot( pEffects, 2, rpgStats.szEffectWheelDust );
	SetSlot( pEffects, 3, rpgStats.szEffectShootDust );
	SetSlot( pEffects, 4, rpgStats.szEffectFatality );
	SetSlot( pEffects, 5, rpgStats.szEffectDisappear );
	SetSlot( pEffects, 6, rpgStats.szSoundMoveStart );
	SetSlot( pEffects, 7, rpgStats.szSoundMoveCycle );
	SetSlot( pEffects, 8, rpgStats.szSoundMoveStop );
	// Old exports had the cycle and stop sounds the wrong way round.
	if ( std::strstr( rpgStats.szSoundMoveStop.c_str(), "cycle" ) != nullptr )
	{
		SetSlot( pEffects, 7, rpgStats.szSoundMoveStop );
		SetSlot( pEffects, 8, rpgStats.szSoundMoveCycle );
	}

	if ( NResourceModel::CTreeItem *pAvia = pCommonProps ? ChildOfType( *pCommonProps, NResourceModel::ETIT_MESH_AVIA_ITEM ) : nullptr )
	{
		SetSlot( pAvia, 0, rpgStats.fMaxHeight );
		SetSlot( pAvia, 1, rpgStats.fDivingAngle );
		SetSlot( pAvia, 2, rpgStats.fClimbAngle );
		SetSlot( pAvia, 3, rpgStats.fTiltAngle );
		SetSlot( pAvia, 4, rpgStats.fTiltRatio );
	}
	if ( NResourceModel::CTreeItem *pTrack = pCommonProps ? ChildOfType( *pCommonProps, NResourceModel::ETIT_MESH_TRACK_ITEM ) : nullptr )
	{
		SetSlot( pTrack, 0, rpgStats.bLeavesTracks );
		SetSlot( pTrack, 1, rpgStats.fTrackWidth );
		SetSlot( pTrack, 2, rpgStats.fTrackOffset );
		SetSlot( pTrack, 3, rpgStats.fTrackStart );
		SetSlot( pTrack, 4, rpgStats.fTrackEnd );
		SetSlot( pTrack, 5, rpgStats.fTrackIntensity );
		SetSlot( pTrack, 6, rpgStats.nTrackLifetime );
	}

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

	if ( NResourceModel::CTreeItem *pDefences = ChildOfType( root, NResourceModel::ETIT_MESH_DEFENCES_ITEM ) )
		for ( const auto &pDefProps : pDefences->GetChildren() )
		{
			const std::string &szSide = pDefProps->GetDisplayName();
			int nIndex = 0;
			if ( szSide == "Left" )
				nIndex = RPG_LEFT;
			else if ( szSide == "Right" )
				nIndex = RPG_RIGHT;
			else if ( szSide == "Top" )
				nIndex = RPG_TOP;
			else if ( szSide == "Bottom" )
				nIndex = RPG_BOTTOM;
			else if ( szSide == "Front" )
				nIndex = RPG_FRONT;
			else if ( szSide == "Back" )
				nIndex = RPG_BACK;
			SetSlot( pDefProps.get(), 0, int( rpgStats.armors[nIndex].fMin ) );
			SetSlot( pDefProps.get(), 1, int( rpgStats.armors[nIndex].fMax ) );
		}

	if ( NResourceModel::CTreeItem *pJoggings = ChildOfType( root, NResourceModel::ETIT_MESH_JOGGINGS_ITEM ) )
	{
		int i = 0;
		for ( const auto &pJogProps : pJoggings->GetChildren() )
		{
			const SMechUnitRPGStats::SJoggingParams &jog = i == 0 ? rpgStats.jx : ( i == 1 ? rpgStats.jy : rpgStats.jz );
			SetSlot( pJogProps.get(), 0, jog.fPeriod1 );
			SetSlot( pJogProps.get(), 1, jog.fPeriod2 );
			SetSlot( pJogProps.get(), 2, jog.fAmp1 );
			SetSlot( pJogProps.get(), 3, jog.fAmp2 );
			SetSlot( pJogProps.get(), 4, jog.fPhase1 );
			SetSlot( pJogProps.get(), 5, jog.fPhase2 );
			if ( ++i == 3 )
				break;
		}
	}

	if ( pPlatforms != nullptr )
	{
		std::size_t nPlatform = 0;
		std::size_t nGun = 0;
		for ( const auto &pPlatformProps : pPlatforms->GetChildren() )
		{
			SetSlot( pPlatformProps.get(), 3, rpgStats.platforms[nPlatform].fVerticalRotationSpeed );
			SetSlot( pPlatformProps.get(), 4, rpgStats.platforms[nPlatform].fHorizontalRotationSpeed );
			if ( NResourceModel::CTreeItem *pGuns = ChildOfType( *pPlatformProps, NResourceModel::ETIT_MESH_GUNS_ITEM ) )
				for ( const auto &pGunProps : pGuns->GetChildren() )
				{
					if ( nGun >= rpgStats.guns.size() )
						break;
					const SMechUnitRPGStats::SGun &gun = rpgStats.guns[nGun++];
					SetSlot( pGunProps.get(), 2, gun.szWeapon );
					SetSlot( pGunProps.get(), 3, gun.nPriority );
					SetSlot( pGunProps.get(), 4, gun.bRecoil );
					SetSlot( pGunProps.get(), 5, int( gun.recoilTime ) );
					SetSlot( pGunProps.get(), 6, gun.nRecoilShakeTime );
					SetSlot( pGunProps.get(), 7, gun.fRecoilShakeAngle );
					SetSlot( pGunProps.get(), 8, gun.nAmmo );
					SetSlot( pGunProps.get(), 9, gun.fReloadCost );
				}
			++nPlatform;
		}
	}
}

// The weapon's stats are a flat file named after the weapon (weapons\mg_37t.xml)
// a particle's a flat file named after the source (effects\particles\flame.xml),
// and a road's or river's the runtime <name>.xml itself (terrain\sets\1\roads3d\road_pavement.xml),
// every other kind's a 1.xml in a folder of its own.
std::filesystem::path RuntimeStatsFile( BkResKind kind, const char *pszPath )
{
	if ( kind == 0 || kind == kParticleKind || kind == kRoadKind || kind == kRiverKind )
	{
		std::error_code ec;
		if ( std::filesystem::is_regular_file( pszPath, ec ) )
			return std::filesystem::path( pszPath );
	}
	return StatsFileIn( pszPath );
}

// ---- Tileset import (TileSetFrm.cpp:557, 782) ----------------------------

void SetTileValue( NResourceModel::CTreeItem &item, int nPropId, const NResourceModel::CVariant &value )
{
	for ( auto &prop : item.MutableValues() )
		if ( prop.nId == nPropId )
		{
			prop.value = value;
			return;
		}
}

// The tiles written for an import, named by their file, held until every one
// of them is cut so a failure leaves the project folder with no half set.
struct STileFile
{
	std::filesystem::path file;
	CPtr<IImage> pImage;
};

// What both imports read: the atlas beside the xml, the tile mask and the
// folder the cut tiles go to. False with the status and message the import
// answers.
struct STileSetSource
{
	CPtr<IImage> pAtlas;
	CPtr<IImage> pMask;
	std::filesystem::path xmlFile;
	std::filesystem::path atlasFile;
	std::filesystem::path outDir;
};

bool OpenTileSetSource( BkResSession *pSession, const char *pszXml, bool bCrossets, STileSetSource &src, BkEditorStatus &status )
{
	ResourceState &state = StateOf( pSession );
	if ( !state.bOpen || !state.pProject || !state.pProject->root || state.nKindOrdinal != kTileSetKind )
	{
		pSession->szMessage = "importing terrains and crossets is a .til feature: open a tileset project first";
		status = BK_EDITOR_REFUSED;
		return false;
	}
	if ( state.szPath.empty() )
	{
		pSession->szMessage = "save the project first: the cut tiles go to its terrains\\ and crossets\\ folders beside it";
		status = BK_EDITOR_REFUSED;
		return false;
	}
	src.xmlFile = std::filesystem::path( pszXml );
	std::error_code ec;
	if ( !std::filesystem::is_regular_file( src.xmlFile, ec ) )
	{
		pSession->szMessage = std::string( "Error: Cannot open file: " ) + src.xmlFile.string();
		status = BK_EDITOR_DATA_MISSING;
		return false;
	}
	src.atlasFile = src.xmlFile;
	src.atlasFile.replace_extension( ".tga" );
	NResourceModel::SExportOutcome outcome;
	src.pAtlas = NResourceModel::NImageExport::LoadPicture( src.atlasFile.string(), outcome );
	if ( src.pAtlas == 0 )
	{
		pSession->szMessage = "Error: " + outcome.szError;
		status = BK_EDITOR_DATA_MISSING;
		return false;
	}
	const std::filesystem::path maskFile = NResourceModel::NStatsExport::FoldedChild( NResourceModel::NStatsExport::FoldedChild( NResourceModel::NStatsExport::FoldedChild( ShippedDataFolder( pSession ), "editor" ), "terrain" ), "tilemask.tga" );
	src.pMask = NResourceModel::NImageExport::LoadPicture( maskFile.string(), outcome );
	if ( src.pMask == 0 )
	{
		pSession->szMessage = "Error: Cannot open terrain mask file: " + maskFile.string();
		status = BK_EDITOR_DATA_MISSING;
		return false;
	}
	src.outDir = std::filesystem::path( state.szPath ).parent_path() / ( bCrossets ? "crossets" : "terrains" );
	return true;
}

// One 64 x 32 tile of the atlas, the 256-wide layout of MFC, or null with the
// message when the place lies outside the picture (MFC read past it).
CPtr<IImage> CutTile( BkResSession *pSession, const STileSetSource &src, int nBeginX, int nBeginY, bool bKeepAlpha, const std::string &szWhat )
{
	const int nSizeX = 64, nSizeY = 32;
	IImage *pAtlas = src.pAtlas;
	if ( nBeginX + nSizeX > pAtlas->GetSizeX() || nBeginY + nSizeY > pAtlas->GetSizeY() )
	{
		pSession->szMessage = szWhat + " lies at " + std::to_string( nBeginX ) + "," + std::to_string( nBeginY ) + " outside the " + std::to_string( pAtlas->GetSizeX() ) + " x " +
		                      std::to_string( pAtlas->GetSizeY() ) + " picture " + src.atlasFile.string();
		return 0;
	}
	CPtr<IImage> pTile = GetImageProcessor()->CreateImage( nSizeX, nSizeY );
	pTile->Set( bKeepAlpha ? SColor( 0, 0, 0, 0 ) : SColor( 255, 0, 0, 0 ) );
	SColor *pRes = pTile->GetLFB();
	const SColor *pSrc = pAtlas->GetLFB();
	const int nStride = pAtlas->GetSizeX();
	for ( int y = 0; y < nSizeY; ++y )
		for ( int x = 0; x < nSizeX; ++x )
		{
			SColor &dst = pRes[y * nSizeX + x];
			const SColor &from = pSrc[( nBeginY + y ) * nStride + nBeginX + x];
			if ( bKeepAlpha )
				dst = from;
			else
			{
				dst.r = from.r;
				dst.g = from.g;
				dst.b = from.b;
				dst.a = 255;
			}
		}
	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = src.pMask->GetSizeX();
	rc.bottom = src.pMask->GetSizeY();
	pTile->ModulateAlphaFrom( src.pMask, &rc, 0, 0 );
	return pTile;
}

std::string TileFileName( int nIndex )
{
	char sz[32];
	std::snprintf( sz, sizeof sz, "%.3d", nIndex );
	return sz;
}

// Writes the held tiles; false with the message of the first that fails.
bool WriteTileFiles( BkResSession *pSession, const STileSetSource &src, const std::vector<STileFile> &tiles )
{
	std::error_code ec;
	std::filesystem::create_directories( src.outDir, ec );
	if ( ec )
	{
		pSession->szMessage = "cannot create the folder " + src.outDir.string() + ": " + ec.message();
		return false;
	}
	for ( const STileFile &tile : tiles )
	{
		NResourceModel::SExportOutcome outcome;
		if ( !NResourceModel::NImageExport::SaveTgaFile( tile.file.string(), tile.pImage, outcome ) )
		{
			pSession->szMessage = outcome.szError;
			return false;
		}
	}
	return true;
}

BkEditorStatus ImportTerrains( BkResSession *pSession, const char *pszXml, int *pnCount )
{
	STileSetSource src;
	BkEditorStatus status = BK_EDITOR_OK;
	if ( !OpenTileSetSource( pSession, pszXml, false, src, status ) )
		return status;
	STilesetDesc tileSetDesc;
	if ( !ReadRuntimeStats( src.xmlFile, tileSetDesc, pSession, status, "tileset" ) )
		return status;
	if ( tileSetDesc.terrtypes.empty() )
	{
		pSession->szMessage = src.xmlFile.string() + " holds no terrain types: it is not a tileset editor file";
		return BK_EDITOR_REFUSED;
	}

	NResourceModel::CTileSetTerrainsItem scratch;
	std::vector<STileFile> tiles;
	for ( const STerrTypeDesc &terrDesc : tileSetDesc.terrtypes )
	{
		auto pTerrain = std::make_unique<NResourceModel::CTileSetTerrainPropsItem>();
		NResourceModel::CTreeItem &terrain = *pTerrain;
		scratch.AddChild( std::move( pTerrain ) );
		SetTileValue( terrain, 1, NResourceModel::CVariant( terrDesc.szName ) );
		SetTileValue( terrain, 2, NResourceModel::CVariant( terrDesc.nCrosset ) );
		SetTileValue( terrain, 3, NResourceModel::CVariant( terrDesc.nPriority ) );
		SetTileValue( terrain, 4, NResourceModel::CVariant( terrDesc.fPassability ) );
		SetTileValue( terrain, 5, NResourceModel::CVariant( !( terrDesc.dwAIClasses & AI_CLASS_HUMAN ) ) );
		SetTileValue( terrain, 6, NResourceModel::CVariant( !( terrDesc.dwAIClasses & AI_CLASS_WHEEL ) ) );
		SetTileValue( terrain, 7, NResourceModel::CVariant( !( terrDesc.dwAIClasses & AI_CLASS_HALFTRACK ) ) );
		SetTileValue( terrain, 8, NResourceModel::CVariant( !( terrDesc.dwAIClasses & AI_CLASS_TRACK ) ) );
		SetTileValue( terrain, 9, NResourceModel::CVariant( bool( terrDesc.bMicroTexture ) ) );
		SetTileValue( terrain, 10, NResourceModel::CVariant( terrDesc.fSoundVolume ) );
		SetTileValue( terrain, 11, NResourceModel::CVariant( terrDesc.szSound ) );
		SetTileValue( terrain, 12, NResourceModel::CVariant( terrDesc.szLoopedSound ) );
		SetTileValue( terrain, 13, NResourceModel::CVariant( bool( terrDesc.bCanEntrench ) ) );
		SetTileValue( terrain, 14, NResourceModel::CVariant( ( terrDesc.dwAIClasses & 0x80000000 ) != 0 ) );
		SetTileValue( terrain, 15, NResourceModel::CVariant( ( terrDesc.cSoilParams & STerrTypeDesc::ESP_TRACE ) != 0 ) );
		SetTileValue( terrain, 16, NResourceModel::CVariant( ( terrDesc.cSoilParams & STerrTypeDesc::ESP_DUST ) != 0 ) );
		terrain.SetItemName( terrDesc.szName );

		NResourceModel::CTreeItem *pTiles = ChildOfType( terrain, NResourceModel::ETIT_TILESET_TILES_ITEM );
		if ( pTiles == nullptr )
		{
			pSession->szMessage = "the terrain \"" + terrDesc.szName + "\" has no Tiles item";
			return BK_EDITOR_FAILED;
		}
		for ( size_t k = 0; k < terrDesc.tiles.size(); ++k )
		{
			const SMainTileDesc &tileDesc = terrDesc.tiles[k];
			auto pTile = std::make_unique<NResourceModel::CTileSetTilePropsItem>();
			NResourceModel::CTileSetTilePropsItem &tile = *pTile;
			const int nTileIndex = tileDesc.nIndex / 2;
			tile.nTileIndex = nTileIndex;
			tile.SetItemName( TileFileName( nTileIndex ) );
			// MFC's flip state: an odd engine index is the flipped twin, and a
			// normal tile followed by its twin is both.
			std::string szFlipped = ( tileDesc.nIndex & 0x01 ) ? "flipped" : "normal";
			if ( !( k & 0x01 ) && k + 1 < terrDesc.tiles.size() && terrDesc.tiles[k + 1].nIndex == tileDesc.nIndex + 1 )
			{
				szFlipped = "normal and flipped";
				++k;
			}
			pTiles->AddChild( std::move( pTile ) );
			SetTileValue( tile, 1, NResourceModel::CVariant( tileDesc.fProbTo - tileDesc.fProbFrom ) );
			SetTileValue( tile, 2, NResourceModel::CVariant( szFlipped ) );

			int nBeginX, nBeginY;
			const int nMod7 = nTileIndex % 7;
			if ( nMod7 < 4 )
			{
				nBeginX = nMod7 * 64;
				nBeginY = ( nTileIndex / 7 ) * 32;
			}
			else
			{
				nBeginX = ( nMod7 - 4 ) * 64 + 32;
				nBeginY = ( nTileIndex / 7 ) * 32 + 16;
			}
			CPtr<IImage> pImage = CutTile( pSession, src, nBeginX, nBeginY, false, "tile " + TileFileName( nTileIndex ) + " of terrain \"" + terrDesc.szName + "\"" );
			if ( pImage == 0 )
				return BK_EDITOR_FAILED;
			tiles.push_back( { src.outDir / ( TileFileName( nTileIndex ) + ".tga" ), pImage } );
		}
	}
	if ( !WriteTileFiles( pSession, src, tiles ) )
		return BK_EDITOR_FAILED;

	ResourceState &state = StateOf( pSession );
	NResourceModel::CTreeItem *pTerrains = ChildOfType( *state.pProject->root, NResourceModel::ETIT_TILESET_TERRAINS_ITEM );
	if ( pTerrains == nullptr )
	{
		pSession->szMessage = "the project has no Terrains item";
		return BK_EDITOR_FAILED;
	}
	pTerrains->MutableChildren().clear();
	for ( auto &pChild : scratch.MutableChildren() )
		pTerrains->MutableChildren().push_back( std::move( pChild ) );
	SetTileValue( *pTerrains, 1, NResourceModel::CVariant( std::string( "terrains\\" ) ) );
	RebuildIds( state );
	if ( pnCount != nullptr )
		*pnCount = int( tiles.size() );
	pSession->szMessage = "imported " + std::to_string( tileSetDesc.terrtypes.size() ) + " terrains and wrote " + std::to_string( tiles.size() ) + " tiles to " + src.outDir.string();
	return BK_EDITOR_OK;
}

BkEditorStatus ImportCrossets( BkResSession *pSession, const char *pszXml, int *pnCount )
{
	STileSetSource src;
	BkEditorStatus status = BK_EDITOR_OK;
	if ( !OpenTileSetSource( pSession, pszXml, true, src, status ) )
		return status;
	SCrossetDesc crossSetDesc;
	if ( !ReadRuntimeStats( src.xmlFile, crossSetDesc, pSession, status, "crosset" ) )
		return status;
	if ( crossSetDesc.crosses.empty() )
	{
		pSession->szMessage = src.xmlFile.string() + " holds no crossets: it is not a crosset editor file";
		return BK_EDITOR_REFUSED;
	}

	NResourceModel::CCrossetsItem scratch;
	std::vector<STileFile> tiles;
	for ( const SCrossDesc &crossDesc : crossSetDesc.crosses )
	{
		auto pCrosset = std::make_unique<NResourceModel::CCrossetPropsItem>();
		NResourceModel::CTreeItem &crosset = *pCrosset;
		scratch.AddChild( std::move( pCrosset ) );
		SetTileValue( crosset, 1, NResourceModel::CVariant( crossDesc.szName ) );
		crosset.SetItemName( crossDesc.szName );
		for ( const SCrossTileTypeDesc &typeDesc : crossDesc.tiles )
		{
			NResourceModel::CTreeItem *pGroup = nullptr;
			for ( const auto &pChild : crosset.GetChildren() )
				if ( pChild->GetItemType() == NResourceModel::ETIT_CROSSET_TILES_ITEM && pChild->GetDisplayName() == typeDesc.szName )
					pGroup = pChild.get();
			if ( pGroup == nullptr )
			{
				pSession->szMessage = "the crosset \"" + crossDesc.szName + "\" has no tile group named \"" + typeDesc.szName + "\" (a to l)";
				return BK_EDITOR_FAILED;
			}
			for ( const SMainTileDesc &tileDesc : typeDesc.tiles )
			{
				auto pTile = std::make_unique<NResourceModel::CCrossetTilePropsItem>();
				NResourceModel::CCrossetTilePropsItem &tile = *pTile;
				tile.nCrossIndex = tileDesc.nIndex;
				tile.SetItemName( TileFileName( tileDesc.nIndex ) );
				pGroup->AddChild( std::move( pTile ) );
				SetTileValue( tile, 1, NResourceModel::CVariant( tileDesc.fProbTo - tileDesc.fProbFrom ) );

				int nBeginX, nBeginY;
				const int nMod7 = ( tileDesc.nIndex / 2 ) % 7;
				if ( nMod7 < 4 )
				{
					nBeginX = nMod7 * 64;
					nBeginY = ( tileDesc.nIndex / 14 ) * 32;
				}
				else
				{
					nBeginX = ( nMod7 - 4 ) * 64 + 32;
					nBeginY = ( tileDesc.nIndex / 14 ) * 32 + 16;
				}
				CPtr<IImage> pImage = CutTile( pSession, src, nBeginX, nBeginY, true, "tile " + TileFileName( tileDesc.nIndex ) + " of crosset \"" + crossDesc.szName + "\"" );
				if ( pImage == 0 )
					return BK_EDITOR_FAILED;
				tiles.push_back( { src.outDir / ( TileFileName( tileDesc.nIndex ) + ".tga" ), pImage } );
			}
		}
	}
	if ( !WriteTileFiles( pSession, src, tiles ) )
		return BK_EDITOR_FAILED;

	ResourceState &state = StateOf( pSession );
	NResourceModel::CTreeItem *pCrossets = ChildOfType( *state.pProject->root, NResourceModel::ETIT_CROSSETS_ITEM );
	if ( pCrossets == nullptr )
	{
		pSession->szMessage = "the project has no Crossets item";
		return BK_EDITOR_FAILED;
	}
	pCrossets->MutableChildren().clear();
	for ( auto &pChild : scratch.MutableChildren() )
		pCrossets->MutableChildren().push_back( std::move( pChild ) );
	SetTileValue( *pCrossets, 1, NResourceModel::CVariant( std::string( "crossets\\" ) ) );
	RebuildIds( state );
	if ( pnCount != nullptr )
		*pnCount = int( tiles.size() );
	pSession->szMessage = "imported " + std::to_string( crossSetDesc.crosses.size() ) + " crossets and wrote " + std::to_string( tiles.size() ) + " tiles to " + src.outDir.string();
	return BK_EDITOR_OK;
}

}
} // extern "C++"

BkEditorStatus BkResImportFromGame( BkResSession *pSession, BkResKind kind, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == nullptr || *pszPath == 0 || kind < 0 || kind >= kKindCount )
			return BK_EDITOR_BAD_ARGUMENT;
		const std::string szExtension = kKindExtensions[kind];
		const bool bPorted = kind <= 3 || kind == kInfantryKind || kind == kMeshKind || kind == kObjectKind || kind == kFenceKind || kind == kBuildingKind || kind == kBridgeKind || kind == kParticleKind || kind == kRoadKind || kind == kRiverKind || kind == kMedalKind;
		if ( !bPorted )
		{
			pSession->szMessage = kind == 4
				? std::string( "importing .spt is refused: MFC's sprite export only composes .san packs and has no reverse path" )
				: kind == 12 ? std::string( NResourceModel::EffectImportRefusal() )
				: kind == 13 ? std::string( NResourceModel::TileSetImportRefusal() )
				: "importing ." + szExtension + " is not ported yet; it comes with its sub-editor";
			return BK_EDITOR_REFUSED;
		}
		if ( GetSLS() == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const std::filesystem::path statsFile = RuntimeStatsFile( kind, pszPath );
		if ( statsFile.empty() )
		{
			pSession->szMessage = kind == 0 ? std::string( "no weapon file " ) + pszPath
				: kind == kParticleKind ? std::string( "no particle file " ) + pszPath
				: kind == kRoadKind || kind == kRiverKind ? std::string( "no " ) + ( kind == kRoadKind ? "road" : "river" ) + " file " + pszPath + " (the path is the runtime .xml itself)"
				: std::string( "no 1.xml in " ) + pszPath;
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
		BkEditorStatus status = BK_EDITOR_OK;
		std::string szKeyName;
		NResourceModel::SObjectFrameData objectFrame;
		bool bObjectFrame = false;
		SBuildingRPGStats buildingStats;
		bool bBuildingFrame = false;
		SBridgeRPGStats bridgeStats;
		bool bBridgeFrame = false;
		std::string szMedalExportFile;
		bool bMedalFrame = false;
		// The KeyName is what the engine's reader found; a file that holds
		// none of this kind's stats leaves it empty.
		switch ( kind )
		{
			case 0:
			{
				SWeaponRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				szKeyName = rpgStats.szKeyName;
				if ( !szKeyName.empty() )
					WeaponStatsToTree( rpgStats, *pRoot );
				break;
			}
			case 1:
			{
				SMineRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				szKeyName = rpgStats.szKeyName;
				MineStatsToTree( rpgStats, *pRoot );
				break;
			}
			case 2:
			{
				SEntrenchmentRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				// A shipped trench's stats carry no key name: it is its folder's.
				szKeyName = rpgStats.szKeyName.empty() ? statsFile.parent_path().filename().string() : rpgStats.szKeyName;
				TrenchStatsToTree( rpgStats, *pRoot );
				break;
			}
			case 3:
			{
				SSquadRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				// A squad has no name of its own in its stats: it is its folder's.
				szKeyName = rpgStats.memberNames.empty() ? std::string() : statsFile.parent_path().filename().string();
				SquadStatsToTree( rpgStats, *pRoot );
				break;
			}
			case kMeshKind:
			{
				// CMeshFrame::LoadRPGStats: the engine's own operator& reads it.
				// The graphics model names keep their defaults (1.mod, 2.mod,
				// 3.mod) as in MFC, so the project sits beside the shipped models.
				SMechUnitRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				szKeyName = rpgStats.szKeyName;
				if ( !szKeyName.empty() )
					MechStatsToTree( rpgStats, *pRoot );
				break;
			}
			case kObjectKind:
			{
				// CObjectFrame::LoadRPGStats: the stats sit in the "desc" chunk.
				SObjectRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status, "desc" ) )
					return status;
				// An object has no name of its own in its stats: it is its folder's.
				szKeyName = statsFile.parent_path().filename().string();
				ObjectStatsToTree( rpgStats, *pRoot );
				objectFrame = ObjectFrameOf( rpgStats );
				bObjectFrame = true;
				break;
			}
			case kBuildingKind:
			{
				// CBuildingFrame::LoadRPGStats: the stats sit in the "desc" chunk and
				// the engine's own operator& reads them.
				if ( !ReadRuntimeStats( statsFile, buildingStats, pSession, status, "desc" ) )
					return status;
				// Twelve shipped europe/summer buildings carry an empty KeyName in a valid desc; the
				// name only labels the message, so the folder's stands in and the stats keep theirs.
				szKeyName = buildingStats.szKeyName.empty() ? statsFile.parent_path().filename().string() : buildingStats.szKeyName;
				NResourceModel::BuildingStatsToTree( buildingStats, *pRoot );
				bBuildingFrame = true;
				break;
			}
			case kBridgeKind:
			{
				// CBridgeFrame::LoadRPGStats: the engine's own operator& reads it. A
				// bridge has no name in its stats, so it is its folder's, and the
				// grids' tiles are placed with the editor camera, as the export reads
				// them back.
				if ( !ReadRuntimeStats( statsFile, bridgeStats, pSession, status ) )
					return status;
				if ( !bridgeStats.segments.empty() )
					szKeyName = statsFile.parent_path().filename().string();
				NResourceModel::SGroundCamera camera;
				if ( !SceneGroundCamera( camera ) )
					camera = NResourceModel::DefaultEditorCamera();
				if ( !szKeyName.empty() )
					NResourceModel::BridgeStatsToTree( bridgeStats, *pRoot, NResourceModel::GridProjection( camera ), szKeyName );
				bBridgeFrame = true;
				break;
			}
			case kParticleKind:
			{
				// CParticleFrame::LoadRPGStats: KeyData/ComplexParticleSource picks the
				// struct and the engine's own operator& reads it. A source has no name
				// in its stats: it is its file's. The structs are shared resources, so
				// they live on the heap as the engine reads them.
				SParticleSourceType sourceType;
				if ( !ReadRuntimeStats( statsFile, sourceType, pSession, status, "KeyData" ) )
					return status;
				if ( !sourceType.bFound )
				{
					pSession->szMessage = statsFile.string() + " has no KeyData chunk: it is not a particle source";
					return BK_EDITOR_FAILED;
				}
				szKeyName = statsFile.stem().string();
				if ( sourceType.bComplexParticleSource )
				{
					CPtr<SSmokinParticleSourceData> pStats = new SSmokinParticleSourceData();
					if ( !ReadRuntimeStats( statsFile, *pStats, pSession, status, "KeyData" ) )
						return status;
					NResourceModel::SParticleRawFile raw;
					if ( !ReadRuntimeStats( statsFile, raw, pSession, status, "KeyData" ) )
						return status;
					NResourceModel::ParticleStatsToTree( *pStats, raw, *pRoot, szKeyName );
				}
				else
				{
					CPtr<SParticleSourceData> pStats = new SParticleSourceData();
					if ( !ReadRuntimeStats( statsFile, *pStats, pSession, status, "KeyData" ) )
						return status;
					NResourceModel::SParticleRawFile raw;
					if ( !ReadRuntimeStats( statsFile, raw, pSession, status, "KeyData" ) )
						return status;
					NResourceModel::ParticleStatsToTree( *pStats, raw, *pRoot, szKeyName );
				}
				break;
			}
			case kRoadKind:
			case kRiverKind:
			{
				// C3DRoadFrame::LoadRPGStats: the engine's own operator& reads the
				// VSODescription chunk. The road or river has no name in its stats:
				// it is its file's. A file with no such chunk leaves the type unknown.
				SVectorStripeObjectDesc vsoDesc;
				SVsoChunk chunk( vsoDesc );
				if ( !ReadRuntimeStats( statsFile, chunk, pSession, status, "VSODescription" ) )
					return status;
				if ( !chunk.bFound )
				{
					pSession->szMessage = statsFile.string() + " is not a " + ( kind == kRoadKind ? "3D road" : "3D river" ) + ": it has no VSODescription";
					return BK_EDITOR_REFUSED;
				}
				// MFC's river export never set the type (TYPE_UNKNOUN), so only a type
				// that is plainly the other kind's is refused.
				const int nType = vsoDesc.eType;
				const bool bRoadType = nType == SVectorStripeObjectDesc::TYPE_ROAD || nType == SVectorStripeObjectDesc::TYPE_RAILROAD;
				if ( nType != SVectorStripeObjectDesc::TYPE_UNKNOUN && bRoadType != ( kind == kRoadKind ) )
				{
					pSession->szMessage = statsFile.string() + " is a " + ( bRoadType ? "road" : "river" ) + " (Type=" + std::to_string( nType ) + "), not a ." + szExtension;
					return BK_EDITOR_REFUSED;
				}
				szKeyName = statsFile.stem().string();
				if ( kind == kRoadKind )
					NResourceModel::Road3DStatsToTree( vsoDesc, *pRoot );
				else
					NResourceModel::River3DStatsToTree( vsoDesc, *pRoot );
				break;
				if ( kind == kRoadKind )
					NResourceModel::Road3DStatsToTree( vsoDesc, *pRoot );
				else
					NResourceModel::River3DStatsToTree( vsoDesc, *pRoot );
				break;
			}
			case kFenceKind:
			{
				// CFenceFrame::LoadRPGStats: the engine's own operator& reads it. The
				// segments' tiles are placed with the editor camera, as the export
				// reads them back.
				SFenceRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				szKeyName = rpgStats.szKeyName;
				NResourceModel::SGroundCamera camera;
				if ( !SceneGroundCamera( camera ) )
					camera = NResourceModel::DefaultEditorCamera();
				if ( !szKeyName.empty() )
					FenceStatsToTree( rpgStats, *pRoot, NResourceModel::GridProjection( camera ) );
				break;
			}
			case kMedalKind:
			{
				SMedalStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				// A medal has no name of its own in its stats: it is its folder's.
				szKeyName = rpgStats.szHeaderText.empty() && rpgStats.szTexture.empty() ? std::string() : statsFile.parent_path().filename().string();
				MedalStatsToTree( rpgStats, *pRoot );
				szMedalExportFile = MedalExportFileName( statsFile );
				bMedalFrame = !szMedalExportFile.empty();
				break;
			}
			default:
			{
				// CAnimationFrame::LoadRPGStats: the engine's own operator& reads it.
				SInfantryRPGStats rpgStats;
				if ( !ReadRuntimeStats( statsFile, rpgStats, pSession, status ) )
					return status;
				szKeyName = rpgStats.szKeyName;
				if ( !szKeyName.empty() )
					InfantryStatsToTree( rpgStats, *pRoot );
				break;
			}
		}
		if ( szKeyName.empty() )
		{
			pSession->szMessage = statsFile.string() + " holds no " + szExtension + " RPG stats";
			return BK_EDITOR_DATA_MISSING;
		}
		auto pProject = std::make_unique<NResourceModel::Project>();
		pProject->document.hasDeclaration = true;
		pProject->document.declaration = " version=\"1.0\"";
		pProject->document.root.kind = NResourceXml::Node::Element;
		pProject->document.root.name = kKindTable[kind].pszTag;
		pProject->root = std::move( pRoot );
		if ( bObjectFrame || bBuildingFrame || bBridgeFrame || bMedalFrame )
		{
			// The frame's chunks sit beside the tree and only a project read from
			// text keeps such elements (in the root's layout), so the imported
			// tree is rendered, given its own_data and desc, and read back.
			std::string szRendered = NResourceModel::Save( *pProject ), szParseError;
			auto pStaged = std::make_unique<NResourceModel::Project>();
			if ( !NResourceModel::Load( szRendered, *pStaged, szParseError ) )
			{
				pSession->szMessage = "cannot stage the imported project: " + szParseError;
				return BK_EDITOR_FAILED;
			}
			if ( bBuildingFrame )
				NResourceModel::WriteBuildingFrameData( pStaged->document.root, buildingStats );
			else if ( bMedalFrame )
			{
				NResourceXml::Node &ownData = FrameChunk( pStaged->document.root, "own_data" );
				NResourceXml::Node file = NewElement( "export_file_name" );
				NResourceXml::Node text;
				text.kind = NResourceXml::Node::Text;
				text.text = szMedalExportFile;
				file.children.push_back( text );
				ownData.children.push_back( file );
			}
			else if ( bBridgeFrame )
			{
				NResourceModel::SGroundCamera camera;
				if ( !SceneGroundCamera( camera ) )
					camera = NResourceModel::DefaultEditorCamera();
				NResourceModel::WriteBridgeFrameData( pStaged->document.root, bridgeStats, NResourceModel::GridProjection( camera ) );
			}
			else
				NResourceModel::WriteObjectFrameData( *pStaged, objectFrame );
			szRendered = NResourceXml::Serialise( pStaged->document );
			pProject = std::make_unique<NResourceModel::Project>();
			if ( !NResourceModel::Load( szRendered, *pProject, szParseError ) )
			{
				pSession->szMessage = "cannot stage the imported project: " + szParseError;
				return BK_EDITOR_FAILED;
			}
		}
		ResourceState &state = StateOf( pSession );
		ResetState( state );
		state.pProject = std::move( pProject );
		state.bOpen = true;
		state.nKindOrdinal = kind;
		RebuildIds( state );
		pSession->szMessage = "imported " + szKeyName + " from " + statsFile.string();
		std::string szLocators;
		RebuildMeshLocators( state, szLocators );
		if ( !szLocators.empty() )
			pSession->szMessage += "; " + szLocators;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResTileSetImport( BkResSession *pSession, const char *pszPath, int nCrossets, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount != nullptr )
			*pnCount = 0;
		if ( pszPath == nullptr || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( GetSLS() == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		return nCrossets != 0 ? ImportCrossets( pSession, pszPath, pnCount ) : ImportTerrains( pSession, pszPath, pnCount );
	} );
}

BkEditorStatus BkResTileSetAddTile( BkResSession *pSession, int nParentId, const char *pszPicture, int *pnOutId )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnOutId != nullptr )
			*pnOutId = 0;
		ResourceState &state = StateOf( pSession );
		if ( !state.bOpen || !state.pProject || !state.pProject->root || state.nKindOrdinal != kTileSetKind )
		{
			pSession->szMessage = "adding a tile is a .til feature: open a tileset project first";
			return BK_EDITOR_REFUSED;
		}
		auto itParent = state.idToItem.find( nParentId );
		if ( itParent == state.idToItem.end() )
		{
			pSession->szMessage = "unknown parent id " + std::to_string( nParentId );
			return BK_EDITOR_REFUSED;
		}
		NResourceModel::CTreeItem *pList = itParent->second;
		if ( pList->GetItemType() == NResourceModel::ETIT_TILESET_TERRAIN_PROPS_ITEM )
			pList = ChildOfType( *pList, NResourceModel::ETIT_TILESET_TILES_ITEM );
		const bool bTerrain = pList != nullptr && pList->GetItemType() == NResourceModel::ETIT_TILESET_TILES_ITEM;
		const bool bCrosset = pList != nullptr && pList->GetItemType() == NResourceModel::ETIT_CROSSET_TILES_ITEM;
		if ( !bTerrain && !bCrosset )
		{
			pSession->szMessage = "node " + std::to_string( nParentId ) + " (\"" + itParent->second->GetDisplayName() + "\") is not a terrain or crosset tiles item: a tile goes under a terrain's Tiles or a crosset group";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// The name is the picture's file name without folder and extension.
		std::string szName = pszPicture != nullptr ? pszPicture : "";
		const std::string::size_type nSlash = szName.find_last_of( "/\\" );
		if ( nSlash != std::string::npos )
			szName.erase( 0, nSlash + 1 );
		const std::string::size_type nDot = szName.find_last_of( '.' );
		if ( nDot != std::string::npos && nDot > 0 )
			szName.resize( nDot );
		if ( szName.empty() )
		{
			pSession->szMessage = "the picture has no file name to name the tile after";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		for ( const auto &pChild : pList->GetChildren() )
			if ( pChild->GetDisplayName() == szName )
			{
				pSession->szMessage = "the tile \"" + szName + "\" is already in \"" + itParent->second->GetDisplayName() + "\"";
				return BK_EDITOR_REFUSED;
			}
		std::unique_ptr<NResourceModel::CTreeItem> pNew;
		if ( bTerrain )
		{
			auto pTile = std::make_unique<NResourceModel::CTileSetTilePropsItem>();
			pTile->nTileIndex = NResourceModel::GetFreeTerrainIndex( *state.pProject->root );
			pNew = std::move( pTile );
		}
		else
		{
			auto pTile = std::make_unique<NResourceModel::CCrossetTilePropsItem>();
			pTile->nCrossIndex = NResourceModel::GetFreeCrossetIndex( *state.pProject->root );
			pNew = std::move( pTile );
		}
		pNew->SetItemName( szName );
		NResourceModel::CTreeItem *pRaw = pNew.get();
		pList->AddChild( std::move( pNew ) );
		RebuildIds( state );
		auto itId = state.itemToId.find( pRaw );
		if ( pnOutId != nullptr )
			*pnOutId = itId == state.itemToId.end() ? 0 : itId->second;
		return BK_EDITOR_OK;
	} );
}

}
