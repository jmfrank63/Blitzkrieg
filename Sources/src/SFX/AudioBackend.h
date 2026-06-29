#ifndef __AUDIOBACKEND_H__
#define __AUDIOBACKEND_H__

namespace NAudioBackend
{
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
	void SetChannel3DAttributes( int nChannel, const CVec3 &vPos );
}

#endif // __AUDIOBACKEND_H__
