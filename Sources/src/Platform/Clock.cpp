#include "Clock.h"

#include <chrono>
#include <atomic>
#include <thread>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <ctime>
#endif

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

	bool SleepPreciseNanoseconds( const std::uint64_t nanoseconds )
	{
#if defined(__APPLE__)
		// mach_wait_until is the one Darwin wait that is not coalesced to the
		// millisecond, so it lands close enough to the deadline that the caller
		// does not need a spin at all. It takes an absolute time on the mach
		// timebase, whose epoch is not the steady_clock one - hence a duration
		// from now rather than a deadline in the caller's clock.
		static mach_timebase_info_data_t timebase;
		if ( timebase.denom == 0 && mach_timebase_info( &timebase ) != KERN_SUCCESS )
			return false;
		const std::uint64_t ticks = nanoseconds * timebase.denom / timebase.numer;
		return mach_wait_until( mach_absolute_time() + ticks ) == KERN_SUCCESS;
#elif !defined(_WIN32) && !defined(_WIN64)
		// POSIX high-resolution timers are what clock_nanosleep waits on. A
		// signal can cut the wait short, which is why the caller still checks
		// the clock afterwards rather than trusting the return.
		timespec request;
		request.tv_sec = static_cast<time_t>( nanoseconds / 1000000000ULL );
		request.tv_nsec = static_cast<long>( nanoseconds % 1000000000ULL );
		return clock_nanosleep( CLOCK_MONOTONIC, 0, &request, nullptr ) == 0;
#elif defined(_WIN32) || defined(_WIN64)
		// The high-resolution waitable timer (Windows 10 1803+) is the one
		// Windows wait that is not rounded to the scheduler quantum - measured
		// on this path it holds a 60 FPS deadline as exactly as the millisecond
		// sleep + spin did, for about 5% of a core less. Where the flag is not
		// supported, creation fails once and the caller keeps the old strategy.
		// One timer per thread: a shared handle would let two waiters overwrite
		// each other's due time.
		static thread_local HANDLE hTimer = CreateWaitableTimerExW( nullptr, nullptr,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS );
		if ( hTimer == nullptr )
			return false;
		LARGE_INTEGER due;
		due.QuadPart = -static_cast<LONGLONG>( nanoseconds / 100ULL );	// negative = relative, 100 ns units
		if ( due.QuadPart == 0 )
			return true;
		if ( !SetWaitableTimer( hTimer, &due, 0, nullptr, nullptr, FALSE ) )
			return false;
		return WaitForSingleObject( hTimer, INFINITE ) == WAIT_OBJECT_0;
#else
		(void)nanoseconds;
		return false;
#endif
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
