#include "Misc/Thread.h"
#include "Platform/Clock.h"

#include <atomic>
#include <cstdio>
#include <thread>

class StressThread final : public CThread
{
	std::atomic<int> &stepCount;
protected:
	void Step() override { ++stepCount; }
public:
	StressThread( const int delay, std::atomic<int> &steps ) : CThread( delay ), stepCount( steps ) {}
};

int main()
{
	NPlatform::Event event( false, true );
	if ( event.IsSet() ) return 1;
	event.Set();
	if ( !event.IsSet() || !event.IsSet() ) return 2;
	event.Reset();
	if ( event.IsSet() ) return 3;

	NPlatform::Event wakeEvent( false, false );
	std::atomic<int> wakeCount{ 0 };
	std::thread waiter( [&]() {
		for ( int i = 0; i != 10000; ++i )
		{
			wakeEvent.Wait();
			wakeCount.store( i + 1, std::memory_order_release );
		}
	} );
	for ( int i = 0; i != 10000; ++i )
	{
		wakeEvent.Set();
		while ( wakeCount.load( std::memory_order_acquire ) != i + 1 ) std::this_thread::yield();
	}
	waiter.join();
	if ( wakeCount.load( std::memory_order_acquire ) != 10000 ) return 4;

	NPlatform::Mutex mutex;
	int guarded = 0;
	{
		mutex.Lock();
		++guarded;
		mutex.Unlock();
		mutex.Lock();
		++guarded;
		mutex.Unlock();
	}
	if ( guarded != 2 ) return 5;

	std::atomic<int> steps{ 0 };
	StressThread worker( 0, steps );
	for ( int i = 0; i != 100; ++i )
	{
		worker.RunThread();
		worker.RunThread();
		NPlatform::SleepMilliseconds( 1 );
		worker.StopThread();
		worker.StopThread();
	}
	std::printf( "sync stress: 100 start/stop cycles, 10000 wake/wait cycles, %d steps\n", steps.load() );
	return 0;
}
