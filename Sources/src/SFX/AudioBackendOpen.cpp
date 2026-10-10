#include "StdAfx.h"

#include "AudioBackendImpl.h"
#include "AudioBackendXiphVorbis.h"
#include "../Platform/Clock.h"
#include "../Platform/Debug.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
		// Stopped by the game but still fading out (StopChannel): the game
		// has forgotten the slot, so it reads as free, and it is reused once
		// the fade has run.
		bool bReleasing;
		// The start or resume ramp has been asked for but the mixer has not
		// taken it up yet; a volume change then keeps the ramp's length.
		bool bStartRampPending;
		unsigned int nStartRampMs;
		// Unpaused at least once: a later unpause resumes, the first starts.
		bool bStarted;
		unsigned int nPausedPosition;
		unsigned int nStartSerial;
	};

	struct SOpenStream
	{
		std::string szFileName;
		bool bLooped;
		std::atomic<NAudioBackend::TStreamCallback> pEndCallback;
		std::atomic<void *> pUserData;
		std::atomic<unsigned int> nCallbackReaders;
		std::atomic<bool> bClosing;
		std::vector<char> encodedData;
		unsigned int nSampleRate;
		unsigned int nChannels;
		unsigned int nBlockAlign;
		bool bUseXiphDecoder;
	};

	const int cMaxOpenChannels = 128;
	SOpenChannel g_channels[cMaxOpenChannels];
	int g_nNextChannel = 0;
	// Streams (music/video audio) own the tail of the slot array so sample
	// voice stealing can never silence them; only CSoundEngine plays streams,
	// so a small reserve is plenty.
	const int cReservedStreamChannels = 16;
	int g_nMaxSampleChannels = cMaxOpenChannels - cReservedStreamChannels;
	int g_nNextStreamChannel = cMaxOpenChannels - cReservedStreamChannels;
	unsigned int g_nStartSerial = 0;
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
		case MA_FAILED_TO_INIT_BACKEND:
			return "MA_FAILED_TO_INIT_BACKEND";
		case MA_FAILED_TO_OPEN_BACKEND_DEVICE:
			return "MA_FAILED_TO_OPEN_BACKEND_DEVICE";
		case MA_FAILED_TO_START_BACKEND_DEVICE:
			return "MA_FAILED_TO_START_BACKEND_DEVICE";
		case MA_FAILED_TO_STOP_BACKEND_DEVICE:
			return "MA_FAILED_TO_STOP_BACKEND_DEVICE";
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

	// BK_AUDIO_FAIL_DEFAULT=1 makes every attempt on the system default device
	// fail as the device open would, so the retry and fallback path can be
	// exercised without an AirPlay link that is actually waking up.
	// BK_AUDIO_FAIL_DEFAULT=all fails the fallback devices too: a machine whose
	// audio service lists no device that opens, so sound is off for the session.
	bool ShouldFailDefaultDevice()
	{
		const char *pszValue = getenv( "BK_AUDIO_FAIL_DEFAULT" );
		return pszValue && pszValue[0] && !( pszValue[0] == '0' && pszValue[1] == 0 );
	}

	bool ShouldFailEveryDevice()
	{
		const char *pszValue = getenv( "BK_AUDIO_FAIL_DEFAULT" );
		return pszValue && strcmp( pszValue, "all" ) == 0;
	}

	// Opens g_engine on one playback device (0 = the system default) and starts
	// it. On failure g_engine is left uninitialized and the context stays up.
	ma_result OpenEngineOnDevice( ma_engine_config engineConfig, ma_device_id *pDeviceID, const char *pszLabel )
	{
		char szAction[MA_MAX_DEVICE_NAME_LENGTH + 64];
		if ( ( !pDeviceID && ShouldFailDefaultDevice() ) || ShouldFailEveryDevice() )
		{
			snprintf( szAction, sizeof( szAction ), "engine init failed on %s (BK_AUDIO_FAIL_DEFAULT)", pszLabel );
			TraceOpenAudioResult( szAction, MA_FAILED_TO_OPEN_BACKEND_DEVICE );
			return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
		}

		engineConfig.pPlaybackDeviceID = pDeviceID;
		ma_result result = ma_engine_init( &engineConfig, &g_engine );
		if ( result != MA_SUCCESS )
		{
			snprintf( szAction, sizeof( szAction ), "engine init failed on %s", pszLabel );
			TraceOpenAudioResult( szAction, result );
			return result;
		}

		result = ma_engine_start( &g_engine );
		if ( result != MA_SUCCESS )
		{
			snprintf( szAction, sizeof( szAction ), "engine start failed on %s", pszLabel );
			TraceOpenAudioResult( szAction, result );
			ma_engine_uninit( &g_engine );
			return result;
		}
		return MA_SUCCESS;
	}

	// Lower ranks are tried first. A device ranked cFallbackRankVirtual only
	// plays when nothing physical is left: sound sent to a loopback device
	// such as BlackHole or a conferencing app's virtual speaker is inaudible.
	const int cFallbackRankBuiltIn = 0;
	const int cFallbackRankPhysical = 1;
	const int cFallbackRankWireless = 2;
	const int cFallbackRankVirtual = 3;

