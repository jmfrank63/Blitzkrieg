#include "StdAfx.h"

#include "SoundEngine.h"

#include "SampleSounds.h"
#include "..\Scene\Scene.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Misc\Win32Helper.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static NWin32Helper::CCriticalSection critSection;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayVisitor : public ISFXVisitor
{
	CSoundEngine *pSFX;
	//
	int RegisterSound( CBaseSound *pSound, const int nChannel )
	{
		if ( nChannel != -1 )
		{
			const int nVolume = pSound->GetVolume() >= 0 ? pSound->GetVolume() * pSFX->GetSFXMasterVolume() : pSFX->GetSFXMasterVolume();
			FSOUND_SetVolume( nChannel, nVolume );
			const int nPan = 128 + 127 * pSound->GetPan();
			FSOUND_SetPan( nChannel, nPan );
			pSFX->MapSound( pSound, nChannel );
		}
		else
		{
			NStr::DebugTrace( "Sound error %d\n", FSOUND_GetError() );
		}
		pSound->SetChannel( nChannel );
		return nChannel;
	}
public:
	//
	void Init( class CSoundEngine *_pSFX ) { pSFX = _pSFX; }
	//
	virtual int STDCALL VisitSound2D( CSound2D *pSound )
	{
		FSOUND_SAMPLE *sample = pSound->GetSample()->GetInternalContainer();
		if ( sample == 0 )
			return -1;
		const int nChannel = FSOUND_PlaySoundEx( FSOUND_FREE, sample, 0, true );
		return RegisterSound( pSound, nChannel );
	}
	virtual int STDCALL VisitSound3D( CSound3D *pSound, const CVec3 &vPos )
	{
		FSOUND_SAMPLE *sample = pSound->GetSample()->GetInternalContainer();
		if ( sample == 0 )
			return -1;
		const int nChannel = FSOUND_PlaySoundEx( FSOUND_FREE, sample, 0,	true );
		FSOUND_3D_SetAttributes( nChannel, const_cast<float*>(vPos.m), 0 );
		return RegisterSound( pSound, nChannel );
	}
};
static CPlayVisitor thePlayVisitor;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundEngine::CSoundEngine() 
: bInited( false ), pStreamingSound( 0 ), bPaused( false ), bStreamingPaused( false ),
	cSFXMasterVolume( 255 ), cStreamMasterVolume( 255 ), bEnableSFX( true ), bEnableStreaming( true ),
	timeLastUpdate( -1 ), timeStreamFinished( -1 ), fStreamCurrentVolume( 1.0f ),
	bStreamPlaying( false ), nStreamingChannel( -1 )
{  
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::SearchDevices()
{
	if ( FSOUND_GetVersion() < FMOD_VERSION )
	{
		OutputDebugString( "Error : You are using the wrong DLL version!\n" );
		return false;
	}
	FSOUND_SetOutput( FSOUND_OUTPUT_DSOUND );
	int nNumDrivers = FSOUND_GetNumDrivers();
	drivers.resize( nNumDrivers );
	for ( int i = 0; i < nNumDrivers; ++i )
	{
		SDriverInfo &dr = drivers[i];
		dr.szDriverName = (const char *) FSOUND_GetDriverName( i );
		unsigned int nCaps;
		FSOUND_GetDriverCaps( i, &nCaps );
		dr.isHardware3DAccelerated = nCaps & FSOUND_CAPS_HARDWARE;
		dr.supportEAXReverb = nCaps & FSOUND_CAPS_EAX2;//FSOUND_CAPS_EAX;
		dr.supportA3DOcclusions = false; // FSOUND_CAPS_GEOMETRY_OCCLUSIONS not in FMOD 3.75
		dr.supportA3DReflections = false; // FSOUND_CAPS_GEOMETRY_REFLECTIONS not in FMOD 3.75
		dr.supportReverb = nCaps & FSOUND_CAPS_EAX2;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::IsInitialized()
{
	return bInited;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CSoundEngine::QI( int nInterfaceTypeID )
{
	if ( nInterfaceTypeID == 0 ) 
		return reinterpret_cast<IRefCount*>( FSOUND_GetOutputHandle() );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::Init( HWND hWnd, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels )
{
	if ( !SearchDevices() )
		return false;

	
	NI_ASSERT_T( nDriver < drivers.size(), NStr::Format("Can't find driver %d (max found %d)", nDriver, drivers.size()) );
	FSOUND_SetDriver( nDriver );
	NI_ASSERT_T( !(output == SFX_OUTPUT_A3D && !drivers[nDriver].supportA3DOcclusions), "Can't set output as A3D with unsupported feature" );
	FSOUND_OUTPUTTYPES eOut;
	bSoundCardPresent = true;
	switch ( output )
	{
		case SFX_OUTPUT_NO: 
			bSoundCardPresent = false;
			eOut = FSOUND_OUTPUT_NOSOUND;
			OutputDebugString("FSOUND_OUTPUT_NOSOUND\n"); 
			break;
		case SFX_OUTPUT_WINMM: 
			eOut = FSOUND_OUTPUT_WINMM; 
			OutputDebugString("FSOUND_OUTPUT_WINMM\n"); 
			break;
		case SFX_OUTPUT_DSOUND: 
			eOut = FSOUND_OUTPUT_DSOUND; 
			OutputDebugString("FSOUND_OUTPUT_DSOUND\n"); 
			break;
		case SFX_OUTPUT_A3D: 
			if ( !drivers[nDriver].supportA3DOcclusions )
			{
				eOut = FSOUND_OUTPUT_DSOUND;
				OutputDebugString("FSOUND_OUTPUT_DSOUND(1)\n"); 
			}
			else
			{
				OutputDebugString("FSOUND_OUTPUT_A3D\n"); 
				eOut = FSOUND_OUTPUT_A3D; 
			}
			break;
		default: 
			NI_ASSERT_T( 0, NStr::Format("Unknown output %d", output) );
	}

	FSOUND_SetOutput( eOut );
	FSOUND_SetHWND( hWnd );
	
	if ( !FSOUND_Init( nMixRate, nMaxChannels, FSOUND_INIT_USEDEFAULTMIDISYNTH ) )
	{
		OutputDebugString( "NFMSound::Start():error!\n" );
		//NI_ASSERT_T( 0, NStr::Format("Failed to init FMOD: %d", FSOUND_GetError()) );
		//soft reaction on error: sound card not found.
		bSoundCardPresent = false;
		return true;
		//return false;
	}

#ifdef _DEBUG
	OutputDebugString( "Using \"" );
	OutputDebugString( drivers[nDriver].szDriverName.c_str() );
	OutputDebugString( "\" sound driver.\n" );
	if ( drivers[nDriver].isHardware3DAccelerated )
		OutputDebugString("- Driver supports hardware 3D sound!\n" );
	if ( drivers[nDriver].supportEAXReverb )
		OutputDebugString("- Driver supports EAX reverb!\n" );
	if ( drivers[nDriver].supportA3DOcclusions )
		OutputDebugString("- Driver supports hardware 3d geometry processing with occlusions!\n" );
	if ( drivers[nDriver].supportA3DReflections )
		OutputDebugString("- Driver supports hardware 3d geometry processing with reflections!\n" );
	if ( drivers[nDriver].supportReverb )
		OutputDebugString("- Driver supports EAX 2.0 reverb!\n" );
	
	OutputDebugString("Mixer = ");
	switch ( FSOUND_GetMixer() )
	{
		case FSOUND_MIXER_BLENDMODE:	
			OutputDebugString("FSOUND_MIXER_BLENDMODE\n"); 
			break;
		case FSOUND_MIXER_MMXP5:		
			OutputDebugString("FSOUND_MIXER_MMXP5\n"); 
			break;
		case FSOUND_MIXER_MMXP6:		
			OutputDebugString("FSOUND_MIXER_MMXP6\n"); 
			break;
		case FSOUND_MIXER_QUALITY_FPU:	
			OutputDebugString("FSOUND_MIXER_QUALITY_FPU\n"); 
			break;
		case FSOUND_MIXER_QUALITY_MMXP5:
			OutputDebugString("FSOUND_MIXER_QUALITY_MMXP5\n"); 
			break;
		case FSOUND_MIXER_QUALITY_MMXP6:
			OutputDebugString("FSOUND_MIXER_QUALITY_MMXP6\n"); 
			break;
	};
#endif

//	GetSingleton<IGameTimer>()->GetTime( &timeUpdate );
//	vLastListenerPos = CVec3(0,0,0);
	fListenerDistance = GetGlobalVar( "Sound.Listener.Distance", 0.0f ) * fWorldCellSize/2.0f;
	
	//cSFXMasterVolume = GetGlobalVar( "Options.Sound.SFXVolume", 100.0f ) / 100.0f * 255;
	//cStreamMasterVolume = GetGlobalVar( "Options.Sound.MusicVolume", 100.0f ) / 100.0f * 255;

	cSFXMasterVolume = GetGlobalVar( "Sound.SFXVolume", 100.0f ) / 100.0f * 255;
	cStreamMasterVolume = GetGlobalVar( "Sound.MusicVolume", 100.0f ) / 100.0f * 255;

	streamFadeOff.Init();
	//
	bInited = true;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::Done()
{
	nextMelody.Clear();
	curMelody.Clear();

	streamFadeOff.Clear();

	drivers.clear();
	CloseStreaming();
	channelsMap.clear();
	soundsMap.clear();
	FSOUND_Close();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::SetDistanceFactor( float fFactor )
{
	FSOUND_3D_SetDistanceFactor( fFactor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::SetRolloffFactor( float fFactor )
{
	NI_ASSERT_TF( (fFactor >= 0) && (fFactor <= 10), NStr::Format("Rolloff factor (%g) must be in range [0..10]", fFactor), return );
	FSOUND_3D_SetRolloffFactor( fFactor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::Update( interface ICamera *pCamera )
{
	// ÝÒÎ ÍÅ ÍÓÆÍÎ, È ÏÐÈÂÎÄÈÒ Ê ÃËÞÊÀÌ Â 2D ÇÂÓÊÀÕ
	/*
	// FMOD treats +X as right, +Y as up, and +Z as forwards
	CVec3 vPos = pCamera->GetAnchor();

	vPos.Set( vPos.x, fListenerDistance, vPos.y );
	CVec3 vFwd( -1, 0, 1 ), vTop( 0, 1, 0 );
	Normalize( &vFwd );

	FSOUND_3D_Listener_SetAttributes( vPos.m, 0, vFwd.x, vFwd.y, vFwd.z, vTop.x, vTop.y, vTop.z );
	FSOUND_3D_Update();
	*/

	//
	timeLastUpdate = GetSingleton<IGameTimer>()->GetAbsTime();
	//
	if ( (timeStreamFinished != -1) && (timeStreamFinished < timeLastUpdate) && (timeLastUpdate - timeStreamFinished > 15000) )
		PlayNextMelody();
	//
	
	const int nNumChannels = FSOUND_GetChannelsPlaying();
	
	//CRAP{ for testing
	{
		IScene * pScene = GetSingleton<IScene>();
		IStatSystem *pStat = pScene->GetStatSystem();
		pStat->UpdateEntry( "SFX: num channels:", NStr::Format("%d", nNumChannels )  );
	}
	//CRAP}


	if ( nNumChannels > 0 )
		ClearChannels();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::CloseStreaming()
{
	if ( pStreamingSound )
	{
		// reset callbacks
		FSOUND_Stream_SetEndCallback( pStreamingSound, 0, 0 );
		FSOUND_Stream_SetSyncCallback( pStreamingSound, 0, 0 );
		//
		FSOUND_StopSound( nStreamingChannel );
		FSOUND_Stream_Close( pStreamingSound );

		pStreamingSound = 0;
		bStreamPlaying = false;
		curMelody.Clear();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
signed char F_CALLBACKAPI NextMelodyCallback( FSOUND_STREAM *stream, void *buff, int len, void *userdata )
{
	CSoundEngine *pSFX = reinterpret_cast<CSoundEngine*>( userdata );
	pSFX->NotifyMelodyFinished();
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::PlayNextMelody()
{
	if ( !bEnableStreaming || nextMelody.szName.empty() )
		return false;
	PlayStream( nextMelody.szName.c_str(), nextMelody.bLooped, 0 );
	nextMelody.Clear();
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::StopStream( const unsigned int nTimeToFade )
{
	if ( nTimeToFade > 0 && bStreamPlaying )
		streamFadeOff.Fade( nTimeToFade );
	else
	{
		CloseStreaming();
		NotifyMelodyFinished();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::SetStreamVolume( const float fVolume )
{
	fStreamCurrentVolume = Clamp( fVolume, 0.0f, 1.0f );
	if ( nStreamingChannel != -1 )
	{
		FSOUND_SetVolume( nStreamingChannel, fStreamCurrentVolume *cStreamMasterVolume );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::MapSound( ISound *pSound, int nChannel )
{
	channelsMap.insert( std::pair<ISound*, int>( pSound, nChannel ) );
	soundsMap.insert( std::pair<int, CPtr<ISound> >( nChannel, pSound ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CSoundEngine::GetStreamVolume() const
{
	return fStreamCurrentVolume;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::SetStreamMasterVolume( float fVolume )
{
	Clamp( fVolume, 0.0f, 1.0f );
	cStreamMasterVolume = BYTE( fVolume * 255.0f );
	if ( bStreamPlaying && nStreamingChannel != -1 )
	{
		FSOUND_SetVolume( nStreamingChannel, fStreamCurrentVolume *cStreamMasterVolume );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::PlayStream( const char *pszFileName, bool bLooped, const unsigned int nTimeToFadePrevious )
{
	if ( !bEnableStreaming ) return;

	if ( bStreamPlaying && curMelody.IsValid() && curMelody.szName == pszFileName )
		return;

	if ( nTimeToFadePrevious && bStreamPlaying )
	{
		nextMelody.szName = pszFileName;
		nextMelody.bLooped = bLooped;
		if ( !streamFadeOff.IsFading() )
			StopStream( nTimeToFadePrevious );
	}
	else
	{
		SetStreamVolume( 1.0f );
		CloseStreaming();
		curMelody.szName = pszFileName;
		curMelody.bLooped = bLooped;
		std::string szFileName = std::string( GetSingleton<IDataStorage>()->GetName() ) + pszFileName + ".mp3";
		std::string szFileName1 = std::string( GetSingleton<IDataStorage>()->GetName() ) + pszFileName + ".ogg";
		
		pStreamingSound = FSOUND_Stream_Open( szFileName.c_str(), FSOUND_2D|(bLooped ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF), 0, 0 );
		if ( !pStreamingSound )
			pStreamingSound = FSOUND_Stream_Open( szFileName1.c_str(), FSOUND_2D|(bLooped ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF), 0, 0 );
		
		if ( pStreamingSound )
		{
			nStreamingChannel = FSOUND_Stream_Play( FSOUND_FREE, pStreamingSound );
			FSOUND_SetPan( nStreamingChannel, FSOUND_STEREOPAN );
			FSOUND_SetVolume( nStreamingChannel, cStreamMasterVolume );
			FSOUND_Stream_SetEndCallback( pStreamingSound, NextMelodyCallback, this );
			if ( bStreamingPaused ) 
				FSOUND_SetPaused( nStreamingChannel, bStreamingPaused );
			//
			NWin32Helper::CCriticalSectionLock lock( critSection );
			timeStreamFinished = -1;
			bStreamPlaying = true;
		}
		else
		{
			curMelody.Clear();
			nStreamingChannel = -1;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::IsPaused()
{
	return bPaused;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::PauseStreaming( bool bPause )
{
	if ( bStreamingPaused != bPause ) 
	{
		if ( nStreamingChannel != -1 ) 
			FSOUND_SetPaused( nStreamingChannel, bPause );
		bStreamingPaused = bPause;
	}
	return bPause;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::Pause( bool bPause )
{
	if ( bPaused != bPause ) 
	{
		for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
		{
			if ( nStreamingChannel != it->first )
			{
				FSOUND_SetPaused( it->first, bPause );
			}
		}
		bPaused = bPause;
	}
	return bPause;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::ClearChannels()
{
	if ( bPaused )
		return;
	//
	std::list<int> channels;
	// collect finished and invalid channels
	for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
	{
		if ( !it->second->IsValid() )
		{
			FSOUND_StopSound( it->first );
		}
		if ( FSOUND_IsPlaying( it->first ) == 0 )
		{
			channels.push_back( it->first );
			FSOUND_StopSound( it->first );
		}
	}
	// clear it
	for ( std::list<int>::iterator it = channels.begin(); it != channels.end(); ++it )
	{
		const int nChannel = *it;
		ISound *pSound = soundsMap[nChannel];
		soundsMap.erase( nChannel );
		channelsMap.erase( pSound );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoundEngine::PlaySample( ISound *pSound, bool bLooped, unsigned int nStartPos )
{
	if ( pSound == 0 || !bEnableSFX )
		return -1;
	//
	thePlayVisitor.Init( this );
	if ( static_cast<CBaseSound*>( pSound )->GetSample() == 0 )
		return -1;

	CSoundSample *pSample = static_cast<CBaseSound*>( pSound )->GetSample();
	pSample->SetLoop( bLooped );
	const int nChannel = pSound->Visit( &thePlayVisitor );
	if ( 0 != nStartPos )
		FSOUND_SetCurrentPosition( nChannel, nStartPos );
	FSOUND_SetPaused( nChannel, false );
	return nChannel;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::UpdateSample( ISound *pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{
		const int nChannel = pos->second;
		const int nPan = Clamp( int(128 + pSound->GetPan() * 127), 0, 255 );
		FSOUND_SetPan( nChannel, nPan );
		const int nVolume = Clamp( int(pSound->GetVolume() >= 0 ? pSound->GetVolume() * GetSFXMasterVolume() : GetSFXMasterVolume()), 0, 255 );
		FSOUND_SetVolume( nChannel, nVolume );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::StopSample( ISound *pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{		
		StopChannel( pos->second );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::IsPlaying( ISound *pSound )
{
	if ( !pSound )
		return false;
	//
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	return pos != channelsMap.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::StopChannel( int nChannel )
{
 	if ( nChannel == -1 )
		return;
	//
	FSOUND_StopSound( nChannel );
	CChannelSoundMap::iterator pos = soundsMap.find( nChannel );
	if ( pos != soundsMap.end() )
	{
		
		channelsMap.erase( pos->second );
		soundsMap.erase( pos );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CSoundEngine::GetCurrentPosition( ISound * pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{
		int nChannel = (*pos).second;
		return FSOUND_GetCurrentPosition( nChannel );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::SetCurrentPosition( ISound * pSound, unsigned int pos )
{
	CSoundChannelMap::iterator it = channelsMap.find( pSound );
	if ( it != channelsMap.end() )
	{
		int nChannel = (*it).second;
		FSOUND_SetCurrentPosition( nChannel, pos );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::ReEnableSounds()
{
	// turn all SFXes off
	if ( !bEnableSFX )
	{
		for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
		{
			if ( FSOUND_IsPlaying(it->first) )
				FSOUND_StopSound( it->first );
		}
		soundsMap.clear();
		channelsMap.clear();
	}
	// turn stream off
	if ( !bEnableStreaming )
		CloseStreaming();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEngine::NotifyMelodyFinished()
{
	NWin32Helper::CCriticalSectionLock lock( critSection );
	if ( nextMelody.IsValid() ) // íóæíî èãðàòü ñëåäóþùóþ
	{
		PlayNextMelody();
	}
	else if ( curMelody.IsValid() && curMelody.bLooped ) // òåêóùàÿ çàùèêëåíà
	{
		nStreamingChannel = FSOUND_Stream_Play( FSOUND_FREE, pStreamingSound );
		FSOUND_SetPan( nStreamingChannel, FSOUND_STEREOPAN );
		FSOUND_SetVolume( nStreamingChannel, cStreamMasterVolume );
		FSOUND_Stream_SetEndCallback( pStreamingSound, NextMelodyCallback, this );
		if ( bStreamingPaused ) 
			FSOUND_SetPaused( nStreamingChannel, bStreamingPaused );
	}
	else
	{
		curMelody.Clear();
		timeStreamFinished = timeLastUpdate;
		bStreamPlaying = false;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEngine::IsStreamPlaying()const
{
	return bStreamPlaying;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoundEngine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		CloseStreaming();
		channelsMap.clear();
		soundsMap.clear();
		timeStreamFinished = timeLastUpdate;
	}

	saver.Add( 1, &fStreamCurrentVolume );
	saver.Add( 2, &bStreamPlaying );
	saver.Add( 3, &bSoundCardPresent );
	saver.Add( 4, &timeLastUpdate );
	saver.Add( 5, &nStreamingChannel );
	saver.Add( 6, &fListenerDistance );
	saver.Add( 7, &vLastListenerPos );
	saver.Add( 9, &streamFadeOff );
	saver.Add( 10, &curMelody );
	saver.Add( 11, &nextMelody );
	saver.Add( 12, &bPaused );
	saver.Add( 13, &bStreamingPaused );

	if ( saver.IsReading() && curMelody.IsValid() )
	{
		bStreamPlaying = false;
		PlayStream( curMelody.szName.c_str(), curMelody.bLooped );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSoundEngine::SMelodyInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bLooped );
	saver.Add( 2, &szName );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
