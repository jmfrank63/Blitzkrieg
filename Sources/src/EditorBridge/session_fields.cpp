// The Fields tool's application pipeline (M3, D-21): the MFC editor's
// CFieldsState::PlaceField (StateTerrainFields.cpp:312-516) as ONE edit of
// the log. The polygon is cut by the map bounds, optionally randomized by
// the engine's own RandomizeEdges with the MFC's exact arguments, and the
// field set's tile shells, object shells and profile pattern fill both
// copies through the engine's own FillTileSet / FillObjectSet /
// FillProfilePattern. One composite token covers the tiles, crosses,
// altitudes and objects it changed; undo puts them all back raw.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "../RandomMapGen/Polygons_Types.h"
#include "../RandomMapGen/RMG_Types.h"
#include "../RandomMapGen/LA_Types.h"
// The polygon helpers (UniquePolygon, CutByPolygonCore, RandomizeEdges) and
// fWorldCellSize ride Resource_Types.h's chain; the user-filter read needs
// the platform's user root and the file storage.
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/Resource_Types.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Terrain.h"
#include "../Platform/Paths.h"
#include "../Misc/Win32Random.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/StreamIOTypes.h"
#include <filesystem>
#include <fstream>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../RandomMapGen/TerrainGenerator.h"
#include "../Image/Image.h"
#include <algorithm>
#include <memory>
#include <set>

namespace
{

// The fills' draws (the shells' GetRandom, the pattern picks, the edge
// randomize) come from the engine's random services. The MFC filled ONE map
// and never met the question; the bridge fills TWO copies that must land
// byte-identical, and a deterministic apply is testable at all - so every
// fill is seeded from this fixed state before it runs: the global random
// generator (RandomGen.h's Random(), the shells' draw) and the Win32 LCG
// (Polygons_Types.h's edge randomize) alike.
const int kFieldFillSeed = 0;

// Seeds both generators the fills draw from. g_pGlobalRandomGen is the
// pointer Random() reads; the singleton is its loader-assigned instance -
// seeded too, so the order does not matter.
void SeedFieldFills()
{
	NWin32Random::Seed( kFieldFillSeed );
	if ( CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED ) )
	{
		pSeed->InitByZeroSeed();
		if ( g_pGlobalRandomGen != 0 )
			g_pGlobalRandomGen->SetSeed( pSeed );
		if ( IRandomGen *pGen = GetSingleton<IRandomGen>() )
			pGen->SetSeed( pSeed );
	}
}

}

