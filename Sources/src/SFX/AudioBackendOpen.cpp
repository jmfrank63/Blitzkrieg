#include "StdAfx.h"

#include "AudioBackendImpl.h"
#include "AudioBackendXiphVorbis.h"
#include "../Platform/Clock.h"
#include "../Platform/Debug.h"

#include <cstdlib>

#if defined(SFX_USE_OPEN_AUDIO_BACKEND)

#define STB_VORBIS_HEADER_ONLY
#include "../../sdk/stb/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "../../sdk/miniaudio/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "../../sdk/stb/stb_vorbis.c"

namespace
{
	void* AudioAllocMalloc( size_t sz, void *pUserData )
	{
		(void)pUserData;
		return std::malloc( sz == 0 ? 1 : sz );
	}

	void* AudioAllocRealloc( void *p, size_t sz, void *pUserData )
	{
		(void)pUserData;
		return std::realloc( p, sz == 0 ? 1 : sz );
	}

	void AudioAllocFree( void *p, void *pUserData )
	{
		(void)pUserData;
		std::free( p );
	}

	ma_context g_context;
	ma_engine g_engine;
	bool g_bContextInitialized = false;
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
		float fUserPan;
		float f3DPan;
		bool bUse3DPan;
		bool bPaused;
		unsigned int nPausedPosition;
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

	const char* GetOpenAudioResultName( ma_result result )
	{
		switch ( result )
		{
		case MA_SUCCESS:
			return "MA_SUCCESS";
		case MA_ERROR:
			return "MA_ERROR";
		case MA_INVALID_ARGS:
			return "MA_INVALID_ARGS";
		case MA_INVALID_OPERATION:
			return "MA_INVALID_OPERATION";
		case MA_OUT_OF_MEMORY:
			return "MA_OUT_OF_MEMORY";
		case MA_ACCESS_DENIED:
			return "MA_ACCESS_DENIED";
		case MA_DOES_NOT_EXIST:
			return "MA_DOES_NOT_EXIST";
		case MA_ALREADY_EXISTS:
			return "MA_ALREADY_EXISTS";
		case MA_TOO_MANY_OPEN_FILES:
			return "MA_TOO_MANY_OPEN_FILES";
		case MA_INVALID_FILE:
			return "MA_INVALID_FILE";
		case MA_TOO_BIG:
			return "MA_TOO_BIG";
		case MA_PATH_TOO_LONG:
			return "MA_PATH_TOO_LONG";
		case MA_NAME_TOO_LONG:
			return "MA_NAME_TOO_LONG";
		case MA_NOT_DIRECTORY:
			return "MA_NOT_DIRECTORY";
		case MA_IS_DIRECTORY:
			return "MA_IS_DIRECTORY";
		case MA_DIRECTORY_NOT_EMPTY:
			return "MA_DIRECTORY_NOT_EMPTY";
		case MA_AT_END:
			return "MA_AT_END";
		case MA_NO_SPACE:
			return "MA_NO_SPACE";
		case MA_BUSY:
			return "MA_BUSY";
		case MA_IO_ERROR:
			return "MA_IO_ERROR";
		case MA_INTERRUPT:
			return "MA_INTERRUPT";
		case MA_UNAVAILABLE:
			return "MA_UNAVAILABLE";
		case MA_ALREADY_IN_USE:
			return "MA_ALREADY_IN_USE";
		case MA_BAD_ADDRESS:
			return "MA_BAD_ADDRESS";
		case MA_BAD_SEEK:
			return "MA_BAD_SEEK";
		case MA_BAD_PIPE:
			return "MA_BAD_PIPE";
		case MA_DEADLOCK:
			return "MA_DEADLOCK";
		case MA_TOO_MANY_LINKS:
			return "MA_TOO_MANY_LINKS";
		case MA_NOT_IMPLEMENTED:
			return "MA_NOT_IMPLEMENTED";
		case MA_NO_MESSAGE:
			return "MA_NO_MESSAGE";
		case MA_BAD_MESSAGE:
			return "MA_BAD_MESSAGE";
		case MA_NO_DATA_AVAILABLE:
			return "MA_NO_DATA_AVAILABLE";
		case MA_INVALID_DATA:
			return "MA_INVALID_DATA";
		case MA_TIMEOUT:
			return "MA_TIMEOUT";
		case MA_NO_NETWORK:
			return "MA_NO_NETWORK";
		case MA_NOT_UNIQUE:
			return "MA_NOT_UNIQUE";
		case MA_NOT_SOCKET:
			return "MA_NOT_SOCKET";
		case MA_NO_ADDRESS:
			return "MA_NO_ADDRESS";
		case MA_BAD_PROTOCOL:
			return "MA_BAD_PROTOCOL";
		case MA_PROTOCOL_UNAVAILABLE:
			return "MA_PROTOCOL_UNAVAILABLE";
		case MA_PROTOCOL_NOT_SUPPORTED:
			return "MA_PROTOCOL_NOT_SUPPORTED";
		case MA_PROTOCOL_FAMILY_NOT_SUPPORTED:
			return "MA_PROTOCOL_FAMILY_NOT_SUPPORTED";
		case MA_ADDRESS_FAMILY_NOT_SUPPORTED:
			return "MA_ADDRESS_FAMILY_NOT_SUPPORTED";
		case MA_SOCKET_NOT_SUPPORTED:
			return "MA_SOCKET_NOT_SUPPORTED";
		case MA_CONNECTION_RESET:
			return "MA_CONNECTION_RESET";
		case MA_ALREADY_CONNECTED:
			return "MA_ALREADY_CONNECTED";
		case MA_NOT_CONNECTED:
			return "MA_NOT_CONNECTED";
		case MA_CONNECTION_REFUSED:
			return "MA_CONNECTION_REFUSED";
		case MA_NO_HOST:
			return "MA_NO_HOST";
		case MA_IN_PROGRESS:
			return "MA_IN_PROGRESS";
		case MA_CANCELLED:
			return "MA_CANCELLED";
		case MA_MEMORY_ALREADY_MAPPED:
			return "MA_MEMORY_ALREADY_MAPPED";
		case MA_CRC_MISMATCH:
			return "MA_CRC_MISMATCH";
		case MA_FORMAT_NOT_SUPPORTED:
			return "MA_FORMAT_NOT_SUPPORTED";
		case MA_DEVICE_TYPE_NOT_SUPPORTED:
			return "MA_DEVICE_TYPE_NOT_SUPPORTED";
		case MA_SHARE_MODE_NOT_SUPPORTED:
			return "MA_SHARE_MODE_NOT_SUPPORTED";
		case MA_NO_BACKEND:
			return "MA_NO_BACKEND";
		case MA_NO_DEVICE:
			return "MA_NO_DEVICE";
		case MA_API_NOT_FOUND:
			return "MA_API_NOT_FOUND";
		case MA_INVALID_DEVICE_CONFIG:
			return "MA_INVALID_DEVICE_CONFIG";
		case MA_LOOP:
			return "MA_LOOP";
		default:
			return "MA_UNKNOWN";
		}
	}

