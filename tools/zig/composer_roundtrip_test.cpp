// The composer round trip (M3, 05-09 and 05-10, D-07/D-40.4): every shipped
// template, graph, container and field set - 43, 102, 404 and 27 in the shipped
// data - is read through the editor's own composer path (BkEditorRmgReadTemplate /
// ReadGraph / ReadContainer / ReadFieldSet, the engine's SRMTemplate / SRMGraph /
// SRMContainer / SRMFieldSet serialisers behind the record structs), written
// back under a scratch name in the scratch user RMG root, read again and compared:
//
//   1. what the bridge reads is what the engine's own LoadDataResource reads
//      (the record path loses nothing);
//   2. what is written reads back equal (the write path loses nothing - the bridge
//      also reads every write back before it says OK);
//   3. writing what was read back writes the SAME BYTES (the serialisers reach a
//      fixed point, so a composer Save is stable) - for a template that includes
//      the QuickLoadMapInfo entry written beside it, which is also compared with
//      what SQuickLoadMapInfo::FillFromRMTemplate makes of the template.
//
// The data-only tier: nothing here needs a map or a picture, only the engine's
// storage, which the bridge's start provides. It needs a hidden SDL window and a
// GPU device like the engine tier, and skips honestly where there is none.
//
// argv: <installation> <scratch>
#include "StdAfx.h"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include "../../Sources/src/BkMemory/bk_memory_sdl.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/Platform/Paths.h"
#include "../../Sources/src/RandomMapGen/RMG_Types.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/Resource_Types.h"
#include "rmg_record_io.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#endif

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

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

static bool SamePath( const char *pszLeft, const char *pszRight )
{
#if defined(_WIN32) || defined(_WIN64)
	char left[_MAX_PATH], right[_MAX_PATH];
	if ( _fullpath( left, pszLeft, _MAX_PATH ) == 0 || _fullpath( right, pszRight, _MAX_PATH ) == 0 )
		return false;
	return _stricmp( left, right ) == 0;
#else
	char left[PATH_MAX], right[PATH_MAX];
	if ( realpath( pszLeft, left ) == 0 || realpath( pszRight, right ) == 0 )
		return false;
	return strcmp( left, right ) == 0;
#endif
}

static std::vector<std::string> ListNames( BkEditorSession *pSession, int nKind )
{
	std::vector<std::string> names;
	int nTotal = 0;
	BkEditorListRmg( pSession, nKind, 0, 0, &nTotal );
	if ( nTotal <= 0 )
		return names;
	std::vector<BkEditorRmgName> entries( static_cast<size_t>( nTotal ) );
	int nGot = 0;
	if ( BkEditorListRmg( pSession, nKind, &( entries[0] ), nTotal, &nGot ) != BK_EDITOR_OK )
		return names;
	for ( int i = 0; i < nGot && i < nTotal; ++i )
		names.push_back( entries[static_cast<size_t>( i )].name );
	return names;
}

