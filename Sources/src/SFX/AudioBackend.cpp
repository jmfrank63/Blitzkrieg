#include "StdAfx.h"

#include "AudioFmodCompat.h"
#include "AudioBackend.h"

namespace NAudioBackend
{
	bool IsVersionSupported()
	{
		return FSOUND_GetVersion() >= FMOD_VERSION;
	}

	void PrepareDeviceSearch()
	{
		FSOUND_SetOutput( FSOUND_OUTPUT_DSOUND );
	}

	int GetNumDrivers()
	{
		return FSOUND_GetNumDrivers();
	}

	SDriverInfo GetDriverInfo( int nDriver )
	{
		SDriverInfo driverInfo;
		unsigned int nCaps = 0;
		driverInfo.szDriverName = (const char *) FSOUND_GetDriverName( nDriver );
		FSOUND_GetDriverCaps( nDriver, &nCaps );
		driverInfo.isHardware3DAccelerated = nCaps & FSOUND_CAPS_HARDWARE;
		driverInfo.supportEAXReverb = nCaps & FSOUND_CAPS_EAX2;
		driverInfo.supportA3DOcclusions = false;
		driverInfo.supportA3DReflections = false;
		driverInfo.supportReverb = nCaps & FSOUND_CAPS_EAX2;
		return driverInfo;
	}

	IRefCount* GetOutputHandle()
	{
		return reinterpret_cast<IRefCount*>( FSOUND_GetOutputHandle() );
	}

	void SetDriver( int nDriver )
	{
		FSOUND_SetDriver( nDriver );
	}

	bool InitDevice( HWND hWnd, ESFXOutputType output, int nMixRate, int nMaxChannels, const SDriverInfo &driverInfo, bool *pSoundCardPresent )
	{
		FSOUND_OUTPUTTYPES eOutput;
		*pSoundCardPresent = true;
		switch ( output )
		{
			case SFX_OUTPUT_NO:
				*pSoundCardPresent = false;
				eOutput = FSOUND_OUTPUT_NOSOUND;
				OutputDebugString("FSOUND_OUTPUT_NOSOUND\n");
				break;
			case SFX_OUTPUT_WINMM:
				eOutput = FSOUND_OUTPUT_WINMM;
				OutputDebugString("FSOUND_OUTPUT_WINMM\n");
				break;
			case SFX_OUTPUT_DSOUND:
				eOutput = FSOUND_OUTPUT_DSOUND;
				OutputDebugString("FSOUND_OUTPUT_DSOUND\n");
				break;
			case SFX_OUTPUT_A3D:
				if ( !driverInfo.supportA3DOcclusions )
				{
					eOutput = FSOUND_OUTPUT_DSOUND;
					OutputDebugString("FSOUND_OUTPUT_DSOUND(1)\n");
				}
				else
				{
					OutputDebugString("FSOUND_OUTPUT_A3D\n");
					eOutput = FSOUND_OUTPUT_A3D;
				}
				break;
			default:
				eOutput = FSOUND_OUTPUT_NOSOUND;
				break;
		}

		FSOUND_SetOutput( eOutput );
		FSOUND_SetHWND( hWnd );
		return FSOUND_Init( nMixRate, nMaxChannels, FSOUND_INIT_USEDEFAULTMIDISYNTH ) != 0;
	}

	void CloseDevice()
	{
		FSOUND_Close();
	}

	void DebugTraceMixer()
	{
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
	}

	void SetDistanceFactor( float fFactor )
	{
		FSOUND_3D_SetDistanceFactor( fFactor );
	}

	void SetRolloffFactor( float fFactor )
	{
		FSOUND_3D_SetRolloffFactor( fFactor );
	}

	static FSOUND_STREAM* AsFmodStream( void *pStream )
	{
		return static_cast<FSOUND_STREAM*>( pStream );
	}

