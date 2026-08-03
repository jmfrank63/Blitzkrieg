#ifndef __GAMETIMERINTERNAL_H__
#define __GAMETIMERINTERNAL_H__
#pragma ONCE
#include "GameTimer.h"
#include "../AILogic/AIConsts.h"
static const NTimer::STime INITIAL_TIME = SAIConsts::AI_SEGMENT_DURATION;
class CTimeSlider : public ITimeSlider
{
	OBJECT_NORMAL_METHODS( CTimeSlider );
	DECLARE_SERIALIZE;
	CPtr<ISingleTimer> pTimer;						// timer, this differentiator attached to
	NTimer::STime timeLastTime;						// last query time
public:
	CTimeSlider() : timeLastTime( 0 ) {  }
	CTimeSlider( ISingleTimer *_pTimer ) : pTimer( _pTimer ) { Reset(); }
	virtual NTimer::STime STDCALL GetDelta()
	{
		NTimer::STime timeCurrTime = pTimer->Get();
		NTimer::STime timeDiffTime = timeCurrTime - timeLastTime;
		timeLastTime = timeCurrTime;
		return timeDiffTime;
	}
	virtual void STDCALL Reset() { timeLastTime = pTimer->Get(); }
};
class CSegmentTimer : public ISegmentTimer
{
	OBJECT_NORMAL_METHODS( CSegmentTimer );
	DECLARE_SERIALIZE;
	NTimer::STime prevTime;						// last segment independent time
	NTimer::STime currTime;						// current independent time
	NTimer::STime tSegmentTime;				// one segment duration
	int nSegment;											// current segment
public:
	CSegmentTimer()
		: prevTime( INITIAL_TIME ), currTime( INITIAL_TIME ), tSegmentTime( SAIConsts::AI_SEGMENT_DURATION ), nSegment( 0 ) {  }
	virtual NTimer::STime STDCALL Get() { return prevTime; }
	virtual void STDCALL Set( const NTimer::STime &time ) { prevTime = time - ( time % tSegmentTime ); }
	virtual void STDCALL SetSegmentTime( const NTimer::STime &time ) { tSegmentTime = time; }
	virtual NTimer::STime STDCALL GetSegmentTime() { return tSegmentTime; }
	virtual bool STDCALL BeginSegments( const NTimer::STime &time )
	{
		if ( prevTime > time )
			Set( time );
		currTime = time;
		return currTime - prevTime >= tSegmentTime;
	}
	virtual bool STDCALL NextSegment()
	{
		NI_ASSERT_TF( currTime >= prevTime, "currrent segment time less then previous!!!", return false );
		NTimer::STime tDiff = currTime - prevTime;
		if ( tDiff >= tSegmentTime )
		{
			prevTime += tSegmentTime;
			++nSegment;
			return true;
		}
		return false;
	}
	virtual int STDCALL GetSegment() { return nSegment; }
	virtual void STDCALL SetSegment( int _nSegment ) { nSegment = _nSegment; }
};
class CSingleTimer : public ISingleTimer
{
	OBJECT_NORMAL_METHODS( CSingleTimer );
	DECLARE_SERIALIZE;
	NTimer::STime prevTime;						// current dependent time
	NTimer::STime currTime;						// current independent time
	bool bPaused;											// is timer paused
	float fTimeScale;									// time scaling
	float fTimeError;									// time rounding error
	float fGuarantieFPS;							// FPS to guarantie (for movie sequence capturing)
	int nGuarantieTimeStep;						// 
public:
	CSingleTimer()
		: prevTime( 0 ), currTime( INITIAL_TIME ), bPaused( false ), fTimeScale( 1 ), fTimeError( 0 ),
			fGuarantieFPS( 0 ), nGuarantieTimeStep( 0 ) {  }
	virtual NTimer::STime STDCALL Get() { return currTime; }
	virtual void STDCALL Set( const NTimer::STime &time ) { currTime = time; }
	virtual void STDCALL Reset() { prevTime = 0; currTime = INITIAL_TIME; }
	virtual void STDCALL Pause( bool bPause ) { bPaused = bPause; }
	virtual ITimeSlider* STDCALL CreateSlider() { return new CTimeSlider( this ); }
	virtual void STDCALL Update( const NTimer::STime &time );
	virtual bool STDCALL IsPaused() const { return bPaused; }
	virtual void STDCALL SetGuarantieFPS( const float fFPS );
	virtual const float STDCALL GetGuarantieFPS() const { return fGuarantieFPS; }
	virtual void STDCALL SetTimeScale( float scale ) { fTimeScale = scale; }
};
class CGameTimer : public IGameTimer
{
	OBJECT_NORMAL_METHODS( CGameTimer );
	DECLARE_SERIALIZE;
	CPtr<ISingleTimer> pGameTimer;				// game timer
	CPtr<ISingleTimer> pSyncTimer;				// syncronization timer
	CPtr<ISingleTimer> pAbsTimer;					// absolute time timer - don't affected by time coeff
	CPtr<ISegmentTimer> pGameSegmentTimer;
	CPtr<ISegmentTimer> pSyncSegmentTimer;
	typedef std::unordered_map<int, int> CPausesMap;
	CPausesMap gamepauses;								// game pauses map
	CPausesMap syncpauses;								// sync pauses map
	int nPauseReason;											// highest game pause reason
	int nTimeCoeff;												// time scaling coeff. [-N..N]
	float fGuarantieFPS;									// game FPS to guarantie with this times
	int nGuarantieTimeStep;								// guarantie game time step
	void SetupTimeScaleForTimers();
	void DoPause( bool bPause, int nType, ISingleTimer *pTimer, CPausesMap &pauses );
public:
	void STDCALL Init();
	ISingleTimer* STDCALL GetGameTimer() { return pGameTimer; }
	ISingleTimer* STDCALL GetSyncTimer() { return pSyncTimer; }
	ISingleTimer* STDCALL GetAbsTimer() { return pAbsTimer; }
	NTimer::STime STDCALL GetGameTime() { return pGameTimer->Get(); }
	NTimer::STime STDCALL GetSyncTime() { return pSyncTimer->Get(); }
	NTimer::STime STDCALL GetAbsTime() { return pAbsTimer->Get(); }
	ISegmentTimer* STDCALL GetGameSegmentTimer() { return pGameSegmentTimer; }
	ISegmentTimer* STDCALL GetSyncSegmentTimer() { return pSyncSegmentTimer; }
	NTimer::STime STDCALL GetGameSegmentTime() { return pGameSegmentTimer->Get(); }
	NTimer::STime STDCALL GetSyncSegmentTime() { return pSyncSegmentTimer->Get(); }
	void STDCALL PauseGame( bool bPause, int nType = 0 );
	void STDCALL PauseSync( bool bPause, int nType = 0 );
	int STDCALL GetPauseReason() const { return nPauseReason; }
	bool STDCALL HasPause( const int nReason ) const;
	void STDCALL SetGuarantieFPS( const float fFPS );
	void STDCALL Update( const NTimer::STime &time );
	int STDCALL SetSpeed( const int nSpeed );
	int STDCALL GetSpeed() const { return nTimeCoeff; }
};
#endif // __GAMETIMERINTERNAL_H__
