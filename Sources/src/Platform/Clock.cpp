#include "Clock.h"

#include <chrono>
#include <thread>

namespace NPlatform
{
	std::uint64_t MonotonicNanoseconds()
	{
		const auto now = std::chrono::steady_clock::now().time_since_epoch();
		return static_cast<std::uint64_t>( std::chrono::duration_cast<std::chrono::nanoseconds>( now ).count() );
	}

	std::uint64_t MonotonicMilliseconds64()
	{
		return MonotonicNanoseconds() / 1000000ULL;
	}

	std::uint32_t MonotonicMilliseconds()
	{
		return static_cast<std::uint32_t>( MonotonicMilliseconds64() );
	}

	std::uint32_t MillisecondsElapsed( const std::uint32_t start, const std::uint32_t finish )
	{
		return finish - start;
	}

	double NanosecondsToSeconds( const std::uint64_t nanoseconds )
	{
		return static_cast<double>( nanoseconds ) / 1000000000.0;
	}

	void SleepMilliseconds( const std::uint32_t milliseconds )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds( milliseconds ) );
	}
}
