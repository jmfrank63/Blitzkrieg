// Bridges as whole span groups (04-06, D-10, D-11, D-12), the MFC Bridges tab
// (RoadDrawState.cpp) taken into the bridge.
//
// A bridge is one entry of CMapInfo::bridges - the link IDs of its spans, in
// order - plus its span objects in objects or scenarioObjects. The game's
// LoadBridges asserts every link of every entry and then dereferences it
// (AILogicInternal.cpp:607-646, compiled-out asserts: Pitfall 7), so no edit
// here ever leaves an entry naming a missing object: an entry is erased
// before its spans go, and its spans are back before it is inserted again.
//
// Every edit is one SGroupEdit in the session's edit log (04-05): the group
// it takes out and the group it puts in, each the entry, its index and the
// span records of both copies with their list places. Draw puts a group in,
// delete takes one out, rotate does both at the same entry index. Undo and
// redo put the stored records back; nothing is planned again (D-03).
//
// The span geometry is NMapGeometry::PlanBridge, the function the map-file
// tier builds its expected maps with (C5). The saved record holds the packed
// frame type (what the MFC save writes, C6); the working copy and the engine
// get a concrete sprite index chosen with the stats' seeded helpers, never the
// rand() ones.
#include "StdAfx.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include "session.h"
#include "world.h"
#include "../MapFile/MapGeometry.h"
#include "../MapFile/MapRecords.h"
#include "../Main/GameDB.h"
#include "../Main/RPGStats.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
#include "../Formats/fmtTerrain.h"

namespace {

// The name the MFC editor tests for "may be built during play"
// (RoadDrawState.cpp:1253): it matches W_WoodenBig_Heavy_01/02 too.
const char *const BUILD_DURING_PLAY_FAMILY = "WoodenBig_Heavy_";

// No shipped bridge comes near this many spans; a longer drag is a mistake.
const int nMaxBridgeSpans = 256;

const SBridgeRPGStats* BridgeStats( const std::string &szDesc, const SGDBObjectDesc **ppDesc )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
		return 0;
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( szDesc.c_str() );
	if ( pDesc == 0 || pDesc->eGameType != SGVOGT_BRIDGE )
		return 0;
	if ( ppDesc != 0 )
		*ppDesc = pDesc;
	return NGDB::GetRPGStats<SBridgeRPGStats>( pObjectsDB, pDesc );
}

// The record of one object in whichever list holds it, with its list and place.
SMapObjectInfo* FindObject( CMapInfo *pMap, int nLinkID, bool *pbScenario, size_t *pnIndex )
{
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( (*lists[nList])[i].link.nLinkID == nLinkID )
			{
				if ( pbScenario != 0 ) *pbScenario = nList == 1;
				if ( pnIndex != 0 ) *pnIndex = i;
				return &(*lists[nList])[i];
			}
	return 0;
}

// Takes an object out of the engine, the way DeleteObjectFromSession does:
// the link released first, so a later restore under the same ID is not a
// "Repeated link".
void RemoveFromEngine( SEditorSession *pSession, int nLinkID )
{
	std::unordered_map<int, CPtr<IRefCount> >::iterator it = pSession->byLinkID.find( nLinkID );
	if ( it != pSession->byLinkID.end() )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		{
			pAIEditor->ReleaseLink( it->second );
			pAIEditor->DeleteObject( it->second );
		}
		pSession->byLinkID.erase( it );
	}
	pSession->futureBuildLinkIDs.erase( std::remove( pSession->futureBuildLinkIDs.begin(), pSession->futureBuildLinkIDs.end(), nLinkID ),
	                                     pSession->futureBuildLinkIDs.end() );
}

// A bridge as the edit log keeps it: its entry, where the entry sits in the
// list, and the span records of both copies in the order they go back
// (ascending list place, so each goes back to exactly where it was). A span
// the map does not hold is not in the records; the entry keeps naming it.
struct SBridgeGroup
{
	int nEntryIndex;
	std::vector<int> linkIDs;
	std::vector<NMapOverlay::SDeletedObject> snapshotSpans, workingSpans;
	SBridgeGroup() : nEntryIndex( 0 ) {  }
};

