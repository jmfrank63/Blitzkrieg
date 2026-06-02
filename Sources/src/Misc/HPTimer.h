#ifndef __HPTIMER_H_
#define __HPTIMER_H_
namespace NHPTimer
{
	typedef int64 STime;
	double GetSeconds( const STime &a );
	void GetTime( STime *pTime );
	double GetTimePassed( STime *pTime );
	double GetClockRate();
};
#endif