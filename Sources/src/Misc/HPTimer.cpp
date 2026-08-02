#include <cstdint>

using int64 = std::int64_t;

#include "HPTimer.h"
#include "../Platform/Clock.h"

double NHPTimer::GetSeconds( const NHPTimer::STime &a )
{
	return NPlatform::NanosecondsToSeconds( static_cast<std::uint64_t>( a ) );
}
double NHPTimer::GetClockRate()
{
	return 1000000000.0;
}
void NHPTimer::GetTime( STime *pTime )
{
	*pTime = static_cast<STime>( NPlatform::MonotonicNanoseconds() );
}
double NHPTimer::GetTimePassed( STime *pTime )
{
	STime old(*pTime );
	GetTime( pTime );
	return GetSeconds( *pTime - old );
}
