#ifndef __STREAMFADEOFF_H__
#define __STREAMFADEOFF_H__

class CStreamFadeOff
{
	DECLARE_SERIALIZE;
	
	HANDLE hThread;
	interface ISFX * pSFX;													//
	bool bStopping; // Added to guard against re-entrant Clear() calls
	
	float fVolume;

	HANDLE hFinishReport;
	HANDLE hStopCommand;

	DWORD timeAccumulator;
	float fVolumeSpeed;										// speed of decrease volume

	// Set by the fade thread when the fade reached silence; consumed by the
	// engine's main-thread Update. The fade thread must NEVER call
	// StopStream itself: closing/opening streams touches the miniaudio node
	// graph, and doing that off the main thread deadlocks against the mixer
	// (fade thread spins on a bus lock the audio thread holds while the
	// audio thread waits for the fade thread in Clear()).
	volatile LONG nFinishedPending;

	void Start();
	void Stop( bool bReachedSilence );

	bool Segment( const int nTimeDelta );
	bool HaveToRun();

	void InitConsts();
public:

	CStreamFadeOff() : timeAccumulator( 0 ), pSFX( 0 ), hThread( 0 ), hFinishReport( 0 ), hStopCommand( 0 ), bStopping( false ), nFinishedPending( 0 ) {}
	~CStreamFadeOff();
	void Fade( const unsigned int nTimeToFade );		// time is in millisecond

	void Clear();													// delete all objects, close handles
	void Init() { InitConsts(); }
	bool IsFading() const;
	// main-thread poll: true once per completed fade
	bool ConsumeFinished() { return InterlockedExchange( &nFinishedPending, 0 ) != 0; }

	friend DWORD WINAPI TheThreadProc( LPVOID lpParameter );
};
#endif // __STREAMFADEOFF_H__