bool SameEntry( const std::vector< std::vector<int> > &rBridges, int nIndex, const std::vector<int> &rLinkIDs )
{
	return nIndex >= 0 && nIndex < int( rBridges.size() ) && rBridges[nIndex] == rLinkIDs;
}

// Takes a bridge out: the entry first (both copies), then every span, in
// descending list place, from both copies and the engine. The records taken
// out replace the group's, so a later AddGroup puts back exactly these, list
// places included.
bool RemoveGroup( SEditorSession *pSession, SBridgeGroup *pGroup )
{
	if ( !SameEntry( pSession->snapshot.bridges, pGroup->nEntryIndex, pGroup->linkIDs ) ||
	     !SameEntry( pSession->working.bridges, pGroup->nEntryIndex, pGroup->linkIDs ) )
	{
		pSession->szMessage = NStr::Format( "bridge %d is not the one the edit log holds", pGroup->nEntryIndex );
		return false;
	}
	// Where each span is now, in the order they go back.
	struct SPlace { bool bScenario; size_t nIndex; int nLinkID; };
	std::vector<SPlace> places;
	for ( size_t i = 0; i < pGroup->linkIDs.size(); ++i )
	{
		SPlace place;
		place.nLinkID = pGroup->linkIDs[i];
		if ( FindObject( &pSession->snapshot, place.nLinkID, &place.bScenario, &place.nIndex ) != 0 &&
		     std::find_if( places.begin(), places.end(), [&]( const SPlace &r ) { return r.nLinkID == place.nLinkID; } ) == places.end() )
			places.push_back( place );
	}
	std::sort( places.begin(), places.end(), []( const SPlace &a, const SPlace &b )
	           { return a.bScenario != b.bScenario ? !a.bScenario : a.nIndex < b.nIndex; } );

	NMapRecords::EraseBridgeEntry( &pSession->snapshot, pGroup->nEntryIndex );
	NMapRecords::EraseBridgeEntry( &pSession->working, pGroup->nEntryIndex );
	pGroup->snapshotSpans.assign( places.size(), NMapOverlay::SDeletedObject() );
	pGroup->workingSpans.assign( places.size(), NMapOverlay::SDeletedObject() );
	for ( size_t k = places.size(); k-- > 0; )
	{
		const int nLinkID = places[k].nLinkID;
		RemoveFromEngine( pSession, nLinkID );
		std::string szWhy;
		if ( !NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szWhy, &pGroup->snapshotSpans[k] ) ||
		     !NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szWhy, &pGroup->workingSpans[k] ) )
		{
			// Nothing names a span once its entry is gone, so this is a map
			// the session no longer understands.
			pSession->szMessage = "a span of the bridge would not come out of the map: " + szWhy + "; reopen the map";
			return false;
		}
	}
	return true;
}

// Takes a group's spans back out after a failed AddGroup: the first nRestored
// records of each copy, and whatever the engine took.
void UndoPartialAdd( SEditorSession *pSession, const SBridgeGroup &rGroup, size_t nRestored )
{
	for ( size_t k = nRestored; k-- > 0; )
	{
		const int nLinkID = rGroup.snapshotSpans[k].object.link.nLinkID;
		RemoveFromEngine( pSession, nLinkID );
		std::string szIgnored;
		NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szIgnored );
		NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );
	}
}