	void TraceOpenAudioResult( const char *pszAction, ma_result result )
	{
		NPlatform::DebugWriteFormat( "SFX open audio %s: %s (%d)\n", pszAction, GetOpenAudioResultName( result ), result );
	}

	void TraceOpenAudioDevice()
	{
		ma_device *pDevice = ma_engine_get_device( &g_engine );
		if ( !pDevice || !pDevice->pContext )
		{
		NPlatform::DebugWrite( "SFX open audio initialized without playback device\n" );
			return;
		}

		char szDeviceName[MA_MAX_DEVICE_NAME_LENGTH + 1];
		szDeviceName[0] = 0;
		ma_device_get_name( pDevice, ma_device_type_playback, szDeviceName, sizeof( szDeviceName ), 0 );

		NPlatform::DebugWriteFormat( "SFX open audio device: backend=%s, device=\"%s\", sampleRate=%u, channels=%u\n",
			ma_get_backend_name( pDevice->pContext->backend ),
			szDeviceName,
			pDevice->sampleRate,
			pDevice->playback.channels );
	}

	// Keep disabled by default: per-read tracing runs on the mixer thread and
	// can add avoidable pressure during load spikes.
	#ifndef SFX_ENABLE_XIPH_READ_TRACE
	#define SFX_ENABLE_XIPH_READ_TRACE 0
	#endif

