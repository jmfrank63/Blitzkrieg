#include "Sync.h"

namespace NPlatform
{
Event::Event( const bool initialState, const bool manualReset )
	: signaled( initialState ), manualReset( manualReset )
{
}

bool Event::Set()
{
	{
		std::lock_guard<std::mutex> lock( mutex );
		signaled = true;
	}
	if ( manualReset ) condition.notify_all();
	else condition.notify_one();
	return true;
}

bool Event::Pulse() { return Set(); }

bool Event::Reset()
{
	std::lock_guard<std::mutex> lock( mutex );
	signaled = false;
	return true;
}

void Event::Wait()
{
	std::unique_lock<std::mutex> lock( mutex );
	condition.wait( lock, [this] { return signaled; } );
	if ( !manualReset ) signaled = false;
}

bool Event::IsSet()
{
	std::unique_lock<std::mutex> lock( mutex );
	if ( !signaled ) return false;
	if ( !manualReset ) signaled = false;
	return true;
}

void Mutex::Lock() { mutex.lock(); }
void Mutex::Unlock() { mutex.unlock(); }
}
