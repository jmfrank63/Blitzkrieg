#include "StdAfx.h"

#include "AudioBackendImpl.h"

#if defined(SFX_USE_OPEN_AUDIO_BACKEND)

namespace
{
	struct SOpenSample
	{
		int nSize;
		int nMode;
		bool bLooped;
		float fMinDistance;
		int nLoopStart;
		int nLoopEnd;
	};

	struct SOpenStream
	{
		std::string szFileName;
		bool bLooped;
		NAudioBackend::TStreamCallback pEndCallback;
		void *pUserData;
	};
}

namespace NAudioBackendImpl
{
	using NAudioBackend::SDriverInfo;
	using NAudioBackend::TStreamCallback;

	bool IsVersionSupported()
	{
		return true;
	}

	void PrepareDeviceSearch()
	{
	}

	int GetNumDrivers()
	{
		return 1;
	}

	SDriverInfo GetDriverInfo( int nDriver )
	{
		SDriverInfo driverInfo;
		driverInfo.szDriverName = nDriver == 0 ? "Open audio silent backend" : "";
		driverInfo.isHardware3DAccelerated = false;
		driverInfo.supportEAXReverb = false;
		driverInfo.supportA3DOcclusions = false;
		driverInfo.supportA3DReflections = false;
		driverInfo.supportReverb = false;
		return driverInfo;
	}

	IRefCount* GetOutputHandle()
	{
		return 0;
	}

	void SetDriver( int nDriver )
	{
	}

	bool InitDevice( HWND hWnd, ESFXOutputType output, int nMixRate, int nMaxChannels, const SDriverInfo &driverInfo, bool *pSoundCardPresent )
	{
		if ( pSoundCardPresent )
			*pSoundCardPresent = output != SFX_OUTPUT_NO;
		OutputDebugString( "SFX open audio silent backend initialized\n" );
		return true;
	}

	void CloseDevice()
	{
	}

	void DebugTraceMixer()
	{
		OutputDebugString( "SFX open audio silent backend\n" );
	}

	void SetDistanceFactor( float fFactor )
	{
	}

	void SetRolloffFactor( float fFactor )
	{
	}

	void FreeSample( void *pSample )
	{
		delete static_cast<SOpenSample*>( pSample );
	}

	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode )
	{
		SOpenSample *pSample = new SOpenSample;
		pSample->nSize = nSize;
		pSample->nMode = nMode;
		pSample->bLooped = false;
		pSample->fMinDistance = 0.0f;
		pSample->nLoopStart = 0;
		pSample->nLoopEnd = 0;
		return pSample;
	}

	void SetSampleMinDistance( void *pSample, float fMinDistance )
	{
		if ( pSample )
			static_cast<SOpenSample*>( pSample )->fMinDistance = fMinDistance;
	}

	void SetSampleLoop( void *pSample, bool bEnable )
	{
		if ( pSample )
			static_cast<SOpenSample*>( pSample )->bLooped = bEnable;
	}

	void SetSampleLoopPoints( void *pSample, int nStart, int nEnd )
	{
		if ( pSample )
		{
			SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
			pOpenSample->nLoopStart = nStart;
			pOpenSample->nLoopEnd = nEnd;
		}
	}

	unsigned int GetSampleLength( void *pSample )
	{
		return pSample ? static_cast<SOpenSample*>( pSample )->nSize : 0;
	}

	unsigned int GetSampleRate( void *pSample )
	{
		return 44100;
	}

	int GetSampleMode2D()
	{
		return 0;
	}

	int GetSampleMode3D()
	{
		return 1;
	}

	bool IsChannelPlayingSample( int nChannel, void *pSample )
	{
		return false;
	}

	int PlaySample( void *pSample )
	{
		return -1;
	}

	int PlaySamplePaused( void *pSample )
	{
		return -1;
	}

	void SetChannelVolume( int nChannel, int nVolume )
	{
	}

	void SetChannelPan( int nChannel, int nPan )
	{
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
	}

	void StopChannel( int nChannel )
	{
	}

	bool IsChannelPlaying( int nChannel )
	{
		return false;
	}

	int GetChannelsPlaying()
	{
		return 0;
	}

	unsigned int GetChannelPosition( int nChannel )
	{
		return 0;
	}

	void SetChannelPosition( int nChannel, unsigned int nPosition )
	{
	}

	int GetLastError()
	{
		return 0;
	}

	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos )
	{
	}

	void* OpenStream( const char *pszFileName, bool bLooped )
	{
		SOpenStream *pStream = new SOpenStream;
		pStream->szFileName = pszFileName ? pszFileName : "";
		pStream->bLooped = bLooped;
		pStream->pEndCallback = 0;
		pStream->pUserData = 0;
		return pStream;
	}

	void CloseStream( void *pStream )
	{
		delete static_cast<SOpenStream*>( pStream );
	}

	void ClearStreamCallbacks( void *pStream )
	{
		if ( pStream )
		{
			SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
			pOpenStream->pEndCallback = 0;
			pOpenStream->pUserData = 0;
		}
	}

	void SetStreamEndCallback( void *pStream, TStreamCallback pCallback, void *pUserData )
	{
		if ( pStream )
		{
			SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
			pOpenStream->pEndCallback = pCallback;
			pOpenStream->pUserData = pUserData;
		}
	}

	int PlayStream( void *pStream )
	{
		return -1;
	}

	void SetStreamChannelPan( int nChannel )
	{
	}
}

#endif // defined(SFX_USE_OPEN_AUDIO_BACKEND)
