#ifndef __GAMETIMER_H__
#define __GAMETIMER_H__
#pragma ONCE
enum
{
	TIMER_BASE_VALUE = 0x10050000,
	TIMER_SINGLE_TIMER	= TIMER_BASE_VALUE + 1,
	TIMER_GAME_TIMER		= TIMER_BASE_VALUE + 2
};
namespace NTimer
{
	typedef DWORD STime;
	inline float GetCoeffFromSpeed( const int nSpeed )
	{
		return nSpeed >= 0 ? nSpeed + 1 : 1.0f / fabsf( float(nSpeed - 1) );
	}
};
class CScaleTimer
{
	NTimer::STime prevTime;						// current dependent time
	NTimer::STime currTime;						// current independent time
	float fScale;											// time scaling
	float fError;											// time rounding error (for scaling)
public:
	CScaleTimer() : prevTime( 0 ), currTime( 0 ), fScale( 1 ), fError( 0 ) {  }
	void SetScale( float _fScale ) { fScale = _fScale; }
	const NTimer::STime& Get() const { return currTime; }
	void Reset( const NTimer::STime &time ) { prevTime = time; currTime = 0; fError = 0; }
	void Update( const NTimer::STime &time )
	{
		NTimer::STime dT = prevTime == 0 ? 0 : time - prevTime;
		prevTime = time;
		const float fdt = float( dT*fScale ) + fError;
		dT = NTimer::STime( MINT(fdt) );
		fError = fdt - float( dT );
		currTime += dT;
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &prevTime );
		saver.Add( 2, &currTime );
		saver.Add( 3, &fScale );
		saver.Add( 4, &fError );
		return 0;
	}
};
interface ITimeSlider : public IRefCount
{
	virtual NTimer::STime STDCALL GetDelta() = 0;
	virtual void STDCALL Reset() = 0;
};
interface IBaseTimer : public IRefCount
{
	virtual NTimer::STime STDCALL Get() = 0;
	virtual void STDCALL Set( const NTimer::STime &time ) = 0;
};
interface ISingleTimer : public IBaseTimer
{
	virtual void STDCALL Reset() = 0;
	virtual void STDCALL Pause( bool bPause ) = 0;
	virtual ITimeSlider* STDCALL CreateSlider() = 0;
	virtual void STDCALL Update( const NTimer::STime &time ) = 0;
	virtual bool STDCALL IsPaused() const = 0;
	virtual void STDCALL SetGuarantieFPS( const float fFPS ) = 0;
	virtual const float STDCALL GetGuarantieFPS() const = 0;
	virtual void STDCALL SetTimeScale( float scale ) = 0;
};
interface ISegmentTimer : public IBaseTimer
{
	virtual void STDCALL SetSegmentTime( const NTimer::STime &time ) = 0;
	virtual NTimer::STime STDCALL GetSegmentTime() = 0;
	virtual bool STDCALL BeginSegments( const NTimer::STime &time ) = 0;
	virtual bool STDCALL NextSegment() = 0;
	virtual int STDCALL GetSegment() = 0;
	virtual void STDCALL SetSegment( int nSegment ) = 0;
};
interface IGameTimer : public IRefCount
{
	enum { tidTypeID = TIMER_GAME_TIMER };
	virtual void STDCALL Init() = 0;
	virtual ISingleTimer* STDCALL GetGameTimer() = 0;
	virtual ISingleTimer* STDCALL GetSyncTimer() = 0;
	virtual ISingleTimer* STDCALL GetAbsTimer() = 0;
	virtual NTimer::STime STDCALL GetGameTime() = 0;
	virtual NTimer::STime STDCALL GetSyncTime() = 0;
	virtual NTimer::STime STDCALL GetAbsTime() = 0;
	virtual ISegmentTimer* STDCALL GetGameSegmentTimer() = 0;
	virtual ISegmentTimer* STDCALL GetSyncSegmentTimer() = 0;
	virtual NTimer::STime STDCALL GetGameSegmentTime() = 0;
	virtual NTimer::STime STDCALL GetSyncSegmentTime() = 0;
	virtual void STDCALL PauseGame( bool bPause, int nType = 0 ) = 0;
	virtual void STDCALL PauseSync( bool bPause, int nType = 0 ) = 0;
	virtual int STDCALL GetPauseReason() const = 0;
	virtual bool STDCALL HasPause( const int nReason ) const = 0;
	virtual void STDCALL SetGuarantieFPS( const float fFPS ) = 0;
	virtual void STDCALL Update( const NTimer::STime &time ) = 0;
	virtual int STDCALL SetSpeed( const int nSpeed ) = 0;
	virtual int STDCALL GetSpeed() const = 0;
};
#endif // __GAMETIMER_H__
