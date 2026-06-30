#include "StdAfx.h"

#include "AudioBackendImpl.h"

namespace NAudioBackend
{
	bool IsVersionSupported()
	{
		return NAudioBackendImpl::IsVersionSupported();
	}

	void PrepareDeviceSearch()
	{
		NAudioBackendImpl::PrepareDeviceSearch();
	}

	int GetNumDrivers()
	{
		return NAudioBackendImpl::GetNumDrivers();
	}

	SDriverInfo GetDriverInfo( int nDriver )
	{
		return NAudioBackendImpl::GetDriverInfo( nDriver );
	}

	IRefCount* GetOutputHandle()
	{
		return NAudioBackendImpl::GetOutputHandle();
	}

	void SetDriver( int nDriver )
	{
		NAudioBackendImpl::SetDriver( nDriver );
	}

	bool InitDevice( HWND hWnd, ESFXOutputType output, int nMixRate, int nMaxChannels, const SDriverInfo &driverInfo, bool *pSoundCardPresent )
	{
		return NAudioBackendImpl::InitDevice( hWnd, output, nMixRate, nMaxChannels, driverInfo, pSoundCardPresent );
	}

	void CloseDevice()
	{
		NAudioBackendImpl::CloseDevice();
	}

	void DebugTraceMixer()
	{
		NAudioBackendImpl::DebugTraceMixer();
	}

	void SetDistanceFactor( float fFactor )
	{
		NAudioBackendImpl::SetDistanceFactor( fFactor );
	}

	void SetRolloffFactor( float fFactor )
	{
		NAudioBackendImpl::SetRolloffFactor( fFactor );
	}

	void FreeSample( void *pSample )
	{
		NAudioBackendImpl::FreeSample( pSample );
	}

	void* LoadSampleFromMemory( const char *pData, int nSize, int nMode )
	{
		return NAudioBackendImpl::LoadSampleFromMemory( pData, nSize, nMode );
	}

	void SetSampleMinDistance( void *pSample, float fMinDistance )
	{
		NAudioBackendImpl::SetSampleMinDistance( pSample, fMinDistance );
	}

	void SetSampleLoop( void *pSample, bool bEnable )
	{
		NAudioBackendImpl::SetSampleLoop( pSample, bEnable );
	}

	void SetSampleLoopPoints( void *pSample, int nStart, int nEnd )
	{
		NAudioBackendImpl::SetSampleLoopPoints( pSample, nStart, nEnd );
	}

	unsigned int GetSampleLength( void *pSample )
	{
		return NAudioBackendImpl::GetSampleLength( pSample );
	}

	unsigned int GetSampleRate( void *pSample )
	{
		return NAudioBackendImpl::GetSampleRate( pSample );
	}

	int GetSampleMode2D()
	{
		return NAudioBackendImpl::GetSampleMode2D();
	}

	int GetSampleMode3D()
	{
		return NAudioBackendImpl::GetSampleMode3D();
	}

	bool IsChannelPlayingSample( int nChannel, void *pSample )
	{
		return NAudioBackendImpl::IsChannelPlayingSample( nChannel, pSample );
	}

	int PlaySample( void *pSample )
	{
		return NAudioBackendImpl::PlaySample( pSample );
	}

	int PlaySamplePaused( void *pSample )
	{
		return NAudioBackendImpl::PlaySamplePaused( pSample );
	}

	void SetChannelVolume( int nChannel, int nVolume )
	{
		NAudioBackendImpl::SetChannelVolume( nChannel, nVolume );
	}

	void SetChannelPan( int nChannel, int nPan )
	{
		NAudioBackendImpl::SetChannelPan( nChannel, nPan );
	}

	void SetChannelPaused( int nChannel, bool bPaused )
	{
		NAudioBackendImpl::SetChannelPaused( nChannel, bPaused );
	}

	void StopChannel( int nChannel )
	{
		NAudioBackendImpl::StopChannel( nChannel );
	}

	bool IsChannelPlaying( int nChannel )
	{
		return NAudioBackendImpl::IsChannelPlaying( nChannel );
	}

	int GetChannelsPlaying()
	{
		return NAudioBackendImpl::GetChannelsPlaying();
	}

	unsigned int GetChannelPosition( int nChannel )
	{
		return NAudioBackendImpl::GetChannelPosition( nChannel );
	}

	void SetChannelPosition( int nChannel, unsigned int nPosition )
	{
		NAudioBackendImpl::SetChannelPosition( nChannel, nPosition );
	}

	int GetLastError()
	{
		return NAudioBackendImpl::GetLastError();
	}

	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos )
	{
		NAudioBackendImpl::SetChannel3DAttributes( nChannel, vPos );
	}

	void* OpenStream( const char *pszFileName, bool bLooped )
	{
		return NAudioBackendImpl::OpenStream( pszFileName, bLooped );
	}

	void CloseStream( void *pStream )
	{
		NAudioBackendImpl::CloseStream( pStream );
	}

	void ClearStreamCallbacks( void *pStream )
	{
		NAudioBackendImpl::ClearStreamCallbacks( pStream );
	}

	void SetStreamEndCallback( void *pStream, TStreamCallback pCallback, void *pUserData )
	{
		NAudioBackendImpl::SetStreamEndCallback( pStream, pCallback, pUserData );
	}

	int PlayStream( void *pStream )
	{
		return NAudioBackendImpl::PlayStream( pStream );
	}

	void SetStreamChannelPan( int nChannel )
	{
		NAudioBackendImpl::SetStreamChannelPan( nChannel );
	}
}