	#if SFX_ENABLE_XIPH_READ_TRACE
	struct SXiphReadTrace { double tStartMs; float fDurMs; unsigned int nReq; unsigned int nGot; };
	const int cXiphReadTraceCapacity = 8192;
	SXiphReadTrace g_xiphReadTrace[cXiphReadTraceCapacity];
	long g_nXiphReadTraceCount = 0;

	double XiphTraceNowMs()
	{
		return static_cast<double>( NPlatform::MonotonicNanoseconds() ) / 1000000.0;
	}

	void DumpXiphReadTrace( const char *pszReason )
	{
		const long nCount = g_nXiphReadTraceCount;
		if ( nCount == 0 )
			return;
		FILE *pFile = fopen( "sfx_trace.log", "ab" );
		if ( !pFile )
			return;
		fprintf( pFile, "=== xiph read trace (%s): %ld reads ===\n", pszReason, nCount );
		double tPrev = g_xiphReadTrace[0].tStartMs;
		for ( long i = 0; i < nCount && i < cXiphReadTraceCapacity; ++i )
		{
			const SXiphReadTrace &entry = g_xiphReadTrace[i];
			fprintf( pFile, "t=%.2f gap=%.2f dur=%.2f req=%u got=%u\n",
			         entry.tStartMs, entry.tStartMs - tPrev, entry.fDurMs, entry.nReq, entry.nGot );
			tPrev = entry.tStartMs;
		}
		fclose( pFile );
		g_nXiphReadTraceCount = 0;
	}
	#endif

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

