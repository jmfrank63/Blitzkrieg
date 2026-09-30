// Roads and rivers (04-05, D-07/D-08/D-09) and the session's edit log.
//
// A road or a river is edited as its control polyline plus a width and an
// opacity at each key point. The sampled points are derived here, once per
// edit, by the MFC tool's own code (CVSOBuilder::CreateVSO, Update with
// DEFAULT_STEP, UpdateZ - VectorStripeObjectsState.cpp:644-657, research C10),
// and the derived record is what both copies and the engine get and what the
// edit log keeps: undo and redo put the stored records back and never sample
// again (D-03).
//
// One put serves an edit, its undo and its redo (PutVso): the snapshot and the
// working copy through NMapRecords, then the engine - for a river first
// IAIEditor::DeleteRiver with the record as it was locked, then the terrain's
// Remove and Add, then IAIEditor::AddRiver with the new record (Pitfall 4).
// Roads never touch the AI (D-09): the game works out road passability when it
// loads the map. Altitudes and shades are never changed; UpdateZ only fits the
// stripe to the heights already there.
#include "StdAfx.h"
#include <algorithm>
#include <cmath>
#include "session.h"
#include "../MapFile/MapRecords.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/VSO_Types.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
#include "../Scene/Terrain.h"

// ---------------------------------------------------------------------------
// The edit log
// ---------------------------------------------------------------------------

int LogEdit( SEditorSession *pSession, IEditRecord *pRecord )
{
	std::unique_ptr<IEditRecord> record( pRecord );
	pSession->edits.push_back( std::move( record ) );
	const int nToken = int( pSession->edits.size() ) - 1;
	pSession->appliedEdits.push_back( nToken );
	pSession->undoneEdits.clear();
	return nToken;
}

bool UndoEditInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( pSession->appliedEdits.empty() || pSession->appliedEdits.back() != nToken )
	{
		pSession->szMessage = "edits are undone newest first";
		*pbRefused = true;
		return false;
	}
	if ( !pSession->edits[nToken]->Revert( pSession ) )
		return false;
	pSession->appliedEdits.pop_back();
	pSession->undoneEdits.push_back( nToken );
	return true;
}

bool RedoEditInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( pSession->undoneEdits.empty() || pSession->undoneEdits.back() != nToken )
	{
		pSession->szMessage = "edits are redone in the order they were undone";
		*pbRefused = true;
		return false;
	}
	if ( !pSession->edits[nToken]->Reapply( pSession ) )
		return false;
	pSession->undoneEdits.pop_back();
	pSession->appliedEdits.push_back( nToken );
	return true;
}

void ClearEditLog( SEditorSession *pSession )
{
	pSession->edits.clear();
	pSession->appliedEdits.clear();
	pSession->undoneEdits.clear();
}

// ---------------------------------------------------------------------------
// Roads and rivers
// ---------------------------------------------------------------------------

