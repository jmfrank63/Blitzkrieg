#ifndef BLITZKRIEG_PLATFORM_CLOCK_H
#define BLITZKRIEG_PLATFORM_CLOCK_H

#include <cstdint>
#include "Compiler.h"

#if defined(BK_PLATFORM_RUNTIME_BUILD)
#define BK_PLATFORM_RUNTIME_API BK_EXPORT
#elif defined(_WIN32) || defined(_WIN64)
#define BK_PLATFORM_RUNTIME_API BK_IMPORT
#else
#define BK_PLATFORM_RUNTIME_API BK_EXPORT
#endif

namespace NPlatform
{
	BK_PLATFORM_RUNTIME_API std::uint64_t MonotonicNanoseconds();
	BK_PLATFORM_RUNTIME_API std::uint64_t MonotonicMilliseconds64();
	BK_PLATFORM_RUNTIME_API std::uint32_t MonotonicMilliseconds();
	BK_PLATFORM_RUNTIME_API std::uint32_t MillisecondsElapsed( std::uint32_t start, std::uint32_t finish );
	BK_PLATFORM_RUNTIME_API double NanosecondsToSeconds( std::uint64_t nanoseconds );
	BK_PLATFORM_RUNTIME_API void SleepMilliseconds( std::uint32_t milliseconds );
	// A wait that lands within tens of microseconds of the duration asked for,
	// for callers pacing to a deadline. SleepMilliseconds cannot do this:
	// std::this_thread::sleep_for goes through nanosleep, which Darwin
	// coalesces to a millisecond or more whatever unit it is handed, so a
	// caller that needs the deadline has to spin out the difference.
	//
	// Returns false when the platform has no high-resolution timer, having
	// waited for nothing at all - the caller keeps whatever coarse strategy it
	// had. Windows is that case today: it has a high-resolution waitable timer
	// from Windows 10 1803 on, but nobody has measured the frame limiter there,
	// and an unmeasured change to frame pacing is not an improvement.
	BK_PLATFORM_RUNTIME_API bool SleepPreciseNanoseconds( std::uint64_t nanoseconds );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicExchangeU32( std::uint32_t *value, std::uint32_t replacement );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicIncrementU32( std::uint32_t *value );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicDecrementU32( std::uint32_t *value );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicCompareExchangeU32( std::uint32_t *value, std::uint32_t expected, std::uint32_t replacement );
}

#endif
