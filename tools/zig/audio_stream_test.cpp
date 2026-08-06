#include "../../Sources/src/Platform/Clock.h"
#include "../../Sources/src/Platform/Sync.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

namespace
{
bool check(bool condition, const char* message)
{
	if (!condition)
		std::fprintf(stderr, "audio stream fixture failed: %s\n", message);
	return condition;
}

class PcmStream final
{
	std::vector<std::int16_t> frames;
	std::size_t cursor = 0;
	bool looped = false;

public:
	PcmStream(std::size_t frameCount, bool shouldLoop)
		: frames(frameCount), looped(shouldLoop)
	{
		for (std::size_t index = 0; index != frames.size(); ++index)
			frames[index] = static_cast<std::int16_t>((index * 17) & 0x7fff);
	}

	std::size_t Read(std::int16_t* output, std::size_t requested)
	{
		if (output == nullptr || requested == 0 || frames.empty())
			return 0;
		std::size_t written = 0;
		while (written != requested)
		{
			if (cursor == frames.size())
			{
				if (!looped)
					break;
				cursor = 0;
			}
			const std::size_t available = frames.size() - cursor;
			const std::size_t count = available < requested - written ? available : requested - written;
			for (std::size_t index = 0; index != count; ++index)
				output[written + index] = frames[cursor + index];
			cursor += count;
			written += count;
		}
		return written;
	}

	bool Seek(std::size_t frame)
	{
		if (frame > frames.size())
			return false;
		cursor = frame;
		return true;
	}

	std::size_t Tell() const { return cursor; }
	std::size_t Length() const { return frames.size(); }
};

struct CallbackOwner
{
	std::atomic<bool> closing{false};
	std::atomic<unsigned int> readers{0};
	std::atomic<unsigned int> calls{0};
	NPlatform::Event stop{false, true};
};

void StreamCallback(CallbackOwner* owner)
{
	owner->readers.fetch_add(1, std::memory_order_acquire);
	if (!owner->closing.load(std::memory_order_acquire))
		owner->calls.fetch_add(1, std::memory_order_release);
	owner->readers.fetch_sub(1, std::memory_order_release);
}

bool testPcmReadSeekLoop()
{
	PcmStream stream(32, false);
	std::int16_t output[40] = {};
	if (!check(stream.Read(output, 40) == 32, "short read must stop at EOF"))
		return false;
	if (!check(stream.Tell() == stream.Length(), "EOF cursor must equal stream length"))
		return false;
	if (!check(stream.Read(output, 1) == 0, "non-loop stream must remain at EOF"))
		return false;
	if (!check(stream.Seek(8) && stream.Tell() == 8, "seek must select an exact frame"))
		return false;
	PcmStream looped(4, true);
	if (!check(looped.Read(output, 6) == 6 && output[4] == output[0], "looped read must wrap at EOF"))
		return false;
	return check(!looped.Seek(5), "seek beyond EOF must fail");
}

bool testCallbackDrain()
{
	for (int cycle = 0; cycle != 100; ++cycle)
	{
		CallbackOwner owner;
		std::thread callbackThread([&owner]() {
			while (!owner.stop.IsSet())
			{
				StreamCallback(&owner);
				NPlatform::SleepMilliseconds(0);
			}
		});
		NPlatform::SleepMilliseconds(1);
		owner.closing.store(true, std::memory_order_release);
		owner.stop.Set();
		callbackThread.join();
		while (owner.readers.load(std::memory_order_acquire) != 0)
			NPlatform::SleepMilliseconds(0);
		if (!check(owner.calls.load(std::memory_order_acquire) != 0, "callback must run before shutdown"))
			return false;
	}
	return true;
}
}

int main()
{
	if (!testPcmReadSeekLoop() || !testCallbackDrain())
		return 1;
	std::printf("audio streams passed: PCM read seek loop EOF callback drain cycles=100\n");
	return 0;
}