namespace {
const int nKinds = 2;
// A caller's polyline longer than this is a caller bug, not a road: the MFC
// editor's own roads have a few dozen control points.
const int nMaxControlPoints = 1024;

bool IsKind( int nKind )
{
	return nKind == 0 || nKind == 1;
}

NMapRecords::EVsoKind ToKind( int nKind )
{
	return nKind == 0 ? NMapRecords::VSO_ROAD : NMapRecords::VSO_RIVER;
}

const TVSOList& SavedList( const SEditorSession &rSession, int nKind )
{
	return nKind == 0 ? rSession.snapshot.terrain.roads3 : rSession.snapshot.terrain.rivers;
}

const char* KindName( int nKind )
{
	return nKind == 0 ? "road" : "river";
}

// The folder of a kind's descriptors under the map's season folder, as the
// MFC editor spells it (TemplateEditorFrame1.cpp:1719-1720): the saved
// szDescName is this folder and the bare name.
std::string DescriptorFolder( const SEditorSession &rSession, int nKind )
{
	return rSession.snapshot.szSeasonFolder + ( nKind == 0 ? "Roads3D\\" : "Rivers\\" );
}

// A bare descriptor name: no folder, no extension, nothing that climbs.
bool IsBareName( const std::string &szName )
{
	if ( szName.empty() || szName.size() > 100 )
		return false;
	return szName.find_first_of( "\\/:." ) == std::string::npos;
}

// World units: the map is tiles * fWorldCellSize across in each direction.
bool OnTheMap( const SEditorSession &rSession, float fX, float fY )
{
	const float fWidth = rSession.working.terrain.tiles.GetSizeX() * fWorldCellSize;
	const float fHeight = rSession.working.terrain.tiles.GetSizeY() * fWorldCellSize;
	return std::isfinite( fX ) && std::isfinite( fY ) && fX >= 0.0f && fY >= 0.0f && fX < fWidth && fY < fHeight;
}

TVSOList& EngineList( ITerrainEditor *pTerrain, int nKind )
{
	STerrainInfo &rInfo = const_cast<STerrainInfo&>( pTerrain->GetTerrainInfo() );
	return nKind == 0 ? rInfo.roads3 : rInfo.rivers;
}

const SVectorStripeObject* FindEngineVso( ITerrainEditor *pTerrain, int nKind, int nEngineID )
{
	const TVSOList &rList = EngineList( pTerrain, nKind );
	for ( size_t i = 0; i < rList.size(); ++i )
		if ( rList[i].nID == nEngineID )
			return &rList[i];
	return 0;
}

// Pitfall 1 as one rule: a record with fewer than two control points or two
// sampled points kills the game's road and river loaders (they loop to
// points.size() - 1), so it never reaches a put.
bool LongEnough( const SVectorStripeObject &rVso )
{
	return rVso.controlpoints.size() >= 2 && rVso.points.size() >= 2;
}

// A new record from a descriptor and a control polyline (the MFC add:
// CreateVSO, Update( false ), UpdateZ). The saved nID is the bridge's own
// (above every nID the saved map uses, D-03); a road whose descriptor has
// passability 0 is saved with 1, because the map reader turns 0 into 1 and
// the save's read-back would refuse the difference (Pitfall 2).
bool BuildVso( SEditorSession *pSession, int nKind, const std::string &szDesc, const std::vector<CVec3> &rPoints,
               float fWidth, float fOpacity, SVectorStripeObject *pOut, bool *pbRefused )
{
	const std::string szDescName = DescriptorFolder( *pSession, nKind ) + szDesc;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 || !pStorage->IsStreamExist( ( szDescName + ".xml" ).c_str() ) )
	{
		pSession->szMessage = NStr::Format( "there is no %s type \"%s\" in this map's season", KindName( nKind ), szDesc.c_str() );
		*pbRefused = true;
		return false;
	}
	SVectorStripeObject vso;
	if ( !CVSOBuilder::CreateVSO( &vso, szDescName, rPoints ) )
	{
		pSession->szMessage = NStr::Format( "the %s type \"%s\" does not load", KindName( nKind ), szDesc.c_str() );
		*pbRefused = true;
		return false;
	}
	if ( vso.controlpoints.size() < 2 )
	{
		pSession->szMessage = NStr::Format( "that %s is too short: it needs two points at least 2 units apart", KindName( nKind ) );
		*pbRefused = true;
		return false;
	}
	CVSOBuilder::Update( &vso, false, CVSOBuilder::DEFAULT_STEP, fWidth, fOpacity );
	if ( vso.points.size() < 2 )
	{
		pSession->szMessage = NStr::Format( "that %s is too short: it is shorter than one sampling step (30 units)", KindName( nKind ) );
		*pbRefused = true;
		return false;
	}
	CVSOBuilder::UpdateZ( pSession->working.terrain.altitudes, &vso );
	if ( nKind == 0 && vso.fPassability == 0 )
		vso.fPassability = 1;
	vso.nID = NMapRecords::NextVsoID( pSession->snapshot );
	*pOut = vso;
	return true;
}

