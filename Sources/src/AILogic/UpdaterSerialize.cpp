#include "stdafx.h"

#include "Updater.h"
#include "SaveDBID.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUpdater::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &pGameSegment );
	saver.Add( 2, &pGameTimer );
	saver.Add( 3, &feedBacks );
	saver.Add( 4, &garbage );
	saver.Add( 5, &bPlacementsUpdated );
	saver.Add( 6, &nShootAreasGroup );
	saver.Add( 7, &updatedPlacements );
	saver.Add( 8, &unitAnimation );
	saver.Add( 29, &simpleUpdates );
	saver.Add( 30, &complexUpdates );

	if ( saver.IsReading() )
		UpdateAreasGroup( -1 );

	int nCheck = 1;
	saver.Add( 31, &nCheck );
	if ( !saver.IsReading() )
	{
		// saver all simple actions with nParam == dbID
		int nChunk = 32;
		for ( CSimpleUpdatesSet::iterator iter = simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].begin(); iter != simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].end(); ++iter )
			SaveDBID( &saver, nChunk++, iter->second.nParam );
	}
	else
	{
		if ( nCheck == 1 )
		{
			int nChunk = 32;
			CSimpleUpdatesSet dbIDs;
			for ( CSimpleUpdatesSet::iterator iter = simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].begin(); iter != simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].end(); ++iter )
			{
				int nDBID;
				LoadDBID( &saver, nChunk++, &nDBID );
				
				SSimpleUpdate newSimpleUpdate( iter->second );
				newSimpleUpdate.nParam = nDBID;
				dbIDs[iter->first] = newSimpleUpdate;
			}

			
			simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].clear();
			simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4] = dbIDs;
		}
	}

	saver.Add( static_cast<SSChunkID>( 32 + simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].size() ), &bDestroying );

	saver.Add( static_cast<SSChunkID>( 33 + simpleUpdates[ACTION_NOTIFY_CHANGE_DBID >> 4].size() ), &bGameFinishUpdateSend );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
