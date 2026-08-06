#include "Clock.h"

#include <chrono>
#include <atomic>
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

	static std::atomic<std::uint32_t> &AtomicRef( std::uint32_t *value )
	{
		return *reinterpret_cast<std::atomic<std::uint32_t> *>( value );
	}

	std::uint32_t AtomicExchangeU32( std::uint32_t *value, const std::uint32_t replacement )
	{
		return AtomicRef( value ).exchange( replacement, std::memory_order_seq_cst );
	}

	std::uint32_t AtomicIncrementU32( std::uint32_t *value )
	{
		return AtomicRef( value ).fetch_add( 1, std::memory_order_seq_cst ) + 1;
	}

	std::uint32_t AtomicDecrementU32( std::uint32_t *value )
	{
		return AtomicRef( value ).fetch_sub( 1, std::memory_order_seq_cst ) - 1;
	}

	std::uint32_t AtomicCompareExchangeU32( std::uint32_t *value, std::uint32_t expected, const std::uint32_t replacement )
	{
		AtomicRef( value ).compare_exchange_strong( expected, replacement, std::memory_order_seq_cst );
		return expected;
	}
}
