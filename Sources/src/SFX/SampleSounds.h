#ifndef __SAMPLESOUNDS_H__
#define __SAMPLESOUNDS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base shared sound sample resource
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundSample : public ISharedResource
{
	OBJECT_NORMAL_METHODS( CSoundSample );
	SHARED_RESOURCE_METHODS( nRefData.a, "Sound" );
	//
	FSOUND_SAMPLE *sample;								// FMOD sound sample
	int nMode;														// FMOD sound sample mode
	bool bLooped;													// is this sample looped ?
	float fMinDistance;										// minimal distance
	//
	void Close() { if ( sample ) FSOUND_Sample_Free( sample ); sample = 0; }
public:
	CSoundSample() : sample( 0 ), nMode( 0 ), fMinDistance( 45 ) {  }
	~CSoundSample() { Close(); }
	//
	void SetSample( FSOUND_SAMPLE *_sample ) 
	{ 
		Close(); 
		sample = _sample; 
		if ( sample )
			FSOUND_Sample_SetMinMaxDistance( sample, fMinDistance, 1000000000.0f );
	}
	int GetMode() const { return nMode; }
	bool IsLooped() const { return bLooped; }
	FSOUND_SAMPLE* GetInternalContainer() { Load(); return sample; }
	//
	void Set3D( bool b3D ) { nMode = b3D ? FSOUND_HW3D : FSOUND_2D; }
	void SetLoop( bool bEnable );
	void SetMinDistance( float _fMinDistance ) 
	{ 
		fMinDistance = _fMinDistance;
		if ( sample )
			FSOUND_Sample_SetMinMaxDistance( sample, fMinDistance, 1000000000.0f );
	}
	//
	void STDCALL SwapData( ISharedResource *pResource );
	// internal container clearing
	void STDCALL ClearInternalContainer() {  }
	bool STDCALL Load( const bool bPreLoad = false );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** other sounds
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseSound : public ISound
{
	DECLARE_SERIALIZE;
	//
	CPtr<CSoundSample> pSample;
	int nChannel;
public:
	CBaseSound() : nChannel( -1 ) {  }
	virtual ~CBaseSound() {  }
	//
	void SetSample( CSoundSample *_pSample ) { pSample = _pSample; }
	CSoundSample* GetSample() { return pSample; }
	int GetChannel() const { return nChannel; }
	void SetChannel( int _nChannel ) { nChannel = _nChannel; }
	//
	bool IsPlaying() 
	{ 
		if ( (nChannel != -1) && FSOUND_IsPlaying(nChannel) )
			return FSOUND_GetCurrentSample( nChannel ) == pSample->GetInternalContainer();
		else
			return false;
	}
	// distance
	void STDCALL SetMinDistance( float fDistance ) { pSample->SetMinDistance( fDistance ); }
	// looping
	void STDCALL SetLooping( bool bEnable, int nStart = -1, int nEnd = -1 )
	{
		FSOUND_Sample_SetMode( pSample->GetInternalContainer(), bEnable ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF );
		if ( (nStart != -1) && (nEnd != -1) )
			FSOUND_Sample_SetLoopPoints( pSample->GetInternalContainer(), nStart, nEnd );
	}

	unsigned int STDCALL GetLenght()
	{
		return FSOUND_Sample_GetLength( pSample->GetInternalContainer() );
	}
	unsigned int STDCALL GetSampleRate()
	{
		int freq = 44000;
		FSOUND_Sample_GetDefaults( pSample->GetInternalContainer(), &freq, 0, 0, 0 );
		return freq;
	}

	void STDCALL SetVolume( float nVolume ) {  }
	float STDCALL GetVolume() const { return 1.0f; }
	void STDCALL SetPan( float nPan ) {  }
	float STDCALL GetPan() const { return 0.0f; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound2D : public CBaseSound
{
	OBJECT_NORMAL_METHODS( CSound2D );
	DECLARE_SERIALIZE;
	//
	float fVolume;
	float fPan;
public:
	CSound2D() : fVolume( 1.0f ), fPan( 0.0f ) {  }
	virtual ~CSound2D() {  }
	// visiting
	int STDCALL Visit( interface ISFXVisitor *pVisitor );
	//
	int STDCALL Play() 
	{ 
		int nChannel = FSOUND_PlaySound( FSOUND_FREE, GetSample()->GetInternalContainer() );
		SetChannel( nChannel );
		return nChannel;
	}
	void STDCALL SetPosition( const CVec3 &vPos3 ) {  }
	const CVec3 STDCALL GetPosition() { return VNULL3; }

	void STDCALL SetVolume( float _fVolume ) { fVolume = _fVolume; }
	float STDCALL GetVolume() const { return fVolume; }
	void STDCALL SetPan( float _fPan ) { fPan = _fPan; }
	float STDCALL GetPan() const { return fPan; }

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound3D : public CBaseSound
{
	OBJECT_NORMAL_METHODS( CSound3D );
	DECLARE_SERIALIZE;
	//
	CVec3 vPos;														// current position
	bool bDopplerFlag;
	NTimer::STime lastUpdateTime;
	CVec3 vLastPos;
public:
	CSound3D() : bDopplerFlag( 0 ), lastUpdateTime( 0 ), vLastPos( VNULL3 ) {  }
	virtual ~CSound3D() {}
	// visiting
	int STDCALL Visit( interface ISFXVisitor *pVisitor );
	//
	int STDCALL Play();
	void STDCALL SetDopplerFlag( bool bDoppler ) { bDopplerFlag = bDoppler; }
	void STDCALL SetPosition( const CVec3 &vPos3 );
	const CVec3 STDCALL GetPosition() { return vPos; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SAMPLESOUNDS_H__
