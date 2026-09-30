// The terrain-editing heart of M3 (D-18/D-20/D-22): the Heights tool's stroke
// machine, Generate heights, Set Zero, the Update Map composite, Fill Entire
// Map and the terrain-mode toggles. Kept apart from session.cpp because it is
// a machine of its own - the MFC's DrawShadeState.cpp and
// TabTerrainAltitudesDialog.cpp with the MFC taken out - riding the D-19
// altitude region primitive (ApplyAltitudesInSession) and the edit log
// 05-01 proved.
//
// A3 (05-RESEARCH Open Question 2, measured 2026-10-01): UpdateAllHeights and
// ApplyPattern live behind IAIEditor, which the bridge session DOES hold
// headless - BkEditorStart's CEditorWorld::Init builds the same singleton
// stack the game does, and every M1/M2 object edit already goes through
// GetSingleton<IAIEditor>() (session.cpp). So Update Map's engine passes call
// the interface the MFC calls (UpdateAllHeights at AIEditorInternal.cpp:450,
// which is CStaticMap::UpdateAllHeights at AIStaticMap.cpp:1134); nothing is
// reimplemented bridge-side. The objects-Z pass over the map's own roads,
// rivers and sounds (the MFC's UpdateObjectsZ, TemplateEditorFrame1.cpp:4704)
// is map data, not engine state, and is implemented here over
// CVSOBuilder::UpdateZ, exactly the MFC's own route.
#include "StdAfx.h"
#include <cstring>
#include "session.h"
#include "world.h"
#include "../MapFile/MapOverlay.h"
#include "../AILogic/AILogic.h"
#include "../RandomMapGen/VA_Types.h"
#include "../RandomMapGen/VSO_Types.h"
#include "../RandomMapGen/TerrainGenerator.h"
#include "../RandomMapGen/PNoise.h"
#include "../Formats/fmtTerrain.h"
#include "../Scene/Terrain.h"

namespace {

// A map saved without altitudes gets its sheet here, as the open path and
// ApplyAltitudesInSession build it (the MFC editor's own load rule,
// TemplateEditorFrame1.cpp:1658): the edit is what turns the implicit flat
// sheet into a real one. Returns false with the reason in szMessage.
bool EnsureAltitudeSheet( SEditorSession *pSession )
{
	STerrainInfo &rSnapshot = pSession->snapshot.terrain;
	if ( rSnapshot.altitudes.GetSizeX() != 0 && rSnapshot.altitudes.GetSizeY() != 0 )
		return true;
	rSnapshot.altitudes.SetSizes( rSnapshot.patches.GetSizeX() * STerrainPatchInfo::nSizeX + 1,
	                              rSnapshot.patches.GetSizeY() * STerrainPatchInfo::nSizeY + 1 );
	rSnapshot.altitudes.SetZero();
	return true;
}

// The MFC's own pattern (TabTerrainAltitudesDialog.cpp:181-203): the profile
// gradient from editor\profile.tga - per column, the height of the first dark
// pixel, normalised to [0, speed] - sampled over a brush*2 square as a radial
// dome (ApplyVAInRadius' Euclidean distance from the centre, VA_Types.h:436).
// Cached in the session; rebuilt when the brush or the speed moves. False
// with the reason in szMessage when the image will not load.
bool EnsureHeightsPattern( SEditorSession *pSession, int nBrush, float fSpeed )
{
	if ( pSession->bHeightsPatternValid && pSession->nHeightsBrush == nBrush && pSession->fHeightsSpeed == fSpeed )
		return true;
	const std::string szFileName = "editor\\profile.tga";
	CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( szFileName.c_str(), STREAM_ACCESS_READ );
	if ( pImageStream == 0 )
	{
		pSession->szMessage = "editor\\profile.tga is not in the data storage";
		return false;
	}
	IImageProcessor *pProcessor = GetImageProcessor();
	if ( pProcessor == 0 )
	{
		pSession->szMessage = "the engine has no image processor";
		return false;
	}
	CPtr<IImage> pImage = pProcessor->LoadImage( pImageStream );
	if ( pImage == 0 )
	{
		pSession->szMessage = "editor\\profile.tga would not decode";
		return false;
	}
	SVAGradient gradient;
	gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, fSpeed ) );
	if ( !pSession->heightsPattern.CreateFromGradient( gradient, nBrush * 2 ) )
	{
		pSession->szMessage = "the profile pattern would not build";
		return false;
	}
	// The level mask (CreateValue(1.0, brush*2), the MFC's m_currentLevelPattern):
	// 1.0 inside the same radial dome, 0 outside - what the level mode moves
	// and what the averages count.
	if ( !pSession->heightsLevelMask.CreateValue( 1.0f, nBrush * 2 ) )
	{
		pSession->szMessage = "the level mask would not build";
		return false;
	}
	pSession->nHeightsBrush = nBrush;
	pSession->fHeightsSpeed = fSpeed;
	pSession->bHeightsPatternValid = true;
	return true;
}