#if defined(MA_HAS_COREAUDIO) && defined(MA_APPLE_DESKTOP)
	typedef CFStringRef (*TCFStringCreateWithCString)( CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding );

	// miniaudio loads CoreFoundation and CoreAudio at run time, so the
	// functions are taken from its context rather than linked into SFX.
	bool GetCoreAudioTransportType( const ma_device_id &deviceID, UInt32 *pTransportType )
	{
		if ( !g_context.coreaudio.AudioObjectGetPropertyData || !g_context.coreaudio.CFRelease )
			return false;
#if defined(MA_NO_RUNTIME_LINKING)
		TCFStringCreateWithCString pCreateString = CFStringCreateWithCString;
#else
		TCFStringCreateWithCString pCreateString = (TCFStringCreateWithCString)ma_dlsym( ma_context_get_log( &g_context ), g_context.coreaudio.hCoreFoundation, "CFStringCreateWithCString" );
#endif
		if ( !pCreateString )
			return false;
		// ma_device_id::coreaudio is the device UID.
		CFStringRef uid = pCreateString( 0, deviceID.coreaudio, kCFStringEncodingUTF8 );
		if ( !uid )
			return false;

		const ma_AudioObjectGetPropertyData_proc pGetPropertyData = (ma_AudioObjectGetPropertyData_proc)g_context.coreaudio.AudioObjectGetPropertyData;
		AudioObjectPropertyAddress address;
		address.mSelector = kAudioHardwarePropertyTranslateUIDToDevice;
		address.mScope = kAudioObjectPropertyScopeGlobal;
		address.mElement = kAudioObjectPropertyElementMain;
		AudioObjectID deviceObjectID = kAudioObjectUnknown;
		UInt32 nDataSize = sizeof( deviceObjectID );
		OSStatus status = pGetPropertyData( kAudioObjectSystemObject, &address, sizeof( uid ), &uid, &nDataSize, &deviceObjectID );
		( (ma_CFRelease_proc)g_context.coreaudio.CFRelease )( uid );
		if ( status != noErr || deviceObjectID == kAudioObjectUnknown )
			return false;

		address.mSelector = kAudioDevicePropertyTransportType;
		UInt32 nTransportType = kAudioDeviceTransportTypeUnknown;
		nDataSize = sizeof( nTransportType );
		status = pGetPropertyData( deviceObjectID, &address, 0, 0, &nDataSize, &nTransportType );
		if ( status != noErr )
			return false;
		*pTransportType = nTransportType;
		return true;
	}

	int GetFallbackRank( const ma_device_info &info, char *pszKind, size_t nKindSize )
	{
		UInt32 nTransportType = kAudioDeviceTransportTypeUnknown;
		if ( !GetCoreAudioTransportType( info.id, &nTransportType ) )
		{
			snprintf( pszKind, nKindSize, "unknown" );
			return cFallbackRankWireless;
		}

		const char szFourCC[5] = { char( ( nTransportType >> 24 ) & 0xff ), char( ( nTransportType >> 16 ) & 0xff ),
			char( ( nTransportType >> 8 ) & 0xff ), char( nTransportType & 0xff ), 0 };
		snprintf( pszKind, nKindSize, "%s", nTransportType == kAudioDeviceTransportTypeUnknown ? "unknown" : szFourCC );
		switch ( nTransportType )
		{
		case kAudioDeviceTransportTypeBuiltIn:
			return cFallbackRankBuiltIn;
		case kAudioDeviceTransportTypeVirtual:
		case kAudioDeviceTransportTypeAggregate:
		case kAudioDeviceTransportTypeAutoAggregate:
			return cFallbackRankVirtual;
		// Wireless links are the ones that fail to open while waking up, so
		// a wired device goes first.
		case kAudioDeviceTransportTypeAirPlay:
		case kAudioDeviceTransportTypeBluetooth:
		case kAudioDeviceTransportTypeBluetoothLE:
		case kAudioDeviceTransportTypeUnknown:
			return cFallbackRankWireless;
		default:
			return cFallbackRankPhysical;
		}
	}
#else
	int GetFallbackRank( const ma_device_info &info, char *pszKind, size_t nKindSize )
	{
		( void )info;
		snprintf( pszKind, nKindSize, "enumerated" );
		return cFallbackRankPhysical;
	}