// Puts a bridge in: every span back into both copies at its place, the spans
// built in the engine in the entry's order, then the entry at its index. All
// or nothing: when the engine will not take a span (off the map), everything
// this call did is taken back and *pbRefused says so.
bool AddGroup( SEditorSession *pSession, const SBridgeGroup &rGroup, bool *pbRefused )
{
	*pbRefused = false;
	const int nEntries = int( pSession->snapshot.bridges.size() );
	if ( rGroup.nEntryIndex < 0 || rGroup.nEntryIndex > nEntries || rGroup.nEntryIndex > int( pSession->working.bridges.size() ) ||
	     rGroup.snapshotSpans.size() != rGroup.workingSpans.size() )
	{
		pSession->szMessage = NStr::Format( "bridge %d cannot go back into the list", rGroup.nEntryIndex );
		return false;
	}
	size_t nRestored = 0;
	for ( ; nRestored < rGroup.snapshotSpans.size(); ++nRestored )
	{
		if ( !NMapOverlay::RestoreObject( &pSession->snapshot, rGroup.snapshotSpans[nRestored] ) )
			break;
		if ( !NMapOverlay::RestoreObject( &pSession->working, rGroup.workingSpans[nRestored] ) )
		{
			std::string szIgnored;
			NMapOverlay::DeleteObject( &pSession->snapshot, rGroup.snapshotSpans[nRestored].object.link.nLinkID, &szIgnored );
			break;
		}
	}
	if ( nRestored != rGroup.snapshotSpans.size() )
	{
		UndoPartialAdd( pSession, rGroup, nRestored );
		pSession->szMessage = "a span's link ID is in use again";
		return false;
	}
	std::vector<SMapObjectInfo> spans;
	for ( size_t k = 0; k < rGroup.workingSpans.size(); ++k )
		spans.push_back( rGroup.workingSpans[k].object );
	const int nPlaced = BuildOneBridge( pSession, rGroup.linkIDs, spans );
	if ( nPlaced != int( spans.size() ) )
	{
		UndoPartialAdd( pSession, rGroup, nRestored );
		pSession->szMessage = NStr::Format( "the engine would not place %d of the bridge's %d spans there (off the map?)",
		                                    int( spans.size() ) - nPlaced, int( spans.size() ) );
		*pbRefused = true;
		return false;
	}
	NMapRecords::InsertBridgeEntry( &pSession->snapshot, rGroup.nEntryIndex, rGroup.linkIDs );
	NMapRecords::InsertBridgeEntry( &pSession->working, rGroup.nEntryIndex, rGroup.linkIDs );
	for ( size_t k = 0; k < rGroup.linkIDs.size(); ++k )
		pSession->nLinkIDFloor = Max( pSession->nLinkIDFloor, rGroup.linkIDs[k] + 1 );
	return true;
}

// One bridge edit: the group it takes out (bOld) and the group it puts in
// (bNew), at the same entry index when both are there (a rotate). Reapply
// takes the old out and puts the new in; Revert the other way round.
struct SGroupEdit : public IEditRecord
{
	bool bOld, bNew;
	SBridgeGroup oldGroup, newGroup;
	SGroupEdit() : bOld( false ), bNew( false ) {  }

	// The edit as it is first made: a refusal (a span off the map) puts back
	// what was taken out and says so in *pbRefused.
	bool Apply( SEditorSession *pSession, bool *pbRefused )
	{
		*pbRefused = false;
		if ( bOld && !RemoveGroup( pSession, &oldGroup ) )
			return false;
		if ( bNew && !AddGroup( pSession, newGroup, pbRefused ) )
		{
			const std::string szWhy = pSession->szMessage;
			bool bIgnored = false;
			if ( bOld && !AddGroup( pSession, oldGroup, &bIgnored ) )
			{
				UpdateSessionWorld( pSession );
				pSession->szMessage = szWhy + "; and the bridge could not be put back: reopen the map";
				*pbRefused = false;
				return false;
			}
			// The world lets go of whatever the engine took and gave back.
			UpdateSessionWorld( pSession );
			pSession->szMessage = szWhy;
			return false;
		}
		UpdateSessionWorld( pSession );
		return true;
	}
	virtual bool Reapply( SEditorSession *pSession )
	{
		bool bRefused = false;
		return Apply( pSession, &bRefused );
	}
	virtual bool Revert( SEditorSession *pSession )
	{
		bool bRefused = false;
		if ( bNew && !RemoveGroup( pSession, &newGroup ) )
			return false;
		const bool bOk = !bOld || AddGroup( pSession, oldGroup, &bRefused );
		UpdateSessionWorld( pSession );
		return bOk;
	}
};

