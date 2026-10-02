// The composer record readers shared by the engine-hosted RMG tools (05-09, 05-10):
// the two-pass counted-array reads of BkEditorRmgRead{Container,Graph,FieldSet,
// Template} - size with empty arrays, then exactly what was answered - behind a
// buffer struct that owns the arrays the record points into. Header only; the
// including tool has already included StdAfx.h and bridge.h.
#pragma once
#include <string>
#include <vector>

struct SContainerBuf
{
	BkEditorRmgContainerRecord record;
	std::vector<BkEditorRmgPatch> patches;
	std::vector<int> indices, ids;
	std::vector<BkEditorRmgName> areas;
	SContainerBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.patches = &patches[0];
		record.indices = &indices[0];
		record.scripts.ids = &ids[0];
		record.scripts.areas = &areas[0];
	}
};

// The two passes: size with empty arrays, then exactly what was answered.
static bool ReadContainer( BkEditorSession *pSession, const std::string &szName, SContainerBuf *pOut )
{
	*pOut = SContainerBuf();
	BkEditorStatus status = BkEditorRmgReadContainer( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgContainerRecord &r = pOut->record;
	const int nIndexTotal = r.index_counts[0] + r.index_counts[1] + r.index_counts[2] + r.index_counts[3];
	if ( status != BK_EDITOR_REFUSED || ( r.patch_count == 0 && nIndexTotal == 0 && r.scripts.id_count == 0 && r.scripts.area_count == 0 ) )
		return false;
	const int nPatches = r.patch_count, nIds = r.scripts.id_count, nAreas = r.scripts.area_count;
	pOut->patches.resize( size_t( nPatches > 0 ? nPatches : 1 ) );
	pOut->indices.resize( size_t( nIndexTotal > 0 ? nIndexTotal : 1 ) );
	pOut->ids.resize( size_t( nIds > 0 ? nIds : 1 ) );
	pOut->areas.resize( size_t( nAreas > 0 ? nAreas : 1 ) );
	pOut->Bind();
	pOut->record.patch_capacity = nPatches;
	pOut->record.index_capacity = nIndexTotal;
	pOut->record.scripts.id_capacity = nIds;
	pOut->record.scripts.area_capacity = nAreas;
	status = BkEditorRmgReadContainer( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.patch_count == nPatches && pOut->record.scripts.id_count == nIds && pOut->record.scripts.area_count == nAreas;
}

struct SGraphBuf
{
	BkEditorRmgGraphRecord record;
	std::vector<BkEditorRmgNode> nodes;
	std::vector<BkEditorRmgLink> links;
	std::vector<int> ids;
	std::vector<BkEditorRmgName> areas;
	SGraphBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.nodes = &nodes[0];
		record.links = &links[0];
		record.scripts.ids = &ids[0];
		record.scripts.areas = &areas[0];
	}
};

static bool ReadGraph( BkEditorSession *pSession, const std::string &szName, SGraphBuf *pOut )
{
	*pOut = SGraphBuf();
	BkEditorStatus status = BkEditorRmgReadGraph( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgGraphRecord &r = pOut->record;
	if ( status != BK_EDITOR_REFUSED || ( r.node_count == 0 && r.link_count == 0 && r.scripts.id_count == 0 && r.scripts.area_count == 0 ) )
		return false;
	const int nNodes = r.node_count, nLinks = r.link_count, nIds = r.scripts.id_count, nAreas = r.scripts.area_count;
	pOut->nodes.resize( size_t( nNodes > 0 ? nNodes : 1 ) );
	pOut->links.resize( size_t( nLinks > 0 ? nLinks : 1 ) );
	pOut->ids.resize( size_t( nIds > 0 ? nIds : 1 ) );
	pOut->areas.resize( size_t( nAreas > 0 ? nAreas : 1 ) );
	pOut->Bind();
	pOut->record.node_capacity = nNodes;
	pOut->record.link_capacity = nLinks;
	pOut->record.scripts.id_capacity = nIds;
	pOut->record.scripts.area_capacity = nAreas;
	status = BkEditorRmgReadGraph( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.node_count == nNodes && pOut->record.link_count == nLinks;
}

struct SFieldSetBuf
{
	BkEditorRmgFieldSetRecord record;
	std::vector<BkEditorRmgTileShell> tileShells;
	std::vector<BkEditorRmgWeightedTile> tiles;
	std::vector<BkEditorRmgObjectShell> objectShells;
	std::vector<BkEditorRmgWeightedName> objects;
	SFieldSetBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.tile_shells = tileShells.empty() ? 0 : &tileShells[0];
		record.tiles = tiles.empty() ? 0 : &tiles[0];
		record.object_shells = objectShells.empty() ? 0 : &objectShells[0];
		record.objects = objects.empty() ? 0 : &objects[0];
	}
};

static bool ReadFieldSet( BkEditorSession *pSession, const std::string &szName, SFieldSetBuf *pOut )
{
	*pOut = SFieldSetBuf();
	BkEditorStatus status = BkEditorRmgReadFieldSet( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgFieldSetRecord &r = pOut->record;
	if ( status != BK_EDITOR_REFUSED || ( r.tile_shell_count == 0 && r.tile_total == 0 && r.object_shell_count == 0 && r.object_total == 0 ) )
		return false;
	const int nTileShells = r.tile_shell_count, nTiles = r.tile_total, nObjectShells = r.object_shell_count, nObjects = r.object_total;
	pOut->tileShells.resize( size_t( nTileShells ) );
	pOut->tiles.resize( size_t( nTiles ) );
	pOut->objectShells.resize( size_t( nObjectShells ) );
	pOut->objects.resize( size_t( nObjects ) );
	pOut->Bind();
	pOut->record.tile_shell_capacity = nTileShells;
	pOut->record.tile_capacity = nTiles;
	pOut->record.object_shell_capacity = nObjectShells;
	pOut->record.object_capacity = nObjects;
	status = BkEditorRmgReadFieldSet( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.tile_shell_count == nTileShells && pOut->record.tile_total == nTiles && pOut->record.object_shell_count == nObjectShells && pOut->record.object_total == nObjects;
}

struct STemplateBuf
{
	BkEditorRmgTemplateRecord record;
	std::vector<BkEditorRmgWeightedName> fields, graphs;
	std::vector<BkEditorRmgVso> vso;
	std::vector<unsigned char> diplomacies;
	std::vector<BkEditorUnitCreationRecord> units;
	std::vector<int> ids;
	std::vector<BkEditorRmgName> areas;
	STemplateBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.fields = fields.empty() ? 0 : &fields[0];
		record.graphs = graphs.empty() ? 0 : &graphs[0];
		record.vso = vso.empty() ? 0 : &vso[0];
		record.diplomacies = diplomacies.empty() ? 0 : &diplomacies[0];
		record.units = units.empty() ? 0 : &units[0];
		record.scripts.ids = ids.empty() ? 0 : &ids[0];
		record.scripts.areas = areas.empty() ? 0 : &areas[0];
	}
};

static bool ReadTemplate( BkEditorSession *pSession, const std::string &szName, STemplateBuf *pOut )
{
	*pOut = STemplateBuf();
	BkEditorStatus status = BkEditorRmgReadTemplate( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgTemplateRecord &r = pOut->record;
	if ( status != BK_EDITOR_REFUSED || ( r.field_count == 0 && r.graph_count == 0 && r.vso_count == 0 && r.diplomacy_count == 0 && r.unit_count == 0 && r.scripts.id_count == 0 && r.scripts.area_count == 0 ) )
		return false;
	const int nFields = r.field_count, nGraphs = r.graph_count, nVso = r.vso_count, nDiplomacies = r.diplomacy_count, nUnits = r.unit_count, nIds = r.scripts.id_count, nAreas = r.scripts.area_count;
	pOut->fields.resize( size_t( nFields ) );
	pOut->graphs.resize( size_t( nGraphs ) );
	pOut->vso.resize( size_t( nVso ) );
	pOut->diplomacies.resize( size_t( nDiplomacies ) );
	pOut->units.resize( size_t( nUnits ) );
	pOut->ids.resize( size_t( nIds ) );
	pOut->areas.resize( size_t( nAreas ) );
	pOut->Bind();
	pOut->record.field_capacity = nFields;
	pOut->record.graph_capacity = nGraphs;
	pOut->record.vso_capacity = nVso;
	pOut->record.diplomacy_capacity = nDiplomacies;
	pOut->record.unit_capacity = nUnits;
	pOut->record.scripts.id_capacity = nIds;
	pOut->record.scripts.area_capacity = nAreas;
	status = BkEditorRmgReadTemplate( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.field_count == nFields && pOut->record.graph_count == nGraphs && pOut->record.vso_count == nVso && pOut->record.unit_count == nUnits;
}
