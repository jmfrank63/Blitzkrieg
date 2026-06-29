#include "StdAfx.h"

#include "AudioFmodCompat.h"
#include "AudioBackend.h"

namespace NAudioBackend
{
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

	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos )
	{
		FSOUND_3D_SetAttributes( nChannel, const_cast<float*>(vPos.m), 0 );
	}
}
