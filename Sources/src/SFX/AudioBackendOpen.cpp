#include "StdAfx.h"

#include "AudioBackendImpl.h"

#if defined(SFX_USE_OPEN_AUDIO_BACKEND)

namespace
{
	struct SOpenSample
	{
		int nMode;
		bool bLooped;
		float fMinDistance;
		int nLoopStart;
		int nLoopEnd;
		unsigned int nSampleRate;
		unsigned int nChannels;
		unsigned int nBitsPerSample;
		unsigned int nBlockAlign;
		unsigned int nPcmBytes;
		std::vector<char> pcmData;
	};

	struct SOpenStream
	{
		std::string szFileName;
		bool bLooped;
		NAudioBackend::TStreamCallback pEndCallback;
		void *pUserData;
	};

	bool HasBytes( const char *pData, int nSize, int nOffset, int nBytes )
	{
		return pData && nOffset >= 0 && nBytes >= 0 && nOffset <= nSize && nBytes <= nSize - nOffset;
	}

	bool IsChunkId( const char *pData, int nSize, int nOffset, const char *pId )
	{
		return HasBytes( pData, nSize, nOffset, 4 ) &&
			pData[nOffset + 0] == pId[0] &&
			pData[nOffset + 1] == pId[1] &&
			pData[nOffset + 2] == pId[2] &&
			pData[nOffset + 3] == pId[3];
	}

	unsigned int ReadU16LE( const char *pData, int nOffset )
	{
		return static_cast<unsigned char>( pData[nOffset] ) |
			(static_cast<unsigned char>( pData[nOffset + 1] ) << 8);
	}

	unsigned int ReadU32LE( const char *pData, int nOffset )
	{
		return static_cast<unsigned char>( pData[nOffset] ) |
			(static_cast<unsigned char>( pData[nOffset + 1] ) << 8) |
			(static_cast<unsigned char>( pData[nOffset + 2] ) << 16) |
			(static_cast<unsigned char>( pData[nOffset + 3] ) << 24);
	}

	void InitializeEmptySample( SOpenSample *pSample, int nMode )
	{
		pSample->nMode = nMode;
		pSample->bLooped = false;
		pSample->fMinDistance = 0.0f;
		pSample->nLoopStart = 0;
		pSample->nLoopEnd = 0;
		pSample->nSampleRate = 0;
		pSample->nChannels = 0;
		pSample->nBitsPerSample = 0;
		pSample->nBlockAlign = 0;
		pSample->nPcmBytes = 0;
	}

	bool ParseWaveSample( const char *pData, int nSize, SOpenSample *pSample )
	{
		if ( !HasBytes( pData, nSize, 0, 12 ) ||
			!IsChunkId( pData, nSize, 0, "RIFF" ) ||
			!IsChunkId( pData, nSize, 8, "WAVE" ) )
		{
			return false;
		}

		bool bHaveFormat = false;
		bool bHaveData = false;
		unsigned int nAudioFormat = 0;
		int nOffset = 12;
		while ( HasBytes( pData, nSize, nOffset, 8 ) )
		{
			const unsigned int nChunkSize = ReadU32LE( pData, nOffset + 4 );
			const int nChunkDataOffset = nOffset + 8;
			if ( nChunkSize > static_cast<unsigned int>( nSize - nChunkDataOffset ) )
				break;

			if ( IsChunkId( pData, nSize, nOffset, "fmt " ) && nChunkSize >= 16 )
			{
				nAudioFormat = ReadU16LE( pData, nChunkDataOffset );
				pSample->nChannels = ReadU16LE( pData, nChunkDataOffset + 2 );
				pSample->nSampleRate = ReadU32LE( pData, nChunkDataOffset + 4 );
				pSample->nBlockAlign = ReadU16LE( pData, nChunkDataOffset + 12 );
				pSample->nBitsPerSample = ReadU16LE( pData, nChunkDataOffset + 14 );
				bHaveFormat = true;
			}
			else if ( IsChunkId( pData, nSize, nOffset, "data" ) )
			{
				pSample->nPcmBytes = nChunkSize;
				pSample->pcmData.assign( pData + nChunkDataOffset, pData + nChunkDataOffset + nChunkSize );
				bHaveData = true;
			}

			nOffset = nChunkDataOffset + nChunkSize + (nChunkSize & 1);
		}

		return bHaveFormat && bHaveData && nAudioFormat == 1 &&
			pSample->nSampleRate > 0 &&
			pSample->nChannels > 0 &&
			pSample->nBlockAlign > 0 &&
			pSample->nBitsPerSample > 0;
	}
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
		InitializeEmptySample( pSample, nMode );
		ParseWaveSample( pData, nSize, pSample );
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
		if ( !pSample )
			return 0;
		const SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
		return pOpenSample->nBlockAlign == 0 ? 0 : pOpenSample->nPcmBytes / pOpenSample->nBlockAlign;
	}

	unsigned int GetSampleRate( void *pSample )
	{
		return pSample ? static_cast<SOpenSample*>( pSample )->nSampleRate : 0;
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
