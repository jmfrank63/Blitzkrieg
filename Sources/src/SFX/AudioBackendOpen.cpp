#include "StdAfx.h"

#include "AudioBackendImpl.h"
#include "AudioBackendXiphVorbis.h"

#if defined(SFX_USE_OPEN_AUDIO_BACKEND)

#define STB_VORBIS_HEADER_ONLY
#include "../../sdk/stb/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "../../sdk/miniaudio/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "../../sdk/stb/stb_vorbis.c"

namespace
{
	ma_engine g_engine;
	bool g_bEngineInitialized = false;

	struct SXiphStreamDataSource
	{
		ma_data_source_base base;
		SXiphVorbisStream *pVorbisStream;
		unsigned int nSampleRate;
		unsigned int nChannels;
		unsigned int nBlockAlign;
		unsigned long long nTotalFrames;
	};

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
		ma_format format;
		ma_audio_buffer buffer;
		bool bBufferInitialized;
	};

	struct SOpenStream;

	struct SOpenChannel
	{
		ma_audio_buffer buffer;
		ma_decoder decoder;
		SXiphStreamDataSource xiphDataSource;
		ma_sound sound;
		bool bBufferInitialized;
		bool bDecoderInitialized;
		bool bXiphDataSourceInitialized;
		bool bSoundInitialized;
		SOpenSample *pSample;
		SOpenStream *pStream;
		float fBaseVolume;
		float fDistanceVolume;
		float fPan;
	};

	struct SOpenStream
	{
		std::string szFileName;
		bool bLooped;
		NAudioBackend::TStreamCallback pEndCallback;
		void *pUserData;
		std::vector<char> encodedData;
		unsigned int nSampleRate;
		unsigned int nChannels;
		unsigned int nBlockAlign;
		bool bUseXiphDecoder;
	};

	const int cMaxOpenChannels = 128;
	SOpenChannel g_channels[cMaxOpenChannels];
	int g_nNextChannel = 0;
	float g_fDistanceFactor = 1.0f;
	float g_fRolloffFactor = 1.0f;

	float ClampFloat( float fValue, float fMin, float fMax )
	{
		if ( fValue < fMin )
			return fMin;
		if ( fValue > fMax )
			return fMax;
		return fValue;
	}

	ma_result XiphDataSourceRead( ma_data_source *pDataSource, void *pFramesOut, ma_uint64 nFrameCount, ma_uint64 *pFramesRead )
	{
		SXiphStreamDataSource *pXiph = static_cast<SXiphStreamDataSource*>( pDataSource );
		if ( pFramesRead )
			*pFramesRead = 0;
		if ( !pXiph || !pXiph->pVorbisStream || pXiph->nBlockAlign == 0 )
			return MA_INVALID_ARGS;

		const ma_uint64 nBytesToRead = nFrameCount * pXiph->nBlockAlign;
		if ( nBytesToRead == 0 )
			return MA_SUCCESS;

		if ( !pFramesOut )
		{
			const unsigned long long nTargetFrame = TellXiphVorbisStream( pXiph->pVorbisStream ) + nFrameCount;
			if ( !SeekXiphVorbisStream( pXiph->pVorbisStream, nTargetFrame ) )
				return MA_ERROR;
			if ( pFramesRead )
				*pFramesRead = nFrameCount;
			return MA_SUCCESS;
		}

		const long nBytesRead = ReadXiphVorbisStream( pXiph->pVorbisStream, static_cast<char*>( pFramesOut ), static_cast<long>( nBytesToRead ) );
		if ( nBytesRead < 0 )
			return MA_ERROR;
		if ( pFramesRead )
			*pFramesRead = nBytesRead / pXiph->nBlockAlign;
		return nBytesRead > 0 ? MA_SUCCESS : MA_AT_END;
	}

	ma_result XiphDataSourceSeek( ma_data_source *pDataSource, ma_uint64 nFrameIndex )
	{
		SXiphStreamDataSource *pXiph = static_cast<SXiphStreamDataSource*>( pDataSource );
		if ( !pXiph || !pXiph->pVorbisStream )
			return MA_INVALID_ARGS;
		return SeekXiphVorbisStream( pXiph->pVorbisStream, nFrameIndex ) ? MA_SUCCESS : MA_ERROR;
	}

	ma_result XiphDataSourceGetDataFormat( ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t nChannelMapCap )
	{
		SXiphStreamDataSource *pXiph = static_cast<SXiphStreamDataSource*>( pDataSource );
		if ( !pXiph )
			return MA_INVALID_ARGS;
		if ( pFormat )
			*pFormat = ma_format_s16;
		if ( pChannels )
			*pChannels = pXiph->nChannels;
		if ( pSampleRate )
			*pSampleRate = pXiph->nSampleRate;
		if ( pChannelMap && nChannelMapCap > 0 )
			ma_channel_map_init_standard( ma_standard_channel_map_default, pChannelMap, nChannelMapCap, pXiph->nChannels );
		return MA_SUCCESS;
	}

	ma_result XiphDataSourceGetCursor( ma_data_source *pDataSource, ma_uint64 *pCursor )
	{
		SXiphStreamDataSource *pXiph = static_cast<SXiphStreamDataSource*>( pDataSource );
		if ( !pXiph || !pXiph->pVorbisStream || !pCursor )
			return MA_INVALID_ARGS;
		*pCursor = TellXiphVorbisStream( pXiph->pVorbisStream );
		return MA_SUCCESS;
	}

	ma_result XiphDataSourceGetLength( ma_data_source *pDataSource, ma_uint64 *pLength )
	{
		SXiphStreamDataSource *pXiph = static_cast<SXiphStreamDataSource*>( pDataSource );
		if ( !pXiph || !pLength )
			return MA_INVALID_ARGS;
		*pLength = pXiph->nTotalFrames;
		return MA_SUCCESS;
	}

	ma_data_source_vtable g_xiphDataSourceVTable =
	{
		XiphDataSourceRead,
		XiphDataSourceSeek,
		XiphDataSourceGetDataFormat,
		XiphDataSourceGetCursor,
		XiphDataSourceGetLength,
		0
	};

	void ResetChannel( int nChannel )
	{
		if ( nChannel < 0 || nChannel >= cMaxOpenChannels )
			return;

		if ( g_channels[nChannel].bSoundInitialized )
		{
			ma_sound_uninit( &g_channels[nChannel].sound );
			g_channels[nChannel].bSoundInitialized = false;
		}
		if ( g_channels[nChannel].bBufferInitialized )
		{
			ma_audio_buffer_uninit( &g_channels[nChannel].buffer );
			g_channels[nChannel].bBufferInitialized = false;
		}
		if ( g_channels[nChannel].bDecoderInitialized )
		{
			ma_decoder_uninit( &g_channels[nChannel].decoder );
			g_channels[nChannel].bDecoderInitialized = false;
		}
		if ( g_channels[nChannel].bXiphDataSourceInitialized )
		{
			ma_data_source_uninit( &g_channels[nChannel].xiphDataSource.base );
			CloseXiphVorbisStream( g_channels[nChannel].xiphDataSource.pVorbisStream );
			memset( &g_channels[nChannel].xiphDataSource, 0, sizeof( g_channels[nChannel].xiphDataSource ) );
			g_channels[nChannel].bXiphDataSourceInitialized = false;
		}
		g_channels[nChannel].pSample = 0;
		g_channels[nChannel].pStream = 0;
		g_channels[nChannel].fBaseVolume = 1.0f;
		g_channels[nChannel].fDistanceVolume = 1.0f;
		g_channels[nChannel].fPan = 0.0f;
	}

	void ApplyChannelMix( int nChannel )
	{
		if ( nChannel < 0 || nChannel >= cMaxOpenChannels || !g_channels[nChannel].bSoundInitialized )
			return;

		ma_sound_set_volume( &g_channels[nChannel].sound, g_channels[nChannel].fBaseVolume * g_channels[nChannel].fDistanceVolume );
		ma_sound_set_pan( &g_channels[nChannel].sound, g_channels[nChannel].fPan );
	}

	float CalculateDistanceVolume( const SOpenSample *pSample, const CVec3 &vPos )
	{
		if ( !pSample )
			return 1.0f;

		const float fMinDistance = Max( pSample->fMinDistance, 1.0f );
		const float fDistance = fabs( vPos ) * Max( g_fDistanceFactor, 0.0f );
		if ( fDistance <= fMinDistance )
			return 1.0f;

		const float fRolloff = Max( g_fRolloffFactor, 0.01f );
		const float fAttenuatedDistance = fMinDistance + (fDistance - fMinDistance) * fRolloff;
		return ClampFloat( fMinDistance / fAttenuatedDistance, 0.0f, 1.0f );
	}

	float CalculatePan( const CVec3 &vPos )
	{
		const float fDistance = fabsxy( vPos );
		if ( fDistance <= 0.001f )
			return 0.0f;

		return ClampFloat( vPos.x / fDistance, -1.0f, 1.0f );
	}

	void OpenStreamEndCallback( void *pUserData, ma_sound *pSound )
	{
		SOpenStream *pStream = static_cast<SOpenStream*>( pUserData );
		if ( pStream && pStream->pEndCallback )
			pStream->pEndCallback( pStream, 0, 0, pStream->pUserData );
	}

	int FindFreeChannel()
	{
		for ( int i = 0; i < cMaxOpenChannels; ++i )
		{
			const int nChannel = (g_nNextChannel + i) % cMaxOpenChannels;
			if ( !g_channels[nChannel].bSoundInitialized || ma_sound_at_end( &g_channels[nChannel].sound ) )
			{
				ResetChannel( nChannel );
				g_nNextChannel = (nChannel + 1) % cMaxOpenChannels;
				return nChannel;
			}
		}
		return -1;
	}

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
		pSample->format = ma_format_unknown;
		pSample->bBufferInitialized = false;
	}

	ma_format GetSampleFormat( unsigned int nBitsPerSample )
	{
		switch ( nBitsPerSample )
		{
			case 8:
				return ma_format_u8;
			case 16:
				return ma_format_s16;
			case 32:
				return ma_format_s32;
			default:
				return ma_format_unknown;
		}
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

		if ( !(bHaveFormat && bHaveData && nAudioFormat == 1 &&
			pSample->nSampleRate > 0 &&
			pSample->nChannels > 0 &&
			pSample->nBlockAlign > 0 &&
			pSample->nPcmBytes > 0 &&
			(pSample->nPcmBytes % pSample->nBlockAlign) == 0 &&
			pSample->nBitsPerSample > 0) )
		{
			return false;
		}

		pSample->format = GetSampleFormat( pSample->nBitsPerSample );
		if ( pSample->format == ma_format_unknown )
			return false;
		if ( pSample->pcmData.empty() )
			return false;

		ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
			pSample->format,
			pSample->nChannels,
			pSample->nPcmBytes / pSample->nBlockAlign,
			&pSample->pcmData[0],
			0 );
		pSample->bBufferInitialized = ma_audio_buffer_init( &bufferConfig, &pSample->buffer ) == MA_SUCCESS;
		return pSample->bBufferInitialized;
	}

	bool ReadWholeStream( IDataStream *pDataStream, std::vector<char> *pData )
	{
		if ( !pDataStream || !pData )
			return false;

		const int nSize = pDataStream->GetSize();
		if ( nSize <= 0 )
			return false;

		pData->resize( nSize );
		return pDataStream->Read( &(*pData)[0], nSize ) == nSize;
	}

	bool LoadStreamData( SOpenStream *pOpenStream )
	{
		if ( !pOpenStream )
			return false;

		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		if ( !pStorage )
			return false;

		if ( CPtr<IDataStream> pDataStream = pStorage->OpenStream( pOpenStream->szFileName.c_str(), STREAM_ACCESS_READ ) )
			return ReadWholeStream( pDataStream, &pOpenStream->encodedData );

		const char *pszStorageName = pStorage->GetName();
		if ( pszStorageName && *pszStorageName )
		{
			const std::string szStorageName = pszStorageName;
			if ( pOpenStream->szFileName.size() > szStorageName.size() &&
				_strnicmp( pOpenStream->szFileName.c_str(), szStorageName.c_str(), szStorageName.size() ) == 0 )
			{
				const std::string szRelativeName = pOpenStream->szFileName.substr( szStorageName.size() );
				if ( CPtr<IDataStream> pRelativeStream = pStorage->OpenStream( szRelativeName.c_str(), STREAM_ACCESS_READ ) )
					return ReadWholeStream( pRelativeStream, &pOpenStream->encodedData );
			}
		}

		return false;
	}

	bool CanDecodeStreamData( const std::vector<char> &encodedData )
	{
		if ( encodedData.empty() )
			return false;

		ma_decoder decoder;
		if ( ma_decoder_init_memory( &encodedData[0], encodedData.size(), 0, &decoder ) != MA_SUCCESS )
			return false;

		ma_decoder_uninit( &decoder );
		return true;
	}

	bool CanDecodeStreamWithXiph( SOpenStream *pOpenStream )
	{
		if ( !pOpenStream || pOpenStream->encodedData.empty() )
			return false;

		SXiphVorbisStream *pVorbisStream = 0;
		if ( !OpenXiphVorbisStreamMemory( &pOpenStream->encodedData[0], static_cast<int>( pOpenStream->encodedData.size() ), &pVorbisStream ) )
			return false;

		pOpenStream->nSampleRate = GetXiphVorbisStreamSampleRate( pVorbisStream );
		pOpenStream->nChannels = GetXiphVorbisStreamChannels( pVorbisStream );
		pOpenStream->nBlockAlign = GetXiphVorbisStreamBlockAlign( pVorbisStream );
		pOpenStream->bUseXiphDecoder = pOpenStream->nSampleRate > 0 && pOpenStream->nChannels > 0 && pOpenStream->nBlockAlign > 0;
		CloseXiphVorbisStream( pVorbisStream );
		if ( !pOpenStream->bUseXiphDecoder )
		{
			return false;
		}
		return true;
	}

	void TraceOpenStream( const char *pszStatus, const SOpenStream *pOpenStream )
	{
		const int nBytes = pOpenStream ? static_cast<int>( pOpenStream->encodedData.size() ) : 0;
		OutputDebugString( NStr::Format( "Open audio stream %s: %s (%d bytes)\n",
																		 pszStatus,
																		 pOpenStream ? pOpenStream->szFileName.c_str() : "",
																		 nBytes ) );
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

		if ( output == SFX_OUTPUT_NO )
			return true;

		if ( g_bEngineInitialized )
			return true;

		ma_engine_config engineConfig = ma_engine_config_init();
		engineConfig.sampleRate = nMixRate > 0 ? nMixRate : 0;
		if ( ma_engine_init( &engineConfig, &g_engine ) != MA_SUCCESS )
		{
			OutputDebugString( "SFX open audio backend failed to initialize miniaudio\n" );
			return false;
		}

		g_bEngineInitialized = true;
		OutputDebugString( "SFX open audio backend initialized miniaudio\n" );
		return true;
	}

	void CloseDevice()
	{
		for ( int i = 0; i < cMaxOpenChannels; ++i )
			ResetChannel( i );

		if ( g_bEngineInitialized )
		{
			ma_engine_uninit( &g_engine );
			g_bEngineInitialized = false;
		}
	}

	void DebugTraceMixer()
	{
		OutputDebugString( "SFX open audio miniaudio backend\n" );
	}

	void SetDistanceFactor( float fFactor )
	{
		g_fDistanceFactor = Max( fFactor, 0.0f );
	}

	void SetRolloffFactor( float fFactor )
	{
		g_fRolloffFactor = ClampFloat( fFactor, 0.0f, 10.0f );
	}

	void FreeSample( void *pSample )
	{
		SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
		if ( pOpenSample )
		{
			if ( pOpenSample->bBufferInitialized )
				ma_audio_buffer_uninit( &pOpenSample->buffer );
			delete pOpenSample;
		}
	}

	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode )
	{
		SOpenSample *pSample = new SOpenSample;
		InitializeEmptySample( pSample, nMode );
		if ( !ParseWaveSample( pData, nSize, pSample ) )
		{
			delete pSample;
			return 0;
		}
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
		return nChannel >= 0 && nChannel < cMaxOpenChannels &&
			g_channels[nChannel].bSoundInitialized &&
			g_channels[nChannel].pSample == pSample &&
			ma_sound_is_playing( &g_channels[nChannel].sound ) != 0;
	}

	int PlaySample( void *pSample )
	{
		const int nChannel = PlaySamplePaused( pSample );
		if ( nChannel != -1 )
			SetChannelPaused( nChannel, false );
		return nChannel;
	}

	int PlaySamplePaused( void *pSample )
	{
		SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
		if ( !g_bEngineInitialized || !pOpenSample || !pOpenSample->bBufferInitialized )
			return -1;

		const int nChannel = FindFreeChannel();
		if ( nChannel == -1 )
			return -1;

		ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
			pOpenSample->format,
			pOpenSample->nChannels,
			pOpenSample->nPcmBytes / pOpenSample->nBlockAlign,
			&pOpenSample->pcmData[0],
			0 );
		if ( ma_audio_buffer_init( &bufferConfig, &g_channels[nChannel].buffer ) != MA_SUCCESS )
			return -1;
		g_channels[nChannel].bBufferInitialized = true;

		if ( ma_sound_init_from_data_source( &g_engine, &g_channels[nChannel].buffer, 0, 0, &g_channels[nChannel].sound ) != MA_SUCCESS )
		{
			ResetChannel( nChannel );
			return -1;
		}

		g_channels[nChannel].bSoundInitialized = true;
		g_channels[nChannel].pSample = pOpenSample;
		g_channels[nChannel].fBaseVolume = 1.0f;
		g_channels[nChannel].fDistanceVolume = 1.0f;
		g_channels[nChannel].fPan = 0.0f;
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenSample->bLooped ? MA_TRUE : MA_FALSE );
		ApplyChannelMix( nChannel );
		return nChannel;
	}

	void SetChannelVolume( int nChannel, int nVolume )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			g_channels[nChannel].fBaseVolume = ClampFloat( static_cast<float>( nVolume ) / 255.0f, 0.0f, 1.0f );
			ApplyChannelMix( nChannel );
		}
	}

	void SetChannelPan( int nChannel, int nPan )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			g_channels[nChannel].fPan = ClampFloat( (static_cast<float>( nPan ) - 128.0f) / 128.0f, -1.0f, 1.0f );
			ApplyChannelMix( nChannel );
		}
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			if ( bPaused )
				ma_sound_stop( &g_channels[nChannel].sound );
			else
				ma_sound_start( &g_channels[nChannel].sound );
		}
	}

	void StopChannel( int nChannel )
	{
		ResetChannel( nChannel );
	}

	bool IsChannelPlaying( int nChannel )
	{
		return nChannel >= 0 && nChannel < cMaxOpenChannels &&
			g_channels[nChannel].bSoundInitialized &&
			ma_sound_is_playing( &g_channels[nChannel].sound ) != 0;
	}

	int GetChannelsPlaying()
	{
		int nPlaying = 0;
		for ( int i = 0; i < cMaxOpenChannels; ++i )
			if ( IsChannelPlaying( i ) )
				++nPlaying;
		return nPlaying;
	}

	unsigned int GetChannelPosition( int nChannel )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			ma_uint64 nCursor = 0;
			if ( ma_sound_get_cursor_in_pcm_frames( &g_channels[nChannel].sound, &nCursor ) == MA_SUCCESS )
				return static_cast<unsigned int>( nCursor );
		}
		return 0;
	}

	void SetChannelPosition( int nChannel, unsigned int nPosition )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
			ma_sound_seek_to_pcm_frame( &g_channels[nChannel].sound, nPosition );
	}

	int GetLastError()
	{
		return 0;
	}

	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			g_channels[nChannel].fDistanceVolume = CalculateDistanceVolume( g_channels[nChannel].pSample, vPos );
			g_channels[nChannel].fPan = CalculatePan( vPos );
			ApplyChannelMix( nChannel );
		}
	}

	void* OpenStream( const char *pszFileName, bool bLooped )
	{
		SOpenStream *pStream = new SOpenStream;
		pStream->szFileName = pszFileName ? pszFileName : "";
		pStream->bLooped = bLooped;
		pStream->pEndCallback = 0;
		pStream->pUserData = 0;
		pStream->nSampleRate = 0;
		pStream->nChannels = 0;
		pStream->nBlockAlign = 0;
		pStream->bUseXiphDecoder = false;
		if ( !LoadStreamData( pStream ) )
		{
			TraceOpenStream( "open failed", pStream );
			delete pStream;
			return 0;
		}
		if ( !CanDecodeStreamData( pStream->encodedData ) )
		{
			if ( !CanDecodeStreamWithXiph( pStream ) )
			{
				TraceOpenStream( "decode failed", pStream );
				delete pStream;
				return 0;
			}
			TraceOpenStream( "opened xiph", pStream );
			return pStream;
		}
		TraceOpenStream( "opened", pStream );
		return pStream;
	}

	void CloseStream( void *pStream )
	{
		SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
		if ( pOpenStream )
		{
			for ( int i = 0; i < cMaxOpenChannels; ++i )
				if ( g_channels[i].pStream == pOpenStream )
					ResetChannel( i );
			delete pOpenStream;
		}
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
		SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
		if ( !g_bEngineInitialized || !pOpenStream )
			return -1;

		const int nChannel = FindFreeChannel();
		if ( nChannel == -1 )
			return -1;

		if ( pOpenStream->encodedData.empty() )
			return -1;

		if ( pOpenStream->bUseXiphDecoder )
		{
			if ( pOpenStream->nBlockAlign == 0 )
				return -1;

			SXiphVorbisStream *pVorbisStream = 0;
			if ( !OpenXiphVorbisStreamMemory( &pOpenStream->encodedData[0], static_cast<int>( pOpenStream->encodedData.size() ), &pVorbisStream ) )
				return -1;

			ma_data_source_config dataSourceConfig = ma_data_source_config_init();
			dataSourceConfig.vtable = &g_xiphDataSourceVTable;
			memset( &g_channels[nChannel].xiphDataSource, 0, sizeof( g_channels[nChannel].xiphDataSource ) );
			g_channels[nChannel].xiphDataSource.pVorbisStream = pVorbisStream;
			g_channels[nChannel].xiphDataSource.nSampleRate = GetXiphVorbisStreamSampleRate( pVorbisStream );
			g_channels[nChannel].xiphDataSource.nChannels = GetXiphVorbisStreamChannels( pVorbisStream );
			g_channels[nChannel].xiphDataSource.nBlockAlign = GetXiphVorbisStreamBlockAlign( pVorbisStream );
			g_channels[nChannel].xiphDataSource.nTotalFrames = GetXiphVorbisStreamLength( pVorbisStream );
			if ( ma_data_source_init( &dataSourceConfig, &g_channels[nChannel].xiphDataSource.base ) != MA_SUCCESS )
			{
				CloseXiphVorbisStream( pVorbisStream );
				memset( &g_channels[nChannel].xiphDataSource, 0, sizeof( g_channels[nChannel].xiphDataSource ) );
				return -1;
			}
			g_channels[nChannel].bXiphDataSourceInitialized = true;

			const ma_uint32 nFlags = pOpenStream->bLooped ? MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_LOOPING : MA_SOUND_FLAG_STREAM;
			if ( ma_sound_init_from_data_source( &g_engine, &g_channels[nChannel].xiphDataSource.base, nFlags, 0, &g_channels[nChannel].sound ) != MA_SUCCESS )
			{
				TraceOpenStream( "sound init failed", pOpenStream );
				ResetChannel( nChannel );
				return -1;
			}
		}
		else
		{
			const ma_uint32 nFlags = pOpenStream->bLooped ? MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_LOOPING : MA_SOUND_FLAG_STREAM;
			if ( ma_decoder_init_memory( &pOpenStream->encodedData[0], pOpenStream->encodedData.size(), 0, &g_channels[nChannel].decoder ) != MA_SUCCESS )
			{
				TraceOpenStream( "decode failed", pOpenStream );
				return -1;
			}
			g_channels[nChannel].bDecoderInitialized = true;

			if ( ma_sound_init_from_data_source( &g_engine, &g_channels[nChannel].decoder, nFlags, 0, &g_channels[nChannel].sound ) != MA_SUCCESS )
			{
				TraceOpenStream( "sound init failed", pOpenStream );
				ResetChannel( nChannel );
				return -1;
			}
		}

		g_channels[nChannel].bSoundInitialized = true;
		g_channels[nChannel].pStream = pOpenStream;
		g_channels[nChannel].fBaseVolume = 1.0f;
		g_channels[nChannel].fDistanceVolume = 1.0f;
		g_channels[nChannel].fPan = 0.0f;
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenStream->bLooped ? MA_TRUE : MA_FALSE );
		ApplyChannelMix( nChannel );
		ma_sound_set_end_callback( &g_channels[nChannel].sound, OpenStreamEndCallback, pOpenStream );
		ma_sound_start( &g_channels[nChannel].sound );
		TraceOpenStream( "started", pOpenStream );
		return nChannel;
	}

	void SetStreamChannelPan( int nChannel )
	{
		SetChannelPan( nChannel, 128 );
	}
}

#endif // defined(SFX_USE_OPEN_AUDIO_BACKEND)