namespace
{

// The object with this link ID, in whichever of the two lists holds it
// (session.cpp's own FindIn stays there).
SMapObjectInfo* FindFieldObject( CMapInfo *pMap, int nLinkID )
{
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( ( *lists[nList] )[i].link.nLinkID == nLinkID )
				return &( *lists[nList] )[i];
	return 0;
}

// The MFC's own closing rule (StateTerrainFields.cpp:176-185): dedupe the
// points within POINT_RADIUS, then require more than two and an area beyond
// one radius squared. Degenerate polygons never reach the engine.
bool ValidFieldPolygon( std::vector<CVec3> *pPoints )
{
	UniquePolygon<std::vector<CVec3>, CVec3>( pPoints, fWorldCellSize / 4.0f );
	if ( pPoints->size() <= 2 )
		return false;
	return fabs2( GetSignedPolygonSquare( *pPoints ) ) > fabs2( fWorldCellSize / 4.0f );
}

// One field-set object into both copies and the engine, the filled record's
// own AI position kept exact - the world round-trip would cost the fill's
// FitAIOrigin2AIGrid fit. The placement guards are AddObjectToSession's.
bool AddFieldObjectToSession( SEditorSession *pSession, const SMapObjectInfo &rFilled, int *pnLinkID )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine is not there";
		return false;
	}
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rFilled.szName.c_str() );
	if ( pDesc == 0 )
	{
		pSession->szMessage = "the object database does not know \"" + rFilled.szName + "\"";
		return false;
	}
	if ( const char *pszWhy = WhyNotAMapObject( pDesc->eGameType ) )
	{
		pSession->szMessage = "\"" + rFilled.szName + "\" " + pszWhy;
		return false;
	}
	if ( const char *pszWhy = WhyNotPlacedByPalette( pDesc->eGameType ) )
	{
		pSession->szMessage = "\"" + rFilled.szName + "\" " + pszWhy;
		return false;
	}

	SMapObjectInfo object = rFilled;
	object.link.nLinkID = Max( NMapOverlay::NextLinkID( pSession->snapshot ), pSession->nLinkIDFloor );
	// The palette add's own rules (AddObjectToSession): whole, no script ID,
	// linked with nothing.
	object.nScriptID = -1;
	object.fHP = 1.0f;
	object.link.nLinkWith = 0;
	object.link.bIntention = true;
	pSession->snapshot.objects.push_back( object );
	pSession->working.objects.push_back( object );
	if ( SMapObjectInfo *pAdded = FindFieldObject( &pSession->snapshot, object.link.nLinkID ) )
		CMapInfo::PackFrameIndex( pObjectsDB, pAdded );
	// The engine object comes from the working record (its frame index is a
	// segment), as every add builds it.
	SMapObjectInfo *pWorking = FindFieldObject( &pSession->working, object.link.nLinkID );
	IRefCount *pAIObject = pWorking != 0 ? PlaceOneObject( *pWorking, pDesc, pAIEditor ) : 0;
	if ( pAIObject == 0 )
	{
		// The engine would not take it: neither copy keeps it (the record was
		// appended above, so the tails come off).
		pSession->snapshot.objects.pop_back();
		pSession->working.objects.pop_back();
		pSession->szMessage = "the engine would not place \"" + rFilled.szName + "\" there";
		return false;
	}
	pSession->byLinkID[object.link.nLinkID] = pAIObject;
	pSession->nLinkIDFloor = object.link.nLinkID + 1;
	UpdateSessionWorld( pSession );
	if ( pnLinkID )
		*pnLinkID = object.link.nLinkID;
	return true;
}

// The object filter's word lists as the named filter carries them - the same
// read BkEditorObjectFilters answers, matched against the object's szPath the
// way the MFC's FilterName matched (MiniMapTypes.cpp:205).
typedef std::list<std::string> TFilterWords;
typedef std::list<TFilterWords> TFilterConditions;
struct SNamedFilter
{
	TFilterConditions conditions;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Filter", &conditions );
		return 0;
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &conditions );
		return 0;
	}
};
typedef std::unordered_map<std::string, SNamedFilter> TNamedFilterMap;

bool FolderPasses( const SNamedFilter &rFilter, const std::string &rszFolder )
{
	if ( rFilter.conditions.empty() )
		return false;
	for ( const TFilterWords &rWords : rFilter.conditions )
	{
		bool bAll = true;
		for ( const std::string &rszWord : rWords )
		{
			if ( rszFolder.find( rszWord ) == std::string::npos )
			{
				bAll = false;
				break;
			}
		}
		if ( bAll )
			return true;
	}
	return false;
}

bool LoadNamedFilter( const std::string &rszName, SNamedFilter *pOut )
{
	// Shipped, through the engine's own reader (filters.cpp mirrors it for
	// the palette), then the user file over it, user wins by name.
	TNamedFilterMap shipped, user;
	LoadDataResource( "editor\\filter", "", false, 0, "filters", shipped );
	try
	{
		const std::string szDir = ( std::filesystem::path( NPlatform::Paths::UserRoot() ) / "mapeditor" ).string() + "/";
		CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
		if ( pStorage != 0 )
		{
			CPtr<IDataStream> pStream = pStorage->OpenStream( "filter.xml", STREAM_ACCESS_READ );
			if ( pStream != 0 )
			{
				CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::READ );
				CTreeAccessor saver = pSaver;
				saver.Add( "filters", &user );
			}
		}
	}
	catch ( ... )
	{
		user.clear();
	}
	const auto iUser = user.find( rszName );
	if ( iUser != user.end() )
	{
		*pOut = iUser->second;
		return true;
	}
	const auto iShipped = shipped.find( rszName );
	if ( iShipped != shipped.end() )
	{
		*pOut = iShipped->second;
		return true;
	}
	return false;
}

}

