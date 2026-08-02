#include "Misc/Thread.h"
#include "Platform/Clock.h"

#include <atomic>
#include <cstdio>

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
	NWin32Helper::CEvent event( false, true );
	if ( event.IsSet() ) return 1;
	event.Set();
	if ( !event.IsSet() || !event.IsSet() ) return 2;
	event.Reset();
	if ( event.IsSet() ) return 3;

	NWin32Helper::CCriticalSection mutex;
	int guarded = 0;
	{
		NWin32Helper::CCriticalSectionLock lock( mutex );
		++guarded;
		lock.Leave();
		lock.Enter();
		++guarded;
	}
	if ( guarded != 2 ) return 4;

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
	std::printf( "sync stress: 100 start/stop cycles, %d steps\n", steps.load() );
	return 0;
}
