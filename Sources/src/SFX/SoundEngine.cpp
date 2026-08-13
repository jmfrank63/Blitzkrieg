#include "StdAfx.h"

#include "AudioBackend.h"
#include "SoundEngine.h"

#include "SampleSounds.h"
#include "../Scene/Scene.h"
#include "../Formats/fmtTerrain.h"
#include "../Misc/Win32Helper.h"
#include "../Platform/Clock.h"
#include "../Platform/Debug.h"
static NWin32Helper::CCriticalSection critSection;

namespace
{
	bool IsAbsoluteStreamName( const char *pszFileName )
	{
		if ( pszFileName == 0 )
			return false;
		if ( pszFileName[0] == '\\' || pszFileName[0] == '/' )
			return true;
		return isalpha( static_cast<unsigned char>(pszFileName[0]) ) && pszFileName[1] == ':';
	}

	std::string MakeStreamFileName( const char *pszFileName, const char *pszExtension )
	{
		const std::string szBaseName = pszFileName == 0 ? "" : pszFileName;
		if ( IsAbsoluteStreamName( pszFileName ) )
			return szBaseName + pszExtension;
		return std::string( GetSingleton<IDataStorage>()->GetName() ) + szBaseName + pszExtension;
	}

	BYTE MasterVolumeToByte( float fVolume )
	{
		const float fClampedVolume = Clamp( fVolume, 0.0f, 1.0f );
		if ( fClampedVolume <= 0.0f )
			return 0;

		const float fDecibels = ( fClampedVolume - 1.0f ) * 60.0f;
		const float fAmplitude = pow( 10.0f, fDecibels / 20.0f );
		const int nVolume = Clamp( int( fAmplitude * 255.0f + 0.999f ), 1, 255 );
		return BYTE( nVolume );
	}
}