// A new group from a plan, not yet in the map: link IDs from the floor up, the
// records appended at the end of objects, the entry at nEntryIndex.
bool NewGroupFromPlan( SEditorSession *pSession, const std::string &szDesc, const std::vector<NMapGeometry::SPlannedPiece> &rPlan,
                       float fHP, int nEntryIndex, SBridgeGroup *pGroup )
{
	const SBridgeRPGStats *pStats = BridgeStats( szDesc, 0 );
	if ( pStats == 0 )
	{
		pSession->szMessage = "\"" + szDesc + "\" is not a bridge type";
		return false;
	}
	int nLinkID = Max( NMapOverlay::NextLinkID( pSession->snapshot ), pSession->nLinkIDFloor );
	pGroup->nEntryIndex = nEntryIndex;
	pGroup->linkIDs.clear();
	pGroup->snapshotSpans.clear();
	pGroup->workingSpans.clear();
	for ( size_t i = 0; i < rPlan.size(); ++i, ++nLinkID )
	{
		SMapObjectInfo span;
		span.szName = szDesc;
		span.vPos = rPlan[i].vPos;
		span.nDir = rPlan[i].nDir;
		span.nPlayer = 0;
		span.nScriptID = -1;
		span.fHP = fHP;
		span.link.nLinkID = nLinkID;
		span.link.bIntention = false;
		span.link.nLinkWith = -1;
		span.nFrameIndex = rPlan[i].nPackedType;
		NMapOverlay::SDeletedObject saved;
		saved.object = span;
		saved.bScenario = false;
		saved.nIndex = size_t( -1 );							// RestoreObject appends
		// The working copy's sprite: the seeded helper, seed 0 for the begin and
		// end spans (the begin's origin is the plan's), the span's place for a
		// middle one, so a long bridge is not one repeated plank.
		int nSeed = rPlan[i].nPackedType == NMapGeometry::BRIDGE_SPAN_CENTER ? int( i ) : 0;
		NMapOverlay::SDeletedObject working = saved;
		working.object.nFrameIndex = pStats->GetIndexFromType( rPlan[i].nPackedType, &nSeed );
		working.object.fHP = 1.0f;
		pGroup->linkIDs.push_back( nLinkID );
		pGroup->snapshotSpans.push_back( saved );
		pGroup->workingSpans.push_back( working );
	}
	return true;
}

// The edit is taken over whether it goes through or not.
bool ApplyAndLogGroup( SEditorSession *pSession, SGroupEdit *pEdit, int *pnToken, bool *pbRefused )
{
	std::unique_ptr<SGroupEdit> edit( pEdit );
	if ( !edit->Apply( pSession, pbRefused ) )
		return false;
	*pnToken = LogEdit( pSession, edit.release() );
	return true;
}
}

bool BridgeDescriptorsInSession( SEditorSession *pSession, std::vector<SBridgeDescriptorInfo> *pOut )
{
	pOut->clear();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
	{
		pSession->szMessage = "the object database is not there";
		return false;
	}
	const SGDBObjectDesc *pDescs = pObjectsDB->GetAllDescs();
	const int nDescs = pObjectsDB->GetNumDescs();
	for ( int i = 0; pDescs != 0 && i < nDescs; ++i )
	{
		if ( pDescs[i].eGameType != SGVOGT_BRIDGE )
			continue;
		const SBridgeRPGStats *pStats = NGDB::GetRPGStats<SBridgeRPGStats>( pObjectsDB, &pDescs[i] );
		if ( pStats == 0 )
			continue;
		SBridgeDescriptorInfo info;
		info.szName = pDescs[i].szKey;
		info.nDirection = int( pStats->direction );
		const std::string szPartner = NMapGeometry::BridgePartnerName( info.szName );
		info.bHasPartner = !szPartner.empty() && pObjectsDB->GetDesc( szPartner.c_str() ) != 0;
		info.bBuildDuringPlay = info.szName.find( BUILD_DURING_PLAY_FAMILY ) != std::string::npos;
		pOut->push_back( info );
	}
	std::sort( pOut->begin(), pOut->end(), []( const SBridgeDescriptorInfo &a, const SBridgeDescriptorInfo &b ) { return a.szName < b.szName; } );
	return true;
}