// The average of the altitudes under the level mask placed at rCorner - the
// MFC's SVAPattern::GetAverageHeight (VA_Methods.cpp:238) with the clip
// ApplyVAPattern gives it: only the map's own vertices are read, only the
// cells whose mask value is not 0 count.
float MaskAverageAt( const STerrainInfo &rTerrain, const SVAPattern &rMask, const CTPoint<int> &rCorner )
{
	const CTRect<int> rBounds( 0, 0, rTerrain.altitudes.GetSizeX(), rTerrain.altitudes.GetSizeY() );
	CTRect<int> rRect( rCorner.x, rCorner.y, rCorner.x + rMask.heights.GetSizeX(), rCorner.y + rMask.heights.GetSizeY() );
	if ( ValidateIndices( rBounds, &rRect ) < 0 )
		return 0.0f;
	double fTotal = 0.0;
	int nCount = 0;
	for ( int nY = rRect.miny; nY < rRect.maxy; ++nY )
	{
		for ( int nX = rRect.minx; nX < rRect.maxx; ++nX )
		{
			if ( rMask.heights[nY - rCorner.y][nX - rCorner.x] != 0.0f )
			{
				fTotal += rTerrain.altitudes[nY][nX].fHeight;
				++nCount;
			}
		}
	}
	return ( nCount != 0 ) ? float( fTotal / nCount ) : 0.0f;
}

// The average of the four vertices of the tile under a world point - the
// MFC's fTileHeight (DrawShadeState.cpp:151-154). False when the point is
// off the map (the MFC's isTileHeightValid).
float TileHeightAt( const STerrainInfo &rTerrain, const CVec3 &vPos, bool *pbValid )
{
	CTPoint<int> tile;
	if ( !CMapInfo::GetTerrainTileIndices( rTerrain, vPos, &tile ) )
	{
		*pbValid = false;
		return 0.0f;
	}
	*pbValid = true;
	return ( rTerrain.altitudes[tile.y + 0][tile.x + 0].fHeight +
	         rTerrain.altitudes[tile.y + 1][tile.x + 0].fHeight +
	         rTerrain.altitudes[tile.y + 1][tile.x + 1].fHeight +
	         rTerrain.altitudes[tile.y + 0][tile.x + 1].fHeight ) / 4.0f;
}

} // namespace

// The MFC's UpdateObjectsZ (TemplateEditorFrame1.cpp:4704-4742): every road,
// river and sound gets its z back on the ground over the altitudes. The MFC
// ignores the rectangle it is handed (the body walks every record), and so
// does this - which also keeps the z a pure function of the altitudes, so an
// altitude undo that re-runs it reproduces the bytes it had (the same
// reasoning that lets a road move re-derive its own z). Both map copies and
// the engine's own records move together; UpdateRoad/UpdateRiver redraw.
void UpdateObjectsZInSession( SEditorSession *pSession )
{
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
		return;
	// The map's own records, both copies: the snapshot is what is saved.
	for ( int nPass = 0; nPass < 2; ++nPass )
	{
		CMapInfo &rMap = ( nPass == 0 ) ? pSession->snapshot : pSession->working;
		for ( int nVSO = 0; nVSO < int( rMap.terrain.roads3.size() ); ++nVSO )
			CVSOBuilder::UpdateZ( rMap.terrain.altitudes, &( rMap.terrain.roads3[nVSO] ) );
		for ( int nVSO = 0; nVSO < int( rMap.terrain.rivers.size() ); ++nVSO )
			CVSOBuilder::UpdateZ( rMap.terrain.altitudes, &( rMap.terrain.rivers[nVSO] ) );
		for ( size_t nSound = 0; nSound < rMap.sounds.sounds.size(); ++nSound )
			CVSOBuilder::UpdateZ( rMap.terrain.altitudes, &( rMap.sounds.sounds[nSound].vPos ) );
	}
	// The engine's own copies and the redraw the MFC asked for.
	STerrainInfo &rEngine = const_cast<STerrainInfo&>( pEngineTerrain->GetTerrainInfo() );
	for ( int nVSO = 0; nVSO < int( rEngine.roads3.size() ); ++nVSO )
	{
		CVSOBuilder::UpdateZ( rEngine.altitudes, &( rEngine.roads3[nVSO] ) );
		pEngineTerrain->UpdateRoad( rEngine.roads3[nVSO].nID );
	}
	for ( int nVSO = 0; nVSO < int( rEngine.rivers.size() ); ++nVSO )
	{
		CVSOBuilder::UpdateZ( rEngine.altitudes, &( rEngine.rivers[nVSO] ) );
		pEngineTerrain->UpdateRiver( rEngine.rivers[nVSO].nID );
	}
}

