// What the editor can place: every object the database knows, as flat records.
//
// A fixed char name[64] rather than a pointer, so nothing is owned across the
// ABI and there is nothing to free. The longest key in shipped Data is well
// inside that, and a longer one is truncated rather than refused - a catalogue
// that failed because of one odd name would be worse than one entry the caller
// cannot place.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "../Main/GameDB.h"
#include "../Main/RPGStats.h"

namespace {

bool SameName( const std::string &rszA, const std::string &rszB )
{
	std::string szA = rszA, szB = rszB;
	NStr::ToLower( szA );
	NStr::ToLower( szB );
	return szA == szB;
}

// A squad to place instead of the soldier: one of that soldier alone if the
// database has it (Us_Sniper -> US_sniper), else the alphabetically first that
// lists it (Allies_Bren -> GB_bren_43), so the answer never depends on the
// database's order. AISingleUnitFormation is left out: it is the AI's own
// template for a soldier left alone in play, not a squad of its own, and the
// MFC editor's palette hid it too.
std::string SquadFor( IObjectsDB *pObjectsDB, const std::string &rszSoldier )
{
	std::string szAlone, szFirst;
	const SGDBObjectDesc *pDescs = pObjectsDB->GetAllDescs();
	const int nDescs = pObjectsDB->GetNumDescs();
	for ( int i = 0; i < nDescs; ++i )
	{
		if ( pDescs[i].eGameType != SGVOGT_SQUAD || SameName( pDescs[i].szKey, "AISingleUnitFormation" ) )
			continue;
		const SSquadRPGStats *pSquad = NGDB::GetRPGStats<SSquadRPGStats>( pObjectsDB, &pDescs[i] );
		if ( pSquad == 0 )
			continue;
		bool bListed = false, bOthers = false;
		for ( size_t m = 0; m < pSquad->memberNames.size(); ++m )
		{
			if ( SameName( pSquad->memberNames[m], rszSoldier ) )
				bListed = true;
			else
				bOthers = true;
		}
		if ( !bListed )
			continue;
		const std::string &rszSquad = pDescs[i].szKey;
		if ( !bOthers && ( szAlone.empty() || rszSquad < szAlone ) )
			szAlone = rszSquad;
		if ( szFirst.empty() || rszSquad < szFirst )
			szFirst = rszSquad;
	}
	return szAlone.empty() ? szFirst : szAlone;
}

}

// A single soldier is an infantry SGVOGT_UNIT: sprite-drawn, where every
// vehicle and gun is a mesh (SGDBObjectDesc::IsHuman, the test the MFC editor
// and the game's client use for "a soldier"). The game builds a soldier's
// formation only from a squad record (CAILogic::AddObject, SGVOGT_SQUAD ->
// CUnitCreation::AddNewFormation); one stored as a unit came up with none and
// crashed the first AI segment in CSoldierRestState::Segment. So, as in the MFC
// editor, whose palette never listed units\Humans, a soldier is placed in a
// squad or not at all.
std::string WhyNotPlacedAlone( IObjectsDB *pObjectsDB, const SGDBObjectDesc &rDesc )
{
	if ( !rDesc.IsHuman() )
		return "";
	const std::string szSquad = pObjectsDB != 0 ? SquadFor( pObjectsDB, rDesc.szKey ) : std::string();
	if ( szSquad.empty() )
		return "is a single soldier, and the game plays soldiers only in squads: place a squad (SGVOGT_SQUAD) instead";
	return "is a single soldier, and the game plays soldiers only in squads: place the squad \"" + szSquad + "\" (SGVOGT_SQUAD) instead";
}

bool ReadCatalogue( SEditorSession *pSession, BkEditorCatalogueEntry *pOut, int nCapacity, int *pnCount )
{
	if ( pSession == 0 || pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) )
		return false;
	*pnCount = 0;
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
	{
		pSession->szMessage = "the object database is not there";
		return false;
	}
	const SGDBObjectDesc *pDescs = pObjectsDB->GetAllDescs();
	const int nDescs = pObjectsDB->GetNumDescs();
	if ( pDescs == 0 || nDescs <= 0 )
	{
		pSession->szMessage = "the object database is empty";
		return false;
	}
	// The count is always the database's, not what fitted: a caller given a
	// short buffer has to be able to tell, and it can ask again with the right
	// one rather than believing it read everything.
	*pnCount = nDescs;
	const int nWrite = nDescs < nCapacity ? nDescs : nCapacity;
	for ( int i = 0; i < nWrite; ++i )
	{
		const std::string &rszName = pDescs[i].szKey;
		const size_t nCopy = rszName.size() < sizeof pOut[i].name - 1 ? rszName.size() : sizeof pOut[i].name - 1;
		memcpy( pOut[i].name, rszName.c_str(), nCopy );
		pOut[i].name[nCopy] = 0;
		pOut[i].game_type = int( pDescs[i].eGameType );
		// The same three questions AddObjectToSession asks; the squad lookup is
		// skipped (no database passed), since only an empty answer matters here.
		// WhyNotPlacedByPalette is D-05: a span, a trench piece and a fence are
		// drawn with their tools, not offered one by one.
		pOut[i].placeable = WhyNotAMapObject( pDescs[i].eGameType ) == 0 && WhyNotPlacedByPalette( pDescs[i].eGameType ) == 0 &&
		                    WhyNotPlacedAlone( 0, pDescs[i] ).empty() ? 1 : 0;
	}
	if ( nDescs > nCapacity )
	{
		pSession->szMessage = NStr::Format( "the catalogue has %d entries and room was given for %d", nDescs, nCapacity );
		return false;
	}
	return true;
}
