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
	}
	if ( nDescs > nCapacity )
	{
		pSession->szMessage = NStr::Format( "the catalogue has %d entries and room was given for %d", nDescs, nCapacity );
		return false;
	}
	return true;
}