static bool FileBytes( const std::string &szPath, std::vector<char> *pBytes )
{
	std::ifstream file( szPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

// The user RMG root's file for a storage name, in the host's separators.
static std::string RmgFile( const std::filesystem::path &rUserRoot, std::string szName )
{
	for ( char &c : szName )
		if ( c == '\\' )
			c = '/';
	return ( rUserRoot / "rmg" / ( szName + ".xml" ) ).string();
}

static bool FloatsEqual( float fLeft, float fRight )
{
	return fabsf( fLeft - fRight ) <= 1e-5f * ( fabsf( fLeft ) > fabsf( fRight ) ? ( fabsf( fLeft ) > 1.0f ? fabsf( fLeft ) : 1.0f ) : ( fabsf( fRight ) > 1.0f ? fabsf( fRight ) : 1.0f ) );
}

// --- Containers -------------------------------------------------------------

static bool SameContainerRecords( const SContainerBuf &rA, const SContainerBuf &rB )
{
	const BkEditorRmgContainerRecord &a = rA.record;
	const BkEditorRmgContainerRecord &b = rB.record;
	if ( a.size_x != b.size_x || a.size_y != b.size_y || a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || a.patch_count != b.patch_count ||
	     a.scripts.id_count != b.scripts.id_count || a.scripts.area_count != b.scripts.area_count )
		return false;
	for ( int d = 0; d < 4; ++d )
		if ( a.index_counts[d] != b.index_counts[d] )
			return false;
	for ( int i = 0; i < a.patch_count; ++i )
		if ( strcmp( a.patches[i].name, b.patches[i].name ) != 0 || strcmp( a.patches[i].place, b.patches[i].place ) != 0 || a.patches[i].size_x != b.patches[i].size_x || a.patches[i].size_y != b.patches[i].size_y )
			return false;
	const int nIndices = a.index_counts[0] + a.index_counts[1] + a.index_counts[2] + a.index_counts[3];
	for ( int i = 0; i < nIndices; ++i )
		if ( a.indices[i] != b.indices[i] )
			return false;
	for ( int i = 0; i < a.scripts.id_count; ++i )
		if ( a.scripts.ids[i] != b.scripts.ids[i] )
			return false;
	for ( int i = 0; i < a.scripts.area_count; ++i )
		if ( strcmp( a.scripts.areas[i].name, b.scripts.areas[i].name ) != 0 )
			return false;
	return true;
}

// The bridge's record against the engine's own load of the same file.
static bool ContainerIsEngines( const SContainerBuf &rBuf, const SRMContainer &rC )
{
	const BkEditorRmgContainerRecord &r = rBuf.record;
	if ( r.patch_count != int( rC.patches.size() ) || r.size_x != rC.size.x || r.size_y != rC.size.y || r.season != rC.nSeason || std::string( r.season_folder ) != rC.szSeasonFolder ||
	     r.scripts.id_count != int( rC.usedScriptIDs.size() ) || r.scripts.area_count != int( rC.usedScriptAreas.size() ) )
		return false;
	for ( int i = 0; i < r.patch_count; ++i )
		if ( rC.patches[size_t( i )].szFileName != r.patches[i].name || rC.patches[size_t( i )].szPlace != r.patches[i].place ||
		     rC.patches[size_t( i )].size.x != r.patches[i].size_x || rC.patches[size_t( i )].size.y != r.patches[i].size_y )
			return false;
	int nAt = 0;
	for ( int d = 0; d < 4; ++d )
	{
		if ( r.index_counts[d] != int( rC.indices[d].size() ) )
			return false;
		for ( int i = 0; i < r.index_counts[d]; ++i )
			if ( r.indices[nAt++] != rC.indices[d][size_t( i )] )
				return false;
	}
	int nId = 0;
	for ( CUsedScriptIDs::const_iterator it = rC.usedScriptIDs.begin(); it != rC.usedScriptIDs.end(); ++it )
		if ( r.scripts.ids[nId++] != *it )
			return false;
	int nArea = 0;
	for ( CUsedScriptAreas::const_iterator it = rC.usedScriptAreas.begin(); it != rC.usedScriptAreas.end(); ++it )
		if ( *it != r.scripts.areas[nArea++].name )
			return false;
	return true;
}

// --- Graphs -----------------------------------------------------------------

static bool SameGraphRecords( const SGraphBuf &rA, const SGraphBuf &rB )
{
	const BkEditorRmgGraphRecord &a = rA.record;
	const BkEditorRmgGraphRecord &b = rB.record;
	if ( a.size_x != b.size_x || a.size_y != b.size_y || a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || a.node_count != b.node_count || a.link_count != b.link_count ||
	     a.scripts.id_count != b.scripts.id_count || a.scripts.area_count != b.scripts.area_count )
		return false;
	for ( int i = 0; i < a.node_count; ++i )
		if ( a.nodes[i].x1 != b.nodes[i].x1 || a.nodes[i].y1 != b.nodes[i].y1 || a.nodes[i].x2 != b.nodes[i].x2 || a.nodes[i].y2 != b.nodes[i].y2 || strcmp( a.nodes[i].container, b.nodes[i].container ) != 0 )
			return false;
	for ( int i = 0; i < a.link_count; ++i )
		if ( a.links[i].a != b.links[i].a || a.links[i].b != b.links[i].b || a.links[i].type != b.links[i].type || a.links[i].parts != b.links[i].parts || strcmp( a.links[i].desc, b.links[i].desc ) != 0 ||
		     !FloatsEqual( a.links[i].radius, b.links[i].radius ) || !FloatsEqual( a.links[i].min_length, b.links[i].min_length ) ||
		     !FloatsEqual( a.links[i].distance, b.links[i].distance ) || !FloatsEqual( a.links[i].disturbance, b.links[i].disturbance ) )
			return false;
	for ( int i = 0; i < a.scripts.id_count; ++i )
		if ( a.scripts.ids[i] != b.scripts.ids[i] )
			return false;
	for ( int i = 0; i < a.scripts.area_count; ++i )
		if ( strcmp( a.scripts.areas[i].name, b.scripts.areas[i].name ) != 0 )
			return false;
	return true;
}

static bool GraphIsEngines( const SGraphBuf &rBuf, const SRMGraph &rG )
{
	const BkEditorRmgGraphRecord &r = rBuf.record;
	if ( r.node_count != int( rG.nodes.size() ) || r.link_count != int( rG.links.size() ) || r.size_x != rG.size.x || r.size_y != rG.size.y || r.season != rG.nSeason ||
	     std::string( r.season_folder ) != rG.szSeasonFolder || r.scripts.id_count != int( rG.usedScriptIDs.size() ) || r.scripts.area_count != int( rG.usedScriptAreas.size() ) )
		return false;
	for ( int i = 0; i < r.node_count; ++i )
	{
		const SRMGraphNode &n = rG.nodes[size_t( i )];
		if ( r.nodes[i].x1 != n.rect.minx || r.nodes[i].y1 != n.rect.miny || r.nodes[i].x2 != n.rect.maxx || r.nodes[i].y2 != n.rect.maxy || n.szContainerFileName != r.nodes[i].container )
			return false;
	}
	for ( int i = 0; i < r.link_count; ++i )
	{
		const SRMGraphLink &l = rG.links[size_t( i )];
		if ( r.links[i].a != l.link.a || r.links[i].b != l.link.b || r.links[i].type != l.nType || l.szDescFileName != r.links[i].desc || r.links[i].parts != l.nParts ||
		     r.links[i].radius != l.fRadius || r.links[i].min_length != l.fMinLength || r.links[i].distance != l.fDistance || r.links[i].disturbance != l.fDisturbance )
			return false;
	}
	int nId = 0;
	for ( CUsedScriptIDs::const_iterator it = rG.usedScriptIDs.begin(); it != rG.usedScriptIDs.end(); ++it )
		if ( r.scripts.ids[nId++] != *it )
			return false;
	int nArea = 0;
	for ( CUsedScriptAreas::const_iterator it = rG.usedScriptAreas.begin(); it != rG.usedScriptAreas.end(); ++it )
		if ( *it != r.scripts.areas[nArea++].name )
			return false;
	return true;
}


// --- Field sets -------------------------------------------------------------

static bool SameFieldSetRecords( const SFieldSetBuf &rA, const SFieldSetBuf &rB )
{
	const BkEditorRmgFieldSetRecord &a = rA.record;
	const BkEditorRmgFieldSetRecord &b = rB.record;
	if ( a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || strcmp( a.profile, b.profile ) != 0 || !FloatsEqual( a.height, b.height ) ||
	     a.pattern_min != b.pattern_min || a.pattern_max != b.pattern_max || !FloatsEqual( a.positive_ratio, b.positive_ratio ) ||
	     a.tile_shell_count != b.tile_shell_count || a.tile_total != b.tile_total || a.object_shell_count != b.object_shell_count || a.object_total != b.object_total )
		return false;
	for ( int i = 0; i < a.tile_shell_count; ++i )
		if ( !FloatsEqual( a.tile_shells[i].width, b.tile_shells[i].width ) || a.tile_shells[i].tile_count != b.tile_shells[i].tile_count )
			return false;
	for ( int i = 0; i < a.tile_total; ++i )
		if ( a.tiles[i].tile != b.tiles[i].tile || a.tiles[i].weight != b.tiles[i].weight )
			return false;
	for ( int i = 0; i < a.object_shell_count; ++i )
		if ( !FloatsEqual( a.object_shells[i].width, b.object_shells[i].width ) || a.object_shells[i].step != b.object_shells[i].step ||
		     !FloatsEqual( a.object_shells[i].ratio, b.object_shells[i].ratio ) || a.object_shells[i].object_count != b.object_shells[i].object_count )
			return false;
	for ( int i = 0; i < a.object_total; ++i )
		if ( strcmp( a.objects[i].name, b.objects[i].name ) != 0 || a.objects[i].weight != b.objects[i].weight )
			return false;
	return true;
}

static bool FieldSetIsEngines( const SFieldSetBuf &rBuf, const SRMFieldSet &rF )
{
	const BkEditorRmgFieldSetRecord &r = rBuf.record;
	if ( r.season != rF.nSeason || std::string( r.season_folder ) != rF.szSeasonFolder || std::string( r.profile ) != rF.szProfileFileName ||
	     r.height != rF.fHeight || r.pattern_min != rF.patternSize.min || r.pattern_max != rF.patternSize.max || r.positive_ratio != rF.fPositiveRatio ||
	     r.tile_shell_count != int( rF.tilesShells.size() ) || r.object_shell_count != int( rF.objectsShells.size() ) )
		return false;
	int nTileAt = 0, nObjectAt = 0;
	for ( int i = 0; i < r.tile_shell_count; ++i )
	{
		const SRMTileSetShell &shell = rF.tilesShells[size_t( i )];
		if ( r.tile_shells[i].width != shell.fWidth || r.tile_shells[i].tile_count != shell.tiles.size() )
			return false;
		for ( int k = 0; k < shell.tiles.size(); ++k, ++nTileAt )
			if ( r.tiles[nTileAt].tile != shell.tiles[k] || r.tiles[nTileAt].weight != shell.tiles.GetWeight( k ) )
				return false;
	}
	for ( int i = 0; i < r.object_shell_count; ++i )
	{
		const SRMObjectSetShell &shell = rF.objectsShells[size_t( i )];
		if ( r.object_shells[i].width != shell.fWidth || r.object_shells[i].step != shell.nBetweenDistance || r.object_shells[i].ratio != shell.fRatio || r.object_shells[i].object_count != shell.objects.size() )
			return false;
		for ( int k = 0; k < shell.objects.size(); ++k, ++nObjectAt )
			if ( shell.objects[k] != r.objects[nObjectAt].name || r.objects[nObjectAt].weight != shell.objects.GetWeight( k ) )
				return false;
	}
	return r.tile_total == nTileAt && r.object_total == nObjectAt;
}

// --- Templates --------------------------------------------------------------

static bool SameUnitRecords( const BkEditorUnitCreationRecord &a, const BkEditorUnitCreationRecord &b )
{
	if ( strcmp( a.party, b.party ) != 0 || strcmp( a.paratroop_name, b.paratroop_name ) != 0 || a.paratroop_count != b.paratroop_count || a.relax_time != b.relax_time || a.appear_count != b.appear_count )
		return false;
	for ( int k = 0; k < 5; ++k )
		if ( strcmp( a.aircraft[k].name, b.aircraft[k].name ) != 0 || a.aircraft[k].formation_size != b.aircraft[k].formation_size || a.aircraft[k].count != b.aircraft[k].count )
			return false;
	for ( int k = 0; k < a.appear_count; ++k )
		if ( !FloatsEqual( a.appear[k].x, b.appear[k].x ) || !FloatsEqual( a.appear[k].y, b.appear[k].y ) || !FloatsEqual( a.appear[k].z, b.appear[k].z ) )
			return false;
	return true;
}

static bool SameTemplateRecords( const STemplateBuf &rA, const STemplateBuf &rB )
{
	const BkEditorRmgTemplateRecord &a = rA.record;
	const BkEditorRmgTemplateRecord &b = rB.record;
	if ( a.size_x != b.size_x || a.size_y != b.size_y || a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || strcmp( a.place, b.place ) != 0 ||
	     a.default_field != b.default_field || a.mission_index != b.mission_index || a.game_type != b.game_type || a.attacking_side != b.attacking_side ||
	     !FloatsEqual( a.camera[0], b.camera[0] ) || !FloatsEqual( a.camera[1], b.camera[1] ) || !FloatsEqual( a.camera[2], b.camera[2] ) ||
	     strcmp( a.script_file, b.script_file ) != 0 || strcmp( a.chapter_name, b.chapter_name ) != 0 || strcmp( a.forest_circle_sounds, b.forest_circle_sounds ) != 0 ||
	     strcmp( a.forest_ambient_sounds, b.forest_ambient_sounds ) != 0 || strcmp( a.mod_name, b.mod_name ) != 0 || strcmp( a.mod_version, b.mod_version ) != 0 ||
	     a.field_count != b.field_count || a.graph_count != b.graph_count || a.vso_count != b.vso_count || a.diplomacy_count != b.diplomacy_count || a.unit_count != b.unit_count ||
	     a.scripts.id_count != b.scripts.id_count || a.scripts.area_count != b.scripts.area_count )
		return false;
	for ( int i = 0; i < a.field_count; ++i )
		if ( strcmp( a.fields[i].name, b.fields[i].name ) != 0 || a.fields[i].weight != b.fields[i].weight )
			return false;
	for ( int i = 0; i < a.graph_count; ++i )
		if ( strcmp( a.graphs[i].name, b.graphs[i].name ) != 0 || a.graphs[i].weight != b.graphs[i].weight )
			return false;
	for ( int i = 0; i < a.vso_count; ++i )
		if ( strcmp( a.vso[i].name, b.vso[i].name ) != 0 || a.vso[i].weight != b.vso[i].weight || !FloatsEqual( a.vso[i].width, b.vso[i].width ) || !FloatsEqual( a.vso[i].opacity, b.vso[i].opacity ) )
			return false;
	for ( int i = 0; i < a.diplomacy_count; ++i )
		if ( a.diplomacies[i] != b.diplomacies[i] )
			return false;
	for ( int i = 0; i < a.unit_count; ++i )
		if ( !SameUnitRecords( a.units[i], b.units[i] ) )
			return false;
	for ( int i = 0; i < a.scripts.id_count; ++i )
		if ( a.scripts.ids[i] != b.scripts.ids[i] )
			return false;
	for ( int i = 0; i < a.scripts.area_count; ++i )
		if ( strcmp( a.scripts.areas[i].name, b.scripts.areas[i].name ) != 0 )
			return false;
	return true;
}

// The bridge's record against the engine's own load of the same file.
static bool TemplateIsEngines( const STemplateBuf &rBuf, const SRMTemplate &rT )
{
	const BkEditorRmgTemplateRecord &r = rBuf.record;
	if ( r.size_x != rT.size.x || r.size_y != rT.size.y || r.season != rT.nSeason || std::string( r.season_folder ) != rT.szSeasonFolder || std::string( r.place ) != rT.szPlace ||
	     r.default_field != rT.nDefaultFieldIndex || r.mission_index != rT.nMissionIndex || r.game_type != rT.nType || r.attacking_side != rT.nAttackingSide ||
	     !FloatsEqual( r.camera[0], rT.vCameraAnchor.x ) || !FloatsEqual( r.camera[1], rT.vCameraAnchor.y ) || !FloatsEqual( r.camera[2], rT.vCameraAnchor.z ) ||
	     std::string( r.script_file ) != rT.szScriptFile || std::string( r.chapter_name ) != rT.szChapterName ||
	     std::string( r.forest_circle_sounds ) != rT.szForestCircleSounds || std::string( r.forest_ambient_sounds ) != rT.szForestAmbientSounds ||
	     std::string( r.mod_name ) != rT.szMODName || std::string( r.mod_version ) != rT.szMODVersion ||
	     r.field_count != rT.fields.size() || r.graph_count != rT.graphs.size() || r.vso_count != rT.vso.size() ||
	     r.diplomacy_count != int( rT.diplomacies.size() ) || r.unit_count != int( rT.unitCreation.units.size() ) ||
	     r.scripts.id_count != int( rT.usedScriptIDs.size() ) || r.scripts.area_count != int( rT.usedScriptAreas.size() ) )
		return false;
	for ( int i = 0; i < r.field_count; ++i )
		if ( rT.fields[i] != r.fields[i].name || rT.fields.GetWeight( i ) != r.fields[i].weight )
			return false;
	for ( int i = 0; i < r.graph_count; ++i )
		if ( rT.graphs[i] != r.graphs[i].name || rT.graphs.GetWeight( i ) != r.graphs[i].weight )
			return false;
	for ( int i = 0; i < r.vso_count; ++i )
		if ( rT.vso[i].szVSODescFileName != r.vso[i].name || rT.vso.GetWeight( i ) != r.vso[i].weight || !FloatsEqual( rT.vso[i].fWidth, r.vso[i].width ) || !FloatsEqual( rT.vso[i].fOpacity, r.vso[i].opacity ) )
			return false;
	for ( int i = 0; i < r.diplomacy_count; ++i )
		if ( rT.diplomacies[size_t( i )] != r.diplomacies[i] )
			return false;
	for ( int i = 0; i < r.unit_count; ++i )
	{
		const SUnitCreation &u = rT.unitCreation.units[size_t( i )];
		const BkEditorUnitCreationRecord &c = r.units[i];
		if ( u.szPartyName != c.party || u.aviation.szParadropSquadName != c.paratroop_name || u.aviation.nParadropSquadCount != c.paratroop_count || u.aviation.nRelaxTime != c.relax_time ||
		     int( u.aviation.vAppearPoints.size() ) != c.appear_count || int( u.aviation.aircrafts.size() ) != 5 )
			return false;
		for ( int k = 0; k < 5; ++k )
			if ( u.aviation.aircrafts[size_t( k )].szName != c.aircraft[k].name || u.aviation.aircrafts[size_t( k )].nFormationSize != c.aircraft[k].formation_size || u.aviation.aircrafts[size_t( k )].nPlanes != c.aircraft[k].count )
				return false;
		int nPoint = 0;
		for ( std::list<CVec3>::const_iterator it = u.aviation.vAppearPoints.begin(); it != u.aviation.vAppearPoints.end(); ++it, ++nPoint )
			if ( !FloatsEqual( it->x, c.appear[nPoint].x ) || !FloatsEqual( it->y, c.appear[nPoint].y ) || !FloatsEqual( it->z, c.appear[nPoint].z ) )
				return false;
	}
	int nId = 0;
	for ( CUsedScriptIDs::const_iterator it = rT.usedScriptIDs.begin(); it != rT.usedScriptIDs.end(); ++it )
		if ( r.scripts.ids[nId++] != *it )
			return false;
	int nArea = 0;
	for ( CUsedScriptAreas::const_iterator it = rT.usedScriptAreas.begin(); it != rT.usedScriptAreas.end(); ++it )
		if ( *it != r.scripts.areas[nArea++].name )
			return false;
	return true;
}

// The QuickLoadMapInfo entry of a template file is what FillFromRMTemplate makes of its template.
static bool QuickLoadIsTemplates( const std::string &szName )
{
	SRMTemplate tpl;
	SQuickLoadMapInfo quick, wanted;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_TEMPLATE_XML_NAME, tpl ) || !LoadDataResource( szName, "", false, 0, RMGC_QUICK_LOAD_MAP_INFO_NAME, quick ) )
		return false;
	wanted.FillFromRMTemplate( tpl );
	return quick.playerParties == wanted.playerParties && quick.diplomacies == wanted.diplomacies && quick.size == wanted.size && quick.nType == wanted.nType &&
	       quick.nAttackingSide == wanted.nAttackingSide && quick.szMODName == wanted.szMODName && quick.szMODVersion == wanted.szMODVersion;
}

