#ifndef __STREAMFADEOFF_H__
#define __STREAMFADEOFF_H__

class CStreamFadeOff
{
	DECLARE_SERIALIZE;
	
	HANDLE hThread;
	interface ISFX * pSFX;													//
	
	float fVolume;

	HANDLE hFinishReport;
	HANDLE hStopCommand;

	DWORD timeAccumulator;
	float fVolumeSpeed;										// speed of decrease volume

	void Start();
	void Stop();

	bool Segment( const int nTimeDelta );
	bool HaveToRun();

	void InitConsts();
public:

	CStreamFadeOff() : timeAccumulator( 0 ), pSFX( 0 ), hThread( 0 ), hFinishReport( 0 ), hStopCommand( 0 ) {}
	~CStreamFadeOff();
	void Fade( const unsigned int nTimeToFade );		// time is in millisecond
	
	void Clear();													// delete all objects, close handles
	void Init() { InitConsts(); }
	bool IsFading() const;

	friend DWORD WINAPI TheThreadProc( LPVOID lpParameter );
};
#endif // __STREAMFADEOFF_H__