bool BridgePlanInputFor( SEditorSession *pSession, const std::string &szDesc, NMapGeometry::SBridgePlanInput *pInput )
{
	const SBridgeRPGStats *pStats = BridgeStats( szDesc, 0 );
	if ( pStats == 0 )
	{
		pSession->szMessage = "\"" + szDesc + "\" is not a bridge type";
		return false;
	}
	const SBridgeRPGStats::SDamageState &rState = pStats->states[0];
	if ( rState.spans.empty() || rState.begins.empty() || rState.lines.empty() || rState.ends.empty() )
	{
		pSession->szMessage = "the bridge type \"" + szDesc + "\" has no spans, or no begin, middle or end span";
		return false;
	}
	int nSeed = 0;
	const int nBegin = pStats->GetRandomBeginIndex( -1, 0, &nSeed );
	const int nLine = rState.lines[0];
	if ( nBegin < 0 || nBegin >= int( rState.spans.size() ) || nLine < 0 || nLine >= int( rState.spans.size() ) ||
	     rState.spans[nBegin].nSlab < 0 || rState.spans[nBegin].nSlab >= int( pStats->segments.size() ) )
	{
		pSession->szMessage = "the bridge type \"" + szDesc + "\" names a span or segment it does not have";
		return false;
	}
	pInput->nDirection = pStats->direction == SBridgeRPGStats::HORIZONTAL ? NMapGeometry::BRIDGE_HORIZONTAL : NMapGeometry::BRIDGE_VERTICAL;
	pInput->fSpanLength = pStats->GetSpanStats( nLine ).fLength * fWorldCellSize / 2.0f;
	pInput->vBeginOrigin = pStats->GetOrigin( nBegin );
	return true;
}

bool PlanBridgeInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast,
                          std::vector<NMapGeometry::SPlannedPiece> *pSpans, bool *pbRefused )
{
	*pbRefused = false;
	NMapGeometry::SBridgePlanInput input;
	if ( !BridgePlanInputFor( pSession, szDesc, &input ) )
	{
		*pbRefused = true;
		return false;
	}
	std::string szWhy;
	if ( !NMapGeometry::PlanBridge( input, vFirst, vLast, pSpans, &szWhy ) )
	{
		pSession->szMessage = szWhy;
		*pbRefused = true;
		return false;
	}
	if ( int( pSpans->size() ) > nMaxBridgeSpans )
	{
		pSession->szMessage = NStr::Format( "that bridge would have %d spans; the editor draws at most %d", int( pSpans->size() ), nMaxBridgeSpans );
		*pbRefused = true;
		return false;
	}
	return true;
}

bool DrawBridgeInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast,
                          int *pnToken, int *pnIndex, bool *pbRefused )
{
	*pbRefused = false;
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !PlanBridgeInSession( pSession, szDesc, vFirst, vLast, &plan, pbRefused ) )
		return false;
	std::unique_ptr<SGroupEdit> edit( new SGroupEdit );
	edit->bNew = true;
	if ( !NewGroupFromPlan( pSession, szDesc, plan, 1.0f, int( pSession->snapshot.bridges.size() ), &edit->newGroup ) )
	{
		*pbRefused = true;
		return false;
	}
	const int nIndex = edit->newGroup.nEntryIndex;
	if ( !ApplyAndLogGroup( pSession, edit.release(), pnToken, pbRefused ) )
		return false;
	*pnIndex = nIndex;
	return true;
}

void ReadSessionBridges( const SEditorSession &rSession, std::vector<SBridgeInfo> *pOut )
{
	pOut->clear();
	CMapInfo &rMap = const_cast<CMapInfo&>( rSession.snapshot );
	for ( size_t i = 0; i < rMap.bridges.size(); ++i )
	{
		SBridgeInfo info;
		info.nSpans = int( rMap.bridges[i].size() );
		bool bAny = false;
		for ( size_t j = 0; j < rMap.bridges[i].size(); ++j )
		{
			const SMapObjectInfo *pSpan = FindObject( &rMap, rMap.bridges[i][j], 0, 0 );
			if ( pSpan == 0 )
				continue;
			if ( info.szDesc.empty() )
				info.szDesc = pSpan->szName;
			if ( pSpan->fHP < 0 )
				info.bBuiltDuringPlay = true;
			const CVec2 vPos( pSpan->vPos.x, pSpan->vPos.y );
			if ( !bAny )
				info.vMin = info.vMax = vPos;
			info.vMin.x = Min( info.vMin.x, vPos.x );
			info.vMin.y = Min( info.vMin.y, vPos.y );
			info.vMax.x = Max( info.vMax.x, vPos.x );
			info.vMax.y = Max( info.vMax.y, vPos.y );
			bAny = true;
		}
		pOut->push_back( info );
	}
}

