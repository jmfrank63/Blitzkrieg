#ifndef BLITZKRIEG_PLATFORM_SYNC_H
#define BLITZKRIEG_PLATFORM_SYNC_H




namespace NPlatform
{
class Event
{
	std::mutex mutex;
	std::condition_variable condition;
	bool signaled;
	const bool manualReset;
public:
	Event( bool initialState = false, bool manualReset = true );
	Event( const Event & ) = delete;
	Event &operator=( const Event & ) = delete;
	bool Set();
	bool Pulse();
	bool Reset();
	void Wait();
	bool IsSet();
};

class Mutex
{
	std::mutex mutex;
public:
	Mutex() = default;
	Mutex( const Mutex & ) = delete;
	Mutex &operator=( const Mutex & ) = delete;
	void Lock();
	void Unlock();
};
}

#endif