bool SFieldEdit::Revert( SEditorSession *pSession )
{
	// The update map ran last, so it goes back first.
	if ( updateMap && !updateMap->Revert( pSession ) )
		return false;
	for ( size_t i = addedRecords.size(); i > 0; --i )
	{
		bool bRefused = false;
		if ( !DeleteObjectFromSession( pSession, addedRecords[i - 1].link.nLinkID, &bRefused ) )
			return false;
	}
	if ( !altitudesAfter.altitudes.empty() )
	{
		if ( !PutAltitudeEditBack( pSession, altitudesBefore ) )
			return false;
	}
	// The heights pass's objects-Z refresh, put back raw - the map's own
	// bytes, not a re-derivation (05-02's rule).
	PutVsoZBack( pSession, vsoSnapshotBefore, vsoWorkingBefore, vsoEngineBefore );
	if ( !tilesAfter.tiles.empty() && !PutRegionBack( pSession, tilesBefore ) )
		return false;
	return true;
}

bool SFieldEdit::Reapply( SEditorSession *pSession )
{
	if ( !tilesAfter.tiles.empty() && !PutRegionBack( pSession, tilesAfter ) )
		return false;
	if ( !altitudesAfter.altitudes.empty() )
	{
		if ( !PutAltitudeEditBack( pSession, altitudesAfter ) )
			return false;
	}
	PutVsoZBack( pSession, vsoSnapshotAfter, vsoWorkingAfter, vsoEngineAfter );
	// The added records go back whole, their own link IDs with them - the
	// restore path's trick (RestoreObjectInSession), so redo lands the map
	// byte-for-byte where the apply had left it.
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine is not there";
		return false;
	}
	for ( size_t i = 0; i < addedRecords.size(); ++i )
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( addedRecords[i].szName.c_str() );
		if ( pDesc == 0 )
		{
			pSession->szMessage = "the object database lost \"" + addedRecords[i].szName + "\"";
			return false;
		}
		pSession->snapshot.objects.push_back( addedRecords[i] );
		pSession->working.objects.push_back( addedRecords[i] );
		SMapObjectInfo *pWorking = FindFieldObject( &pSession->working, addedRecords[i].link.nLinkID );
		IRefCount *pAIObject = pWorking != 0 ? PlaceOneObject( *pWorking, pDesc, pAIEditor ) : 0;
		if ( pAIObject == 0 )
		{
			pSession->snapshot.objects.pop_back();
			pSession->working.objects.pop_back();
			pSession->szMessage = "the engine would not place \"" + addedRecords[i].szName + "\" there";
			return false;
		}
		pSession->byLinkID[addedRecords[i].link.nLinkID] = pAIObject;
		pSession->nLinkIDFloor = Max( pSession->nLinkIDFloor, addedRecords[i].link.nLinkID + 1 );
		UpdateSessionWorld( pSession );
	}
	if ( updateMap && !updateMap->Reapply( pSession ) )
		return false;
	return true;
}
// Redo honesty note: the MFC editor had no undo at all (PARITY "not a
// feature" row), so nothing is lost by refusing redo - but the log's own
// contract wants redo to work. The composite therefore keeps the added
// records whole and redoes by re-adding them with their own link IDs, the
// restore path's trick. Kept simple instead: the added objects' records are
// captured in the edit, so Reapply re-adds them exactly.