		#if SFX_ENABLE_XIPH_READ_TRACE
		const double tStart = XiphTraceNowMs();
		#endif
		const long nBytesRead = ReadXiphVorbisStream( pXiph->pVorbisStream, static_cast<char*>( pFramesOut ), static_cast<long>( nBytesToRead ) );
		#if SFX_ENABLE_XIPH_READ_TRACE
		if ( g_nXiphReadTraceCount < cXiphReadTraceCapacity )
		{
			SXiphReadTrace &entry = g_xiphReadTrace[g_nXiphReadTraceCount];
			entry.tStartMs = tStart;
			entry.fDurMs = static_cast<float>( XiphTraceNowMs() - tStart );
			entry.nReq = static_cast<unsigned int>( nFrameCount );
			entry.nGot = nBytesRead > 0 ? static_cast<unsigned int>( nBytesRead / pXiph->nBlockAlign ) : 0;
			++g_nXiphReadTraceCount;
		}
		#endif
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
			ma_sound_stop( &g_channels[nChannel].sound );
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
		g_channels[nChannel].fUserPan = 0.0f;
		g_channels[nChannel].f3DPan = 0.0f;
		g_channels[nChannel].bUse3DPan = false;
		g_channels[nChannel].bPaused = false;
		g_channels[nChannel].nPausedPosition = 0;
	}

	float ChannelTargetVolume( int nChannel )
	{
		return g_channels[nChannel].fBaseVolume * g_channels[nChannel].fDistanceVolume;
	}

	// Volume changes ride the sound's FADER (short chase ramp), never an
	// instant ma_sound_set_volume: the engine updates volumes once per main-
	// loop tick, and when that thread is busy (menu init after the intro
	// video, save-load storms) the ticks are 100-500ms apart — instant steps
	// of a fading music stream then zipper audibly ("stuttering"). The fader
	// runs on the mixer thread, so a 40ms ramp per update stays smooth no
	// matter how coarse the updates are. volumeBeg -1 = chase from current.
	void ApplyChannelMix( int nChannel )
	{
		if ( nChannel < 0 || nChannel >= cMaxOpenChannels || !g_channels[nChannel].bSoundInitialized )
			return;

		ma_sound_set_fade_in_milliseconds( &g_channels[nChannel].sound, -1.0f, ChannelTargetVolume( nChannel ), 40 );
		ma_sound_set_pan( &g_channels[nChannel].sound, g_channels[nChannel].bUse3DPan ? g_channels[nChannel].f3DPan : g_channels[nChannel].fUserPan );
	}

	// For sound STARTS: the fader must sit at the target before the first
	// frame (a chase from the default 1.0 would blip one-shots louder than
	// their mix volume).
	void ApplyChannelMixInstant( int nChannel )
	{
		if ( nChannel < 0 || nChannel >= cMaxOpenChannels || !g_channels[nChannel].bSoundInitialized )
			return;

		const float fTarget = ChannelTargetVolume( nChannel );
		ma_sound_set_fade_in_milliseconds( &g_channels[nChannel].sound, fTarget, fTarget, 0 );
		ma_sound_set_pan( &g_channels[nChannel].sound, g_channels[nChannel].bUse3DPan ? g_channels[nChannel].f3DPan : g_channels[nChannel].fUserPan );
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

	unsigned int GetSampleFrameCount( const SOpenSample *pSample )
	{
		if ( !pSample || pSample->nBlockAlign == 0 )
			return 0;
		return pSample->nPcmBytes / pSample->nBlockAlign;
	}

	void ApplySampleLoopPoints( SOpenChannel *pChannel, const SOpenSample *pSample )
	{
		if ( !pChannel || !pSample || !pChannel->bSoundInitialized )
			return;

		if ( pSample->nLoopEnd <= pSample->nLoopStart )
			return;

		const unsigned int nLength = GetSampleFrameCount( pSample );
		if ( nLength == 0 )
			return;

		const unsigned int nLoopStart = Clamp( pSample->nLoopStart, 0, static_cast<int>( nLength - 1 ) );
		const unsigned int nLoopEnd = Clamp( pSample->nLoopEnd, static_cast<int>( nLoopStart + 1 ), static_cast<int>( nLength ) );
		ma_data_source_set_loop_point_in_pcm_frames( ma_sound_get_data_source( &pChannel->sound ), nLoopStart, nLoopEnd );
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

	bool InitializeSampleBuffer( SOpenSample *pSample )
	{
		if ( !pSample || pSample->format == ma_format_unknown ||
			pSample->nChannels == 0 ||
			pSample->nBlockAlign == 0 ||
			pSample->nPcmBytes == 0 ||
			(pSample->nPcmBytes % pSample->nBlockAlign) != 0 ||
			pSample->pcmData.empty() )
		{
			return false;
		}

		ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
			pSample->format,
			pSample->nChannels,
			pSample->nPcmBytes / pSample->nBlockAlign,
			&pSample->pcmData[0],
			0 );
		pSample->bBufferInitialized = ma_audio_buffer_init( &bufferConfig, &pSample->buffer ) == MA_SUCCESS;
		return pSample->bBufferInitialized;
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

		return InitializeSampleBuffer( pSample );
	}

	bool DecodeSampleWithMiniAudio( const char *pData, int nSize, SOpenSample *pSample )
	{
		if ( !pData || nSize <= 0 || !pSample )
			return false;

		ma_decoder_config decoderConfig = ma_decoder_config_init( ma_format_s16, 0, 0 );
		ma_decoder decoder;
		if ( ma_decoder_init_memory( pData, static_cast<size_t>( nSize ), &decoderConfig, &decoder ) != MA_SUCCESS )
			return false;

		ma_uint64 nFrames = 0;
		if ( ma_decoder_get_length_in_pcm_frames( &decoder, &nFrames ) != MA_SUCCESS ||
			nFrames == 0 ||
			decoder.outputChannels == 0 ||
			decoder.outputSampleRate == 0 )
		{
			ma_decoder_uninit( &decoder );
			return false;
		}

		const unsigned int nDecodedChannels = decoder.outputChannels;
		const unsigned int nDecodedSampleRate = decoder.outputSampleRate;
		const unsigned int nBlockAlign = nDecodedChannels * sizeof( ma_int16 );
		if ( nBlockAlign == 0 || nFrames > (0xFFFFFFFFull / nBlockAlign) )
		{
			ma_decoder_uninit( &decoder );
			return false;
		}

		const unsigned int nPcmBytes = static_cast<unsigned int>( nFrames * nBlockAlign );
		pSample->pcmData.resize( nPcmBytes );

		ma_uint64 nFramesRead = 0;
		const ma_result readResult = ma_decoder_read_pcm_frames( &decoder, &pSample->pcmData[0], nFrames, &nFramesRead );
		ma_decoder_uninit( &decoder );
		if ( readResult != MA_SUCCESS && readResult != MA_AT_END )
			return false;
		if ( nFramesRead == 0 )
			return false;

		if ( nFramesRead < nFrames )
		{
			pSample->pcmData.resize( static_cast<size_t>( nFramesRead * nBlockAlign ) );
		}

		pSample->format = ma_format_s16;
		pSample->nSampleRate = nDecodedSampleRate;
		pSample->nChannels = nDecodedChannels;
		pSample->nBitsPerSample = 16;
		pSample->nBlockAlign = nBlockAlign;
		pSample->nPcmBytes = static_cast<unsigned int>( pSample->pcmData.size() );
		return InitializeSampleBuffer( pSample );
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

		if ( CPtr<IDataStream> pFileStream = OpenFileStream( pOpenStream->szFileName, STREAM_ACCESS_READ ) )
			return ReadWholeStream( pFileStream, &pOpenStream->encodedData );

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
		NPlatform::DebugWriteFormat( "Open audio stream %s: %s (%d bytes)\n",
												 pszStatus,
												 pOpenStream ? pOpenStream->szFileName.c_str() : "",
												 nBytes );
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
		driverInfo.szDriverName = nDriver == 0 ? "Open audio miniaudio backend" : "";
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

	ma_uint32 SelectBackends( ESFXOutputType output, ma_backend *pBackends, ma_uint32 nCapacity )
	{
		if ( !pBackends || nCapacity < 4 )
			return 0;

		pBackends[0] = ma_backend_wasapi;
		pBackends[1] = ma_backend_dsound;
		pBackends[2] = ma_backend_winmm;
		pBackends[3] = ma_backend_null;
		if ( output == SFX_OUTPUT_WINMM )
		{
			pBackends[0] = ma_backend_winmm;
			pBackends[1] = ma_backend_null;
			return 2;
		}
		if ( output == SFX_OUTPUT_DSOUND )
		{
			pBackends[0] = ma_backend_dsound;
			pBackends[1] = ma_backend_null;
			return 2;
		}
		return 4;
	}

	bool InitDevice( ESFXOutputType output, int nMixRate, int nMaxChannels, const SDriverInfo &driverInfo, bool *pSoundCardPresent )
	{
		if ( pSoundCardPresent )
			*pSoundCardPresent = output != SFX_OUTPUT_NO;

		if ( output == SFX_OUTPUT_NO )
			return true;

		if ( g_bEngineInitialized )
			return true;

		// WASAPI first: it is the native event-driven path on modern Windows;
		// dsound/winmm are emulated polling layers that kept underrunning
		// (audible stutter) whenever the main thread ran hot — load storms,
		// first-frame texture uploads. They remain as fallbacks only.
		ma_context_config contextConfig = ma_context_config_init();
		contextConfig.threadPriority = ma_thread_priority_realtime;

		contextConfig.allocationCallbacks.pUserData = 0;
		contextConfig.allocationCallbacks.onMalloc  = AudioAllocMalloc;
		contextConfig.allocationCallbacks.onRealloc = AudioAllocRealloc;
		contextConfig.allocationCallbacks.onFree    = AudioAllocFree;

		ma_backend backends[4] = {};
		const ma_uint32 backendCount = SelectBackends( output, backends, sizeof( backends ) / sizeof( backends[0] ) );
		ma_result result = backendCount == 0 ? MA_INVALID_ARGS : ma_context_init( backends, backendCount, &contextConfig, &g_context );
		if ( result != MA_SUCCESS )
		{
			TraceOpenAudioResult( "context init failed", result );
			return false;
		}
		g_bContextInitialized = true;

		ma_engine_config engineConfig = ma_engine_config_init();
		engineConfig.pContext = &g_context;
		engineConfig.channels = nMaxChannels > 0 ? static_cast<ma_uint32>( nMaxChannels ) : 0;
		engineConfig.sampleRate = nMixRate > 0 ? nMixRate : 0;
		// 40ms device period for immediate audio start. Stutter prevention
		// comes from MA_SOUND_FLAG_DECODE in PlayStream: the entire file is
		// decoded to PCM at init time, so the mixer callback only copies
		// samples — zero allocations, zero decode, zero disk I/O.
		engineConfig.periodSizeInMilliseconds = 40;
		engineConfig.allocationCallbacks.pUserData = 0;
		engineConfig.allocationCallbacks.onMalloc  = AudioAllocMalloc;
		engineConfig.allocationCallbacks.onRealloc = AudioAllocRealloc;
		engineConfig.allocationCallbacks.onFree    = AudioAllocFree;
		result = ma_engine_init( &engineConfig, &g_engine );
		if ( result != MA_SUCCESS )
		{
			TraceOpenAudioResult( "engine init failed", result );
			ma_context_uninit( &g_context );
			g_bContextInitialized = false;
			return false;
		}

		result = ma_engine_start( &g_engine );
		if ( result != MA_SUCCESS )
		{
			TraceOpenAudioResult( "engine start failed", result );
			ma_engine_uninit( &g_engine );
			ma_context_uninit( &g_context );
			g_bContextInitialized = false;
			return false;
		}

		g_bEngineInitialized = true;
		NPlatform::DebugWrite( "SFX open audio backend initialized miniaudio\n" );
		TraceOpenAudioDevice();
		return true;
	}

	void CloseDevice()
	{
		if ( g_bEngineInitialized )
			ma_engine_stop( &g_engine );

		for ( int i = 0; i < cMaxOpenChannels; ++i )
			ResetChannel( i );

		if ( g_bEngineInitialized )
		{
			ma_engine_uninit( &g_engine );
			g_bEngineInitialized = false;
		}
		if ( g_bContextInitialized )
		{
			ma_context_uninit( &g_context );
			g_bContextInitialized = false;
		}
	}

	void DebugTraceMixer()
	{
		NPlatform::DebugWrite( "SFX open audio miniaudio backend\n" );
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
			for ( int i = 0; i < cMaxOpenChannels; ++i )
				if ( g_channels[i].pSample == pOpenSample )
					ResetChannel( i );

			if ( pOpenSample->bBufferInitialized )
				ma_audio_buffer_uninit( &pOpenSample->buffer );
			delete pOpenSample;
		}
	}

	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode )
	{
		SOpenSample *pSample = new SOpenSample;
		InitializeEmptySample( pSample, nMode );
		if ( ParseWaveSample( pData, nSize, pSample ) )
			return pSample;

		InitializeEmptySample( pSample, nMode );
		if ( !DecodeSampleWithMiniAudio( pData, nSize, pSample ) )
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
		if ( !pSample )
			return;

		SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
		pOpenSample->bLooped = bEnable;
		for ( int i = 0; i < cMaxOpenChannels; ++i )
			if ( g_channels[i].bSoundInitialized && g_channels[i].pSample == pOpenSample )
				ma_sound_set_looping( &g_channels[i].sound, bEnable ? MA_TRUE : MA_FALSE );
	}

	void SetSampleLoopPoints( void *pSample, int nStart, int nEnd )
	{
		if ( !pSample )
			return;

		SOpenSample *pOpenSample = static_cast<SOpenSample*>( pSample );
		pOpenSample->nLoopStart = nStart;
		pOpenSample->nLoopEnd = nEnd;
		for ( int i = 0; i < cMaxOpenChannels; ++i )
			if ( g_channels[i].bSoundInitialized && g_channels[i].pSample == pOpenSample )
				ApplySampleLoopPoints( &g_channels[i], pOpenSample );
	}

	unsigned int GetSampleLength( void *pSample )
	{
		if ( !pSample )
			return 0;
		return GetSampleFrameCount( static_cast<SOpenSample*>( pSample ) );
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
		g_channels[nChannel].fUserPan = 0.0f;
		g_channels[nChannel].f3DPan = 0.0f;
		g_channels[nChannel].bUse3DPan = pOpenSample->nMode == GetSampleMode3D();
		g_channels[nChannel].bPaused = true;
		g_channels[nChannel].nPausedPosition = 0;
		ApplySampleLoopPoints( &g_channels[nChannel], pOpenSample );
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenSample->bLooped ? MA_TRUE : MA_FALSE );
		ApplyChannelMixInstant( nChannel );
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
			g_channels[nChannel].fUserPan = ClampFloat( (static_cast<float>( nPan ) - 128.0f) / 128.0f, -1.0f, 1.0f );
			ApplyChannelMix( nChannel );
		}
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
		if ( nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized )
		{
			if ( bPaused )
			{
				g_channels[nChannel].nPausedPosition = GetChannelPosition( nChannel );
				g_channels[nChannel].bPaused = true;
				// Abruptly stopping mid-waveform is an audible pop (the "stutter"
				// heard at save-load start); ramp to silence first.
				ma_sound_stop_with_fade_in_milliseconds( &g_channels[nChannel].sound, 60 );
			}
			else
			{
				if ( g_channels[nChannel].bPaused )
					ma_sound_seek_to_pcm_frame( &g_channels[nChannel].sound, g_channels[nChannel].nPausedPosition );
				g_channels[nChannel].bPaused = false;
				// The fade-stop above leaves a scheduled stop + zero fade on the
				// sound; clear it and ramp back in, or the restart pops too.
				ma_sound_reset_stop_time_and_fade( &g_channels[nChannel].sound );
				ma_sound_set_fade_in_milliseconds( &g_channels[nChannel].sound, 0.0f, ChannelTargetVolume( nChannel ), 60 );
				if ( ma_sound_start( &g_channels[nChannel].sound ) != MA_SUCCESS )
					NPlatform::DebugWrite( "SFX open audio failed to start sample channel\n" );
			}
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
			(g_channels[nChannel].bPaused || ma_sound_is_playing( &g_channels[nChannel].sound ) != 0);
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
		{
			ma_sound_seek_to_pcm_frame( &g_channels[nChannel].sound, nPosition );
			if ( g_channels[nChannel].bPaused )
				g_channels[nChannel].nPausedPosition = nPosition;
		}
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
			g_channels[nChannel].f3DPan = CalculatePan( vPos );
			g_channels[nChannel].bUse3DPan = true;
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
		// Load the entire encoded file into RAM now, before the save-load
		// disk storm begins. The audio thread then plays from memory with
		// zero disk I/O — no contention with the main thread's reads.
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
			#if SFX_ENABLE_XIPH_READ_TRACE
			DumpXiphReadTrace( pOpenStream->szFileName.c_str() );
			#endif
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

		// Data was already loaded into RAM by OpenStream — zero disk I/O here.
		if ( pOpenStream->encodedData.empty() )
			return -1;

		// DECODE, not STREAM: the entire encoded file is decoded to PCM in
		// RAM at init time (on miniaudio's own thread). The mixer callback
		// then only copies samples — zero allocations, zero decode, zero
		// disk I/O during playback. Combined with the private heap, the
		// audio thread never contends with the main thread at all.
		const ma_uint32 nStreamFlags = pOpenStream->bLooped
			? MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_LOOPING
			: MA_SOUND_FLAG_DECODE;

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

			if ( ma_sound_init_from_data_source( &g_engine, &g_channels[nChannel].xiphDataSource.base, nStreamFlags, 0, &g_channels[nChannel].sound ) != MA_SUCCESS )
			{
				TraceOpenStream( "sound init failed", pOpenStream );
				ResetChannel( nChannel );
				return -1;
			}
		}
		else
		{
			if ( ma_decoder_init_memory( &pOpenStream->encodedData[0], pOpenStream->encodedData.size(), 0, &g_channels[nChannel].decoder ) != MA_SUCCESS )
			{
				TraceOpenStream( "decode failed", pOpenStream );
				return -1;
			}
			g_channels[nChannel].bDecoderInitialized = true;

			if ( ma_sound_init_from_data_source( &g_engine, &g_channels[nChannel].decoder, nStreamFlags, 0, &g_channels[nChannel].sound ) != MA_SUCCESS )
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
		g_channels[nChannel].fUserPan = 0.0f;
		g_channels[nChannel].f3DPan = 0.0f;
		g_channels[nChannel].bUse3DPan = false;
		g_channels[nChannel].bPaused = false;
		g_channels[nChannel].nPausedPosition = 0;
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenStream->bLooped ? MA_TRUE : MA_FALSE );
		ApplyChannelMixInstant( nChannel );
		ma_sound_set_end_callback( &g_channels[nChannel].sound, OpenStreamEndCallback, pOpenStream );
		// Streams ramp in from silence to their mix volume — no start transient.
		ma_sound_set_fade_in_milliseconds( &g_channels[nChannel].sound, 0.0f, ChannelTargetVolume( nChannel ), 80 );
		if ( ma_sound_start( &g_channels[nChannel].sound ) != MA_SUCCESS )
		{
			TraceOpenStream( "start failed", pOpenStream );
			ResetChannel( nChannel );
			return -1;
		}
		TraceOpenStream( "started", pOpenStream );
		return nChannel;
	}

	void SetStreamChannelPan( int nChannel )
	{
		SetChannelPan( nChannel, 128 );
	}
}

#endif // defined(SFX_USE_OPEN_AUDIO_BACKEND)