// The one put of a road or river (see the file's head): pBefore is what the
// saved list holds at nIndex now (null for an insert there), pAfter what it
// is to hold (null for an erase). Both copies first, then the engine, the
// engine IDs kept in step. False with the reason in szMessage, the copies as
// they were when the map would not take it.
bool PutVso( SEditorSession *pSession, int nKind, int nIndex, const SVectorStripeObject *pBefore, const SVectorStripeObject *pAfter )
{
	ITerrainEditor *pTerrain = EngineTerrain();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pTerrain == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	std::vector<int> &rIDs = pSession->vsoEngineIDs[nKind];
	if ( nIndex < 0 || nIndex > int( rIDs.size() ) || ( pBefore != 0 && nIndex >= int( rIDs.size() ) ) )
	{
		pSession->szMessage = NStr::Format( "no %s %d", KindName( nKind ), nIndex );
		return false;
	}
	const NMapRecords::EVsoKind eKind = ToKind( nKind );

	// The map: the snapshot, then the working copy, the snapshot put back if
	// the working copy will not take it.
	if ( pBefore != 0 && pAfter != 0 )
	{
		if ( !NMapRecords::ReplaceVso( &pSession->snapshot, eKind, nIndex, *pAfter ) )
			return false;
		if ( !NMapRecords::ReplaceVso( &pSession->working, eKind, nIndex, *pAfter ) )
		{
			NMapRecords::ReplaceVso( &pSession->snapshot, eKind, nIndex, *pBefore );
			return false;
		}
	}
	else if ( pAfter != 0 )
	{
		if ( !NMapRecords::InsertVso( &pSession->snapshot, eKind, nIndex, *pAfter ) )
			return false;
		if ( !NMapRecords::InsertVso( &pSession->working, eKind, nIndex, *pAfter ) )
		{
			NMapRecords::EraseVso( &pSession->snapshot, eKind, nIndex );
			return false;
		}
	}
	else if ( pBefore != 0 )
	{
		if ( !NMapRecords::EraseVso( &pSession->snapshot, eKind, nIndex ) )
			return false;
		if ( !NMapRecords::EraseVso( &pSession->working, eKind, nIndex ) )
		{
			NMapRecords::InsertVso( &pSession->snapshot, eKind, nIndex, *pBefore );
			return false;
		}
	}
	else
		return true;

	// The engine: the old stripe goes, the new one comes, each by the engine's
	// own ID. RemoveRoad's answer is not trusted (it answered false for years,
	// Pitfall 3); VsoMatchesEngine is what says the two agree.
	if ( pBefore != 0 )
	{
		if ( nKind == 1 )
			pAIEditor->DeleteRiver( *pBefore );
		if ( nKind == 1 )
			pTerrain->RemoveRiver( rIDs[nIndex] );
		else
			pTerrain->RemoveRoad( rIDs[nIndex] );
		rIDs.erase( rIDs.begin() + nIndex );
	}
	if ( pAfter != 0 )
	{
		const int nEngineID = nKind == 1 ? pTerrain->AddRiver( *pAfter ) : pTerrain->AddRoad( *pAfter );
		rIDs.insert( rIDs.begin() + nIndex, nEngineID );
		if ( nKind == 1 )
			pAIEditor->AddRiver( *pAfter );
	}
	return true;
}

// One road or river edit: the record before and after (either may be absent,
// for an add or a delete) at a list position. Undo and redo are the same put
// with the two swapped.
struct SVsoEdit : public IEditRecord
{
	int nKind;
	int nIndex;
	bool bBefore, bAfter;
	SVectorStripeObject before, after;

	SVsoEdit() : nKind( 0 ), nIndex( 0 ), bBefore( false ), bAfter( false ) {  }
	virtual bool Revert( SEditorSession *pSession )
	{
		return PutVso( pSession, nKind, nIndex, bAfter ? &after : 0, bBefore ? &before : 0 );
	}
	virtual bool Reapply( SEditorSession *pSession )
	{
		return PutVso( pSession, nKind, nIndex, bBefore ? &before : 0, bAfter ? &after : 0 );
	}
};

// Puts an edit through and logs it: the edit's record is taken over whether
// the put succeeds or not.
bool ApplyAndLog( SEditorSession *pSession, SVsoEdit *pEdit, int *pnToken )
{
	std::unique_ptr<SVsoEdit> edit( pEdit );
	if ( !edit->Reapply( pSession ) )
		return false;
	*pnToken = LogEdit( pSession, edit.release() );
	return true;
}