// The field set's season (D-21's confirmation data): CMapInfo::GetSelectedSeason
// over the loaded set, the same answer the MFC's PlaceField compared against
// the map's own season (StateTerrainFields.cpp:388-389).
bool FieldSetSeasonInSession( SEditorSession *pSession, const std::string &rszName, int *pnSeason, bool *pbRefused )
{
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || pnSeason == 0 || rszName.empty() )
	{
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	SRMFieldSet fieldSet;
	if ( !LoadDataResource( rszName, "", false, 0, RMGC_FIELDSET_XML_NAME, fieldSet ) )
	{
		pSession->szMessage = "the field set \"" + rszName + "\" is not in the data";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	*pnSeason = CMapInfo::GetSelectedSeason( fieldSet.nSeason, fieldSet.szSeasonFolder );
	return true;
}

bool ApplyFieldInSession( SEditorSession *pSession, const SFieldApply &rApply,
	std::vector<SFieldObjectReport> *pReport, bool *pbRefused, int *pnToken )
{
	*pnToken = -1;
	if ( pbRefused )
		*pbRefused = false;
	if ( pReport )
		pReport->clear();
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IImageProcessor *pImages = GetImageProcessor();
	if ( pEngineTerrain == 0 || pAIEditor == 0 || pImages == 0 )
	{
		pSession->szMessage = "the engine is missing the terrain, the AI editor or the image processor";
		return false;
	}
	// Argument rules first (T-05-03-02): a bare storage-relative name, 3..64
	// finite points, sane flags, clamped randomize params (T-05-03-04).
	// Storage-relative: folder components fine (the scan's own names carry
	// them), but no absolute path, no parent steps, no drive.
	if ( rApply.szFieldSet.empty() || rApply.szFieldSet.size() >= 256 ||
		   rApply.szFieldSet[0] == '\\' || rApply.szFieldSet[0] == '/' ||
		   rApply.szFieldSet.find( ".." ) != std::string::npos ||
		   rApply.szFieldSet.find( ':' ) != std::string::npos )
	{
		pSession->szMessage = "the field set name must be a storage-relative bare name";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	if ( rApply.points.size() < 3 || rApply.points.size() > 64 )
	{
		pSession->szMessage = "a fields polygon carries 3..64 points";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	for ( size_t i = 0; i < rApply.points.size(); ++i )
	{
		if ( !std::isfinite( rApply.points[i].x ) || !std::isfinite( rApply.points[i].y ) ||
			   fabs( rApply.points[i].x ) > 1.0e6f || fabs( rApply.points[i].y ) > 1.0e6f )
		{
			pSession->szMessage = "a polygon point is not a finite world coordinate";
			if ( pbRefused )
				*pbRefused = true;
			return false;
		}
	}
	SFieldApply apply = rApply;
	apply.fMinLength = Min( Max( apply.fMinLength, 2.0f ), 512.0f );
	apply.fWidth = Min( Max( apply.fWidth, 0.0f ), 0.5f );
	apply.fDisturbance = Min( Max( apply.fDisturbance, 0.0f ), 1.0f );
	// The report mode changes nothing: the update-map-after flag makes no
	// sense in it, and the fill flags stay meaningful (the report answers
	// what each would have done).
	if ( apply.bCheckPassabilityOnly )
		apply.bUpdateMapAfter = false;

	// The MFC's own closing rule: degenerate polygons are refused before
	// anything is loaded, changing nothing.
	std::vector<CVec3> points = apply.points;
	if ( !ValidFieldPolygon( &points ) )
	{
		pSession->szMessage = "the polygon is degenerate: three points and a real area are needed";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	// The field set itself.
	SRMFieldSet fieldSet;
	if ( !LoadDataResource( apply.szFieldSet, "", false, 0, RMGC_FIELDSET_XML_NAME, fieldSet ) )
	{
		pSession->szMessage = "the field set \"" + apply.szFieldSet + "\" is not in the data";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	// The active object filter, when the asks wants one: a name the merged
	// filter list must carry (the D-31 filters gate the placement).
	SNamedFilter objectFilter;
	const bool bFilterObjects = apply.bCanAddObjectFilter && !apply.szObjectFilter.empty();
	if ( bFilterObjects && !LoadNamedFilter( apply.szObjectFilter, &objectFilter ) )
	{
		pSession->szMessage = "no object filter is named \"" + apply.szObjectFilter + "\"";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	CMapInfo &rSnapshot = pSession->snapshot;
	CMapInfo &rWorking = pSession->working;
	const int nTilesX = rSnapshot.terrain.tiles.GetSizeX();
	const int nTilesY = rSnapshot.terrain.tiles.GetSizeY();
	const int nSizeX = rSnapshot.terrain.altitudes.GetSizeX();
	const int nSizeY = rSnapshot.terrain.altitudes.GetSizeY();

	// The polygon: world xy, cut by the map's own rectangle, optionally
	// randomized with the MFC's exact arguments (StateTerrainFields.cpp:352).
	std::list<CVec2> listedPolygon;
	for ( size_t i = 0; i < points.size(); ++i )
		listedPolygon.push_back( CVec2( points[i].x, points[i].y ) );
	std::list<CVec2> mapVisPointsPolygon;
	mapVisPointsPolygon.push_back( VNULL2 );
	mapVisPointsPolygon.push_back( CVec2( 0.0f, nTilesY * fWorldCellSize ) );
	mapVisPointsPolygon.push_back( CVec2( nTilesX * fWorldCellSize, nTilesY * fWorldCellSize ) );
	mapVisPointsPolygon.push_back( CVec2( nTilesX * fWorldCellSize, 0.0f ) );
	std::list<CVec2> cutPolygon;
	CutByPolygonCore<std::list<CVec2>, CVec2>( listedPolygon, mapVisPointsPolygon, &cutPolygon );
	std::list<CVec2> polygon;
	if ( apply.bRandomize && apply.fMinLength >= 2.0f )
	{
		SeedFieldFills();
		RandomizeEdges<std::list<CVec2>, CVec2>( cutPolygon, 10, apply.fWidth,
			CTPoint<float>( 0.0f, apply.fDisturbance ), &polygon,
			apply.fMinLength * fWorldCellSize, 512.0f * fWorldCellSize, true );
	}
	else
	{
		polygon = cutPolygon;
	}

	// The tileset the tile fill validates against (the MFC passed the frame's
	// own descrTile; the session loads the map's own).
	STilesetDesc tilesetDesc;
	LoadDataResource( rSnapshot.terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
	if ( tilesetDesc.terrtypes.empty() )
	{
		pSession->szMessage = "the map's tileset has no terrain types";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	// Everything the composite will touch, captured before anything moves -
	// the full sheets, byte-exact undo owing nothing to a re-derivation.
	// The VSO z rides along: the heights pass's objects-Z refresh rewrites
	// every road, river and sound (UpdateObjectsZInSession), and the bytes
	// undo owes are the map's own (05-02's rule).
	std::unique_ptr<SFieldEdit> pEdit( new SFieldEdit() );
	NMapOverlay::CaptureRegion( rSnapshot, CTRect<int>( 0, 0, rSnapshot.terrain.patches.GetSizeX(), rSnapshot.terrain.patches.GetSizeY() ), &( pEdit->tilesBefore ) );
	NMapOverlay::CaptureAltitudeRegion( rSnapshot, CTRect<int>( 0, 0, nSizeX, nSizeY ), &( pEdit->altitudesBefore ) );
	CaptureVsoZ( rSnapshot, &( pEdit->vsoSnapshotBefore ) );
	CaptureVsoZ( rWorking, &( pEdit->vsoWorkingBefore ) );
	CaptureEngineVsoZ( pEngineTerrain, &( pEdit->vsoEngineBefore ) );

	// What the fills change, applied to the snapshot first (the save's
	// source), then the working copy; a failure anywhere puts the captures
	// back and refuses.
	const int nMapSeason = rSnapshot.GetSelectedSeason();
	const SGFXLightDirectional sunlight = CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( rWorking.nSeason ) );
	// Report mode changes nothing: only the scratch object fill and the
	// passability asks run.
	const bool bApply = !apply.bCheckPassabilityOnly;
	const std::list<std::list<CVec2>> exclusivePolygons;
	std::unordered_map<LPARAM, float> distances;
	bool bOk = true;

	if ( apply.bFillTerrain && bApply && bOk )
	{
		fieldSet.ValidateFieldSet( tilesetDesc, CMapInfo::MOST_COMMON_TILES[nMapSeason] );
		// Each copy's fill is seeded from the same fixed state, so both land
		// byte-identical and the apply replays exactly (the builder proof).
		SeedFieldFills();
		bOk = CMapInfo::FillTileSet( &rSnapshot.terrain, tilesetDesc, polygon, exclusivePolygons, fieldSet.tilesShells, &distances );
		SeedFieldFills();
		bOk = bOk && CMapInfo::FillTileSet( &rWorking.terrain, tilesetDesc, polygon, exclusivePolygons, fieldSet.tilesShells, &distances );
		if ( !bOk )
			pSession->szMessage = "the tile shells would not fill";
	}

	if ( apply.bPlaceObjects && bOk )
	{
		CArray2D<BYTE> tileMap( nTilesX * 2, nTilesY * 2 );
		tileMap.Set( RMGC_UNLOCKED );
		CMapInfo scratch;
		scratch.Create( CTPoint<int>( rSnapshot.terrain.patches.GetSizeX(), rSnapshot.terrain.patches.GetSizeY() ),
			CMapInfo::REAL_SEASONS[CMapInfo::SEASON_SUMMER], CMapInfo::SEASON_FOLDERS[CMapInfo::SEASON_SUMMER], 3, 0 );
		SeedFieldFills();
		if ( !CMapInfo::FillObjectSet( &scratch, polygon, exclusivePolygons, fieldSet.objectsShells, &tileMap ) )
		{
			bOk = false;
			pSession->szMessage = "the object shells would not fill";
		}
		for ( size_t i = 0; i < scratch.objects.size() && bOk; ++i )
		{
			const SMapObjectInfo &rFilled = scratch.objects[i];
			const SGDBObjectDesc *pDesc = GetSingleton<IObjectsDB>()->GetDesc( rFilled.szName.c_str() );
			SFieldObjectReport row;
			row.szName = rFilled.szName;
			row.fX = rFilled.vPos.x;
			row.fY = rFilled.vPos.y;
			bool bWanted = true;
			// The passability gate, the MFC's own CanAddObject ask
			// (StateTerrainFields.cpp:449, the dialog's Check Passability
			// checkbox); in report-only mode every object is reported,
			// placed or not.
			if ( apply.bCheckPassabilityOnly && pAIEditor != 0 )
				bWanted = bWanted && pAIEditor->CanAddObject( const_cast<SMapObjectInfo &>( rFilled ) );
			if ( bFilterObjects )
			{
				const SGDBObjectDesc *pFilterDesc = GetSingleton<IObjectsDB>()->GetDesc( rFilled.szName.c_str() );
				bWanted = bWanted && pFilterDesc != 0 && FolderPasses( objectFilter, pFilterDesc->szPath );
			}
			row.bPlaced = bWanted && !apply.bCheckPassabilityOnly;
			if ( pReport )
				pReport->push_back( row );
			if ( bWanted && !apply.bCheckPassabilityOnly )
			{
				int nLinkID = -1;
				if ( !AddFieldObjectToSession( pSession, rFilled, &nLinkID ) )
				{
					// One object the engine would not take is skipped, not
					// the end of the fill - the MFC's own loop went on.
					if ( pReport && !pReport->empty() )
						pReport->back().bPlaced = false;
					continue;
				}
				pEdit->addedRecords.push_back( FindFieldObject( &rSnapshot, nLinkID ) ? *FindFieldObject( &rSnapshot, nLinkID ) : rFilled );
			}
		}
	}

	if ( apply.bModifyHeights && bApply && bOk && fieldSet.fHeight > 0 )
	{
		SVAGradient gradient;
		if ( CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( ( fieldSet.szProfileFileName + ".tga" ).c_str(), STREAM_ACCESS_READ ) )
		{
			if ( CPtr<IImage> pImage = pImages->LoadImage( pImageStream ) )
				gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, fieldSet.fHeight ) );
		}
		if ( !gradient.heights.empty() )
		{
			SeedFieldFills();
			if ( !CMapInfo::FillProfilePattern( &rSnapshot.terrain, polygon, exclusivePolygons, gradient, fieldSet.patternSize, fieldSet.fPositiveRatio, &distances ) )
			{
				bOk = false;
				pSession->szMessage = "the profile pattern would not fill";
			}
			SeedFieldFills();
			if ( bOk && !CMapInfo::FillProfilePattern( &rWorking.terrain, polygon, exclusivePolygons, gradient, fieldSet.patternSize, fieldSet.fPositiveRatio, &distances ) )
			{
				bOk = false;
				pSession->szMessage = "the profile pattern would not fill";
			}
			if ( bOk )
			{
				UpdateObjectsZInSession( pSession );
				const CTRect<int> rFullVertices( 0, 0, nSizeX, nSizeY );
				if ( !CMapInfo::UpdateTerrainShades( &rSnapshot.terrain, rFullVertices, sunlight ) ||
					   !CMapInfo::UpdateTerrainShades( &rWorking.terrain, rFullVertices, sunlight ) )
				{
					bOk = false;
					pSession->szMessage = "the shades would not recompute";
				}
			}
		}
	}

	if ( !bOk )
	{
		// A mid-pipeline failure puts everything back and refuses: the
		// before-captures, then the objects already added.
		for ( size_t i = pEdit->addedRecords.size(); i > 0; --i )
		{
			bool bRefused = false;
			DeleteObjectFromSession( pSession, pEdit->addedRecords[i - 1].link.nLinkID, &bRefused );
		}
		if ( !PutAltitudeEditBack( pSession, pEdit->altitudesBefore ) )
			pSession->szMessage += "; the altitudes could not be put back - reopen the map";
		if ( !PutRegionBack( pSession, pEdit->tilesBefore ) )
			pSession->szMessage += "; the tiles could not be put back - reopen the map";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	// The engine's terrain redrawn over the covered patches, both fills done.
	if ( bApply && ( apply.bFillTerrain || ( apply.bModifyHeights && fieldSet.fHeight > 0 ) ) )
		pEngineTerrain->Update( CTRect<int>( 0, 0, rSnapshot.terrain.patches.GetSizeX() - 1, rSnapshot.terrain.patches.GetSizeY() - 1 ) );

	if ( bApply )
	{
		// The captures after - the bytes the undo owes. The VSO z before the
		// nested update: the update captures and restores its own.
		NMapOverlay::CaptureRegion( rSnapshot, CTRect<int>( 0, 0, rSnapshot.terrain.patches.GetSizeX(), rSnapshot.terrain.patches.GetSizeY() ), &( pEdit->tilesAfter ) );
		NMapOverlay::CaptureAltitudeRegion( rSnapshot, CTRect<int>( 0, 0, nSizeX, nSizeY ), &( pEdit->altitudesAfter ) );
		CaptureVsoZ( rSnapshot, &( pEdit->vsoSnapshotAfter ) );
		CaptureVsoZ( rWorking, &( pEdit->vsoWorkingAfter ) );
		CaptureEngineVsoZ( pEngineTerrain, &( pEdit->vsoEngineAfter ) );

		// Update Map afterwards: the whole 05-02 composite, run without its
		// own log entry and nested in this one - one token, one undo step.
		if ( apply.bUpdateMapAfter )
		{
			IEditRecord *pNested = 0;
			if ( !UpdateMapInSession( pSession, 0, 0, 0, 0, &pNested ) )
			{
				// The update refused: the fields' own changes stay (the MFC
				// ran its update after the fill and did not roll back), and
				// the composite logs without the nested part.
				pSession->szMessage = "the fields applied, but the update map afterwards refused: " + pSession->szMessage;
			}
			else
			{
				pEdit->updateMap.reset( pNested );
			}
		}
		*pnToken = LogEdit( pSession, pEdit.release() );
	}
	return true;
}