	void FreeSample( void *pSample )
	{
		if ( pSample )
			FSOUND_Sample_Free( static_cast<FSOUND_SAMPLE*>(pSample) );
	}

	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode )
	{
		return FSOUND_Sample_Load( FSOUND_UNMANAGED, pData, nMode | FSOUND_LOADMEMORY, 0, nSize );
	}

	void SetSampleMinDistance( void *pSample, float fMinDistance )
	{
		if ( pSample )
			FSOUND_Sample_SetMinMaxDistance( static_cast<FSOUND_SAMPLE*>(pSample), fMinDistance, 1000000000.0f );
	}

	void SetSampleLoop( void *pSample, bool bEnable )
	{
		if ( pSample )
			FSOUND_Sample_SetMode( static_cast<FSOUND_SAMPLE*>(pSample), bEnable ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF );
	}

	void SetSampleLoopPoints( void *pSample, int nStart, int nEnd )
	{
		if ( pSample )
			FSOUND_Sample_SetLoopPoints( static_cast<FSOUND_SAMPLE*>(pSample), nStart, nEnd );
	}

	unsigned int GetSampleLength( void *pSample )
	{
		return FSOUND_Sample_GetLength( static_cast<FSOUND_SAMPLE*>(pSample) );
	}

	unsigned int GetSampleRate( void *pSample )
	{
		int nFrequency = 44000;
		FSOUND_Sample_GetDefaults( static_cast<FSOUND_SAMPLE*>(pSample), &nFrequency, 0, 0, 0 );
		return nFrequency;
	}

	int GetSampleMode2D()
	{
		return FSOUND_2D;
	}

	int GetSampleMode3D()
	{
		return FSOUND_HW3D;
	}

	bool IsChannelPlayingSample( int nChannel, void *pSample )
	{
		return (nChannel != -1) && FSOUND_IsPlaying(nChannel) &&
			FSOUND_GetCurrentSample( nChannel ) == static_cast<FSOUND_SAMPLE*>(pSample);
	}

	int PlaySample( void *pSample )
	{
		return FSOUND_PlaySound( FSOUND_FREE, static_cast<FSOUND_SAMPLE*>(pSample) );
	}

	int PlaySamplePaused( void *pSample )
	{
		return FSOUND_PlaySoundEx( FSOUND_FREE, static_cast<FSOUND_SAMPLE*>(pSample), 0, true );
	}

	void SetChannelVolume( int nChannel, int nVolume )
	{
		FSOUND_SetVolume( nChannel, nVolume );
	}

	void SetChannelPan( int nChannel, int nPan )
	{
		FSOUND_SetPan( nChannel, nPan );
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
		FSOUND_SetPaused( nChannel, bPaused );
	}

	void StopChannel( int nChannel )
	{
		FSOUND_StopSound( nChannel );
	}

	bool IsChannelPlaying( int nChannel )
	{
		return FSOUND_IsPlaying( nChannel ) != 0;
	}

	int GetChannelsPlaying()
	{
		return FSOUND_GetChannelsPlaying();
	}

	unsigned int GetChannelPosition( int nChannel )
	{
		return FSOUND_GetCurrentPosition( nChannel );
	}

	void SetChannelPosition( int nChannel, unsigned int nPosition )
	{
		FSOUND_SetCurrentPosition( nChannel, nPosition );
	}

	int GetLastError()
	{
		return FSOUND_GetError();
	}

	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos )
	{
		FSOUND_3D_SetAttributes( nChannel, const_cast<float*>(vPos.m), 0 );
	}

	void* OpenStream( const char *pszFileName, bool bLooped )
	{
		return FSOUND_Stream_Open( pszFileName, FSOUND_2D | (bLooped ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF), 0, 0 );
	}

	void CloseStream( void *pStream )
	{
		if ( pStream )
			FSOUND_Stream_Close( AsFmodStream(pStream) );
	}

	void ClearStreamCallbacks( void *pStream )
	{
		if ( pStream )
		{
			FSOUND_Stream_SetEndCallback( AsFmodStream(pStream), 0, 0 );
			FSOUND_Stream_SetSyncCallback( AsFmodStream(pStream), 0, 0 );
		}
	}

	void SetStreamEndCallback( void *pStream, TStreamCallback pCallback, void *pUserData )
	{
		if ( pStream )
			FSOUND_Stream_SetEndCallback( AsFmodStream(pStream), reinterpret_cast<FSOUND_STREAMCALLBACK>(pCallback), pUserData );
	}

	int PlayStream( void *pStream )
	{
		return FSOUND_Stream_Play( FSOUND_FREE, AsFmodStream(pStream) );
	}

	void SetStreamChannelPan( int nChannel )
	{
		FSOUND_SetPan( nChannel, FSOUND_STEREOPAN );
	}
}