class CPlayVisitor : public ISFXVisitor
{
	CSoundEngine *pSFX;
	int RegisterSound( CBaseSound *pSound, const int nChannel )
	{
		if ( nChannel != -1 )
		{
			const int nVolume = pSound->GetVolume() >= 0 ? pSound->GetVolume() * pSFX->GetSFXMasterVolume() : pSFX->GetSFXMasterVolume();
			NAudioBackend::SetChannelVolume( nChannel, nVolume );
			const int nPan = 128 + 127 * pSound->GetPan();
			NAudioBackend::SetChannelPan( nChannel, nPan );
			pSFX->MapSound( pSound, nChannel );
		}
		else
		{
			NStr::DebugTrace( "Sound error %d\n", NAudioBackend::GetLastError() );
		}
		pSound->SetChannel( nChannel );
		return nChannel;
	}
public:
	void Init( class CSoundEngine *_pSFX ) { pSFX = _pSFX; }
	virtual int STDCALL VisitSound2D( CSound2D *pSound )
	{
		void *sample = pSound->GetSample()->GetInternalContainer();
		if ( sample == 0 )
			return -1;
		const int nChannel = NAudioBackend::PlaySamplePaused( sample );
		return RegisterSound( pSound, nChannel );
	}
	virtual int STDCALL VisitSound3D( CSound3D *pSound, const CVec3 &vPos )
	{
		void *sample = pSound->GetSample()->GetInternalContainer();
		if ( sample == 0 )
			return -1;
		const int nChannel = NAudioBackend::PlaySamplePaused( sample );
		const int nRegisteredChannel = RegisterSound( pSound, nChannel );
		if ( nRegisteredChannel != -1 )
			pSFX->Update3DChannel( pSound, nRegisteredChannel );
		return nRegisteredChannel;
	}
};
static CPlayVisitor thePlayVisitor;
CSoundEngine::CSoundEngine()
: bInited( false ), pStreamingSound( 0 ), bPaused( false ), bStreamingPaused( false ),
	cSFXMasterVolume( 255 ), cStreamMasterVolume( 255 ), bEnableSFX( true ), bEnableStreaming( true ),
	timeLastUpdate( -1 ), timeStreamFinished( -1 ), fStreamCurrentVolume( 1.0f ),
	bStreamPlaying( false ), nStreamingChannel( -1 ), vLastListenerPos( VNULL3 ),
	nMelodyFinishedPending( 0 )
{
}
bool CSoundEngine::SearchDevices()
{
	if ( !NAudioBackend::IsVersionSupported() )
	{
		NPlatform::DebugWrite( "Error : You are using the wrong DLL version!\n" );
		return false;
	}
	NAudioBackend::PrepareDeviceSearch();
	int nNumDrivers = NAudioBackend::GetNumDrivers();
	drivers.resize( nNumDrivers );
	for ( int i = 0; i < nNumDrivers; ++i )
	{
		drivers[i] = NAudioBackend::GetDriverInfo( i );
	}
	return true;
}
bool CSoundEngine::IsInitialized()
{
	return bInited;
}
IRefCount* CSoundEngine::QI( int nInterfaceTypeID )
{
	if ( nInterfaceTypeID == 0 ) 
		return NAudioBackend::GetOutputHandle();
	return 0;
}
bool CSoundEngine::Init( int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels )
{
	if ( !SearchDevices() )
		return false;

	
	NI_ASSERT_T( nDriver < drivers.size(), NStr::Format("Can't find driver %d (max found %d)", nDriver, drivers.size()) );
	NAudioBackend::SetDriver( nDriver );
	NI_ASSERT_T( !(output == SFX_OUTPUT_A3D && !drivers[nDriver].supportA3DOcclusions), "Can't set output as A3D with unsupported feature" );
	if ( !NAudioBackend::InitDevice( output, nMixRate, nMaxChannels, drivers[nDriver], &bSoundCardPresent ) )
	{
		NPlatform::DebugWrite( "NFMSound::Start():error!\n" );
		bSoundCardPresent = false;
		return true;
	}

#ifdef _DEBUG
	NPlatform::DebugWriteFormat( "Using \"%s\" sound driver.\n", drivers[nDriver].szDriverName.c_str() );
	if ( drivers[nDriver].isHardware3DAccelerated )
		NPlatform::DebugWrite( "- Driver supports hardware 3D sound!\n" );
	if ( drivers[nDriver].supportEAXReverb )
		NPlatform::DebugWrite( "- Driver supports EAX reverb!\n" );
	if ( drivers[nDriver].supportA3DOcclusions )
		NPlatform::DebugWrite( "- Driver supports hardware 3d geometry processing with occlusions!\n" );
	if ( drivers[nDriver].supportA3DReflections )
		NPlatform::DebugWrite( "- Driver supports hardware 3d geometry processing with reflections!\n" );
	if ( drivers[nDriver].supportReverb )
		NPlatform::DebugWrite( "- Driver supports EAX 2.0 reverb!\n" );
	
	NPlatform::DebugWrite( "Mixer = " );
	NAudioBackend::DebugTraceMixer();
#endif

	fListenerDistance = GetGlobalVar( "Sound.Listener.Distance", 0.0f ) * fWorldCellSize/2.0f;
	

	cSFXMasterVolume = MasterVolumeToByte( GetGlobalVar( "Sound.SFXVolume", 100.0f ) / 100.0f );
	cStreamMasterVolume = MasterVolumeToByte( GetGlobalVar( "Sound.MusicVolume", 100.0f ) / 100.0f );

	streamFadeOff.Init();
	bInited = true;
	return true;
}
void CSoundEngine::Done()
{
	nextMelody.Clear();
	curMelody.Clear();

	streamFadeOff.Clear();

	drivers.clear();
	CloseStreaming();
	channelsMap.clear();
	soundsMap.clear();
	NAudioBackend::CloseDevice();
}
void CSoundEngine::SetDistanceFactor( float fFactor )
{
	NAudioBackend::SetDistanceFactor( fFactor );
}
void CSoundEngine::SetRolloffFactor( float fFactor )
{
	NI_ASSERT_TF( (fFactor >= 0) && (fFactor <= 10), NStr::Format("Rolloff factor (%g) must be in range [0..10]", fFactor), return );
	NAudioBackend::SetRolloffFactor( fFactor );
}
void CSoundEngine::Update( interface ICamera *pCamera )
{
	// [sfx-trace] passive stream-cursor watchdog (main thread, read-only): if
	// the music's playback cursor advanced far less than wall-clock since the
	// last check, the mixer starved or the stream gapped — logged with both
	// deltas. Detects stalls retroactively without touching the audio graph.
	// Strip with the other sound diagnostics.
	if ( bStreamPlaying && nStreamingChannel != -1 && !bStreamingPaused && !bPaused )
	{
		static std::uint32_t s_tPrev = 0;
		static unsigned int s_nPrevPos = 0;
		const std::uint32_t tNow = NPlatform::MonotonicMilliseconds();
		// The mixer advances the cursor in whole periods (~40ms), so judging
		// windows shorter than a few periods false-positives; sample >=200ms.
		if ( s_tPrev == 0 || NPlatform::MillisecondsElapsed( s_tPrev, tNow ) >= 200 )
		{
			const unsigned int nPos = NAudioBackend::GetChannelPosition( nStreamingChannel );
			if ( s_tPrev != 0 )
			{
				const long nDeltaMs = static_cast<long>( NPlatform::MillisecondsElapsed( s_tPrev, tNow ) );
				const long nDeltaFrames = static_cast<long>( nPos - s_nPrevPos );
				// expect ~44.1 frames/ms; flag anything under 60% of realtime
				if ( nDeltaMs < 5000 && nDeltaFrames >= 0 && nDeltaFrames * 10 < nDeltaMs * 441 * 6 / 10 )
				{
					FILE *pFile = fopen( "sfx_trace.log", "ab" );
					if ( pFile )
					{
						fprintf( pFile, "%u [cursor] stall: dt=%ldms frames=%ld (expected ~%ld)\n", tNow, nDeltaMs, nDeltaFrames, nDeltaMs * 44 );
						fclose( pFile );
					}
				}
			}
			s_tPrev = tNow;
			s_nPrevPos = nPos;
		}
	}

	timeLastUpdate = GetSingleton<IGameTimer>()->GetAbsTime();
	if ( pCamera )
		UpdateCameraPos( pCamera->GetAnchor() );
	// Deferred melody handling: the audio-thread end callback and the fade
	// thread only raise flags — all stream open/close/switch work happens
	// here, on the main thread. (Doing it on those threads froze the mixer
	// while the next music file loaded, and deadlocked at exit.)
	if ( nMelodyFinishedPending.exchange( 0, std::memory_order_acq_rel ) )
		NotifyMelodyFinished();
	if ( streamFadeOff.ConsumeFinished() )
		StopStream( 0 );
	if ( (timeStreamFinished != -1) && (timeStreamFinished < timeLastUpdate) && (timeLastUpdate - timeStreamFinished > 15000) )
		PlayNextMelody();
	
	const int nNumChannels = NAudioBackend::GetChannelsPlaying();

	{
		IScene * pScene = GetSingleton<IScene>();
		IStatSystem *pStat = pScene->GetStatSystem();
		pStat->UpdateEntry( "SFX: num channels:", NStr::Format("%d", nNumChannels )  );
	}

	// [sfx-trace] BK_SFX_TRACE=1: periodic voice census on stderr. channels
	// must return to a quiet baseline after battles; a persistent gap above
	// mapped means orphaned backend voices (the immortal-loop bug). Strip with
	// the other sound diagnostics.
	{
		static const bool s_bTrace = getenv( "BK_SFX_TRACE" ) != 0;
		static int s_nTraceFrames = 0;
		if ( s_bTrace && ++s_nTraceFrames >= 120 )
		{
			s_nTraceFrames = 0;
			fprintf( stderr, "[sfx] channels=%d mapped=%d\n", nNumChannels, static_cast<int>( soundsMap.size() ) );
		}
	}


	if ( nNumChannels > 0 )
		ClearChannels();
}
void CSoundEngine::Update3DChannel( CSound3D *pSound, int nChannel )
{
	if ( pSound == 0 || nChannel == -1 )
		return;

	NAudioBackend::SetChannel3DAttributes( nChannel, pSound->GetPosition() - vLastListenerPos );
}
void CSoundEngine::UpdateCameraPos( const CVec3 &vPos )
{
	vLastListenerPos.Set( vPos.x, vPos.z, vPos.y );
	for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
	{
		if ( CSound3D *pSound3D = dynamic_cast<CSound3D*>( it->second.GetPtr() ) )
			Update3DChannel( pSound3D, it->first );
	}
}
void CSoundEngine::CloseStreaming()
{
	if ( pStreamingSound )
	{
		NAudioBackend::ClearStreamCallbacks( pStreamingSound );
		NAudioBackend::StopChannel( nStreamingChannel );
		NAudioBackend::CloseStream( pStreamingSound );

		pStreamingSound = 0;
		bStreamPlaying = false;
		curMelody.Clear();
	}
}
signed char STDCALL NextMelodyCallback( void *stream, void *buff, int len, void *userdata )
{
	// Runs on the miniaudio device (mixer) thread — must not touch streams.
	CSoundEngine *pSFX = reinterpret_cast<CSoundEngine*>( userdata );
	pSFX->QueueMelodyFinishedNotification();
	return true;
}
bool CSoundEngine::PlayNextMelody()
{
	SMelodyInfo melodyToPlay;
	{
		NWin32Helper::CCriticalSectionLock lock( critSection );
		if ( !bEnableStreaming || nextMelody.szName.empty() )
			return false;
		melodyToPlay = nextMelody;
		nextMelody.Clear();
	}
	PlayStream( melodyToPlay.szName.c_str(), melodyToPlay.bLooped, 0 );
	return true;
}
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
void CSoundEngine::SetStreamVolume( const float fVolume )
{
	fStreamCurrentVolume = Clamp( fVolume, 0.0f, 1.0f );
	if ( nStreamingChannel != -1 )
	{
		NAudioBackend::SetChannelVolume( nStreamingChannel, fStreamCurrentVolume *cStreamMasterVolume );
	}
}
void CSoundEngine::MapSound( ISound *pSound, int nChannel )
{
	// The maps must stay exact inverses: the backend reuses channel indices,
	// and map::insert silently keeps a stale entry — StopChannel then erases
	// the wrong sound, the live voice loses its only handle, and a looping
	// one plays forever. Unlink both stale pairings before linking anew.
	CChannelSoundMap::iterator itChannel = soundsMap.find( nChannel );
	if ( itChannel != soundsMap.end() )
		channelsMap.erase( itChannel->second );
	CSoundChannelMap::iterator itSound = channelsMap.find( pSound );
	if ( itSound != channelsMap.end() )
		soundsMap.erase( itSound->second );
	channelsMap[pSound] = nChannel;
	soundsMap[nChannel] = pSound;
}
float CSoundEngine::GetStreamVolume() const
{
	return fStreamCurrentVolume;
}
void CSoundEngine::SetSFXMasterVolume( float fVolume )
{
	cSFXMasterVolume = MasterVolumeToByte( fVolume );
}
void CSoundEngine::SetStreamMasterVolume( float fVolume )
{
	cStreamMasterVolume = MasterVolumeToByte( fVolume );
	if ( bStreamPlaying && nStreamingChannel != -1 )
	{
		NAudioBackend::SetChannelVolume( nStreamingChannel, fStreamCurrentVolume *cStreamMasterVolume );
	}
}

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
		streamFadeOff.Clear();
		SetStreamVolume( 1.0f );
		CloseStreaming();
		curMelody.szName = pszFileName;
		curMelody.bLooped = bLooped;
		std::string szFileName = MakeStreamFileName( pszFileName, ".mp3" );
		std::string szFileName1 = MakeStreamFileName( pszFileName, ".ogg" );

		pStreamingSound = NAudioBackend::OpenStream( szFileName.c_str(), bLooped );
		if ( pStreamingSound )
		{
			nStreamingChannel = NAudioBackend::PlayStream( pStreamingSound );
			if ( nStreamingChannel == -1 )
			{
				NAudioBackend::CloseStream( pStreamingSound );
				pStreamingSound = 0;
			}
		}

		if ( !pStreamingSound )
		{
			pStreamingSound = NAudioBackend::OpenStream( szFileName1.c_str(), bLooped );
			if ( pStreamingSound )
			{
				nStreamingChannel = NAudioBackend::PlayStream( pStreamingSound );
				if ( nStreamingChannel == -1 )
				{
					NAudioBackend::CloseStream( pStreamingSound );
					pStreamingSound = 0;
				}
			}
		}

		if ( pStreamingSound && nStreamingChannel != -1 )
		{
			NAudioBackend::SetStreamChannelPan( nStreamingChannel );
			NAudioBackend::SetChannelVolume( nStreamingChannel, cStreamMasterVolume );
			NAudioBackend::SetStreamEndCallback( pStreamingSound, NextMelodyCallback, this );
			if ( bStreamingPaused ) 
				NAudioBackend::SetChannelPaused( nStreamingChannel, bStreamingPaused );
			NWin32Helper::CCriticalSectionLock lock( critSection );
			timeStreamFinished = -1;
			bStreamPlaying = true;
		}
		else
		{
			if ( pStreamingSound )
			{
				NAudioBackend::CloseStream( pStreamingSound );
				pStreamingSound = 0;
			}
			curMelody.Clear();
			nStreamingChannel = -1;
			bStreamPlaying = false;
		}
	}
}
void CSoundEngine::PlayVideoStream( const char *pszFileName, bool bLooped )
{
	PlayStream( pszFileName, bLooped, 0 );
	if ( bStreamPlaying && nStreamingChannel != -1 )
	{
		const BYTE cVideoMasterVolume = MasterVolumeToByte( GetGlobalVar( "Sound.VideoStreamMasterVolume", 1.0f ) );
		NAudioBackend::SetChannelVolume( nStreamingChannel, cVideoMasterVolume );
	}
}
bool CSoundEngine::IsPaused()
{
	return bPaused;
}
bool CSoundEngine::PauseStreaming( bool bPause )
{
	if ( bStreamingPaused != bPause ) 
	{
		if ( nStreamingChannel != -1 ) 
			NAudioBackend::SetChannelPaused( nStreamingChannel, bPause );
		bStreamingPaused = bPause;
	}
	return bPause;
}
bool CSoundEngine::Pause( bool bPause )
{
	if ( bPaused != bPause ) 
	{
		for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
		{
			if ( nStreamingChannel != it->first )
			{
				NAudioBackend::SetChannelPaused( it->first, bPause );
			}
		}
		bPaused = bPause;
	}
	return bPause;
}
void CSoundEngine::ClearChannels()
{
	if ( bPaused )
		return;
	std::list<int> channels;
	for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
	{
		if ( !it->second->IsValid() )
		{
			NAudioBackend::StopChannel( it->first );
		}
		if ( !NAudioBackend::IsChannelPlaying( it->first ) )
		{
			channels.push_back( it->first );
			NAudioBackend::StopChannel( it->first );
		}
	}
	for ( std::list<int>::iterator it = channels.begin(); it != channels.end(); ++it )
	{
		const int nChannel = *it;
		ISound *pSound = soundsMap[nChannel];
		soundsMap.erase( nChannel );
		channelsMap.erase( pSound );
	}
}
int CSoundEngine::PlaySample( ISound *pSound, bool bLooped, unsigned int nStartPos )
{
	if ( pSound == 0 || !bEnableSFX )
		return -1;
	thePlayVisitor.Init( this );
	if ( static_cast<CBaseSound*>( pSound )->GetSample() == 0 )
		return -1;

	CSoundSample *pSample = static_cast<CBaseSound*>( pSound )->GetSample();
	pSample->SetLoop( bLooped );
	const int nChannel = pSound->Visit( &thePlayVisitor );
	if ( 0 != nStartPos )
		NAudioBackend::SetChannelPosition( nChannel, nStartPos );
	NAudioBackend::SetChannelPaused( nChannel, false );
	return nChannel;
}
void CSoundEngine::UpdateSample( ISound *pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{
		const int nChannel = pos->second;
		const int nPan = Clamp( int(128 + pSound->GetPan() * 127), 0, 255 );
		NAudioBackend::SetChannelPan( nChannel, nPan );
		const int nVolume = Clamp( int(pSound->GetVolume() >= 0 ? pSound->GetVolume() * GetSFXMasterVolume() : GetSFXMasterVolume()), 0, 255 );
		NAudioBackend::SetChannelVolume( nChannel, nVolume );
	}
}
void CSoundEngine::StopSample( ISound *pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{		
		StopChannel( pos->second );
	}
}
bool CSoundEngine::IsPlaying( ISound *pSound )
{
	if ( !pSound )
		return false;
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	return pos != channelsMap.end();
}
void CSoundEngine::StopChannel( int nChannel )
{
 	if ( nChannel == -1 )
		return;
	NAudioBackend::StopChannel( nChannel );
	CChannelSoundMap::iterator pos = soundsMap.find( nChannel );
	if ( pos != soundsMap.end() )
	{
		
		channelsMap.erase( pos->second );
		soundsMap.erase( pos );
	}
}
unsigned int CSoundEngine::GetCurrentPosition( ISound * pSound )
{
	CSoundChannelMap::iterator pos = channelsMap.find( pSound );
	if ( pos != channelsMap.end() )
	{
		int nChannel = (*pos).second;
		return NAudioBackend::GetChannelPosition( nChannel );
	}
	return 0;
}
void CSoundEngine::SetCurrentPosition( ISound * pSound, unsigned int pos )
{
	CSoundChannelMap::iterator it = channelsMap.find( pSound );
	if ( it != channelsMap.end() )
	{
		int nChannel = (*it).second;
		NAudioBackend::SetChannelPosition( nChannel, pos );
	}
}
void CSoundEngine::ReEnableSounds()
{
	if ( !bEnableSFX )
	{
		for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
		{
			if ( NAudioBackend::IsChannelPlaying( it->first ) )
				NAudioBackend::StopChannel( it->first );
		}
		soundsMap.clear();
		channelsMap.clear();
	}
	if ( !bEnableStreaming )
		CloseStreaming();
}
void CSoundEngine::NotifyMelodyFinished()
{
	SMelodyInfo melodyToPlay;
	bool bRestartLoopedMelody = false;

	{
		NWin32Helper::CCriticalSectionLock lock( critSection );
		if ( nextMelody.IsValid() )
		{
			melodyToPlay = nextMelody;
			nextMelody.Clear();
		}
		else if ( curMelody.IsValid() && curMelody.bLooped )
		{
			bRestartLoopedMelody = true;
		}
		else
		{
			curMelody.Clear();
			timeStreamFinished = timeLastUpdate;
			bStreamPlaying = false;
			return;
		}
	}

	if ( !melodyToPlay.szName.empty() )
	{
		PlayStream( melodyToPlay.szName.c_str(), melodyToPlay.bLooped, 0 );
		return;
	}

	if ( bRestartLoopedMelody && pStreamingSound )
	{
		nStreamingChannel = NAudioBackend::PlayStream( pStreamingSound );
		if ( nStreamingChannel == -1 )
		{
			NWin32Helper::CCriticalSectionLock lock( critSection );
			timeStreamFinished = timeLastUpdate;
			bStreamPlaying = false;
			return;
		}
		NAudioBackend::SetStreamChannelPan( nStreamingChannel );
		NAudioBackend::SetChannelVolume( nStreamingChannel, cStreamMasterVolume );
		NAudioBackend::SetStreamEndCallback( pStreamingSound, NextMelodyCallback, this );
		if ( bStreamingPaused ) 
			NAudioBackend::SetChannelPaused( nStreamingChannel, bStreamingPaused );

		NWin32Helper::CCriticalSectionLock lock( critSection );
		timeStreamFinished = -1;
		bStreamPlaying = true;
	}
}
bool CSoundEngine::IsStreamPlaying()const
{
	return bStreamPlaying;
}
int CSoundEngine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		CloseStreaming();
		// The loaded world knows nothing of the voices still sounding — stop
		// them before dropping the maps, or looping ones survive the load. The
		// backend sweep also kills voices the maps already lost track of.
		for ( CChannelSoundMap::iterator it = soundsMap.begin(); it != soundsMap.end(); ++it )
			NAudioBackend::StopChannel( it->first );
		channelsMap.clear();
		soundsMap.clear();
		NAudioBackend::StopAllSampleChannels();
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
int CSoundEngine::SMelodyInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bLooped );
	saver.Add( 2, &szName );
	return 0;
}