bool PickGroupInSession( SEditorSession *pSession, float sx, float sy, int *pnKind, int *pnIndex, bool *pbRefused )
{
	*pbRefused = false;
	*pnKind = -1;
	*pnIndex = -1;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 || pSession->pWorld == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nCount = 0;
	pScene->Pick( CVec2( sx, sy ), &pObjects, &nCount, SGVOGT_UNKNOWN );
	for ( int i = 0; i < nCount; ++i )
	{
		IVisObj *pVisObj = pObjects[i].first;
		IRefCount *pAIObject = 0;
		int nKind = 0;
		// A span is the world's own object (CWorldBase::AddToScene for a span),
		// its slab and girders each a visual that picks it.
		if ( SBridgeSpanObject *pSpan = pSession->pWorld->FindSpanByVis( pVisObj ) )
		{
			pAIObject = pSpan->pAIObj;
			nKind = 1;
		}
		else if ( SMapObject *pMapObject = pSession->pWorld->FindByVis( pVisObj ) )
		{
			if ( pMapObject->pDesc != 0 && pMapObject->pDesc->eGameType == SGVOGT_BRIDGE )
				nKind = 1;
			else if ( pMapObject->pDesc != 0 && pMapObject->pDesc->eGameType == SGVOGT_ENTRENCHMENT )
				nKind = 2;
			pAIObject = pMapObject->pAIObj;
		}
		if ( nKind == 0 || pAIObject == 0 )
			continue;
		std::unordered_map<IRefCount*, int>::const_iterator it = pSession->linkByAI.find( pAIObject );
		if ( it == pSession->linkByAI.end() || it->second == 0 )
			continue;
		const int nLinkID = it->second;
		if ( nKind == 1 )
		{
			const std::vector< std::vector<int> > &rBridges = pSession->snapshot.bridges;
			for ( size_t b = 0; b < rBridges.size(); ++b )
				if ( std::find( rBridges[b].begin(), rBridges[b].end(), nLinkID ) != rBridges[b].end() )
				{
					*pnKind = 1;
					*pnIndex = int( b );
					return true;
				}
		}
		else
		{
			const std::vector<SEntrenchmentInfo> &rTrenches = pSession->snapshot.entrenchments;
			for ( size_t t = 0; t < rTrenches.size(); ++t )
				for ( size_t k = 0; k < rTrenches[t].sections.size(); ++k )
					if ( std::find( rTrenches[t].sections[k].begin(), rTrenches[t].sections[k].end(), nLinkID ) != rTrenches[t].sections[k].end() )
					{
						*pnKind = 2;
						*pnIndex = int( t );
						return true;
					}
		}
	}
	pSession->szMessage = "no bridge or entrenchment there";
	*pbRefused = true;
	return false;
}

namespace {
// Whether every span of an entry is one the editor can take out and put back:
// held by exactly one record of the map, of a type the database knows, and
// held by the engine. A bridge with any other span is kept as read (the
// preservation invariant): its undo could not rebuild it.
bool CanTakeOutWhole( SEditorSession *pSession, const std::vector<int> &rLinkIDs, std::string *pWhy )
{
	if ( rLinkIDs.empty() )
	{
		*pWhy = "that bridge has no spans";
		return false;
	}
	for ( size_t i = 0; i < rLinkIDs.size(); ++i )
	{
		const int nLinkID = rLinkIDs[i];
		int nHolders = 0;
		const std::vector<SMapObjectInfo> *lists[2] = { &pSession->snapshot.objects, &pSession->snapshot.scenarioObjects };
		for ( int nList = 0; nList < 2; ++nList )
			for ( size_t k = 0; k < lists[nList]->size(); ++k )
				if ( (*lists[nList])[k].link.nLinkID == nLinkID )
					++nHolders;
		if ( nLinkID == 0 || nHolders != 1 )
		{
			*pWhy = NStr::Format( "span %d of that bridge (link ID %d) is held by %d objects of the map; the bridge is kept as it is", int( i ), nLinkID, nHolders );
			return false;
		}
		if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() ||
		     pSession->byLinkID.find( nLinkID ) == pSession->byLinkID.end() )
		{
			*pWhy = NStr::Format( "span %d of that bridge (link ID %d) is not one the engine holds; the bridge is kept as it is", int( i ), nLinkID );
			return false;
		}
	}
	return true;
}
}

