#ifndef BLITZKRIEG_PLATFORM_CLOCK_H
#define BLITZKRIEG_PLATFORM_CLOCK_H



namespace NPlatform
{
	std::uint64_t MonotonicNanoseconds();
	std::uint64_t MonotonicMilliseconds64();
	std::uint32_t MonotonicMilliseconds();
	std::uint32_t MillisecondsElapsed( std::uint32_t start, std::uint32_t finish );
	double NanosecondsToSeconds( std::uint64_t nanoseconds );
	void SleepMilliseconds( std::uint32_t milliseconds );
}

#endif