// The two files hold the same bytes (read whole, fresh from disk).
static bool SameFileBytes( const std::string &szLeft, const std::string &szRight )
{
	std::vector<char> left, right;
	return FileBytes( szLeft, &left ) && FileBytes( szRight, &right ) && left == right;
}

// A skip is a pass that checked nothing, so CI sets BK_REQUIRE_ENGINE=1 on the runners that
// do have a video driver, the staged game and a GPU device: there a skip is the runner
// regressing, not a green result (05-REVIEW WR-D03). Unset (a laptop with no display), a
// skip stays an exit code of 0.
static int SkipOrFail( const char *pszTool, const std::string &szWhy )
{
	const char *pszRequire = getenv( "BK_REQUIRE_ENGINE" );
	if ( pszRequire != 0 && *pszRequire != 0 && strcmp( pszRequire, "0" ) != 0 )
	{
		printf( "FAIL: %s: %s, and BK_REQUIRE_ENGINE is set\n", pszTool, szWhy.c_str() );
		return 1;
	}
	printf( "%s: skipped: %s\n", pszTool, szWhy.c_str() );
	return 0;
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
	// Hooks first: the first SDL call of the process must already allocate through BkMemory.
	BkMemoryInstallSdlFunctions();
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( strstr( pszError, "video driver" ) != 0 || strstr( pszError, "No available" ) != 0 )
		{
			return SkipOrFail( "composer-roundtrip", std::string( "no video driver (" ) + pszError + ")" );
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "composer-roundtrip-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::filesystem::path scratch = argc > 2 ? argv[2] : szSelfDir;
	std::filesystem::create_directories( scratch );
	if ( !std::filesystem::exists( std::string( pszRoot ) + "/Data/consts.xml" ) )
	{
		const int nSkipped = SkipOrFail( "composer-roundtrip", std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( !Check( SamePath( szSelfDir.c_str(), pszRoot ), "the executable lives in the installation it tests" ) )
	{
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		const int nSkipped = SkipOrFail( "composer-roundtrip", std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const std::string szBase = NPlatform::Paths::BaseRoot();
		const std::string szOriginalUser = NPlatform::Paths::UserRoot();
		const std::filesystem::path root = scratch / "composer-roundtrip";
		const std::filesystem::path user = root / "user";
		std::error_code error;
		std::filesystem::remove_all( root, error );
		const std::string szUser = user.string() + "/";
		NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szUser.c_str() );

		// The shipped files come from the folder scan (D-08), taken before anything
		// is written under the user root.
		const std::vector<std::string> containers = ListNames( pSession, 3 );
		const std::vector<std::string> graphs = ListNames( pSession, 2 );
		const std::vector<std::string> fieldSets = ListNames( pSession, 0 );
		const std::vector<std::string> templates = ListNames( pSession, 1 );
		// The shipped counts are lower bounds: the loops below compare against what the scan
		// found, so a scan that returned a subset (a changed ListRmg filter, a sparse checkout
		// without a Data/Scenarios subfolder) would otherwise pass on whatever it did find.
		Check( templates.size() >= 43 && graphs.size() >= 102 && containers.size() >= 404 && fieldSets.size() >= 27,
		       ( "the scan finds the shipped records (" + std::to_string( templates.size() ) + " templates, " + std::to_string( graphs.size() ) +
		         " graphs, " + std::to_string( containers.size() ) + " containers, " + std::to_string( fieldSets.size() ) + " field sets; at least 43/102/404/27)" ).c_str() );
		int nContainersOk = 0, nGraphsOk = 0, nFieldSetsOk = 0, nTemplatesOk = 0;
		for ( size_t i = 0; i < containers.size(); ++i )
		{
			const std::string &szName = containers[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\containers\\roundtrip\\c%04d", int( i ) );
			sprintf( szAgain, "scenarios\\containers\\roundtrip\\c%04d_b", int( i ) );
			SContainerBuf first, second, third;
			SRMContainer direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_CONTAINER_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadContainer( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ContainerIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteContainer( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadContainer( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameContainerRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( BkEditorRmgWriteContainer( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadContainer( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameContainerRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes" );
			if ( bOk )
				++nContainersOk;
		}
		for ( size_t i = 0; i < graphs.size(); ++i )
		{
			const std::string &szName = graphs[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\graphs\\roundtrip\\g%04d", int( i ) );
			sprintf( szAgain, "scenarios\\graphs\\roundtrip\\g%04d_b", int( i ) );
			SGraphBuf first, second, third;
			SRMGraph direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_GRAPH_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadGraph( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( GraphIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteGraph( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadGraph( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameGraphRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( BkEditorRmgWriteGraph( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadGraph( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameGraphRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes" );
			if ( bOk )
				++nGraphsOk;
		}
		for ( size_t i = 0; i < fieldSets.size(); ++i )
		{
			const std::string &szName = fieldSets[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\fieldsets\\roundtrip\\f%04d", int( i ) );
			sprintf( szAgain, "scenarios\\fieldsets\\roundtrip\\f%04d_b", int( i ) );
			SFieldSetBuf first, second, third;
			SRMFieldSet direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_FIELDSET_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadFieldSet( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( FieldSetIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteFieldSet( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadFieldSet( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameFieldSetRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( BkEditorRmgWriteFieldSet( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadFieldSet( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameFieldSetRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes" );
			if ( bOk )
				++nFieldSetsOk;
		}
		for ( size_t i = 0; i < templates.size(); ++i )
		{
			const std::string &szName = templates[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\templates\\roundtrip\\t%04d", int( i ) );
			sprintf( szAgain, "scenarios\\templates\\roundtrip\\t%04d_b", int( i ) );
			STemplateBuf first, second, third;
			SRMTemplate direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_TEMPLATE_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadTemplate( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( TemplateIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteTemplate( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadTemplate( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameTemplateRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( QuickLoadIsTemplates( szCopy ), szName + ": the copy's QuickLoadMapInfo is the template's own" ) &&
			           Check( BkEditorRmgWriteTemplate( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadTemplate( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameTemplateRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes (Template and QuickLoadMapInfo)" );
			if ( bOk )
				++nTemplatesOk;
		}
		const bool bAll = nContainersOk == int( containers.size() ) && nGraphsOk == int( graphs.size() ) && nFieldSetsOk == int( fieldSets.size() ) && nTemplatesOk == int( templates.size() );
		if ( bAll )
			printf( "composer-roundtrip: %d templates, %d graphs, %d containers, %d field sets ok\n", nTemplatesOk, nGraphsOk, nContainersOk, nFieldSetsOk );
		else
			printf( "composer-roundtrip: only %d of %d templates, %d of %d graphs, %d of %d containers and %d of %d field sets round-tripped\n", nTemplatesOk, int( templates.size() ), nGraphsOk, int( graphs.size() ), nContainersOk, int( containers.size() ), nFieldSetsOk, int( fieldSets.size() ) );
		if ( g_nFailures == 0 )
			std::filesystem::remove_all( root, error );
		else
			printf( "kept: %s\n", root.string().c_str() );
		NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szOriginalUser.c_str() );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "composer-roundtrip: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