bool DeleteBridgeFromSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused )
{
	*pbRefused = false;
	const std::vector< std::vector<int> > &rBridges = pSession->snapshot.bridges;
	if ( nIndex < 0 || nIndex >= int( rBridges.size() ) )
	{
		pSession->szMessage = NStr::Format( "no bridge %d", nIndex );
		*pbRefused = true;
		return false;
	}
	std::string szWhy;
	if ( !CanTakeOutWhole( pSession, rBridges[nIndex], &szWhy ) )
	{
		pSession->szMessage = szWhy;
		*pbRefused = true;
		return false;
	}
	std::unique_ptr<SGroupEdit> edit( new SGroupEdit );
	edit->bOld = true;
	edit->oldGroup.nEntryIndex = nIndex;
	edit->oldGroup.linkIDs = rBridges[nIndex];
	return ApplyAndLogGroup( pSession, edit.release(), pnToken, pbRefused );
}

namespace {
// The span records of an entry, in the entry's order, from the snapshot.
bool EntrySpans( SEditorSession *pSession, int nIndex, std::vector<const SMapObjectInfo*> *pSpans, std::string *pWhy )
{
	pSpans->clear();
	const std::vector< std::vector<int> > &rBridges = pSession->snapshot.bridges;
	if ( nIndex < 0 || nIndex >= int( rBridges.size() ) )
	{
		*pWhy = NStr::Format( "no bridge %d", nIndex );
		return false;
	}
	for ( size_t i = 0; i < rBridges[nIndex].size(); ++i )
	{
		const SMapObjectInfo *pSpan = FindObject( &pSession->snapshot, rBridges[nIndex][i], 0, 0 );
		if ( pSpan == 0 )
		{
			*pWhy = NStr::Format( "span %d of bridge %d is not in the map", int( i ), nIndex );
			return false;
		}
		pSpans->push_back( pSpan );
	}
	if ( pSpans->empty() )
	{
		*pWhy = "that bridge has no spans";
		return false;
	}
	return true;
}

// The HP edit of a toggle: the snapshot's HP of each span before and after.
// The working copy and the engine keep 1 either way (the engine will not take
// a negative HP); futureBuildLinkIDs and the mark follow the snapshot.
struct SBridgeBuildEdit : public IEditRecord
{
	std::vector<int> linkIDs;
	std::vector<float> before, after;

	bool Put( SEditorSession *pSession, const std::vector<float> &rHPs )
	{
		for ( size_t i = 0; i < linkIDs.size(); ++i )
			if ( !NMapRecords::SetObjectHP( &pSession->snapshot, linkIDs[i], rHPs[i] ) )
			{
				pSession->szMessage = NStr::Format( "span link ID %d is not in the map", linkIDs[i] );
				return false;
			}
		for ( size_t i = 0; i < linkIDs.size(); ++i )
		{
			std::vector<int> &rFuture = pSession->futureBuildLinkIDs;
			rFuture.erase( std::remove( rFuture.begin(), rFuture.end(), linkIDs[i] ), rFuture.end() );
			if ( rHPs[i] < 0 && pSession->byLinkID.find( linkIDs[i] ) != pSession->byLinkID.end() )
				rFuture.push_back( linkIDs[i] );
		}
		ApplyBridgeMarks( pSession );
		return true;
	}
	virtual bool Reapply( SEditorSession *pSession ) { return Put( pSession, after ); }
	virtual bool Revert( SEditorSession *pSession ) { return Put( pSession, before ); }
};
}

