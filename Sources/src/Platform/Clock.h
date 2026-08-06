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
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicExchangeU32( std::uint32_t *value, std::uint32_t replacement );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicIncrementU32( std::uint32_t *value );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicDecrementU32( std::uint32_t *value );
	BK_PLATFORM_RUNTIME_API std::uint32_t AtomicCompareExchangeU32( std::uint32_t *value, std::uint32_t expected, std::uint32_t replacement );
}

#endif
