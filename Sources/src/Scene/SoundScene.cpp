#include "StdAfx.h"

#include "SoundScene.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Scene\Terrain.h"
#include "..\Formats\fmtMap.h"
#include "..\Main\RPGStats.h"
#include "..\Misc\FreeIds.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CSoundScene );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
NTimer::STime CSoundScene::curTime;			// ����� �� ���������� �����
SIntPair CSoundScene::vLimit;						// ������ � ������� ���� �������� �����
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															SSoundSceneConsts										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSoundSceneConsts::SS_SOUND_CELL_SIZE;									// ����� ������� � ������
NTimer::STime SSoundSceneConsts::SS_MIX_DELTA;							// ������������ ������� �� ������� 
NTimer::STime SSoundSceneConsts::SS_UPDATE_PERIOD;					// � ������������
NTimer::STime SSoundSceneConsts::SS_SOUND_DIM_TIME;								// ����� ��������� ����� ��� ��������
NTimer::STime SSoundSceneConsts::SS_COMBAT_MUSIC_FADE;
NTimer::STime SSoundSceneConsts::SS_COMBAT_MUSIC_PLAY_WO_NOTIFY;	// ������� ������ ������� ��������� ����� ��������� ��������� ������
NTimer::STime SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE;				// ��� ���������� ������� ������ ����� Combat ����� �������� 
NTimer::STime SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE_RND;		// IDLE
NTimer::STime SSoundSceneConsts::SS_IDLE_PAUSE; 
NTimer::STime SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE_RANDOM;//for changing looped sounds from time to time
NTimer::STime SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE;			//for changing not looped sounds from time to time
int SSoundSceneConsts::SS_AMBIENT_TERRAIN_SOUNDS;						//number of playing terrain sounds simualteniously 
float SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MIN;					// radius of constant volume for sounds from terrain(with respect to screen size)
float SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MAX;
float SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT;						// terrain weight needed to set volume of 1.0f

float SSoundSceneConsts::DEFAULT_SCREEN_WIDTH;
float SSoundSceneConsts::DEFAULT_SCREEN_HEIGHT;
NTimer::STime SSoundSceneConsts::MAP_SOUNDS_UPDATE;

float SSoundSceneConsts::COMBAT_MUSIC_VOLUME;
float SSoundSceneConsts::IDLE_MUSIC_VOLUME;
float SSoundSceneConsts::COMBAT_SOUNDR_FEAR_RADIUS;
float SSoundSceneConsts::COMBAT_FEAR_TIME;
int SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MIN_RADIUS;
int SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MAX_RADIUS;

NTimer::STime SSoundSceneConsts::SS_MAP_SOUND_PERIOND;
NTimer::STime SSoundSceneConsts::SS_MAP_SOUND_PERIOND_RANDOM;
int SSoundSceneConsts::MAP_SOUND_CELL;

