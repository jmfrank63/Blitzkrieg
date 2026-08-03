#ifndef __STREAMFADEOFF_H__
#define __STREAMFADEOFF_H__




class CStreamFadeOff : private CThread
{
	DECLARE_SERIALIZE;
	interface ISFX *pSFX;
	bool bStopping;
	float fVolume;
	DWORD timeAccumulator;
	float fVolumeSpeed;
	std::atomic<bool> bThreadStarted;
	std::atomic<bool> bWorkerFinished;
	std::atomic<int> nFinishedPending;

	virtual void Step();
	bool Segment( const int nTimeDelta );
	void InitConsts();
public:
	CStreamFadeOff()
		: CThread( 100 ), pSFX( 0 ), bStopping( false ), fVolume( 0 ), timeAccumulator( 0 ), fVolumeSpeed( 0 ),
		  bThreadStarted( false ), bWorkerFinished( false ), nFinishedPending( 0 ) {}
	~CStreamFadeOff();
	void Fade( const unsigned int nTimeToFade );
	void Clear();
	void Init() { InitConsts(); }
	bool IsFading() const;
	bool ConsumeFinished() { return nFinishedPending.exchange( 0 ) != 0; }
};
#endif // __STREAMFADEOFF_H__
