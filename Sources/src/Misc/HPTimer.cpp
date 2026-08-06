#include <cstdint>

// Keep the legacy public ABI stable across LP64 POSIX targets. The shared
// project headers define int64 as long long, while std::int64_t is long on
// x86_64 Linux; using the latter here changes the mangled HPTimer symbols.
using int64 = long long;

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
