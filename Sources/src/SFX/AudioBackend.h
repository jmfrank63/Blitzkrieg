#ifndef __AUDIOBACKEND_H__
#define __AUDIOBACKEND_H__

namespace NAudioBackend
{
	typedef signed char (STDCALL *TStreamCallback)( void *pStream, void *pBuffer, int nLength, void *pUserData );

	struct SDriverInfo
	{
		std::string szDriverName;
		bool isHardware3DAccelerated;
		bool supportEAXReverb;
		bool supportA3DOcclusions;
		bool supportA3DReflections;
		bool supportReverb;
	};

	bool IsVersionSupported();
	void PrepareDeviceSearch();
	int GetNumDrivers();
	SDriverInfo GetDriverInfo( int nDriver );
	IRefCount* GetOutputHandle();
	void SetDriver( int nDriver );
	bool InitDevice( HWND hWnd, ESFXOutputType output, int nMixRate, int nMaxChannels, const SDriverInfo &driverInfo, bool *pSoundCardPresent );
	void CloseDevice();
	void DebugTraceMixer();
	void SetDistanceFactor( float fFactor );
	void SetRolloffFactor( float fFactor );
	void FreeSample( void *pSample );
	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode );
	void SetSampleMinDistance( void *pSample, float fMinDistance );
	void SetSampleLoop( void *pSample, bool bEnable );
	void SetSampleLoopPoints( void *pSample, int nStart, int nEnd );
	unsigned int GetSampleLength( void *pSample );
	unsigned int GetSampleRate( void *pSample );
	int GetSampleMode2D();
	int GetSampleMode3D();
	bool IsChannelPlayingSample( int nChannel, void *pSample );
	int PlaySample( void *pSample );
	int PlaySamplePaused( void *pSample );
	void SetChannelVolume( int nChannel, int nVolume );
	void SetChannelPan( int nChannel, int nPan );
	void SetChannelPaused( int nChannel, bool bPaused );
	void StopChannel( int nChannel );
	bool IsChannelPlaying( int nChannel );
	int GetChannelsPlaying();
	unsigned int GetChannelPosition( int nChannel );
	void SetChannelPosition( int nChannel, unsigned int nPosition );
	int GetLastError();
	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos );
	void* OpenStream( const char *pszFileName, bool bLooped );
	void CloseStream( void *pStream );
	void ClearStreamCallbacks( void *pStream );
	void SetStreamEndCallback( void *pStream, TStreamCallback pCallback, void *pUserData );
	int PlayStream( void *pStream );
	void SetStreamChannelPan( int nChannel );
}

#endif // __AUDIOBACKEND_H__