bool SamePoint( const SVectorStripeObjectPoint &rLeft, const SVectorStripeObjectPoint &rRight )
{
	return rLeft.vPos == rRight.vPos && rLeft.vNorm == rRight.vNorm && rLeft.fRadius == rRight.fRadius &&
	       rLeft.fWidth == rRight.fWidth && rLeft.bKeyPoint == rRight.bKeyPoint && rLeft.fOpacity == rRight.fOpacity;
}

// Where two records of the same kind differ, in the things a road or river
// edit changes, or "" when they agree. The nID is left out on purpose: the
// engine's is its own.
std::string VsoDifference( const SVectorStripeObject &rMap, const SVectorStripeObject &rEngine )
{
	if ( rMap.szDescName != rEngine.szDescName )
		return "the descriptor";
	if ( rMap.controlpoints.size() != rEngine.controlpoints.size() )
		return NStr::Format( "the control point count (%d, the engine %d)", int( rMap.controlpoints.size() ), int( rEngine.controlpoints.size() ) );
	for ( size_t i = 0; i < rMap.controlpoints.size(); ++i )
		if ( !( rMap.controlpoints[i] == rEngine.controlpoints[i] ) )
			return NStr::Format( "control point %d", int( i ) );
	if ( rMap.points.size() != rEngine.points.size() )
		return NStr::Format( "the point count (%d, the engine %d)", int( rMap.points.size() ), int( rEngine.points.size() ) );
	for ( size_t i = 0; i < rMap.points.size(); ++i )
		if ( !SamePoint( rMap.points[i], rEngine.points[i] ) )
			return NStr::Format( "point %d", int( i ) );
	return "";
}
}

void ResetVsoEngineIDs( SEditorSession *pSession )
{
	for ( int nKind = 0; nKind < nKinds; ++nKind )
	{
		std::vector<int> &rIDs = pSession->vsoEngineIDs[nKind];
		rIDs.clear();
		ITerrainEditor *pTerrain = EngineTerrain();
		if ( pTerrain == 0 )
			continue;
		const TVSOList &rList = EngineList( pTerrain, nKind );
		for ( size_t i = 0; i < rList.size(); ++i )
			rIDs.push_back( rList[i].nID );
	}
}

int VsoCount( const SEditorSession &rSession, int nKind )
{
	return IsKind( nKind ) ? int( SavedList( rSession, nKind ).size() ) : -1;
}

const SVectorStripeObject* SessionVso( const SEditorSession &rSession, int nKind, int nIndex )
{
	if ( !IsKind( nKind ) )
		return 0;
	const TVSOList &rList = SavedList( rSession, nKind );
	return nIndex >= 0 && nIndex < int( rList.size() ) ? &rList[nIndex] : 0;
}

bool VsoDescriptors( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames )
{
	pNames->clear();
	if ( !IsKind( nKind ) )
		return false;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
	{
		pSession->szMessage = "the engine has no data storage";
		return false;
	}
	// The storage's names are compared lower-cased and with backslashes: a
	// directory storage and a pak do not agree on either.
	std::string szFolder = DescriptorFolder( *pSession, nKind );
	NStr::ToLower( szFolder );
	std::replace( szFolder.begin(), szFolder.end(), '/', '\\' );
	CPtr<IStorageEnumerator> pEnumerator = pStorage->CreateEnumerator();
	if ( pEnumerator == 0 )
	{
		pSession->szMessage = "the data storage cannot list its files";
		return false;
	}
	pEnumerator->Reset( "*.*" );
	while ( pEnumerator->Next() )
	{
		const SStorageElementStats *pStats = pEnumerator->GetStats();
		if ( pStats == 0 || pStats->pszName == 0 )
			continue;
		std::string szName = pStats->pszName;
		NStr::ToLower( szName );
		std::replace( szName.begin(), szName.end(), '/', '\\' );
		if ( szName.size() <= szFolder.size() + 4 || szName.compare( 0, szFolder.size(), szFolder ) != 0 )
			continue;
		if ( szName.compare( szName.size() - 4, 4, ".xml" ) != 0 )
			continue;
		const std::string szBare = szName.substr( szFolder.size(), szName.size() - szFolder.size() - 4 );
		if ( szBare.find( '\\' ) != std::string::npos )
			continue;
		pNames->push_back( szBare );
	}
	std::sort( pNames->begin(), pNames->end() );
	pNames->erase( std::unique( pNames->begin(), pNames->end() ), pNames->end() );
	return true;
}