// One stroke step: the pattern math over the snapshot's own vertices, the
// D-19 apply (set + shades over the grown region + engine push) and the MFC's
// own invalid-height rollback (DrawShadeState.cpp:261-311 - the pattern
// subtracted back unless Ctrl is held). Every step is one edit of the log;
// the core merges a gesture's tokens into one undo step.
bool ApplyHeightsStrokeInSession( SEditorSession *pSession, const SHeightsStroke &rStroke, bool *pbRefused, int *pnToken )
{
	*pnToken = -1;
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		return false;
	}
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	if ( rStroke.nBrush < 2 || rStroke.nBrush > 16 )
	{
		pSession->szMessage = "the heights brush is 2..16";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	if ( !EnsureAltitudeSheet( pSession ) )
		return false;
	STerrainInfo &rSnapshot = pSession->snapshot.terrain;
	const int nSizeX = rSnapshot.altitudes.GetSizeX();
	const int nSizeY = rSnapshot.altitudes.GetSizeY();

	// Where the brush sits: the tile under the cursor, the pattern's corner
	// above-left of it (DrawShadeState.cpp:206-207), the pattern rectangle
	// clipped to the map's own vertices (ApplyVAPattern's isIgnoreInvalidIndices
	// clip) and the shade-kernel growth of that (DrawShadeState.cpp:210-213).
	CTPoint<int> tile;
	if ( !CMapInfo::GetTerrainTileIndices( rSnapshot, rStroke.vPos, &tile ) )
	{
		pSession->szMessage = "the cursor is not over the map";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	const int nPatternSize = rStroke.nBrush * 2;
	const CTPoint<int> corner( tile.x - ( nPatternSize / 2 - 1 ), tile.y - ( nPatternSize / 2 - 1 ) );
	CTRect<int> rEdit( corner.x, corner.y, corner.x + nPatternSize, corner.y + nPatternSize );
	const CTRect<int> rBounds( 0, 0, nSizeX, nSizeY );
	if ( ValidateIndices( rBounds, &rEdit ) < 0 )
	{
		pSession->szMessage = "the brush is not over the map";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	if ( !EnsureHeightsPattern( pSession, rStroke.nBrush, rStroke.fHeightSpeed ) )
		return false;
	const SVAPattern &rPattern = pSession->heightsPattern;
	const SVAPattern &rMask = pSession->heightsLevelMask;

	// The stroke-start cache: the click modes' targets are frozen at the
	// stroke's beginning (the MFC's Update on the press, DrawShadeState.cpp:139-184,
	// whose fTileHeight/fAverageHeight no drag recomputes). The instant
	// average is recomputed per step, exactly the MFC's own split.
	if ( rStroke.bStrokeStart )
	{
		pSession->vClickRefStroke = rStroke.vClickRef;
		bool bTileValid = false;
		pSession->fClickTileHeight = TileHeightAt( rSnapshot, rStroke.vClickRef, &bTileValid );
		pSession->bClickTileValid = bTileValid;
		CTPoint<int> refTile;
		if ( CMapInfo::GetTerrainTileIndices( rSnapshot, rStroke.vClickRef, &refTile ) )
		{
			const CTPoint<int> refCorner( refTile.x - ( nPatternSize / 2 - 1 ), refTile.y - ( nPatternSize / 2 - 1 ) );
			pSession->fClickAverageHeight = MaskAverageAt( rSnapshot, rMask, refCorner );
		}
		else
		{
			pSession->fClickAverageHeight = 0.0f;
		}
	}

	float fTarget = 0.0f;
	if ( rStroke.nAction == 2 )
	{
		switch ( rStroke.nLevelMode )
		{
			case 0:
				fTarget = 0.0f;
				break;
			case 1:
				if ( !pSession->bClickTileValid )
				{
					pSession->szMessage = "the stroke's tile left the map";
					if ( pbRefused )
						*pbRefused = true;
					return false;
				}
				fTarget = pSession->fClickTileHeight;
				break;
			case 2:
				fTarget = MaskAverageAt( rSnapshot, rMask, corner );
				break;
			default:
				fTarget = pSession->fClickAverageHeight;
				break;
		}
	}
	const float fRatio = rStroke.fLevelRatioPercent / 100.0f;

	// The values: whole records from the snapshot's own storage (the
	// raw-struct padding rule), only the height the stroke's.
	const size_t nCount = size_t( rEdit.maxx - rEdit.minx ) * size_t( rEdit.maxy - rEdit.miny );
	std::vector<SVertexAltitude> values( nCount );
	size_t nValue = 0;
	for ( int nY = rEdit.miny; nY < rEdit.maxy; ++nY )
	{
		for ( int nX = rEdit.minx; nX < rEdit.maxx; ++nX, ++nValue )
		{
			memcpy( &values[nValue], &rSnapshot.altitudes[nY][nX], sizeof( SVertexAltitude ) );
			const float fAt = rSnapshot.altitudes[nY][nX].fHeight;
			const float fPattern = rPattern.heights[nY - corner.y][nX - corner.x];
			float fHeight = fAt;
			switch ( rStroke.nAction )
			{
				case 1:
					fHeight = fAt - fPattern;
					break;
				case 2:
					// The level mask gates the move (SVALevelAndCreateUndoPatternFunctional,
					// VA_Types.h:148-157); the ratio is the step toward the target.
					if ( rMask.heights[nY - corner.y][nX - corner.x] != 0.0f )
						fHeight = fAt + ( fTarget - fAt ) * fRatio;
					break;
				default:
					fHeight = fAt + fPattern;
					break;
			}
			values[nValue].fHeight = fHeight;
		}
	}

	// The D-19 apply, with the MFC's validity gate between the heights and
	// the shades: everything is captured over the grown region before
	// anything moves, so a rollback puts the ring's shades back too.
	const CTRect<int> rGrown = NMapOverlay::GrowForShades( pSession->snapshot, rEdit );
	const SGFXLightDirectional sunlight = CVertexAltitudeInfo::GetSunLight(
		static_cast<CMapInfo::SEASON>( pSession->working.nSeason ) );
	NMapOverlay::SAltitudeUndo before, workingBefore, engineBefore;
	NMapOverlay::CaptureAltitudeRegion( pSession->snapshot, rGrown, &before );
	NMapOverlay::CaptureAltitudeRegion( pSession->working, rGrown, &workingBefore );
	NMapOverlay::CaptureTerrainAltitudeRegion( pEngineTerrain->GetTerrainInfo(), rGrown, &engineBefore );

	if ( !NMapOverlay::SetAltitudeRegion( &pSession->snapshot, rEdit, values, 0 ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "the map would not take that height edit";
		return false;
	}
	// The MFC's own rollback (DrawShadeState.cpp:261/285/308): the pattern is
	// subtracted back - here, the snapshot put back raw - when the grown
	// rectangle holds a height IsValidHeight refuses and Ctrl is not held.
	if ( rStroke.bCtrlHeld == 0 && !CVertexAltitudeInfo::IsValidHeight( rSnapshot.altitudes, rGrown ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "invalid height";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	if ( !CMapInfo::UpdateTerrainShades( &rSnapshot, rGrown, sunlight ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "the map would not take that height edit";
		return false;
	}
	STerrainInfo &rWorking = pSession->working.terrain;
	if ( !NMapOverlay::SetAltitudeRegion( &pSession->working, rEdit, values, 0 ) ||
	     !CMapInfo::UpdateTerrainShades( &rWorking, rGrown, sunlight ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		NMapOverlay::UndoAltitudeRegion( &pSession->working, workingBefore );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "the map would not take that height edit";
		return false;
	}

	NMapOverlay::SAltitudeUndo after;
	NMapOverlay::CaptureAltitudeRegion( pSession->snapshot, rGrown, &after );
	PutEngineAltitudes( pEngineTerrain, after );

	// Instant Update (D-20): the engine's own objects ride the stroke, the
	// MFC's ApplyPattern + UpdateObjectsZ pair (DrawShadeState.cpp:268-272),
	// lower with the MFC's own ratio flip (:317-319). Map data never moves
	// here - only the engine-side units and statics the AI editor holds.
	if ( pSession->bInstantUpdate )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		{
			SVAPattern enginePattern( rPattern );
			enginePattern.pos = corner;
			if ( rStroke.nAction == 1 )
				enginePattern.fRatio = -1.0f;
			else if ( rStroke.nAction == 2 )
				enginePattern = rMask;
			pAIEditor->ApplyPattern( enginePattern );
			if ( rStroke.nAction == 1 )
				enginePattern.fRatio = 1.0f;
		}
		UpdateObjectsZInSession( pSession );
	}

	SAltitudeEdit *pEdit = new SAltitudeEdit();
	pEdit->before = before;
	pEdit->after = after;
	*pnToken = LogEdit( pSession, pEdit );
	return true;
}

// Generate heights: the MFC's own noise (TabTerrainAltitudesDialog.cpp:312-357)
// over the whole sheet, then the D-19 apply. The confirmation is the caller's.
bool GenerateHeightsInSession( SEditorSession *pSession, int nType, float fGranularity, float fMinZ, float fMaxZ,
                               bool *pbRefused, int *pnToken )
{
	*pnToken = -1;
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		return false;
	}
	if ( nType != TG_FBM && nType != TG_HYBRID && nType != TG_RIDGED )
	{
		pSession->szMessage = "the generator is Hills, Rocks or Dunes";
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	if ( !EnsureAltitudeSheet( pSession ) )
		return false;
	const int nSizeX = pSession->snapshot.terrain.altitudes.GetSizeX();
	const int nSizeY = pSession->snapshot.terrain.altitudes.GetSizeY();

	NPerlinNoise::Init();
	CHField hfield( nSizeX, nSizeY );
	SfBmValues fBmValue = CHField::fBmDefVals[nType];
	fBmValue.featSize = fGranularity;
	hfield.Generate( fBmValue );

	CTPoint<float> currentRange( 0.0f, 0.0f );
	const float fCurrentRange = hfield.AltitudeRange( &( currentRange.min ), &( currentRange.max ) );
	std::vector<float> heights( size_t( nSizeX ) * size_t( nSizeY ) );
	for ( int nX = 0; nX < nSizeX; ++nX )
	{
		for ( int nY = 0; nY < nSizeY; ++nY )
		{
			const size_t nAt = size_t( nY ) * size_t( nSizeX ) + size_t( nX );
			// The MFC's formula verbatim (TabTerrainAltitudesDialog.cpp:344); a
			// flat field (zero range) is the min everywhere, not a divide by zero.
			if ( fCurrentRange > FP_EPSILON )
				heights[nAt] = ( ( hfield.H( nX, nY ) - currentRange.min ) * ( fMaxZ - fMinZ ) * fWorldCellSize / fCurrentRange ) + ( fMinZ * fWorldCellSize );
			else
				heights[nAt] = fMinZ * fWorldCellSize;
		}
	}
	return ApplyAltitudesInSession( pSession, CTRect<int>( 0, 0, nSizeX, nSizeY ), heights, pbRefused, pnToken );
}

// Set Zero: every height 0, shades recomputed - the MFC's altitudes.SetZero()
// plus the update it ran after (TabTerrainAltitudesDialog.cpp:359-387). The
// confirmation is the caller's.
bool SetZeroHeightsInSession( SEditorSession *pSession, bool *pbRefused, int *pnToken )
{
	*pnToken = -1;
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		return false;
	}
	if ( !EnsureAltitudeSheet( pSession ) )
		return false;
	const int nSizeX = pSession->snapshot.terrain.altitudes.GetSizeX();
	const int nSizeY = pSession->snapshot.terrain.altitudes.GetSizeY();
	std::vector<float> heights( size_t( nSizeX ) * size_t( nSizeY ), 0.0f );
	return ApplyAltitudesInSession( pSession, CTRect<int>( 0, 0, nSizeX, nSizeY ), heights, pbRefused, pnToken );
}

// The terrain-mode toggles (D-20). Kept here beside their only readers.
bool SetTerrainModesInSession( SEditorSession *pSession, int bInstantUpdate, int bFitToGrid )
{
	if ( pSession == 0 )
		return false;
	pSession->bInstantUpdate = ( bInstantUpdate != 0 );
	pSession->bFitToGrid = ( bFitToGrid != 0 );
	return true;
}