bool RotateBridgeInSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused )
{
	*pbRefused = false;
	std::vector<const SMapObjectInfo*> spans;
	std::string szWhy;
	if ( !EntrySpans( pSession, nIndex, &spans, &szWhy ) || !CanTakeOutWhole( pSession, pSession->snapshot.bridges[nIndex], &szWhy ) )
	{
		pSession->szMessage = szWhy;
		*pbRefused = true;
		return false;
	}
	const std::string szName = spans.front()->szName;
	const std::string szPartner = NMapGeometry::BridgePartnerName( szName );
	if ( szPartner.empty() || BridgeStats( szPartner, 0 ) == 0 )
	{
		pSession->szMessage = "no rotated variant of " + szName;
		*pbRefused = true;
		return false;
	}
	bool bBuilt = false;
	for ( size_t i = 0; i < spans.size(); ++i )
		bBuilt = bBuilt || spans[i]->fHP < 0;
	// The old centre, map units to world units.
	CVec3 vCentre;
	AI2Vis( &vCentre, ( spans.front()->vPos.x + spans.back()->vPos.x ) / 2.0f, ( spans.front()->vPos.y + spans.back()->vPos.y ) / 2.0f, 0.0f );
	NMapGeometry::SBridgePlanInput input;
	if ( !BridgePlanInputFor( pSession, szPartner, &input ) )
	{
		*pbRefused = true;
		return false;
	}
	CVec2 vFirst, vLast;
	NMapGeometry::RotatedBridgeDrag( input, CVec2( vCentre.x, vCentre.y ), int( spans.size() ), &vFirst, &vLast );
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !PlanBridgeInSession( pSession, szPartner, vFirst, vLast, &plan, pbRefused ) )
		return false;
	std::unique_ptr<SGroupEdit> edit( new SGroupEdit );
	edit->bOld = true;
	edit->oldGroup.nEntryIndex = nIndex;
	edit->oldGroup.linkIDs = pSession->snapshot.bridges[nIndex];
	edit->bNew = true;
	if ( !NewGroupFromPlan( pSession, szPartner, plan, bBuilt ? -1.0f : 1.0f, nIndex, &edit->newGroup ) )
	{
		*pbRefused = true;
		return false;
	}
	return ApplyAndLogGroup( pSession, edit.release(), pnToken, pbRefused );
}

bool ToggleBridgeBuildInSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused )
{
	*pbRefused = false;
	std::vector<const SMapObjectInfo*> spans;
	std::string szWhy;
	if ( !EntrySpans( pSession, nIndex, &spans, &szWhy ) )
	{
		pSession->szMessage = szWhy;
		*pbRefused = true;
		return false;
	}
	for ( size_t i = 0; i < spans.size(); ++i )
		if ( spans[i]->szName.find( BUILD_DURING_PLAY_FAMILY ) == std::string::npos )
		{
			pSession->szMessage = "only WoodenBig_Heavy bridges can be built during play";
			*pbRefused = true;
			return false;
		}
	// The MFC toggle goes by the span's own HP (RoadDrawState.cpp:1257): built
	// during play when it is negative. All spans follow the first.
	const bool bBuilt = spans.front()->fHP < 0;
	std::unique_ptr<SBridgeBuildEdit> edit( new SBridgeBuildEdit );
	for ( size_t i = 0; i < spans.size(); ++i )
	{
		edit->linkIDs.push_back( spans[i]->link.nLinkID );
		edit->before.push_back( spans[i]->fHP );
		edit->after.push_back( bBuilt ? 1.0f : -1.0f );
	}
	if ( !edit->Reapply( pSession ) )
		return false;
	*pnToken = LogEdit( pSession, edit.release() );
	return true;
}

void ApplyBridgeMarks( SEditorSession *pSession )
{
	if ( pSession == 0 || pSession->pWorld == 0 )
		return;
	const std::vector< std::vector<int> > &rBridges = pSession->snapshot.bridges;
	for ( size_t b = 0; b < rBridges.size(); ++b )
		for ( size_t i = 0; i < rBridges[b].size(); ++i )
		{
			const int nLinkID = rBridges[b][i];
			std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.find( nLinkID );
			if ( it == pSession->byLinkID.end() )
				continue;
			SBridgeSpanObject *pSpan = pSession->pWorld->FindSpanByAI( it->second );
			if ( pSpan == 0 )
				continue;
			const bool bFuture = std::find( pSession->futureBuildLinkIDs.begin(), pSession->futureBuildLinkIDs.end(), nLinkID ) != pSession->futureBuildLinkIDs.end();
			pSpan->SetSpecular( bFuture ? 0xFF0000FF : 0x00000000 );
		}
}