bool AddVsoToSession( SEditorSession *pSession, int nKind, const std::string &szDesc, const std::vector<CVec3> &rPoints,
                      float fWidthTiles, float fOpacity, int *pnToken, int *pnIndex, bool *pbRefused )
{
	*pbRefused = false;
	*pnToken = -1;
	*pnIndex = -1;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	// bridge.cpp turned these away already; the guards stay because the
	// engine's asserts are compiled out.
	if ( !IsKind( nKind ) || int( rPoints.size() ) > nMaxControlPoints || !( fWidthTiles >= 1.0f && fWidthTiles <= 16.0f ) || !( fOpacity >= 0.0f && fOpacity <= 1.0f ) )
	{
		pSession->szMessage = "a road or river takes a kind of 0 or 1, width 1 to 16 and opacity 0 to 1";
		return false;
	}
	if ( !IsBareName( szDesc ) )
	{
		pSession->szMessage = NStr::Format( "\"%s\" is not a %s type name", szDesc.c_str(), KindName( nKind ) );
		*pbRefused = true;
		return false;
	}
	for ( size_t i = 0; i < rPoints.size(); ++i )
		if ( !OnTheMap( *pSession, rPoints[i].x, rPoints[i].y ) )
		{
			pSession->szMessage = NStr::Format( "point %d of the %s is not on the map", int( i ), KindName( nKind ) );
			*pbRefused = true;
			return false;
		}
	std::unique_ptr<SVsoEdit> edit( new SVsoEdit );
	if ( !BuildVso( pSession, nKind, szDesc, rPoints, fWidthTiles * fWorldCellSize / 2.0f, fOpacity, &edit->after, pbRefused ) )
		return false;
	edit->nKind = nKind;
	edit->nIndex = VsoCount( *pSession, nKind );
	edit->bAfter = true;
	const int nIndex = edit->nIndex;
	if ( !ApplyAndLog( pSession, edit.release(), pnToken ) )
		return false;
	*pnIndex = nIndex;
	return true;
}

bool VsoMatchesEngine( SEditorSession *pSession )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	ITerrainEditor *pTerrain = EngineTerrain();
	if ( pTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	for ( int nKind = 0; nKind < nKinds; ++nKind )
	{
		const TVSOList &rMap = SavedList( *pSession, nKind );
		const TVSOList &rWorking = nKind == 0 ? pSession->working.terrain.roads3 : pSession->working.terrain.rivers;
		const TVSOList &rEngine = EngineList( pTerrain, nKind );
		const std::vector<int> &rIDs = pSession->vsoEngineIDs[nKind];
		if ( rMap.size() != rEngine.size() || rMap.size() != rIDs.size() || rMap.size() != rWorking.size() )
		{
			pSession->szMessage = NStr::Format( "the map has %d %ss, the working copy %d, the engine %d and the ID map %d",
			                                    int( rMap.size() ), KindName( nKind ), int( rWorking.size() ), int( rEngine.size() ), int( rIDs.size() ) );
			return false;
		}
		for ( size_t i = 0; i < rMap.size(); ++i )
		{
			const SVectorStripeObject *pEngine = FindEngineVso( pTerrain, nKind, rIDs[i] );
			if ( pEngine == 0 )
			{
				pSession->szMessage = NStr::Format( "the engine has no %s with ID %d (%s %d of the map)", KindName( nKind ), rIDs[i], KindName( nKind ), int( i ) );
				return false;
			}
			std::string szWhere = VsoDifference( rMap[i], *pEngine );
			if ( szWhere.empty() )
				szWhere = VsoDifference( rMap[i], rWorking[i] );
			if ( !szWhere.empty() )
			{
				pSession->szMessage = NStr::Format( "%s %d differs from the engine's (or the working copy's) at %s", KindName( nKind ), int( i ), szWhere.c_str() );
				return false;
			}
			for ( size_t j = 0; j < i; ++j )
				if ( rIDs[j] == rIDs[i] )
				{
					pSession->szMessage = NStr::Format( "%ss %d and %d share the engine ID %d", KindName( nKind ), int( j ), int( i ), rIDs[i] );
					return false;
				}
		}
	}
	return true;
}
