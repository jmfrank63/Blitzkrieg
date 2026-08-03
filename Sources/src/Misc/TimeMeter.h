#ifndef __TIMEMETER_H__
#define __TIMEMETER_H__
#pragma ONCE
#include "..\Misc\HPTimer.h"
struct SDebugTraceReporter
{
	static void Report( const char *pszName, const int nMilliseconds )
	{
		NStr::DebugTrace( "*** Time meter for \"%s\" = %d\n", pszName, nMilliseconds );
	}
};
struct SConsoleStreamReporter
{
	static void Report( const char *pszName, const int nMilliseconds )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, 
			 NStr::Format("*** Time meter for \"%s\" = %d", pszName, nMilliseconds), 0xffffffff, true );
	}
};
template <class TReporter = SDebugTraceReporter>
class CTimeMeter
{
	NHPTimer::STime time;
public:
	CTimeMeter() { Reset(); }
	void Reset()
	{
		NHPTimer::GetTime( &time );
	}
	void Sample( const char *pszName )
	{
		TReporter::Report( pszName, NHPTimer::GetTimePassed( &time ) * 1000.0 );
	}
};
#endif // __TIMEMETER_H__