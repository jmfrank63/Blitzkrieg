// The resource editor's C ABI implementation. Every entry point is wrapped in
// the shared Guarded template (guarded.h) so no exception crosses into Zig.
// T01 stubbed every call to BK_EDITOR_OK. T02 fills in the Project+Tree group:
// New/Open/Save (safe-save read-back), Close, KindOf, Lock/LockOwner, Nodes,
// Props, SetProp, InsertNode, MoveNode, DeleteNode, RestoreNode. The remaining
// groups (geometry, references, export, mod, preview, import) stay stubbed and
// are replaced in T03-T06.
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

/* All get/set pairs are stubbed. T04 fills them with the real NResourceModel
   reads. Each returns BK_EDITOR_OK and zeroed outputs for now, which is
   enough for the smoke tier below to link and run. */
BkEditorStatus BkResGetPassabilityCells( BkResSession *pSession, int, unsigned char *, int, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnW != 0 ) *pnW = 0;
		if ( pnH != 0 ) *pnH = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetPassabilityCells( BkResSession *pSession, int, const unsigned char *, int, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetLockedTiles( BkResSession *pSession, int, unsigned char *, int, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnW != 0 ) *pnW = 0;
		if ( pnH != 0 ) *pnH = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetLockedTiles( BkResSession *pSession, int, const unsigned char *, int, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetTransparencyLines( BkResSession *pSession, int, BkResPoint2 *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount != 0 ) *pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetTransparencyLines( BkResSession *pSession, int, const BkResPoint2 *, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetZeroPoint( BkResSession *pSession, int, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 ) { pOut->x = 0; pOut->y = 0; }
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetZeroPoint( BkResSession *pSession, int, const BkResPoint2 * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetEntrance( BkResSession *pSession, int, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 ) { pOut->x = 0; pOut->y = 0; }
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetEntrance( BkResSession *pSession, int, const BkResPoint2 * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

#define BKRES_GET_AIMED_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResAimedPoint *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_AIMED_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResAimedPoint *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_AIMED_STUB( BkResGetShootPoints )
BKRES_SET_AIMED_STUB( BkResSetShootPoints )
BKRES_GET_AIMED_STUB( BkResGetFirePoints )
BKRES_SET_AIMED_STUB( BkResSetFirePoints )
BKRES_GET_AIMED_STUB( BkResGetSmokePoints )
BKRES_SET_AIMED_STUB( BkResSetSmokePoints )
BKRES_GET_AIMED_STUB( BkResGetDirectedExplosionPoints )
BKRES_SET_AIMED_STUB( BkResSetDirectedExplosionPoints )
#undef BKRES_GET_AIMED_STUB
#undef BKRES_SET_AIMED_STUB

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
