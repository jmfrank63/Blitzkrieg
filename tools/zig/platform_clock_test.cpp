#include "Platform/Clock.h"
#include <cstdint>
// HPTimer.cpp defines int64 as the project headers do (long long); std::int64_t is long on
// LP64 Linux, which would mangle the HPTimer calls into symbols nobody defines.
using int64 = long long;
#include "Misc/HPTimer.h"


int main()
{
	const std::uint64_t first = NPlatform::MonotonicNanoseconds();
	const std::uint64_t second = NPlatform::MonotonicNanoseconds();
	if ( second < first )
		return 1;

	if ( NPlatform::MillisecondsElapsed( 0xfffffff0U, 0x00000010U ) != 32U )
		return 2;
	if ( NPlatform::NanosecondsToSeconds( 250000000ULL ) < 0.249999 || NPlatform::NanosecondsToSeconds( 250000000ULL ) > 0.250001 )
		return 3;

	const std::uint64_t sleepStart = NPlatform::MonotonicNanoseconds();
	NPlatform::SleepMilliseconds( 10 );
	const std::uint64_t sleepElapsed = NPlatform::MonotonicNanoseconds() - sleepStart;
	const double sleepSeconds = NPlatform::NanosecondsToSeconds( sleepElapsed );
	if ( sleepSeconds < 0.005 || sleepSeconds > 0.250 )
		return 4;

	NHPTimer::STime timer = 0;
	NHPTimer::GetTime( &timer );
	NPlatform::SleepMilliseconds( 2 );
	const double timerElapsed = NHPTimer::GetTimePassed( &timer );
	if ( timerElapsed <= 0.0 || timerElapsed > 0.250 )
		return 5;

	std::uint32_t atomic = 10;
	if ( NPlatform::AtomicExchangeU32( &atomic, 20 ) != 10 || atomic != 20 )
		return 6;
	if ( NPlatform::AtomicIncrementU32( &atomic ) != 21 || NPlatform::AtomicDecrementU32( &atomic ) != 20 )
		return 7;
	if ( NPlatform::AtomicCompareExchangeU32( &atomic, 20, 42 ) != 20 || atomic != 42 )
		return 8;
	if ( NPlatform::AtomicCompareExchangeU32( &atomic, 20, 99 ) != 42 || atomic != 42 )
		return 9;

	return 0;
}
