#include "Platform/Clock.h"
#include <cstdint>
using int64 = std::int64_t;
#include "Misc/HPTimer.h"

#include <cstdio>

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

	std::printf( "clock monotonic: %.3f ms, hptimer: %.3f ms\n", sleepSeconds * 1000.0, timerElapsed * 1000.0 );
	return 0;
}
