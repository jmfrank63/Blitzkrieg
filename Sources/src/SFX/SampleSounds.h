#ifndef __SAMPLESOUNDS_H__
#define __SAMPLESOUNDS_H__
class CSoundSample : public ISharedResource
{
	OBJECT_NORMAL_METHODS( CSoundSample );
	SHARED_RESOURCE_METHODS( nRefData.a, "Sound" );
	void *sample;
	int nMode;
	bool bLooped;													// is this sample looped ?
	float fMinDistance;										// minimal distance
	void Close();
public:
	CSoundSample() : sample( 0 ), nMode( 0 ), bLooped( false ), fMinDistance( 45 ) {  }
	~CSoundSample() { Close(); }
	void SetSample( void *_sample );
	int GetMode() const { return nMode; }
	bool IsLooped() const { return bLooped; }
	void* GetInternalContainer() { Load(); return sample; }
	void Set3D( bool b3D );
	void SetLoop( bool bEnable );
	void SetMinDistance( float _fMinDistance );
	void STDCALL SwapData( ISharedResource *pResource );
	void STDCALL ClearInternalContainer() {  }
	bool STDCALL Load( const bool bPreLoad = false );
};
class CBaseSound : public ISound
{
	DECLARE_SERIALIZE;
	CPtr<CSoundSample> pSample;
	int nChannel;
public:
	CBaseSound() : nChannel( -1 ) {  }
	virtual ~CBaseSound() {  }
	void SetSample( CSoundSample *_pSample ) { pSample = _pSample; }
	CSoundSample* GetSample() { return pSample; }
	int GetChannel() const { return nChannel; }
	void SetChannel( int _nChannel ) { nChannel = _nChannel; }
	bool IsPlaying();
	void STDCALL SetMinDistance( float fDistance ) { pSample->SetMinDistance( fDistance ); }
	void STDCALL SetLooping( bool bEnable, int nStart = -1, int nEnd = -1 );

	unsigned int STDCALL GetLenght();
	unsigned int STDCALL GetSampleRate();

	void STDCALL SetVolume( float nVolume ) {  }
	float STDCALL GetVolume() const { return 1.0f; }
	void STDCALL SetPan( float nPan ) {  }
	float STDCALL GetPan() const { return 0.0f; }
};
class CSound2D : public CBaseSound
{
	OBJECT_NORMAL_METHODS( CSound2D );
	DECLARE_SERIALIZE;
	float fVolume;
	float fPan;
public:
	CSound2D() : fVolume( 1.0f ), fPan( 0.0f ) {  }
	virtual ~CSound2D() {  }
	int STDCALL Visit( interface ISFXVisitor *pVisitor );
	int STDCALL Play();
	void STDCALL SetPosition( const CVec3 &vPos3 ) {  }
	const CVec3 STDCALL GetPosition() { return VNULL3; }

	void STDCALL SetVolume( float _fVolume ) { fVolume = _fVolume; }
	float STDCALL GetVolume() const { return fVolume; }
	void STDCALL SetPan( float _fPan ) { fPan = _fPan; }
	float STDCALL GetPan() const { return fPan; }

};
class CSound3D : public CBaseSound
{
	OBJECT_NORMAL_METHODS( CSound3D );
	DECLARE_SERIALIZE;
	CVec3 vPos;														// current position
	bool bDopplerFlag;
	NTimer::STime lastUpdateTime;
	CVec3 vLastPos;
public:
	CSound3D() : bDopplerFlag( 0 ), lastUpdateTime( 0 ), vLastPos( VNULL3 ) {  }
	virtual ~CSound3D() {}
	int STDCALL Visit( interface ISFXVisitor *pVisitor );
	int STDCALL Play();
	void STDCALL SetDopplerFlag( bool bDoppler ) { bDopplerFlag = bDoppler; }
	void STDCALL SetPosition( const CVec3 &vPos3 );
	const CVec3 STDCALL GetPosition() { return vPos; }
};
#endif // __SAMPLESOUNDS_H__
