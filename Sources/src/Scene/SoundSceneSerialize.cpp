#include "StdAfx.h"

#include "SoundScene.h"
int CSoundScene::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	
	if ( saver.IsReading() )
	{
		/*pSFX = GetSingleton<ISFX>();
		pSoundManager  = GetSingleton<ISoundManager>();
		pGameTimer = GetSingleton<IGameTimer>();
		pObjectsDB = GetSingleton<IObjectsDB>();*/
		InitConsts();
	}
	saver.Add( 2, &freeIDs );
	saver.Add( 5, &curTime );
	saver.Add( 6, &timeLastUpdate );
	saver.Add( 7, &vFormerCameraCell );
	saver.Add( 8, &substTable );
	saver.Add( 9, &vLimit );
	saver.Add( 10, &soundIDs );
	saver.Add( 12, &interfaceSounds );
	saver.Add( 13, &soundCellsInBounds  );
	saver.Add( 14, &streamingSounds );
	saver.Add( 15, &terrainSounds );
	
	saver.Add( 16, &mapSounds );
	if ( saver.IsReading() )
		mapSounds.SetSoundScene( this );
	saver.Add( 17, &vScreenResize );
	saver.Add( 18, &soundCellsWithSound );
	saver.Add( 19, &soundCellsOutOfBounds );
	saver.Add( 20, &eSoundSceneMode );
	saver.Add( 21, &finishedInterfaceSounds );
	saver.Add( 22, &deletedInterfaceSounds );

	if ( saver.IsReading() )
		cellsPHS.Clear();

	saver.Add( 23, &cellsPHS );
	
	if ( saver.IsReading() )
		if ( !cellsPHS.IsInitted() )
			cellsPHS.Init( vLimit.x, vLimit.y );
	return 0;
}
int CMapSounds::CMapSoundCell::SMapSounds::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &instanceIDs ); // это не сериализовать
	saver.Add( 3, &nCount );
	return 0;
}
int CMapSounds::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 4, &soundIDs );
	saver.Add( 16, &mapCells );
	saver.Add( 17, &cells );
	saver.Add( 8, &instanceIDs );
	saver.Add( 9, &timeNextUpdate );
	saver.Add( 18, &registeredSounds );
	return 0;
}
int CMapSounds::CMapSoundCell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &playingLoopedSound );
	saver.Add( 2, &playingSound );
	saver.Add( 3, &cellSounds );
	saver.Add( 4, &timeNextRun );
	saver.Add( 5, &cellLoopedSounds );
	return 0;

}
int CSoundScene::CSubstSound::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		pSFX = GetSingleton<ISFX>();
	}

	ISound * pS = pSample;

	saver.Add( 3, &pSample );
	return 0;
}

int CSoundScene::CSound::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &wID );														// 
	saver.Add( 3, &timeBegin );
	saver.Add( 4, &timeToPlay );
	saver.Add( 5, &timeBeginDim );
	saver.Add( 6, &szName );
	saver.Add( 7, &eMixType );
	saver.Add( 8, &vPos );
	saver.Add( 9, &bLooped );
	saver.Add( 10, &bStartedMark );
	saver.Add( 11, &bFinishedMark );
	saver.Add( 12, &bDimMark );
	saver.Add( 13, &pSubstitute );

	saver.Add( 14, &pSample );
	saver.Add( 15, &nMaxRadius );
	saver.Add( 16, &nMinRadius );
	
	saver.Add( 17, &eCombatType );
	return 0;
}
int CSoundScene::CSoundCell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &nRadius );
	saver.Add( 4, &sounds );
	saver.Add( 5, &timeLastCombatHear );
	return 0;
}
int CSoundScene::CStreamingSounds::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		

	if ( saver.IsReading() )
	{
		pSFX = GetSingleton<ISFX>();
		pGameTimer = GetSingleton<IGameTimer>();
	}
	saver.Add( 2, &eState );
	saver.Add( 3, &bCombatNotify );
	saver.Add( 4, &timeLastCombatNotify );
	saver.Add( 5, &timeLastUpdate );
	saver.Add( 6, &timeDesiredPause );
	saver.Add( 7, &pIdle );
	saver.Add( 8, &pCombat );
	if ( saver.IsReading() && ( ESSS_COMBAT == eState || ESSS_START_COMBAT == eState ) )
	{
		eState = ESSS_START_COMBAT_AFTER_LOAD;
	}
	return 0;
}
int CSoundScene::CPlayList::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		

	saver.Add( 1, &melodies );
	saver.Add( 2, &nIter );

	return 0;
}
int CSoundScene::CTerrainSounds::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		

	if ( saver.IsReading() )
	{
		pSoundManager = GetSingleton<ISoundManager>();
		pSFX = GetSingleton<ISFX>();
	}
	saver.Add( 1, &terrainSounds );
	saver.Add( 2, &vCameraAncor );
	saver.Add( 3, &pTerrain );
	saver.Add( 4, &lastUpdateTime	);
	saver.Add( 5, &vScreen );
	saver.Add( 6, &bMuteAll );
	return 0;
}
int CSoundScene::CTerrainSounds::CTerrainSound::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		
	saver.Add( 1, &timeRestart );
	saver.Add( 2, &bMustPlay );
	saver.Add( 6, &fVolume );
	saver.Add( 7, &fPan );
	saver.Add( 8, &vOffset );
	saver.Add( 9, &bNeedUpdate );
	saver.Add( 10, &cycledSounds );
	saver.Add( 11, &vSoundPos );
	saver.Add( 12, &wSound );

	return 0;
}
int CSoundScene::CTerrainSounds::CTerrainSound::SSoundInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		
	saver.Add( 1, &pSound );
	saver.Add( 2, &bPeaceful );

	return 0;
}
int CCellsConglomerateContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		
	saver.Add( 1, &conglomerates );
	saver.Add( 2, &nMaxRank );
	saver.Add( 3, &bInitted );
	return 0;
}