int SSoundSceneConsts::MIN_SOUND_COUNT_TO_PLAY_LOOPED;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IObjectsDB * CSoundScene::pObjectsDB;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CMapSounds
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::CMapSoundCell::AddSound( const WORD wSoundID, const CVec2 &vPos, const CMapSounds::RegisteredSounds &registeredSounds, const WORD wInstanceID, const bool bLooped )
{
	if ( bLooped )
	{
		cellLoopedSounds[wSoundID].instanceIDs[wInstanceID] = vPos;
		++cellLoopedSounds[wSoundID].nCount;
	}
	else
	{
		cellSounds[wSoundID].instanceIDs[wInstanceID] = vPos;
		++cellSounds[wSoundID].nCount;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::CMapSoundCell::RemoveSound( const WORD wInstanceID, class CSoundScene * pScene )
{
	// if removed sound is playing - remove it from sound scene
	if ( wInstanceID == playingLoopedSound.wInstanceID && 0 != playingLoopedSound.wSceneID)
	{
		pScene->RemoveSound( playingLoopedSound.wSceneID );
		playingLoopedSound.Clear();
	}
	// if currently playing looped sound mustn't play anymore, stop it.
	if ( wInstanceID == playingSound.wInstanceID && 0 != playingSound.wSceneID )
	{
		pScene->RemoveSound( playingSound.wSceneID );
		playingSound.Clear();
	}

	//if ( bLooped )
		RemoveSound( &cellLoopedSounds, wInstanceID );
	//else
		RemoveSound( &cellSounds, wInstanceID );

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::CMapSoundCell::RemoveSound( CMapSounds::CMapSoundCell::CellSounds *pCellSounds, const WORD wInstanceID )
{
	// remove this sound from types list
	for ( CellSounds::iterator it = pCellSounds->begin(); pCellSounds->end() != it ; ++it )
	{
		if ( it->second.nCount != 0 )
		{
			SMapSounds &snds = it->second;
			if ( snds.instanceIDs.find( wInstanceID ) != snds.instanceIDs.end() )
			{
				snds.instanceIDs.erase( wInstanceID );
				--snds.nCount;
				if ( 0 == snds.nCount )
					pCellSounds->erase( it );
				break;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::CMapSoundCell::Update( class CSoundScene * pScene, const CMapSounds::RegisteredSounds &registeredSounds )
{
	if ( timeNextRun < CSoundScene::GetCurTime() )
	{
		// if sounds currently playing cannot be played amymore
		if ( 0 != playingLoopedSound.wSoundTypeID && cellSounds[playingLoopedSound.wSoundTypeID].nCount < SSoundSceneConsts::MIN_SOUND_COUNT_TO_PLAY_LOOPED )
		{
			pScene->RemoveSound( playingLoopedSound.wSceneID );
			playingLoopedSound.Clear();
		}

		// find new looped sound to play
		if ( 0 == playingLoopedSound.wSceneID )
		{
			// if there is no sounds plaing or looped run them ( if have to )
			SMaxCountPredicate pr;
			CellSounds::iterator maxElement = std::max_element( cellLoopedSounds.begin(), cellLoopedSounds.end(), pr );
			if ( maxElement != cellLoopedSounds.end() &&
						!maxElement->second.instanceIDs.empty() &&
						maxElement->second.nCount >= SSoundSceneConsts::MIN_SOUND_COUNT_TO_PLAY_LOOPED )
			{
				std::unordered_map<WORD,CVec2>::iterator element = maxElement->second.instanceIDs.begin();
				playingLoopedSound.wInstanceID = element->first;
				playingLoopedSound.wSoundTypeID = maxElement->first;
				playingLoopedSound.wSceneID = pScene->AddSound( registeredSounds.ToT1(playingLoopedSound.wSoundTypeID).c_str(),
																												CVec3( element->second, 0 ),
																												SFX_MIX_IF_TIME_EQUALS,
																												SAM_LOOPED_NEED_ID, 
																												ESCT_ASK_RPG,0,0 );
			}
		}		

		//run next non looped sound
			// remove current cound
		if ( 0 != playingSound.wSceneID )
		{
			pScene->RemoveSound( playingSound.wSceneID );
			playingSound.Clear();
		}
		SMaxCountPredicate pr;
		CellSounds::iterator maxElement = std::max_element( cellSounds.begin(), cellSounds.end(), pr );
		
		if ( maxElement != cellSounds.end() && !maxElement->second.instanceIDs.empty() )
		{
			std::unordered_map<WORD,CVec2>::iterator element = maxElement->second.instanceIDs.begin();
			playingSound.wInstanceID = element->first;
			playingSound.wSoundTypeID = maxElement->first;
			playingSound.wSceneID = pScene->AddSound( registeredSounds.ToT1( playingSound.wSoundTypeID ).c_str(),
																								CVec3( element->second, 0 ),
																												SFX_MIX_IF_TIME_EQUALS,
																												SAM_NEED_ID, 
																												ESCT_ASK_RPG,0,0 );

		}

		// set next run time
		timeNextRun = CSoundScene::GetCurTime() + SSoundSceneConsts::SS_MAP_SOUND_PERIOND + rand() * SSoundSceneConsts::SS_MAP_SOUND_PERIOND_RANDOM / RAND_MAX;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CMapSounds
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::SetSoundScene( class CSoundScene *_pSoundScene )
{
	pSoundScene = _pSoundScene;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::InitSizes( const int nSizeX, const int nSizeY )
{
	// �����
	soundIDs.Clear();
	mapCells.SetSizes( nSizeX / SSoundSceneConsts::MAP_SOUND_CELL + 1, nSizeY / SSoundSceneConsts::MAP_SOUND_CELL + 1 );
	cells.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::RemoveSound( const WORD wInstanceID )
{
	if ( 0 == wInstanceID ) return ;
	NI_ASSERT_T( cells.find( wInstanceID ) != cells.end(), "wrong instace deleted" );
	const SIntPair & vPos = cells[wInstanceID];
	mapCells( vPos.x, vPos.y ).RemoveSound( wInstanceID, pSoundScene );
	instanceIDs.AddToFreeId( wInstanceID );			// crash fixed
	//soundIDs.AddToFreeId( wInstanceID );			// crash
	cells.erase( wInstanceID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CMapSounds::AddSound( const CVec2 &vPos, const char *pszName )
{
	const std::string szName = pszName;
	if ( szName.empty() ) return 0;
	CGDBPtr<SGDBObjectDesc> pDesc = CSoundScene::GetObjectDB()->GetDesc( pszName );

	if ( !pDesc ) return 0;
	NI_ASSERT_T( 0 != dynamic_cast<const SSoundRPGStats*>( CSoundScene::GetObjectDB()->GetRPGStats( pDesc ) ), "not a sound" );
	CGDBPtr<SSoundRPGStats> pStats = static_cast<const SSoundRPGStats*>( CSoundScene::GetObjectDB()->GetRPGStats( pDesc ) );


	// ���������������� ����
	if ( !registeredSounds.IsPresent( pszName ) )
	{
		const WORD wNewID = soundIDs.GetFreeId();
		registeredSounds.Add( szName, wNewID );
	}
	// wSoundID - ������������������ ����.
	const WORD wSoundID = registeredSounds.ToT2( pszName );

	// ���������� � ����� ������ �� ���������.
	const SIntPair vCellPos( vPos.x / SSoundSceneConsts::MAP_SOUND_CELL, vPos.y / SSoundSceneConsts::MAP_SOUND_CELL );


	const WORD wInstanceID = instanceIDs.GetFreeId();
	
	// �������� ��� � ��� ������.
	mapCells( vCellPos.x, vCellPos.y ).AddSound( wSoundID, vPos, registeredSounds, wInstanceID, pStats->bLooped );

	// �������� � ����� ������ - ID �����.
	cells[wInstanceID] = vCellPos;
	
	return wInstanceID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::Clear()
{
	soundIDs.Clear();
	mapCells.Clear();
	cells.clear();
	registeredSounds.Clear();
	instanceIDs.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapSounds::Update( interface ICamera *pCamera )
{
	if ( timeNextUpdate < CSoundScene::GetCurTime() )
	{
		const CVec3 vPos(  pCamera->GetAnchor() );

		const int nMinX = Max( 0, int(vPos.x - SSoundSceneConsts::DEFAULT_SCREEN_WIDTH*2) ) / SSoundSceneConsts::MAP_SOUND_CELL;
		const int nMaxX = Min( mapCells.GetSizeX()-1, int(vPos.x + SSoundSceneConsts::DEFAULT_SCREEN_WIDTH*2) / SSoundSceneConsts::MAP_SOUND_CELL );
		const int nMinY = Max( 0, int(vPos.y - SSoundSceneConsts::DEFAULT_SCREEN_WIDTH*2) ) / SSoundSceneConsts::MAP_SOUND_CELL;
		const int nMaxY = Min( mapCells.GetSizeY()-1, int(vPos.y + SSoundSceneConsts::DEFAULT_SCREEN_WIDTH*2) / SSoundSceneConsts::MAP_SOUND_CELL );

		//scan only triugh cells near screen
		for ( int x = nMinX; x < nMaxX; ++x )
		{
			for ( int y = nMinY; y < nMaxY; ++y )
			{
				mapCells( x, y ).Update( pSoundScene, registeredSounds );
			}
		}
		timeNextUpdate = CSoundScene::GetCurTime() + SSoundSceneConsts::SS_MAP_SOUND_PERIOND;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CPlayList::Reset()
{ 
	nIter = 0; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CPlayList::Clear()
{
	melodies.clear(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CPlayList::Shuffle()
{ 
	std::random_shuffle( melodies.begin(), melodies.end() ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CPlayList::AddMelody( const std::string &pszFileName ) 
{
	melodies.push_back( pszFileName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CSoundScene::CPlayList::GetNextMelody() 
{
	if ( 0 == melodies.size() )
		return 0;
	if ( melodies.size() <= nIter )
	{
		Shuffle();
		nIter = 0;
	}
	return melodies[nIter++].c_str();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CTerrainSound*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::AddCycledSound( const char * szName, interface ISoundManager *pSoundManager )
{
	CGDBPtr<SGDBObjectDesc> pDesc = CSoundScene::GetObjectDB()->GetDesc( szName );
	CGDBPtr<SSoundRPGStats> pStats = static_cast<const SSoundRPGStats*>( CSoundScene::GetObjectDB()->GetRPGStats( pDesc ) );
	NI_ASSERT_T( !pStats.IsEmpty(), NStr::Format( "wrong terrrain looped sound %s", szName ) );

	if ( !pStats->sounds.empty() )
	{
		const SSoundRPGStats::SSound &soundDesc = pStats->sounds[pStats->GetRandomSoundIndex()];
		CPtr<ISound> pNewSound = pSoundManager->GetSound2D( soundDesc.szPathName.c_str() );
		NI_ASSERT_T( pNewSound!=0, NStr::Format( "sound path not found %s for sound object %s", soundDesc.szPathName.c_str(), szName ) );
		cycledSounds.push_back( SSoundInfo(pNewSound, soundDesc.bPeacefull) );
		bNeedUpdate = true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::StartCycledSounds( ISFX *pSFX, bool bNonPeacefulOnly )
{
	for ( CCycledSounds::iterator it = cycledSounds.begin(); it != cycledSounds.end(); ++it )
	{
		if ( bMustPlay && 
				 ( !bNonPeacefulOnly	|| !it->bPeaceful ) &&
				 !pSFX->IsPlaying( it->pSound ) )
			pSFX->PlaySample( it->pSound, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::Update(	const SSoundTerrainInfo& info, 
										const CVec3 &vCameraAnchor, 
										const CVec2 &vScreenSize, 
										const float fRelativeVolume )
{
	//calc volume and pan and set new volume to ISounds 
	//if something changed set bNeedupdateFlag
	vSoundPos	= info.vPos;

	const CVec2 vNewOffset( info.vPos.x - vCameraAnchor.x, info.vPos.y - vCameraAnchor.y );
	if ( vNewOffset != vOffset )
	{
		float fScreenSize = fabs( vScreenSize );
		vOffset = vNewOffset;
		//calc pan
		fPan = vOffset.x / fScreenSize;
		
		//calc volume
		const float fOffset = fabs(vOffset);
			//dependence from distance
		const float fMinRadius = SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MIN * fScreenSize;
		float fVolumeDist;
		if ( fOffset < fMinRadius )
			fVolumeDist = 1.0f;
		else
			fVolumeDist = ( fOffset - fMinRadius ) / 
					( SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MAX * fScreenSize - fMinRadius );
		fVolumeDist = fVolumeDist < 0.0f ? 0.0f : fVolumeDist;

			//dependence from weight		
		float fVolumeWeight;
		if ( info.fWeight > SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT )
			fVolumeWeight = 1.0f;
		else
			fVolumeWeight = info.fWeight / SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT;
		
		fVolume = fVolumeDist * fVolumeWeight * fRelativeVolume ;
		bNeedUpdate = true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::SetMustPlay( bool _bMustPlay ) 
{ 
	bNeedUpdate &= ( bMustPlay == _bMustPlay );
	bMustPlay = _bMustPlay; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::DoUpdate( ISFX * pSFX )
{
	if ( bMustPlay )
	{
		for ( CCycledSounds::iterator it = cycledSounds.begin(); it != cycledSounds.end(); ++it )
		{
			it->pSound->SetVolume( fVolume );
			it->pSound->SetPan( fPan );
			if ( pSFX->IsPlaying( it->pSound ) )
				pSFX->UpdateSample( it->pSound );
		}
	}
	else
		StopSounds( pSFX, false );

	bNeedUpdate = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::SetSound( const char *pszName, NTimer::STime timeWhenRestart )
{
	IScene * pScene = GetSingleton<IScene>();
	if ( wSound )
	{
		if ( pScene->IsSoundFinished( wSound ) )
		{
			pScene->RemoveSound( wSound );
			wSound = 0;
		}
	}

	if ( !wSound )
	{
		wSound = pScene->AddSound( pszName, vSoundPos, SFX_MIX_IF_TIME_EQUALS, 
										SAM_NEED_ID, 
										ESCT_ASK_RPG,
										SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MIN_RADIUS,
										SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MAX_RADIUS );
	}
	timeRestart = timeWhenRestart;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::CTerrainSound::StopSounds( ISFX * pSFX, bool bOnlyPeaceful )
{
	for ( CCycledSounds::iterator it = cycledSounds.begin(); it != cycledSounds.end(); ++it )
	{
		if ( !bOnlyPeaceful || it->bPeaceful )
			pSFX->StopSample( it->pSound );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CTerrainSounds*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::Init( interface ITerrain *_pTerrain )
{
	Clear();
	pTerrain = _pTerrain;
	pSoundManager = GetSingleton<ISoundManager>();
	pSFX = GetSingleton<ISFX>();
	IGFX * pGFX = GetSingleton<IGFX>();
	const RECT rScreen = pGFX->GetScreenRect();
	vScreen.x = abs( rScreen.right - rScreen.left );
	vScreen.y = abs( rScreen.top - rScreen.bottom );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::Clear()
{
	for ( CSounds::iterator it = terrainSounds.begin(); it != terrainSounds.end();  ++it )
		(*it).second.StopSounds( pSFX, false );

	terrainSounds.clear();
	vCameraAncor = CVec3(-1,-1,-1);
	lastUpdateTime = 0;
	bMuteAll = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::Mute( const bool bMute )
{
	bMuteAll = bMute;
	if ( bMuteAll )
	{
		for ( CSounds::iterator it = terrainSounds.begin(); it != terrainSounds.end();  ++it )
		{
			CTerrainSound &sound = (*it).second;
			sound.StopSounds( pSFX, false );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CTerrainSounds::Update( interface ICamera *pCamera, const bool bCombat )
{
	if ( !pTerrain || bMuteAll ) return ;

	// ���� ������ ���������� - �������� ���������� � ������ ������,
	// ��� �� ���������
	const CVec3 vNewCameraAnchor( pCamera->GetAnchor() );

	if ( vCameraAncor != vNewCameraAnchor || 
		CSoundScene::GetCurTime() - lastUpdateTime > SSoundSceneConsts::SS_UPDATE_PERIOD )
	{
		lastUpdateTime = CSoundScene::GetCurTime();
		vCameraAncor = vNewCameraAnchor;
		SSoundTerrainInfo *pInfo;
		int nSize = SSoundSceneConsts::SS_AMBIENT_TERRAIN_SOUNDS;
		pTerrain->GetTerrainMassData( &pInfo, &nSize );

		if ( 0 != nSize )
		{
			// mark sounds that must play (for one pass, so code may be ugly)
			const int nMaxTerrain = pInfo[nSize-1].nTerrainType;
			int nCurSizeIndex = 0;
			for ( int i = 0; i <= nMaxTerrain; ++i )
			{
				if ( i == pInfo[nCurSizeIndex].nTerrainType )
				{
					terrainSounds[i].Update( pInfo[nCurSizeIndex], vCameraAncor, vScreen, pTerrain->GetSoundVolume( i ) );
					terrainSounds[i].SetMustPlay( true );
					if ( nCurSizeIndex + 1 < nSize ) 
						++nCurSizeIndex;
				}
				else
					terrainSounds[i].SetMustPlay( false );
			}
		}
	}
	
	// ���������� all sounds 
	for ( CSounds::iterator it = terrainSounds.begin(); it != terrainSounds.end();  ++it )
	{
		CTerrainSound &sound = (*it).second;
		//if sound is to be changed and it is finished
		// not cycle sound
		if ( sound.GetRestartTime() <= CSoundScene::GetCurTime() && sound.IsMustPlay() )
		{
			//Update sound, run it again
			const char *pszSound = pTerrain->GetTerrainSound( (*it).first );

			sound.SetSound( pszSound, CSoundScene::GetCurTime() + SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE + 
																SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE_RANDOM * rand() / RAND_MAX );
				
		}

		// create cycle sound
		if ( !sound.HasCycleSound() ) //sound isn't started yet
		{
			std::string szName = pTerrain->GetTerrainCycleSound( (*it).first );
			if ( !szName.empty() )
				sound.AddCycledSound( szName.c_str(), pSoundManager );
			sound.StartCycledSounds( pSFX, bCombat );
		}

		// if in combat situation mute peaceful sounds or restart muted in noncombat situation
		if ( bCombat )
			sound.StopSounds( pSFX, true );
		else
			sound.StartCycledSounds( pSFX, false );

		//update sounds
		if ( sound.IsNeedUpdate() )
			sound.DoUpdate( pSFX );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CFreeIds														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::Init( class CSoundScene::CPlayList *_pIdle, class CSoundScene::CPlayList *_pCombat )
{
	pIdle = _pIdle;
	pCombat = _pCombat;
	pSFX = GetSingleton<ISFX>();
	pGameTimer = GetSingleton<IGameTimer>();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::Init( const std::string &_szPartyName )
{
	std::string szPartyName = _szPartyName;
	NStr::ToLower( szPartyName );

	CMusicSettingsList settingsForLoad;
	CMusicSettingsList settings;

	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "MusicSettings.xml" , STREAM_ACCESS_READ );
	if ( pStream )
	{
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "MusicSettings", &settingsForLoad);
	}

	CMusicSettingsList::iterator current = settingsForLoad.end();
	for ( CMusicSettingsList::iterator it = settingsForLoad.begin(); it != settingsForLoad.end(); ++it )
	{
		std::string szName = it->first;
		NStr::ToLower( szName );
		if ( szName == szPartyName )
		{
			current = it;
			break;
		}
	}

	CPtr<CPlayList> pIdle = new CPlayList;
	CPtr<CPlayList> pCombat = new CPlayList;

	if ( settings.end() != current )
	{
		for ( int i = 0;  i < current->second.combat.size(); ++i )
			pCombat->AddMelody( current->second.combat[i] );

		for ( int i = 0; i < current->second.explore.size(); ++i )
			pIdle->AddMelody( current->second.explore[i] );

		pIdle->Shuffle();
		pCombat->Shuffle();
	}
	
	Init( pIdle, pCombat );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CStreamingSounds::CStreamingSounds() 
: eState( ESSS_IDLE_PAUSE ),
	bCombatNotify( false ),
	timeDesiredPause( -1 ),
	timeLastCombatNotify( -1 ),
	timeLastUpdate( 0 )
{  
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::CombatNotify()
{
	bCombatNotify = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::Update()
{
	const NTimer::STime curTime = pGameTimer->GetGameTime();
	if ( pSFX == 0 )
		return;

	if ( bCombatNotify )
		timeLastCombatNotify = curTime;
		
	switch ( eState )
	{
	case ESSS_IDLE: 
		if ( bCombatNotify )
		{
			timeLastUpdate = curTime;
			timeLastCombatNotify = curTime;
			eState = ESSS_FADE_IDLE;
		}
		else if ( !pSFX->IsStreamPlaying() )
		{
			timeLastUpdate = curTime;
			eState = ESSS_IDLE_PAUSE;
		}

		break;
	case ESSS_IDLE_PAUSE: 
		if ( bCombatNotify )
		{
			timeLastUpdate = curTime;
			timeLastCombatNotify = curTime;
			eState = ESSS_FADE_IDLE;
		}
		else if ( curTime - timeLastUpdate > SSoundSceneConsts::SS_IDLE_PAUSE || 0 == timeLastUpdate )
		{
			StartIdleMusic();
			eState = ESSS_IDLE;
		}

		break;
	case ESSS_FADE_IDLE:
		{
			const float fCurVol = pSFX->GetStreamVolume();
			// ��������� Volume �����
			if ( 0.0f == fCurVol ) //���� ��� ����� Volume==0
			{
				timeLastCombatNotify = curTime;
				timeLastUpdate = curTime;
				timeDesiredPause = SSoundSceneConsts::SS_COMBAT_MUSIC_FADE;
				pSFX->StopStream();
				eState = ESSS_START_COMBAT;
			}
			else
			{
				const float fDesiredVol = SSoundSceneConsts::IDLE_MUSIC_VOLUME * ( 1.0f - 1.0f * (curTime - timeLastUpdate) / SSoundSceneConsts::SS_COMBAT_MUSIC_FADE );
				if ( fabs(fCurVol - fDesiredVol) > 0.01f )
					pSFX->SetStreamVolume( fDesiredVol );
			}
		}

		break;
	case ESSS_START_COMBAT_AFTER_LOAD:
		StartCombatMusic();
		eState = ESSS_COMBAT;

		break;
	case ESSS_START_COMBAT:
		if ( !pSFX->IsStreamPlaying() )
		{
			// ��������� ��������� ������
			eState = ESSS_COMBAT;
		}

		break;
	case ESSS_COMBAT:
		if ( curTime - timeLastCombatNotify > SSoundSceneConsts::SS_COMBAT_MUSIC_PLAY_WO_NOTIFY )
		{
			timeLastUpdate = curTime;
			eState = ESSS_COMBAT_FADE;
		}
		else if ( !pSFX->IsStreamPlaying() ) // ���� ������� ����������� - ���������� �����
			StartCombatMusic();
		
		break;
	case ESSS_COMBAT_FADE:
		if ( bCombatNotify )
		{
			timeLastUpdate = curTime;
			eState = ESSS_COMBAT_RESTORE_VOLUME;
		}
		else
		{
			const float fCurVol = pSFX->GetStreamVolume();
			// ��������� Volume �����
			if ( 0.0f == fCurVol ) //���� ��� ����� Volume ==0
			{
				timeLastUpdate = curTime;
				timeDesiredPause = SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE +
						SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE_RND * rand() / RAND_MAX;
				eState = ESSS_PAUSE;
				pSFX->StopStream();
			}
			else
			{
				const float fDesiredVol = SSoundSceneConsts::COMBAT_MUSIC_VOLUME * ( 1.0f - 1.0f * (curTime - timeLastUpdate)/SSoundSceneConsts::SS_COMBAT_MUSIC_FADE );
				if ( fabs(fCurVol - fDesiredVol) > 0.01f )
					pSFX->SetStreamVolume( fDesiredVol );
			}
		}

		break;
	case ESSS_COMBAT_RESTORE_VOLUME:
		{
			const float fCurVol = pSFX->GetStreamVolume();
			if (  SSoundSceneConsts::COMBAT_MUSIC_VOLUME <= fCurVol /* ���� ����� ������� ���������*/)
				eState = ESSS_COMBAT;
			else
			{
				//��������� Volume �����
				const float fDesiredVol = SSoundSceneConsts::COMBAT_MUSIC_VOLUME * ( curTime - timeLastUpdate ) / SSoundSceneConsts::SS_COMBAT_MUSIC_FADE;
				if ( fDesiredVol - fCurVol > 0.01 )
					pSFX->SetStreamVolume( fDesiredVol );
			}
		}

		break;
	case ESSS_PAUSE:
		if ( bCombatNotify )
		{
			eState = ESSS_COMBAT_RESTORE_VOLUME;
			timeLastUpdate = curTime;
		}
		else if ( curTime - timeLastUpdate > timeDesiredPause )
		{
			pSFX->StopStream();
			eState = ESSS_IDLE;
		}

		break;
	}

	bCombatNotify = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::StartIdleMusic()
{
	if ( pIdle && !pIdle->IsEmpty() )
	{
		pSFX->PlayStream( pIdle->GetNextMelody() );
		pSFX->SetStreamVolume( SSoundSceneConsts::IDLE_MUSIC_VOLUME );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::Clear()
{
	if ( eState == ESSS_IDLE ||
			 eState == ESSS_FADE_IDLE ||
			 eState == ESSS_COMBAT ||
			 eState == ESSS_COMBAT_FADE ||
			 eState == ESSS_COMBAT_RESTORE_VOLUME )
	{
		pSFX->StopStream();
	}
	bCombatNotify = false;
	eState = ESSS_IDLE_PAUSE;
	timeDesiredPause =  -1;
	timeLastCombatNotify =  -1;
	timeLastUpdate = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CStreamingSounds::StartCombatMusic()
{
	if ( pCombat && !pCombat->IsEmpty() )
	{
		pSFX->PlayStream( pCombat->GetNextMelody(), true );
		pSFX->SetStreamVolume( SSoundSceneConsts::COMBAT_MUSIC_VOLUME );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CSoundScene::CSound*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSound::CSound(	const WORD wID, 
															const std::string &szName,
															interface ISound *pSound, 
															const enum ESoundMixType eMixType,
															const CVec3 &vPos,
															const bool bLooped,
															const enum ESoundCombatType eCombatType,
															float fMinRadius,
															float fMaxRadius)
: pSample( pSound ), 
	eMixType( eMixType ), 
	bLooped( bLooped ),
	wID( wID ),
	timeBegin( 0 ),
	timeBeginDim( 0 ) ,
	bStartedMark( false ),
	bFinishedMark( false ),
	bDimMark( false ),
	vPos( vPos ),
	szName( szName ),
	nMinRadius( fMinRadius / SSoundSceneConsts::SS_SOUND_CELL_SIZE ),
	nMaxRadius( fMaxRadius / SSoundSceneConsts::SS_SOUND_CELL_SIZE ),
	eCombatType( eCombatType )
{
	nMaxRadius = nMaxRadius == 0 ? 1 : nMaxRadius;
	nMinRadius = nMinRadius == 0 ? 1 : nMinRadius;
	timeToPlay = pSample == 0 ? 0 : 1000 * pSample->GetLenght() / pSample->GetSampleRate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundScene::CSound::IsTimeToFinish()
{
	return CSoundScene::GetCurTime() - GetBeginTime() >= timeToPlay;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CSoundScene::CSound::GetSamplesPassed()
{
	return ( CSoundScene::GetCurTime() - GetBeginTime() ) * GetSound()->GetSampleRate() / 1000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSound::~CSound()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CSoundScene::CSound::GetVolume( const NTimer::STime time, const float fDist ) const
{
	// ������� - ��������� ���������� �����
	float fVolDim = 0.0f;
	if ( bDimMark ) 
		fVolDim = 1.0f * (time - timeBeginDim) / SSoundSceneConsts::SS_SOUND_DIM_TIME;
	if ( fVolDim > 1.0f )
		return 0.0f;

	// ������ ����������� �� ����������
	float fVolDist = 0.0f;
	if ( fDist / SSoundSceneConsts::SS_SOUND_CELL_SIZE > nMinRadius )
		fVolDist = 1.0f* ( fDist / SSoundSceneConsts::SS_SOUND_CELL_SIZE - nMinRadius ) / ( nMaxRadius - nMinRadius );
	if ( fVolDist > 1.0f )
		return 0.0f;

	return ( 1.0f - fVolDim ) * ( 1.0f - fVolDist );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::MarkToDim( const NTimer::STime time )
{
	timeBeginDim = time;
	bDimMark = true;
	wID = 0; // ���� ������ ����������� ������
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string& CSoundScene::CSound::GetName() const
{
	return szName;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::UnSubstitute()
{
	pSubstitute =0;
	timeToPlay = pSample== 0 ? 0 : 1000 * pSample->GetLenght() / pSample->GetSampleRate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::Substitute( CSoundScene::CSubstSound *_pSubstitute, NTimer::STime nStartTime )
{
	pSubstitute = _pSubstitute;
	ISound * pSound = pSubstitute->GetSound();
	int nSampleRate = pSound->GetSampleRate();
	timeBegin = nStartTime;
	timeToPlay = pSound == 0 ? 0 : 1000 * pSound->GetLenght() / nSampleRate;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoundScene::CSound::GetRadiusMax() const
{
	return nMaxRadius;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::SetPos( const CVec3 &_vPos )
{
	vPos = _vPos;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::SetBeginTime( const NTimer::STime time )
{
	timeBegin = time;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSound::MarkStarted()
{
	bStartedMark = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSubstSound * CSoundScene::CSound::GetSubst()
{
	return pSubstitute;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ISound * CSoundScene::CSound::GetSound()
{
	if ( pSubstitute )
		return pSubstitute->GetSound();
	return pSample;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CSoundScene::CSoundCell::*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSoundCell::CSoundCell()
: nRadius( 0 ), timeLastCombatHear( 0 )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::SetLastHearCombat( const NTimer::STime hearTime )
{
	timeLastCombatHear = hearTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::Clear()
{
	sounds.clear();
	nRadius = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundScene::CSoundCell::IsSoundHearable( const CSound *pSound, const int nRadius ) const
{
	return !pSound->IsMarkedFinished() &&
		pSound->GetRadiusMax() > nRadius &&
		( pSound->GetCombatType() == ESCT_MUTE_DURING_COMBAT ? !IsCombat() : true ) ;
		
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundScene::CSoundCell::IsCombat() const
{
	return CSoundScene::GetCurTime() < timeLastCombatHear + SSoundSceneConsts::COMBAT_FEAR_TIME;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::RecountForDelete()
{
	nRadius = 0;
	for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); ++it )
	{
		NI_ASSERT_T( 0 != (*it)->GetRadiusMax(), " 0 max radius" );
		nRadius = Max( nRadius, (*it)->GetRadiusMax() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::AddSound( class CSoundScene::CSound *pSound )
{
	NI_ASSERT_T( 0 != pSound->GetRadiusMax(), " 0 max radius" );
	sounds.push_back( pSound );
	nRadius = Max( nRadius, pSound->GetRadiusMax() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::RemoveSound( const WORD wID, ISFX * pSFX )
{
	for ( CSounds::iterator it = sounds.begin(); it != sounds.end();  )
	{
		if ( (*it)->GetID() == wID )
		{
			CSound * s = *it;
			if ( pSFX && !s->IsSubstituted() )
				pSFX->StopSample( s->GetSound() );
			it = sounds.erase( it );
			RecountForDelete();
			return;
		}
		++it;
	}
	NI_ASSERT_T( false, "not present" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSound * CSoundScene::CSoundCell::GetSound( const WORD wID )
{
	for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); ++it )
	{
		if ( (*it)->GetID() == wID )
			return (*it);
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundCell::Update( ISFX * pSFX )
{
	bool bSomeSoundErased = false;
	for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); )
	{
		CPtr<CSound> sound = (*it);

		if ( 0.0f == sound->GetVolume( CSoundScene::GetCurTime(), 0.0f ) )
		{
			if ( 0 == sound->GetID() )
			{
				pSFX->StopSample( sound->GetSound() );
				it = sounds.erase( it );
				bSomeSoundErased = true;
				continue;
			}
		}

		if ( !sound->IsMarkedFinished() && !sound->IsLooped() )
		{
			if( sound->IsTimeToFinish() )
			{
				if ( 0 == sound->GetID() ) // ���� ������ ��������� ������
				{
					it = sounds.erase( it );
					bSomeSoundErased = true;
					continue;
				}
				sound->MarkFinished();
			}
		}
		++it;
	}

	if ( bSomeSoundErased )
		RecountForDelete();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CSoundScene::CSoundsCollector::
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundsCollector::operator()( int nRadius, const SIntPair & vCell )
{
	NI_ASSERT_SLOW_T( cellsWSound.find(vCell) != cellsWSound.end(), NStr::Format("Can't find cell at {%d : %d}", vCell.x, vCell.y) );
	cellsWSound[vCell]->EnumHearableSounds( nRadius, *this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CSoundsCollector::operator()( CSoundScene::CSound * sound, bool bHearable )
{
	if ( bHearable )
	{
		CSoundSubstTable::iterator it = substTable.find( sound->GetName() );
		if ( substTable.end() == it )
		{
			// ��� ���� ����� ����� �������
			sounds[sound->GetName()].push_back( sound );
		}
		else
			sounds[(*it).second].push_back( sound );
	}
	else
	{
		muteSounds.push_back( sound );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CSoundScene
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::InitMap( const struct CMapSoundInfo *pSound, int nElements )
{
	for ( int nSoundIndex = 0; nSoundIndex < nElements; ++nSoundIndex )
	{
		AddSoundToMap( pSound[nSoundIndex].szName.c_str(), pSound[nSoundIndex].vPos );
		NStr::DebugTrace( "added sound to map \"%s\", to (%.2f, %.2f, %.2f)\n", pSound[nSoundIndex].szName.c_str(), pSound[nSoundIndex].vPos.x, pSound[nSoundIndex].vPos.y, pSound[nSoundIndex].vPos.z );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::InitTerrain( interface ITerrain *pTerrain )
{
	terrainSounds.Init( pTerrain );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::InitMusic(	const std::string &szParty )
{
	streamingSounds.Init( szParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::InitConsts()
{
	pGameTimer = GetSingleton<IGameTimer>();
	pSFX = GetSingleton<ISFX>();
	pSoundManager = GetSingleton<ISoundManager>();
	pObjectsDB = GetSingleton<IObjectsDB>();

	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	SSoundSceneConsts::MAP_SOUND_CELL= constsTbl.GetInt( "Scene", "Sound.SpeedTuning.MapSoundCellSize", 500 );
	SSoundSceneConsts::SS_SOUND_CELL_SIZE = constsTbl.GetInt( "Scene", "Sound.SpeedTuning.SoundCellSize", 200 );
	
	SSoundSceneConsts::SS_MIX_DELTA = constsTbl.GetInt( "Scene", "Sound.MixDelta", 1000);
	SSoundSceneConsts::SS_UPDATE_PERIOD = constsTbl.GetInt( "Scene", "Sound.UpdatePeriod", 1000);
	
	SSoundSceneConsts::SS_SOUND_DIM_TIME = constsTbl.GetInt( "Scene", "Sound.DimTime", 1000);
	
	SSoundSceneConsts::SS_COMBAT_MUSIC_PLAY_WO_NOTIFY = constsTbl.GetInt( "Scene", "Sound.StreamingSounds.CombatMusicPlayWONotify", 3000);
	SSoundSceneConsts::SS_COMBAT_MUSIC_FADE = constsTbl.GetInt( "Scene", "Sound.StreamingSounds.CombatMusicFade", 3000);
	SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE = constsTbl.GetInt( "Scene", "Sound.StreamingSounds.SilentPause", 3000);
	SSoundSceneConsts::SS_STREAMING_SILENT_PAUSE_RND = constsTbl.GetInt( "Scene", "Sound.StreamingSounds.SilentPauseRandom", 1000);
	SSoundSceneConsts::SS_IDLE_PAUSE = constsTbl.GetInt( "Scene", "Sound.StreamingSounds.PauseBetween2Idles", 3000);
	SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE_RANDOM = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.Pause", 1000 );
	SSoundSceneConsts::SS_AMBIENT_SOUND_CHANGE = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.PauseRandom", 5000);
	SSoundSceneConsts::SS_AMBIENT_TERRAIN_SOUNDS = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.NumSounds", 2 );
	SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MIN	= constsTbl.GetFloat( "Scene", "Sound.TerrainSounds.MinRadius", 0.5f );
	SSoundSceneConsts::TERRAIN_SOUND_RADIUS_MAX = constsTbl.GetFloat( "Scene", "Sound.TerrainSounds.MaxRadius", 0.8f );
	
	SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.CriticalWeight", 1000 );
	SSoundSceneConsts::MAP_SOUNDS_UPDATE = constsTbl.GetInt( "Scene", "Sound.MapSounds.UpdateTime", 1500 );
	
	SSoundSceneConsts::DEFAULT_SCREEN_WIDTH = constsTbl.GetInt( "Scene", "Sound.ScreenWidth", 1024 );
	SSoundSceneConsts::DEFAULT_SCREEN_HEIGHT = constsTbl.GetInt( "Scene", "Sound.ScreenHeight", 768 );
	
	SSoundSceneConsts::COMBAT_MUSIC_VOLUME = constsTbl.GetFloat( "Scene", "Sound.StreamingSounds.CombatMusicVolume", 1.0f );
	SSoundSceneConsts::IDLE_MUSIC_VOLUME = constsTbl.GetFloat( "Scene", "Sound.StreamingSounds.IdleMusicVolume", 1.0f );
	SSoundSceneConsts::COMBAT_SOUNDR_FEAR_RADIUS = constsTbl.GetFloat( "Scene", "Sound.CombatSounds.FearRadius", 15 );
	SSoundSceneConsts::COMBAT_FEAR_TIME = constsTbl.GetInt( "Scene", "Sound.CombatSounds.FearTime", 3000 );
	SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MIN_RADIUS = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.NonCycledMinRadius", 15 );
	SSoundSceneConsts::TERRAIN_NONCYCLED_SOUNDS_MAX_RADIUS = constsTbl.GetInt( "Scene", "Sound.TerrainSounds.NonCycledMaxRadius", 30 );
	SSoundSceneConsts::SS_MAP_SOUND_PERIOND = constsTbl.GetInt( "Scene", "Sound.MapSounds.Period", 3000 );
	SSoundSceneConsts::SS_MAP_SOUND_PERIOND_RANDOM = constsTbl.GetInt( "Scene", "Sound.MapSounds.PeriodRandom", 3000 );
	
	SSoundSceneConsts::MIN_SOUND_COUNT_TO_PLAY_LOOPED = constsTbl.GetInt( "Scene", "Sound.MapSounds.MinCountToPlayLooped", 1 );
	
	InitScreenResolutionConsts();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::InitScreenResolutionConsts()
{
	const int nScreenWidth = GetGlobalVar( "GFX.Mode.Mission.SizeX", 1024 );
	const int nScreenHeight = GetGlobalVar( "GFX.Mode.Mission.SizeY", 768 );
	NI_ASSERT_T( nScreenWidth && nScreenHeight, NStr::Format( "wrong screen sizes %d x %d \n", nScreenWidth, nScreenHeight) );

	vScreenResize.x = 1.0f * nScreenWidth / SSoundSceneConsts::DEFAULT_SCREEN_WIDTH;
	vScreenResize.y = 1.0f * nScreenHeight / SSoundSceneConsts::DEFAULT_SCREEN_HEIGHT;

	SSoundSceneConsts::COMBAT_SOUNDR_FEAR_RADIUS *= fWorldCellSize * sqrt( vScreenResize.x *  vScreenResize.y) / SSoundSceneConsts::SS_SOUND_CELL_SIZE ; 
	SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT = vScreenResize.x * vScreenResize.y * SSoundSceneConsts::TERRAIN_CRITICAL_WEIGHT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::Init( const int nMaxX, const int nMaxY )
{
	InitConsts();
	//
	vLimit.x = nMaxX;
	vLimit.y = nMaxY;
	soundCellsInBounds.SetSizes( vLimit.x, vLimit.y );
	for ( int x = 0; x < vLimit.x; ++x )
		for ( int y = 0; y < vLimit.y; ++y )
			soundCellsInBounds( x,y ) = new CSoundCell;

	curTime = 0;
	vFormerCameraCell.x = -1;
	vFormerCameraCell.y = -1;
	timeLastUpdate = pGameTimer->GetAbsTime();

 	cellsPHS.Init( nMaxX, nMaxY );
	mapSounds.InitSizes( nMaxX * fWorldCellSize, nMaxY * fWorldCellSize );
	
	//CRAP{ SOUNDS TESTING FOR LARGE RADIUS
	/*{
		IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
		const int nMaxDescs = pIDB->GetNumDescs();
		const SGDBObjectDesc *pDescs = pIDB->GetAllDescs();

		for ( int i = 0; i < nMaxDescs; ++i )
		{
			if ( SGVOGT_SOUND == pDescs[i].eGameType )
			{
				const SSoundRPGStats *pStats = checked_cast<const SSoundRPGStats*>( pIDB->GetRPGStats( pDescs+i ) );
				for ( int nSound = 0; nSound < pStats->sounds.size(); ++nSound )
				{
					if ( pStats->sounds[nSound].fMaxDist > 40 )
					{
						NStr::DebugTrace( "\"%s\" max radius =%f\n", pStats->szParentName.c_str(), pStats->sounds[nSound].fMaxDist );
					}
				}
			}
		}
	}*/
	//CRAP}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSoundScene()
: pSFX( 0 ), pSoundManager( 0 ), pGameTimer( 0 )
{
	mapSounds.SetSoundScene( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::Clear()
{
	freeIDs.Clear();
	substTable.clear();
	//interfaceSounds.clear();
	streamingSounds.Clear();
	soundCellsInBounds.Clear();
	soundCellsWithSound.clear();
	soundCellsOutOfBounds.clear();
	terrainSounds.Clear();
	mapSounds.Clear();
	vLimit.x = 0;
	vLimit.y = 0;
	soundIDs.clear();			// � ����� ������ ��������� ����.
	finishedInterfaceSounds.clear();
	deletedInterfaceSounds.clear();
	cellsPHS.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::To2DSoundPos( const CVec3 &vPos, CVec3 *v2DPos )
{
	v2DPos->x = vPos.x - vPos.z * FP_SQRT_2;
	v2DPos->y = vPos.y + vPos.z * FP_SQRT_2;
	v2DPos->z = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MuteTerrain( const bool bMute )
{
	terrainSounds.Mute( bMute );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CSoundScene::AddSound( const char *pszName, const CVec3 &vPos,
												    const enum ESoundMixType eMixMode, const enum ESoundAddMode eAddMode,
														const enum ESoundCombatType _eCombatType,
												    const int nMinRadius, const int nMaxRadius,
														const unsigned int nTimeAfterStart )
{
	if ( !pSFX->IsSFXEnabled() || 0 == pszName || pszName[0] == 0 ) 
		return 0;

	CGDBPtr<SGDBObjectDesc> pDesc = pObjectsDB->GetDesc( pszName );

	const char *pszSoundPath = 0;
	float fMinDist = nMinRadius;
	float fMaxDist = nMaxRadius;
	ESoundCombatType eCombatType = _eCombatType;
	if ( pDesc ) 
	{
		NI_ASSERT_T( 0 != dynamic_cast<const SSoundRPGStats*>( pObjectsDB->GetRPGStats( pDesc ) ), "not a sound" );
		CGDBPtr<SSoundRPGStats> pStats = static_cast<const SSoundRPGStats*>( pObjectsDB->GetRPGStats( pDesc ) );
		if ( !pStats )
			return 0;
		if ( pStats )
		{
			const SSoundRPGStats::SSound & rRandomSound = pStats->sounds[ pStats->GetRandomSoundIndex() ];
			pszSoundPath = rRandomSound.szPathName.c_str();
			fMinDist = rRandomSound.fMinDist;
			fMaxDist = rRandomSound.fMaxDist;
			if ( ESCT_ASK_RPG == eCombatType )
				eCombatType = rRandomSound.bPeacefull ? ESCT_MUTE_DURING_COMBAT : ESCT_GENERIC;
		}
	}

	if ( !pszSoundPath )
		pszSoundPath = pszName;

	if ( pszSoundPath[0] == 0 ) // empty sound rolled, don't add it to SoundScene
		return 0;

	
	const bool bLooped = eAddMode == SAM_LOOPED_NEED_ID;
	const bool bNeedID = eAddMode == SAM_NEED_ID || eAddMode == SAM_LOOPED_NEED_ID;
	
	CPtr<ISound> pSound = pSoundManager->GetSound2D( pszSoundPath );
	if ( pSound )
	{
		const WORD wID = bNeedID ? freeIDs.GetFreeId() : 0;
		const bool bToCell = eMixMode != SFX_INTERFACE;

	// �������� �� ��������� � ������ ��� ������� ������	
		CVec3 vRealSoundPos = VNULL3;
		if ( bToCell )
			To2DSoundPos( vPos, &vRealSoundPos );
		CPtr<CSound> pSnd = new CSound( wID, pszSoundPath, pSound, eMixMode, vRealSoundPos, bLooped, 
																		eCombatType,
																	  fMinDist * fWorldCellSize * vScreenResize.x,
																	  fMaxDist* fWorldCellSize * vScreenResize.y );
		pSnd->SetBeginTime( GetCurTime() - nTimeAfterStart );
		if ( bToCell )
		{
			const SIntPair vCell( vRealSoundPos.x / SSoundSceneConsts::SS_SOUND_CELL_SIZE,
														vRealSoundPos.y / SSoundSceneConsts::SS_SOUND_CELL_SIZE );
			AddSound( vCell, pSnd );	
		}
		else
		{
			interfaceSounds[pSnd->GetName()].push_back( pSnd );
		}

		return wID;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime & CSoundScene::GetCurTime()
{
	return curTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::SetMode( const enum ESoundSceneMode _eSoundSceneMode )
{
	eSoundSceneMode = _eSoundSceneMode;
	switch( eSoundSceneMode )
	{
	case ESSM_INGAME:

		break;
	case ESSM_INTERMISSION_INTERFACE:
		// clear all game sounds
		mapSounds.Clear();
		freeIDs.Clear();
		substTable.clear();
		streamingSounds.Clear();
		soundCellsInBounds.Clear();
		soundCellsWithSound.clear();
		soundCellsOutOfBounds.clear();
		terrainSounds.Clear();
		mapSounds.Clear();
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundScene::IsInBounds( int x, int y )
{ 
	return x >= 0 && x < vLimit.x && y >=0 && y < vLimit.y ; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helper function
CSoundScene::CSoundCell * CSoundScene::GetSoundCell( const SIntPair &vCell )
{
	if ( IsInBounds( vCell.x, vCell.y ) )
	{
		if ( !soundCellsInBounds( vCell.x, vCell.y ) )
			soundCellsInBounds( vCell.x, vCell.y ) = new CSoundCell;
		return
			soundCellsInBounds( vCell.x, vCell.y );
	}
	else
	{
		if ( !soundCellsOutOfBounds[vCell] )
			soundCellsOutOfBounds[vCell] = new CSoundCell;
		return
			soundCellsOutOfBounds[vCell];
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::AddSound( const SIntPair &vCell, CSoundScene::CSound *pSound )
{
	CSoundCell *pCell = GetSoundCell( vCell );
	NI_ASSERT_SLOW_T( pCell != 0, NStr::Format("Can't get cell at {%d : %d}", vCell.x, vCell.y) );
	soundCellsWithSound[vCell] = pCell;
	const float fFormerRadius = pCell->GetRadius();
	pCell->AddSound( pSound );
	const float fNewRadius = pCell->GetRadius();
	UpdatePHSMap( vCell, fFormerRadius, fNewRadius );
	UpdateCombatMap( vCell, pSound );
	if ( 0 != pSound->GetID() )
		soundIDs[pSound->GetID()] = vCell;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::UpdateCombatMap( const SIntPair &vCell, CSoundScene::CSound *pSound )
{
	if ( pSound->GetCombatType() == ESCT_COMBAT )
	{
		const int nRadius = SSoundSceneConsts::COMBAT_SOUNDR_FEAR_RADIUS;

		const SIntPair vMinPos( vCell.x - nRadius, vCell.y - nRadius );
		const SIntPair vMaxPos( vCell.x + nRadius, vCell.y + nRadius );

		SIntPair vCur( vMinPos );
		for ( vCur.x = vMinPos.x; vCur.x < vMaxPos.x; ++vCur.x  )
			for ( vCur.y = vMinPos.y; vCur.y < vMaxPos.y; ++vCur.y )
				if ( CSoundScene::IsInBounds( vCur.x, vCur.y ) )
					soundCellsInBounds( vCur.x, vCur.y )->SetLastHearCombat( CSoundScene::GetCurTime() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::UpdatePHSMap( const SIntPair &vCell, const int nFormerRadius, const int nNewRadius )
{
	if ( nNewRadius != nFormerRadius )
	{
		//CRAP{ TO DO
		// for decrease and increase radius - special case
		//CRAP}
		cellsPHS.RemoveHearCell( vCell, nFormerRadius );
		cellsPHS.AddHearCell( vCell, nNewRadius );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::RemoveSound( const WORD wID )
{
	if ( !pSFX->IsSFXEnabled() || 0 == wID ) 
		return;

	if ( soundIDs.find( wID ) == soundIDs.end() )
	{
		if ( finishedInterfaceSounds.find( wID ) == finishedInterfaceSounds.end() )
			deletedInterfaceSounds.insert( wID );
		else
			finishedInterfaceSounds.erase( wID );
		return;
	}
	if ( soundCellsWithSound.find( soundIDs[wID] ) == soundCellsWithSound.end() ) 
		return ;

	const SIntPair vCell = soundIDs[wID];
	CSoundCell *pCell = GetSoundCell( vCell );

	CSound * pSound = pCell->GetSound( wID );
	if ( pSound )
	{
		const WORD wID = pSound->GetID();

		if ( pSound->IsLooped() )
			pSound->MarkToDim( GetCurTime() );
		else
		{
			const int nFormerRadius = pCell->GetRadius();
			pCell->RemoveSound( wID, pSFX );
			const int nNewRadius = pCell->GetRadius();
			UpdatePHSMap( vCell, nFormerRadius, nNewRadius );
		}
		
		if ( wID )
		{
			soundIDs.erase( wID );
			freeIDs.AddToFreeId( wID );
		}
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::SetSoundPos( const WORD wID, const CVec3 &vPos )
{
	if (	!pSFX->IsSFXEnabled() ||
			0 == wID ||
			soundIDs.find( wID ) == soundIDs.end() ||
			soundCellsWithSound.find( soundIDs[wID] ) == soundCellsWithSound.end() )
		return;

	const SIntPair &vFormerCell= soundIDs[wID];
	CSoundCell *pFormerCell = GetSoundCell( vFormerCell );
	CSound *pSound = pFormerCell->GetSound( wID );

	if ( !pSound ) 
		return;
	NI_ASSERT_T( pSound != 0, "sound doesn't exist" );

	CVec3 vRealSoundPos;
	To2DSoundPos( vPos, &vRealSoundPos );
	pSound->SetPos( vRealSoundPos );
	const SIntPair vNewCell(  vRealSoundPos.x / SSoundSceneConsts::SS_SOUND_CELL_SIZE,
														vRealSoundPos.y / SSoundSceneConsts::SS_SOUND_CELL_SIZE );
	// ���� ������������ � ������ ������
	if ( vFormerCell != vNewCell )
	{
		CSoundCell *pNewCell = GetSoundCell( vNewCell );
		
		// add sound to new cell
		NI_ASSERT_T( pNewCell != 0, NStr::Format( "cannot create cell at (%d, %d )", vNewCell.x, vNewCell.y ) );
		const int nFormerRadiusNewCell = pNewCell->GetRadius();
		pNewCell->AddSound( pSound );
		const int nNewRadiusNewCell = pNewCell->GetRadius();


		// remove sound form old cell
		const int nFormerRadiusOldCell = pFormerCell->GetRadius();
		pFormerCell->RemoveSound( wID );
		const int nNewRadiusOldCell = pFormerCell->GetRadius();
		
		// update PHS map
		UpdatePHSMap( vNewCell, nFormerRadiusNewCell, nNewRadiusNewCell );
		UpdatePHSMap( vFormerCell, nFormerRadiusOldCell, nNewRadiusOldCell );

		soundCellsWithSound[vNewCell] = pNewCell;
		soundIDs[wID] = vNewCell;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IObjectsDB * CSoundScene::GetObjectDB()
{
	return pObjectsDB;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundScene::IsFinished( const WORD wID )
{
	if (	!pSFX->IsSFXEnabled()||
			0 == wID ) return true;

	if ( soundIDs.find( wID ) == soundIDs.end() ) // sound is from interface.
		return finishedInterfaceSounds.find( wID ) != finishedInterfaceSounds.end();

	CSoundCellsWithSound::iterator pos = soundCellsWithSound.find( soundIDs[wID] );
	if ( pos == soundCellsWithSound.end() ) 
		return true;

	CSound *pSound = pos->second->GetSound( wID );
	return !pSound || pSound->IsTimeToFinish();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CombatNotify()
{
	if ( pSFX->IsStreamingEnabled() )
		streamingSounds.CombatNotify();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::Update( interface ICamera *pCamera )
{
	pSFX->Update( pCamera ); //�������� ������ ������ ������ �� ������!

	MixInterfaceSounds(); // ����� �� ���������� ������ ������ ��� ������
	
	if ( ESSM_INGAME == eSoundSceneMode ) 
	{
		if ( pSFX->IsStreamingEnabled() )
			streamingSounds.Update();
		
		if ( pSFX->IsSFXEnabled() && !pSFX->IsPaused() ) 
		{

			const CVec3 vCameraPos( pCamera->GetAnchor() );
			const SIntPair vCameraCell( vCameraPos.x / SSoundSceneConsts::SS_SOUND_CELL_SIZE,
														vCameraPos.y / SSoundSceneConsts::SS_SOUND_CELL_SIZE );
			
 			NI_ASSERT_T( IsInBounds( vCameraCell.x, vCameraCell.y ), "camera is out of scene bounds" );

			terrainSounds.Update( pCamera, soundCellsInBounds(vCameraCell.x,vCameraCell.y)->IsCombat() );
			mapSounds.Update( pCamera );
			
			CHearableSounds sounds;
			CSoundsList			muteSounds;
			CSoundsCollector collector( substTable, sounds, soundCellsWithSound, muteSounds );
			
			if ( CSoundScene::GetCurTime() > timeLastUpdate + SSoundSceneConsts::SS_UPDATE_PERIOD ||
					((vCameraCell.x == -1 || vCameraCell.y != -1) && vFormerCameraCell != vCameraCell) ) 
			{// ������ ���������� - ������ ��������
				// ------------- ������� ��� ���������� �����
				for ( CSoundCellsWithSound::iterator it = soundCellsWithSound.begin(); it != soundCellsWithSound.end(); ++it )
				{
					NI_ASSERT_SLOW_T( it->second.IsValid(), NStr::Format("Invalid cell at ( delete finished sounds ){%d : %d}", it->first.x, it->first.y) );
					CSoundCell * pCell = (*it).second;
					const int nFormerRadius = pCell->GetRadius();
					pCell->Update( pSFX );
					const int nNewRadius = pCell->GetRadius();
					if ( nFormerRadius != nNewRadius ) // collect updated
						updatedCells.push_back( SUpdatedCell( (*it).first, nFormerRadius, nNewRadius ) );
				}

				for ( CUpdatedCells::iterator it = updatedCells.begin(); it != updatedCells.end();  )
				{
					UpdatePHSMap( (*it).vCell, (*it).nFormerRadius, (*it).nNewRadius );
					it = updatedCells.erase( it );
				}
				
				// -- ����� ��� �����, ������� ������ � ����������� ������
				for ( CSoundCellsWithSound::iterator it = soundCellsWithSound.begin(); it != soundCellsWithSound.end(); ++it )
				{
					NI_ASSERT_SLOW_T( it->second.IsValid(), NStr::Format("Invalid cell at ( enum all sounds ){%d : %d}", it->first.x, it->first.y) );
					const int nRadius = abs( vCameraCell.x - (*it).first.x ) + abs( vCameraCell.y - (*it).first.y );
					(*it).second->EnumAllSounds( collector, nRadius );
				}
				timeLastUpdate = CSoundScene::GetCurTime();
				vFormerCameraCell = vCameraCell;
			}
			else
				cellsPHS.EnumHearableCells( collector, vCameraCell );
	
			MuteSounds( &muteSounds );
			MixMixedAlways( sounds, vCameraPos );
			MixMixedWithDelta( sounds, vCameraPos );
			MixSingle( sounds, vCameraPos );
		}
	}
	curTime = pGameTimer->GetAbsTime() + 1000000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MixInterfaceSounds()
{
	for ( CHearableSounds::iterator it = interfaceSounds.begin(); it != interfaceSounds.end(); ++it )
	{
		//clear finished
		for ( CSoundsList::iterator soundIter = (*it).second.begin(); soundIter != (*it).second.end(); )
		{
			CSound * pSound = (*soundIter);
			if ( pSound->IsTimeToFinish() || 
					(pSound->IsMarkedStarted() && !pSFX->IsPlaying(pSound->GetSound())) )
			{
				pSound->UnSubstitute();
				const WORD wID = pSound->GetID();

				if ( wID && deletedInterfaceSounds.end() == deletedInterfaceSounds.find( wID ) )
				{
					finishedInterfaceSounds.insert( pSound->GetID() );
				}
				soundIter = (*it).second.erase( soundIter );
			}
			else
				++soundIter;
		}

		//mix not started

		//������������� �� �� ������� ������.
		CSoundStartTimePredicate pr;
		(*it).second.sort( pr );

		// ����� ������ ������, � ������� ������� �� ������� ������ Delta 
		// � �������� �� Mix()
		CSoundsList::iterator beginIterator = (*it).second.begin();
		CSoundsList::iterator endIterator = (*it).second.begin();
		while( (*it).second.end() != beginIterator )
		{
			CSound *s = (*beginIterator);
			const std::string szSubstName = s->GetName();
			CSoundsWithinDeltaPredicate pr( s->GetBeginTime(), SSoundSceneConsts::SS_MIX_DELTA );
			endIterator = std::find_if( beginIterator, (*it).second.end(), pr );
			Mix( (*it).second, beginIterator, endIterator, szSubstName, VNULL3, SFX_INTERFACE, 1, false );
			beginIterator = endIterator;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MixSingle( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos )
{
	//���������� ��������� �� ��������� ��� ����������.
	for ( CHearableSounds::iterator substIter = sounds.begin(); substIter != sounds.end(); ++substIter )
	{
		const std::string &substName = (*substIter).first;
		CSoundsList & curSounds = (*substIter).second;

		CSoundsList::iterator beginIterator = curSounds.begin();
		CSoundsList::iterator endIterator = curSounds.begin();
		while( curSounds.end() != beginIterator )
		{
			CSoundsList::iterator temp = beginIterator;
			endIterator = ++temp;
			Mix( curSounds, beginIterator, endIterator, substName, vCameraPos, SFX_MIX_ALL, 0 );
			beginIterator = endIterator;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MixMixedWithDelta( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos )
{
	// 2) ������� �����, ������� ����� ���������� ���� ������ ��������� �� �������.
				//������� ������.
	for ( CHearableSounds::iterator substIter = sounds.begin(); substIter != sounds.end(); ++substIter )
	{
		const std::string &substName = (*substIter).first;

		//������������� �� �� ������� ������.
		CSoundsList & curSounds = (*substIter).second;
		CSoundStartTimePredicate pr;
		curSounds.sort( pr );

		// ����� ������ ������, � ������� ������� �� ������� ������ Delta 
		// � �������� �� Mix()
		CSoundsList::iterator beginIterator = curSounds.begin();
		CSoundsList::iterator endIterator = curSounds.begin();
		while( curSounds.end() != beginIterator )
		{
			CSound * s = (*beginIterator);
			CSoundsWithinDeltaPredicate pr( s->GetBeginTime(), SSoundSceneConsts::SS_MIX_DELTA );
			endIterator = std::find_if( beginIterator, curSounds.end(), pr );
			Mix( curSounds, beginIterator, endIterator, substName, vCameraPos, SFX_MIX_IF_TIME_EQUALS, 2 );
			beginIterator = endIterator;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MixMixedAlways( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos )
{
	// 1) ������� �����, ������� ������ ��� ���� ������, ���������� �� ����������.
				//��������� �� ��������.
	for ( CHearableSounds::iterator substIter = sounds.begin(); substIter != sounds.end(); ++substIter )
	{
		CSoundsList & curSounds = (*substIter).second;
		const std::string &substName = (*substIter).first;
		Mix( curSounds, curSounds.begin(), curSounds.end(), substName, vCameraPos, SFX_MIX_ALWAYS, 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::Mix(	CSoundsList & curSounds,
												const CSoundsList::iterator begin_iter,
												const CSoundsList::iterator end_iter,
												const std::string &szSubstName,
												const CVec3 &vCameraPos,
												const ESoundMixType eMixType,
												const int nMixMinimum,
												bool bDelete ) 
{
	CVec3 vSoundCoord( VNULL3 );					//���������� ����� ������������ ������
	
	float fPan = 0;
	float fVolume = 0;
	float fMaxVolume = 0;
	float fMaxHearRadius = 0;

	CPtr<CSubstSound> pSubstSruct;
	CSound *pSound = 0;										//������ �� �������� ����
	int nSounds = 0;
	NTimer::STime nStartTime = 0;
	bool bLooped = false;


	for ( CSoundsList::iterator soundsIter = begin_iter; soundsIter != end_iter; ++soundsIter )
	{
		CSound &sound = *(*soundsIter);
		if ( SFX_MIX_ALL == eMixType || sound.GetMixType() == eMixType )
		{
			const CVec3 v ( sound.GetPos() - vCameraPos );
			const float fVol = sound.GetVolume( CSoundScene::GetCurTime(), fabs( v ) );
			fMaxVolume += fVol;
			vSoundCoord += v * fVol;
			
			++nSounds;
			bLooped |= sound.IsLooped(); // ������ ��������� ���� ���� ���� ������ ��������
			if ( !pSound ) pSound = &sound;

			if ( !pSubstSruct && sound.IsSubstituted() && sound.IsMarkedStarted() ) 
			{	// ���� - ��� �������� � �� ��� ������
				pSubstSruct = sound.GetSubst();
				nStartTime = sound.GetBeginTime();
			}
		}
	}

	if ( nSounds != 0 && nSounds >= nMixMinimum )
	{
		vSoundCoord /= nSounds;
		
		if ( !pSubstSruct )
		{
			// ������ ������� ������ � ��������� �� ��������
			CPtr<ISound> pSubstituteSound = pSoundManager->GetSound2D( szSubstName.c_str() );
			if ( pSubstituteSound == 0 ) // ������ ���� ������ �� �����
				pSubstituteSound = pSound->GetSound();
			pSubstSruct = new CSubstSound( pSubstituteSound, pSFX );
		}

		// ������� ������ ������ � ������� �� �� ���������� ������ ������
		unsigned int nStartSample = 0; // ����� ��� ������������� �����
		NTimer::STime nStartTime = 0;
		// ���������� ����� ������ ( � ������� )
		for ( CSoundsList::iterator soundsIter = begin_iter; soundsIter != end_iter; ++soundsIter)
		{
			CSound & sound = *(*soundsIter);
			if ( SFX_MIX_ALL == eMixType || sound.GetMixType() == eMixType )
			{
				nStartSample = Max( nStartSample, sound.GetSamplesPassed() );
				const NTimer::STime tmp = sound.GetBeginTime();
				if ( tmp < nStartTime || nStartTime == 0 )
					nStartTime = tmp; 
			}
		}

		// ��� ��������
		for ( CSoundsList::iterator soundsIter = begin_iter; soundsIter != end_iter; )
		{
			CSound & sound = *(*soundsIter);
			if ( SFX_MIX_ALL == eMixType || sound.GetMixType() == eMixType )
			{
				if ( !sound.IsSubstituted() )
				{
					pSFX->StopSample( sound.GetSound() );
					sound.Substitute( pSubstSruct, nStartTime);
				}
				sound.MarkStarted();
				if ( bDelete )
					soundsIter = curSounds.erase( soundsIter );
				else
					++soundsIter;
			}
			else
				++soundsIter;
		}

		// ������������ ���� � ������������ � ���, ��� �������������
		fMaxHearRadius = 1.0f * pSound->GetRadiusMax() * SSoundSceneConsts::SS_SOUND_CELL_SIZE;
		CalcVolNPan( &fVolume, &fPan, vSoundCoord, fMaxHearRadius );

		fVolume *= fMaxVolume / nSounds;
		ISound * pSubstSound = pSubstSruct->GetSound();

		pSubstSound->SetPan( fPan );
		pSubstSound->SetVolume( fVolume );
		const int nSoundLenght = pSubstSound->GetLenght();

		if ( nSoundLenght )
		{	
			if ( bLooped )
			{
				nStartSample = nStartSample % nSoundLenght ;
				if ( nStartSample > nSoundLenght * 8 / 10 )
					nStartSample = 0;
				/*if ( nStartSample >= nSoundLenght )
					nStartSample = 0;*/
			}
			
			
			if ( !pSFX->IsPlaying( pSubstSound ) && ( nStartSample < nSoundLenght ) )
			{
				// ����������� �������� ����� 
				const int nChannel = pSFX->PlaySample( pSubstSound, bLooped, nStartSample );
			}
			else
				pSFX->UpdateSample( pSubstSound );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::MuteSounds( CSoundScene::CSoundsList	* muteSounds )
{
// ��������� ��� ��������� �����.
	// ---------------------------------------------
	for ( CSoundsList::iterator it = muteSounds->begin(); it != muteSounds->end(); ++it )
	{
		CSound *sound = (*it);
		if ( !sound->IsMarkedFinished() )
		{
			sound->UnSubstitute();
			pSFX->StopSample( sound->GetSound() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::CalcVolNPan( float *fVolume, float *fPan, const CVec3 &vSound, const float fMaxHear )
{
	*fVolume = 1.0f - fabs( vSound ) / fMaxHear;
	*fPan = ( vSound.x ) / fMaxHear;
	if ( *fVolume < 0.0f )
	{
		*fVolume = 0.0f;
		*fPan = 0.0f;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CSoundScene::AddSoundToMap( const char *pszName, const CVec3 &vPos )
{
	CVec3 v2DPos;
	To2DSoundPos( vPos, &v2DPos );
	return mapSounds.AddSound( CVec2(v2DPos.x,v2DPos.y), pszName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::RemoveSoundFromMap( const WORD	wInstanceID )
{
	mapSounds.RemoveSound( wInstanceID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