#endif

	struct SFallbackDevice
	{
		ma_device_info info;
		int nRank;
	};

	// The system default failed to open; try every other playback device,
	// best rank first, enumeration order within a rank.
	bool OpenEngineOnFallbackDevice( const ma_engine_config &engineConfig )
	{
		ma_device_info *pPlaybackInfos = 0;
		ma_uint32 nPlaybackCount = 0;
		ma_result result = ma_context_get_devices( &g_context, &pPlaybackInfos, &nPlaybackCount, 0, 0 );
		if ( result != MA_SUCCESS )
		{
			TraceOpenAudioResult( "device enumeration failed", result );
			return false;
		}

		// Copied: the context owns the enumeration buffer and may refill it.
		std::vector<SFallbackDevice> candidates;
		for ( ma_uint32 i = 0; i < nPlaybackCount; ++i )
		{
			const ma_device_info &info = pPlaybackInfos[i];
			char szKind[16];
			SFallbackDevice candidate;
			candidate.info = info;
			candidate.nRank = GetFallbackRank( info, szKind, sizeof( szKind ) );
			NPlatform::DebugWriteFormat( "SFX open audio playback device: \"%s\", transport=%s, rank=%d%s\n",
				info.name, szKind, candidate.nRank, info.isDefault ? ", default (skipped)" : "" );
			if ( !info.isDefault )
				candidates.push_back( candidate );
		}
		std::stable_sort( candidates.begin(), candidates.end(),
			[]( const SFallbackDevice &a, const SFallbackDevice &b ) { return a.nRank < b.nRank; } );

		for ( size_t i = 0; i < candidates.size(); ++i )
		{
			char szLabel[MA_MAX_DEVICE_NAME_LENGTH + 32];
			snprintf( szLabel, sizeof( szLabel ), "fallback device \"%s\"", candidates[i].info.name );
			if ( OpenEngineOnDevice( engineConfig, &candidates[i].info.id, szLabel ) == MA_SUCCESS )
			{
				NPlatform::DebugWriteFormat( "SFX open audio falling back to \"%s\"\n", candidates[i].info.name );
				return true;
			}
		}
		return false;
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

	// BK_SOUND_TRACE=1 (the sound scene's switch) also names every voice the
	// backend starts and every one it cuts off while still sounding.
	bool IsVoiceTraceOn()
	{
		static const bool bOn = getenv( "BK_SOUND_TRACE" ) != 0;
		return bOn;
	}

	// The sample value (full scale 1.0, first channel) under a voice's cursor:
	// how far the waveform jumps when the voice is cut there.
	float SampleAmplitudeAt( const SOpenSample *pSample, unsigned int nFrame )
	{
		if ( !pSample || pSample->nBlockAlign == 0 || pSample->pcmData.empty() )
			return 0.0f;
		const unsigned int nFrames = pSample->nPcmBytes / pSample->nBlockAlign;
		if ( nFrame >= nFrames )
			return 0.0f;
		const char *pFrame = &pSample->pcmData[0] + size_t( nFrame ) * pSample->nBlockAlign;
		switch ( pSample->format )
		{
		case ma_format_u8:
			return ( float( static_cast<unsigned char>( pFrame[0] ) ) - 128.0f ) / 128.0f;
		case ma_format_s16:
			return float( ma_int16( static_cast<unsigned char>( pFrame[0] ) | ( static_cast<unsigned char>( pFrame[1] ) << 8 ) ) ) / 32768.0f;
		default:
			return 0.0f;
		}
	}

	void ResetChannel( int nChannel )
	{
		if ( nChannel < 0 || nChannel >= cMaxOpenChannels )
			return;

		if ( g_channels[nChannel].bSoundInitialized )
		{
			if ( IsVoiceTraceOn() && ma_sound_is_playing( &g_channels[nChannel].sound ) && !ma_sound_at_end( &g_channels[nChannel].sound ) )
			{
				ma_uint64 nCursor = 0;
				ma_sound_get_cursor_in_pcm_frames( &g_channels[nChannel].sound, &nCursor );
				const float fGain = ma_sound_get_current_fade_volume( &g_channels[nChannel].sound );
				fprintf( stderr, "BK_SOUND_TRACE: voice cut at=%llu ch=%d sample=%p cursor=%llu/%u gain=%.3f amp=%.3f\n", (unsigned long long)ma_engine_get_time_in_pcm_frames( &g_engine ), nChannel,
					(void*)g_channels[nChannel].pSample, (unsigned long long)nCursor,
					g_channels[nChannel].pSample && g_channels[nChannel].pSample->nBlockAlign ? g_channels[nChannel].pSample->nPcmBytes / g_channels[nChannel].pSample->nBlockAlign : 0u,
					fGain, fGain * SampleAmplitudeAt( g_channels[nChannel].pSample, unsigned( nCursor ) ) );
			}
			ma_sound_set_end_callback( &g_channels[nChannel].sound, 0, 0 );
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
		g_channels[nChannel].bReleasing = false;
		g_channels[nChannel].bStartRampPending = false;
		g_channels[nChannel].nStartRampMs = 0;
		g_channels[nChannel].bStarted = false;
		g_channels[nChannel].nPausedPosition = 0;
		g_channels[nChannel].nStartSerial = 0;
	}

	float ChannelTargetVolume( int nChannel )
	{
		return g_channels[nChannel].fBaseVolume * g_channels[nChannel].fDistanceVolume;
	}

	// A fresh sample voice ramps in over a few milliseconds: enough to hide a
	// start that is not at a zero crossing (a loop resumed mid-sample by the
	// sound scene), short enough to keep a shot's own attack. FMOD, which the
	// game was made with, started voices at full volume.
	const unsigned int cSampleStartRampMs = 5;
	// Resuming from a pause ramps in as the pause ramped out.
	const unsigned int cSampleResumeRampMs = 60;
	const unsigned int cSamplePauseRampMs = 60;
	const unsigned int cStreamStartRampMs = 80;
	// StopChannel: a voice cut mid-waveform clicks; a short release does not.
	const unsigned int cSampleStopRampMs = 10;
	const unsigned int cVolumeChaseMs = 40;

	bool IsLiveChannel( int nChannel )
	{
		return nChannel >= 0 && nChannel < cMaxOpenChannels && g_channels[nChannel].bSoundInitialized && !g_channels[nChannel].bReleasing;
	}

	// ma_sound_set_fade_* only posts a request (one slot: a later request
	// replaces an earlier one the mixer has not taken up yet) and the mixer
	// resolves volumeBeg -1 against the fader's own state when it takes it
	// up. A new ma_sound's fader sits at 1.0, full scale, so a start whose
	// ramp request was replaced by a volume update before the mixer's next
	// period (a 40 ms window; the game updates volume and 3D position every
	// frame) began at full scale and dropped to the mix volume over 40 ms:
	// the sharp crack heard on map sounds and engine starts, and on the
	// music at the start of a mission. The sound is not being mixed yet, so
	// its fader can be set directly to silence; every later request then
	// ramps from where the voice really is.
	void SeedSilentFader( int nChannel )
	{
		ma_fader_set_fade( &g_channels[nChannel].sound.engineNode.fader, 0.0f, 0.0f, 0 );
	}

	// True while the mixer has not yet taken up the last fade request.
	bool IsFadeRequestPending( ma_sound *pSound )
	{
		return ma_atomic_uint64_get( &pSound->engineNode.fadeSettings.fadeLengthInFrames ) != ~(ma_uint64)0;
	}

	void ApplyChannelPan( int nChannel )
	{
		ma_sound_set_pan( &g_channels[nChannel].sound, g_channels[nChannel].bUse3DPan ? g_channels[nChannel].f3DPan : g_channels[nChannel].fUserPan );
	}

	// Starts or resumes a voice's ramp from where its fader is to the mix
	// volume.
	void StartChannelRamp( int nChannel, unsigned int nRampMs )
	{
		g_channels[nChannel].bStartRampPending = true;
		g_channels[nChannel].nStartRampMs = nRampMs;
		ma_sound_set_fade_in_milliseconds( &g_channels[nChannel].sound, -1.0f, ChannelTargetVolume( nChannel ), nRampMs );
	}

	// Volume changes ride the sound's FADER (short chase ramp), never an
	// instant ma_sound_set_volume: the engine updates volumes once per main-
	// loop tick, and when that thread is busy (menu init after the intro
	// video, save-load storms) the ticks are 100-500ms apart — instant steps
	// of a fading music stream then zipper audibly ("stuttering"). The fader
	// runs on the mixer thread, so a 40ms ramp per update stays smooth no
	// matter how coarse the updates are. volumeBeg -1 = chase from current.
	// A paused voice keeps its fade-out request: replacing it would cut the
	// voice off at the scheduled stop instead of fading it. The resume ramp
	// picks up the latest volume.
	void ApplyChannelMix( int nChannel )
	{
		if ( !IsLiveChannel( nChannel ) )
			return;

		ApplyChannelPan( nChannel );
		if ( g_channels[nChannel].bPaused )
			return;
		SOpenChannel &channel = g_channels[nChannel];
		if ( channel.bStartRampPending && !IsFadeRequestPending( &channel.sound ) )
			channel.bStartRampPending = false;
		if ( channel.bStartRampPending && IsVoiceTraceOn() )
			fprintf( stderr, "BK_SOUND_TRACE: voice volume set before its start ramp was mixed at=%llu ch=%d\n", (unsigned long long)ma_engine_get_time_in_pcm_frames( &g_engine ), nChannel );
		const unsigned int nRampMs = channel.bStartRampPending ? channel.nStartRampMs : cVolumeChaseMs;
		ma_sound_set_fade_in_milliseconds( &channel.sound, -1.0f, ChannelTargetVolume( nChannel ), nRampMs );
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
		if ( !pStream )
			return;
		pStream->nCallbackReaders.fetch_add( 1, std::memory_order_acquire );
		if ( !pStream->bClosing.load( std::memory_order_acquire ) )
		{
			NAudioBackend::TStreamCallback pCallback = pStream->pEndCallback.load( std::memory_order_acquire );
			void *pCallbackUserData = pStream->pUserData.load( std::memory_order_acquire );
			if ( pCallback )
				pCallback( pStream, 0, 0, pCallbackUserData );
		}
		pStream->nCallbackReaders.fetch_sub( 1, std::memory_order_release );
	}

	int FindFreeChannelInRange( int nBegin, int nEnd, int *pNextChannel )
	{
		const int nCount = nEnd - nBegin;
		if ( nCount <= 0 )
			return -1;
		for ( int i = 0; i < nCount; ++i )
		{
			const int nChannel = nBegin + (*pNextChannel - nBegin + i) % nCount;
			if ( !g_channels[nChannel].bSoundInitialized || ma_sound_at_end( &g_channels[nChannel].sound ) ||
				( g_channels[nChannel].bReleasing && !ma_sound_is_playing( &g_channels[nChannel].sound ) ) )
			{
				ResetChannel( nChannel );
				*pNextChannel = nBegin + (nChannel - nBegin + 1) % nCount;
				return nChannel;
			}
		}
		return -1;
	}

	// A LOOPING ma_sound never reports at_end, so a full pool must be stolen
	// from, not refused: refusing would strand any looping voice whose map
	// entry upstream was lost — it would play forever. FMOD's FSOUND_FREE
	// recycled voices the same way; the oldest running voice is the least
	// audible loss.
	int FindFreeSampleChannel()
	{
		const int nFree = FindFreeChannelInRange( 0, g_nMaxSampleChannels, &g_nNextChannel );
		if ( nFree != -1 )
			return nFree;

		// A voice still fading out after its stop goes first.
		int nOldest = -1;
		for ( int i = 0; i < g_nMaxSampleChannels; ++i )
		{
			if ( !g_channels[i].bSoundInitialized || g_channels[i].pStream || g_channels[i].bPaused )
				continue;
			if ( nOldest != -1 && g_channels[nOldest].bReleasing != g_channels[i].bReleasing )
			{
				if ( g_channels[i].bReleasing )
					nOldest = i;
				continue;
			}
			if ( nOldest == -1 || g_channels[i].nStartSerial < g_channels[nOldest].nStartSerial )
				nOldest = i;
		}
		if ( nOldest != -1 )
			ResetChannel( nOldest );
		return nOldest;
	}

	int FindFreeStreamChannel()
	{
		return FindFreeChannelInRange( g_nMaxSampleChannels, cMaxOpenChannels, &g_nNextStreamChannel );
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

	// BK_AUDIO_CAPTURE=<file.wav> records the engine's final mix (32-bit
	// float WAV at the engine rate) from the mixer thread: what reaches the
	// device, so clicks and levels can be measured instead of guessed. With
	// BK_AUDIO_NULL=1 it records a run that plays nothing aloud.
	FILE *g_pCaptureFile = 0;
	unsigned int g_nCaptureChannels = 0;
	unsigned long long g_nCaptureFrames = 0;

	void WriteCaptureU32( unsigned int nValue )
	{
		const unsigned char bytes[4] = { (unsigned char)( nValue & 0xff ), (unsigned char)( ( nValue >> 8 ) & 0xff ), (unsigned char)( ( nValue >> 16 ) & 0xff ), (unsigned char)( ( nValue >> 24 ) & 0xff ) };
		fwrite( bytes, 1, 4, g_pCaptureFile );
	}

	void WriteCaptureU16( unsigned int nValue )
	{
		const unsigned char bytes[2] = { (unsigned char)( nValue & 0xff ), (unsigned char)( ( nValue >> 8 ) & 0xff ) };
		fwrite( bytes, 1, 2, g_pCaptureFile );
	}

	void WriteCaptureHeader( unsigned int nSampleRate )
	{
		const unsigned int nDataBytes = unsigned( Min( g_nCaptureFrames * g_nCaptureChannels * 4ull, 0xffffff00ull ) );
		fseek( g_pCaptureFile, 0, SEEK_SET );
		fwrite( "RIFF", 1, 4, g_pCaptureFile );
		WriteCaptureU32( 36 + nDataBytes );
		fwrite( "WAVEfmt ", 1, 8, g_pCaptureFile );
		WriteCaptureU32( 16 );
		WriteCaptureU16( 3 );
		WriteCaptureU16( g_nCaptureChannels );
		WriteCaptureU32( nSampleRate );
		WriteCaptureU32( nSampleRate * g_nCaptureChannels * 4 );
		WriteCaptureU16( g_nCaptureChannels * 4 );
		WriteCaptureU16( 32 );
		fwrite( "data", 1, 4, g_pCaptureFile );
		WriteCaptureU32( nDataBytes );
	}

	void CaptureEngineOutput( void *pUserData, float *pFramesOut, ma_uint64 nFrameCount )
	{
		( void )pUserData;
		if ( g_pCaptureFile && pFramesOut )
		{
			fwrite( pFramesOut, sizeof( float ) * g_nCaptureChannels, size_t( nFrameCount ), g_pCaptureFile );
			g_nCaptureFrames += nFrameCount;
		}
	}

	void OpenCapture()
	{
		const char *pszPath = getenv( "BK_AUDIO_CAPTURE" );
		if ( !pszPath || !pszPath[0] )
			return;
		g_nCaptureChannels = ma_engine_get_channels( &g_engine );
		g_nCaptureFrames = 0;
		g_pCaptureFile = fopen( pszPath, "wb" );
		if ( !g_pCaptureFile )
		{
			NPlatform::DebugWriteFormat( "SFX open audio capture: cannot write %s\n", pszPath );
			return;
		}
		WriteCaptureHeader( ma_engine_get_sample_rate( &g_engine ) );
		NPlatform::DebugWriteFormat( "SFX open audio capture: recording the mix to %s\n", pszPath );
	}

	void CloseCapture()
	{
		if ( !g_pCaptureFile )
			return;
		WriteCaptureHeader( ma_engine_get_sample_rate( &g_engine ) );
		fclose( g_pCaptureFile );
		g_pCaptureFile = 0;
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

	// BK_AUDIO_NULL=1 plays into miniaudio's null device only. Everything
	// above the device still runs - voices start, advance and finish - but
	// nothing reaches a speaker, so a headless harness can run the real game
	// with sound on (map-editor-game-reads-it checks a map sound starts)
	// without playing it through the default output.
	bool IsNullAudioRequested()
	{
		const char *pszValue = getenv( "BK_AUDIO_NULL" );
		return pszValue && pszValue[0] && !( pszValue[0] == '0' && pszValue[1] == 0 );
	}

	ma_uint32 SelectBackends( ESFXOutputType output, ma_backend *pBackends, ma_uint32 nCapacity )
	{
		if ( !pBackends || nCapacity < 4 )
			return 0;

		if ( IsNullAudioRequested() )
		{
			pBackends[0] = ma_backend_null;
			return 1;
		}

#if defined(_WIN32) || defined(_WIN64)
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
#else
		// The WASAPI/DirectSound/WinMM list above does not exist off Windows,
		// so requesting it left ma_backend_null as the only match: every call
		// reported success and the mixer ran into a null device, i.e. silence.
		// SFX_OUTPUT_WINMM/DSOUND are Windows driver choices and carry no
		// meaning here, so the native backend is used whatever they say.
		( void )output;
#if defined(__APPLE__)
		pBackends[0] = ma_backend_coreaudio;
		pBackends[1] = ma_backend_null;
		return 2;
#else
		pBackends[0] = ma_backend_pulseaudio;
		pBackends[1] = ma_backend_alsa;
		pBackends[2] = ma_backend_jack;
		pBackends[3] = ma_backend_null;
		return 4;
#endif
#endif
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

		// nMaxChannels is the game's VOICE budget (FMOD semantics). It must not
		// go into engineConfig.channels — that is the device SPEAKER count, and
		// feeding 32 in asked for a 32-speaker mix. Native layout stays 0; the
		// budget caps the sample slot pool instead (streams keep their reserve).
		if ( nMaxChannels > 0 )
		{
			g_nMaxSampleChannels = Clamp( nMaxChannels, 1, cMaxOpenChannels - cReservedStreamChannels );
			g_nNextChannel = 0;
			g_nNextStreamChannel = g_nMaxSampleChannels;
		}

		ma_engine_config engineConfig = ma_engine_config_init();
		engineConfig.pContext = &g_context;
		engineConfig.sampleRate = nMixRate > 0 ? nMixRate : 0;
		// 40ms device period for immediate audio start. Stutter prevention
		// comes from MA_SOUND_FLAG_DECODE in PlayStream: the entire file is
		// decoded to PCM at init time, so the mixer callback only copies
		// samples — zero allocations, zero decode, zero disk I/O.
		engineConfig.periodSizeInMilliseconds = 40;
		engineConfig.onProcess = CaptureEngineOutput;
		engineConfig.allocationCallbacks.pUserData = 0;
		engineConfig.allocationCallbacks.onMalloc  = AudioAllocMalloc;
		engineConfig.allocationCallbacks.onRealloc = AudioAllocRealloc;
		engineConfig.allocationCallbacks.onFree    = AudioAllocFree;
		// An AirPlay default output fails to open (MA_FAILED_TO_OPEN_BACKEND_DEVICE)
		// while its link wakes up or reconnects, which left whole sessions
		// silent. Give it two short retries, then take another real device
		// rather than none. The waits only happen on failure.
		const unsigned int cDefaultRetryDelaysMs[] = { 500, 1000 };
		const int nDefaultAttempts = 1 + int( sizeof( cDefaultRetryDelaysMs ) / sizeof( cDefaultRetryDelaysMs[0] ) );
		for ( int nAttempt = 0; nAttempt < nDefaultAttempts; ++nAttempt )
		{
			if ( nAttempt > 0 )
			{
				NPlatform::DebugWriteFormat( "SFX open audio retrying default device in %u ms (attempt %d of %d)\n",
					cDefaultRetryDelaysMs[nAttempt - 1], nAttempt + 1, nDefaultAttempts );
				NPlatform::SleepMilliseconds( cDefaultRetryDelaysMs[nAttempt - 1] );
			}
			result = OpenEngineOnDevice( engineConfig, 0, "default device" );
			if ( result == MA_SUCCESS )
				break;
		}

		if ( result != MA_SUCCESS && !OpenEngineOnFallbackDevice( engineConfig ) )
		{
			NPlatform::DebugWrite( "SFX open audio found no playback device that opens; sound is off\n" );
			ma_context_uninit( &g_context );
			g_bContextInitialized = false;
			return false;
		}

		g_bEngineInitialized = true;
		NPlatform::DebugWrite( "SFX open audio backend initialized miniaudio\n" );
		TraceOpenAudioDevice();
		OpenCapture();
		return true;
	}

	void CloseDevice()
	{
		if ( g_bEngineInitialized )
		{
			ma_engine_stop( &g_engine );
			CloseCapture();
		}

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

	// The game never stops the device itself (CloseDevice uninits it), so a
	// stopped device means the output went away under us. After a Mac sleeps
	// with an AirPlay default output, miniaudio reroutes to the returning link
	// but its AudioOutputUnitStart fails while the link wakes up; miniaudio
	// then marks the device stopped without telling anyone and the whole
	// session stays silent. Called every frame; retries once a second.
	void RestartStoppedDevice()
	{
		if ( !g_bEngineInitialized )
			return;
		ma_device *pDevice = ma_engine_get_device( &g_engine );
		if ( !pDevice || ma_device_get_state( pDevice ) != ma_device_state_stopped )
			return;

		static std::uint32_t s_tLastAttempt = 0;
		static bool s_bReportedFailure = false;
		const std::uint32_t tNow = NPlatform::MonotonicMilliseconds();
		if ( s_tLastAttempt != 0 && NPlatform::MillisecondsElapsed( s_tLastAttempt, tNow ) < 1000 )
			return;
		s_tLastAttempt = tNow;

		const ma_result result = ma_device_start( pDevice );
		if ( result == MA_SUCCESS )
		{
			NPlatform::DebugWrite( "SFX open audio device had stopped; restarted\n" );
			TraceOpenAudioDevice();
			s_bReportedFailure = false;
		}
		else if ( !s_bReportedFailure )
		{
			TraceOpenAudioResult( "device had stopped; restart failed, retrying", result );
			s_bReportedFailure = true;
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

		// Sample default only, picked up when a voice starts (FMOD's
		// FSOUND_Sample_SetMode semantics). Retro-applying to every playing
		// channel of a shared sample turned live one-shots into immortal loops
		// (and vice versa) whenever a later play toggled the flag.
		static_cast<SOpenSample*>( pSample )->bLooped = bEnable;
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
		return IsLiveChannel( nChannel ) &&
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

		const int nChannel = FindFreeSampleChannel();
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
		g_channels[nChannel].nStartSerial = ++g_nStartSerial;
		ApplySampleLoopPoints( &g_channels[nChannel], pOpenSample );
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenSample->bLooped ? MA_TRUE : MA_FALSE );
		SeedSilentFader( nChannel );
		ApplyChannelPan( nChannel );
		return nChannel;
	}

	void SetChannelVolume( int nChannel, int nVolume )
	{
		if ( IsLiveChannel( nChannel ) )
		{
			g_channels[nChannel].fBaseVolume = ClampFloat( static_cast<float>( nVolume ) / 255.0f, 0.0f, 1.0f );
			ApplyChannelMix( nChannel );
		}
	}

	void SetChannelPan( int nChannel, int nPan )
	{
		if ( IsLiveChannel( nChannel ) )
		{
			g_channels[nChannel].fUserPan = ClampFloat( (static_cast<float>( nPan ) - 128.0f) / 128.0f, -1.0f, 1.0f );
			ApplyChannelMix( nChannel );
		}
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
		if ( IsLiveChannel( nChannel ) )
		{
			if ( bPaused )
			{
				g_channels[nChannel].nPausedPosition = GetChannelPosition( nChannel );
				g_channels[nChannel].bPaused = true;
				// Abruptly stopping mid-waveform is an audible pop (the "stutter"
				// heard at save-load start); ramp to silence first.
				ma_sound_stop_with_fade_in_milliseconds( &g_channels[nChannel].sound, cSamplePauseRampMs );
			}
			else
			{
				const bool bFirstStart = !g_channels[nChannel].bStarted;
				g_channels[nChannel].bStarted = true;
				if ( g_channels[nChannel].bPaused )
					ma_sound_seek_to_pcm_frame( &g_channels[nChannel].sound, g_channels[nChannel].nPausedPosition );
				g_channels[nChannel].bPaused = false;
				// The fade-stop above leaves a scheduled stop + zero fade on the
				// sound; clear it and ramp back in, or the restart pops too.
				ma_sound_reset_stop_time_and_fade( &g_channels[nChannel].sound );
				ApplyChannelPan( nChannel );
				StartChannelRamp( nChannel, bFirstStart ? cSampleStartRampMs : cSampleResumeRampMs );
				if ( ma_sound_start( &g_channels[nChannel].sound ) != MA_SUCCESS )
					NPlatform::DebugWrite( "SFX open audio failed to start sample channel\n" );
				if ( IsVoiceTraceOn() && g_channels[nChannel].pSample )
					fprintf( stderr, "BK_SOUND_TRACE: voice start at=%llu ch=%d sample=%p from=%u/%u rate=%u looped=%d volume=%.3f\n", (unsigned long long)ma_engine_get_time_in_pcm_frames( &g_engine ), nChannel,
						(void*)g_channels[nChannel].pSample, g_channels[nChannel].nPausedPosition, GetSampleFrameCount( g_channels[nChannel].pSample ),
						g_channels[nChannel].pSample->nSampleRate, int( g_channels[nChannel].pSample->bLooped ), ChannelTargetVolume( nChannel ) );
			}
		}
	}

	// The game forgets the channel at once; a sample voice still sounding
	// fades out over a few milliseconds in its slot first (a hard cut of an
	// engine loop or a mixed-down group clicks), then the slot is reused.
	void StopChannel( int nChannel )
	{
		if ( IsLiveChannel( nChannel ) && !g_channels[nChannel].pStream && !g_channels[nChannel].bPaused &&
			ma_sound_is_playing( &g_channels[nChannel].sound ) && !ma_sound_at_end( &g_channels[nChannel].sound ) )
		{
			if ( IsVoiceTraceOn() )
				fprintf( stderr, "BK_SOUND_TRACE: voice release at=%llu ch=%d sample=%p\n", (unsigned long long)ma_engine_get_time_in_pcm_frames( &g_engine ), nChannel, (void*)g_channels[nChannel].pSample );
			g_channels[nChannel].bReleasing = true;
			g_channels[nChannel].bStartRampPending = false;
			ma_sound_stop_with_fade_in_milliseconds( &g_channels[nChannel].sound, cSampleStopRampMs );
			return;
		}
		ResetChannel( nChannel );
	}

	// Savegame load: the loaded world knows nothing of the voices still
	// sounding, including any the engine's maps lost track of — sweep every
	// non-stream slot so orphaned loops cannot outlive the load.
	void StopAllSampleChannels()
	{
		for ( int i = 0; i < cMaxOpenChannels; ++i )
			if ( !g_channels[i].pStream )
				ResetChannel( i );
	}

	bool IsChannelPlaying( int nChannel )
	{
		return IsLiveChannel( nChannel ) &&
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
		if ( IsLiveChannel( nChannel ) )
		{
			ma_uint64 nCursor = 0;
			if ( ma_sound_get_cursor_in_pcm_frames( &g_channels[nChannel].sound, &nCursor ) == MA_SUCCESS )
				return static_cast<unsigned int>( nCursor );
		}
		return 0;
	}

	void SetChannelPosition( int nChannel, unsigned int nPosition )
	{
		if ( IsLiveChannel( nChannel ) )
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
		if ( IsLiveChannel( nChannel ) )
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
		pStream->pEndCallback.store( 0 );
		pStream->pUserData.store( 0 );
		pStream->nCallbackReaders.store( 0 );
		pStream->bClosing.store( false );
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
			pOpenStream->bClosing.store( true, std::memory_order_release );
			pOpenStream->pEndCallback.store( 0, std::memory_order_release );
			pOpenStream->pUserData.store( 0, std::memory_order_release );
			for ( int i = 0; i < cMaxOpenChannels; ++i )
				if ( g_channels[i].pStream == pOpenStream )
					ResetChannel( i );
			while ( pOpenStream->nCallbackReaders.load( std::memory_order_acquire ) != 0 )
				NPlatform::SleepMilliseconds( 0 );
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
			pOpenStream->bClosing.store( true, std::memory_order_release );
			pOpenStream->pEndCallback.store( 0, std::memory_order_release );
			pOpenStream->pUserData.store( 0, std::memory_order_release );
		}
	}

	void SetStreamEndCallback( void *pStream, TStreamCallback pCallback, void *pUserData )
	{
		if ( pStream )
		{
			SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
			pOpenStream->pUserData.store( pUserData, std::memory_order_release );
			pOpenStream->pEndCallback.store( pCallback, std::memory_order_release );
			pOpenStream->bClosing.store( false, std::memory_order_release );
		}
	}

	int PlayStream( void *pStream )
	{
		SOpenStream *pOpenStream = static_cast<SOpenStream*>( pStream );
		if ( !g_bEngineInitialized || !pOpenStream )
			return -1;

		const int nChannel = FindFreeStreamChannel();
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
		g_channels[nChannel].nStartSerial = ++g_nStartSerial;
		g_channels[nChannel].bStarted = true;
		ma_sound_set_looping( &g_channels[nChannel].sound, pOpenStream->bLooped ? MA_TRUE : MA_FALSE );
		ma_sound_set_end_callback( &g_channels[nChannel].sound, OpenStreamEndCallback, pOpenStream );
		// Streams ramp in from silence to their mix volume — no start transient.
		SeedSilentFader( nChannel );
		ApplyChannelPan( nChannel );
		StartChannelRamp( nChannel, cStreamStartRampMs );
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

// Test entry (tools/zig/sfx_module_test.cpp): plays a synthetic voice
// through this backend on an engine without a device and returns the
// rendered stereo mix, so a test can measure its gain and every step in the
// waveform. The voice is a constant 0.5 (a click or a gain jump shows as a
// step). Scenarios, each driven the way CSoundEngine drives a voice:
//   1 start: play paused, set volume and pan, unpause, then one more volume
//     update before the mixer's next period (the game updates every frame);
//   2 stop: a looped voice at its mix volume is stopped mid-play;
//   3 pause: a playing voice is paused and its volume updated while paused.
// Returns the frames rendered, or a negative number on failure; the engine
// is closed again either way. Not for use while the game's own engine runs.
extern "C" BK_EXPORT int STDCALL BkSFXRenderVoiceTest( int nScenario, float *pStereoOut, int nFrames )
{
	using namespace NAudioBackendImpl;
	if ( g_bEngineInitialized || !pStereoOut || nFrames <= 0 )
		return -1;

	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.noDevice = MA_TRUE;
	engineConfig.channels = 2;
	engineConfig.sampleRate = 44100;
	if ( ma_engine_init( &engineConfig, &g_engine ) != MA_SUCCESS )
		return -2;
	g_bEngineInitialized = true;

	// One second of a constant 0.5, 16-bit mono PCM in a RIFF/WAVE image.
	const unsigned int nSampleFrames = 44100;
	std::vector<char> wave( 44 + nSampleFrames * 2 );
	const unsigned int header[] = { 0x46464952u, 36 + nSampleFrames * 2, 0x45564157u, 0x20746d66u, 16, 0x00010001u, 44100, 88200, 0x00100002u, 0x61746164u, nSampleFrames * 2 };
	for ( unsigned int i = 0; i < sizeof( header ) / sizeof( header[0] ); ++i )
		for ( int b = 0; b < 4; ++b )
			wave[i * 4 + b] = char( ( header[i] >> ( 8 * b ) ) & 0xff );
	for ( unsigned int i = 0; i < nSampleFrames; ++i )
	{
		wave[44 + i * 2] = char( 0x00 );
		wave[44 + i * 2 + 1] = char( 0x40 );
	}
	void *pSample = LoadSampleFromMemory( &wave[0], int( wave.size() ), GetSampleMode2D() );

	int nRendered = 0;
	const int nChunk = 441;
	const int nVolume = 64;
	auto render = [&]( int nCount ) {
		while ( nCount > 0 && nRendered < nFrames )
		{
			const int nNow = Min( Min( nChunk, nCount ), nFrames - nRendered );
			ma_engine_read_pcm_frames( &g_engine, pStereoOut + nRendered * 2, ma_uint64( nNow ), 0 );
			nRendered += nNow;
			nCount -= nNow;
		}
	};

	int nResult = -3;
	if ( pSample )
	{
		SetSampleLoop( pSample, nScenario == 2 );
		const int nChannel = PlaySamplePaused( pSample );
		if ( nChannel >= 0 )
		{
			SetChannelVolume( nChannel, nVolume );
			SetChannelPan( nChannel, 128 );
			SetChannelPaused( nChannel, false );
			if ( nScenario == 1 )
			{
				SetChannelVolume( nChannel, nVolume );
				render( nFrames );
			}
			else if ( nScenario == 2 )
			{
				render( nFrames / 2 );
				StopChannel( nChannel );
				render( nFrames );
			}
			else if ( nScenario == 3 )
			{
				render( nFrames / 2 );
				SetChannelPaused( nChannel, true );
				SetChannelVolume( nChannel, nVolume );
				render( nFrames );
			}
			nResult = nRendered;
		}
		FreeSample( pSample );
	}
	CloseDevice();
	return nResult;
}

#endif // defined(SFX_USE_OPEN_AUDIO_BACKEND)
