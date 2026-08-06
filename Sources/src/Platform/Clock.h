#ifndef BLITZKRIEG_PLATFORM_CLOCK_H
#define BLITZKRIEG_PLATFORM_CLOCK_H

#include <cstdint>

namespace NPlatform
{
	std::uint64_t MonotonicNanoseconds();
	std::uint64_t MonotonicMilliseconds64();
	std::uint32_t MonotonicMilliseconds();
	std::uint32_t MillisecondsElapsed( std::uint32_t start, std::uint32_t finish );
	double NanosecondsToSeconds( std::uint64_t nanoseconds );
	void SleepMilliseconds( std::uint32_t milliseconds );
	std::uint32_t AtomicExchangeU32( std::uint32_t *value, std::uint32_t replacement );
	std::uint32_t AtomicIncrementU32( std::uint32_t *value );
	std::uint32_t AtomicDecrementU32( std::uint32_t *value );
	std::uint32_t AtomicCompareExchangeU32( std::uint32_t *value, std::uint32_t expected, std::uint32_t replacement );
}

#endif
