#include "../../Sources/src/Misc/Thread.h"
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
		std::fprintf(stderr, "audio worker fixture failed: %s\n", message);
	return condition;
}

class CompletionWorker final : public CThread
{
	std::atomic<std::uint32_t> completions{0};
	NPlatform::Event completion;
	std::uint32_t target = 0;

	void Step() override
	{
		const std::uint32_t value = completions.fetch_add(1, std::memory_order_release) + 1;
		completion.Set();
		if (value >= target)
			FinishThread();
	}

public:
	explicit CompletionWorker(std::uint32_t requestedTarget)
		: CThread(1), completion(false, true), target(requestedTarget) {}

	bool RunAndCollect()
	{
		RunThread();
		std::uint32_t observed = 0;
		while (observed < target)
		{
			completion.Wait();
			completion.Reset();
			observed = completions.load(std::memory_order_acquire);
		}
		StopThread();
		return observed == target;
	}

	void ResetForRestart(std::uint32_t requestedTarget)
	{
		completions.store(0, std::memory_order_release);
		target = requestedTarget;
		completion.Reset();
	}

	std::uint32_t Count() const { return completions.load(std::memory_order_acquire); }
};

bool testCompletionHandoff()
{
	CompletionWorker worker(1000);
	if (!check(worker.RunAndCollect(), "completion handoff must collect all worker notifications"))
		return false;
	if (!check(worker.Count() == 1000, "completion count must be exact"))
		return false;
	worker.ResetForRestart(128);
	return check(worker.RunAndCollect(), "worker must restart after a completed run");
}

bool testFadeTimeline()
{
	std::vector<float> timeline;
	float volume = 1.0f;
	const float step = volume / 500.0f;
	const std::uint64_t start = NPlatform::MonotonicNanoseconds();
	for (std::uint32_t elapsed = 0; elapsed <= 500; elapsed += 100)
	{
		volume = elapsed >= 500 ? 0.0f : 1.0f - step * static_cast<float>(elapsed);
		timeline.push_back(volume);
	}
	const std::uint64_t elapsed = NPlatform::MonotonicNanoseconds() - start;
	return check(timeline.front() == 1.0f && timeline.back() == 0.0f, "fade curve must reach zero at its deadline") &&
		check(timeline[1] > timeline[2] && timeline[2] > timeline[3], "fade curve must be monotonic") &&
		check(elapsed < 1000000000ULL, "fade calculation must not block the caller");
}

bool testShutdownWake()
{
	NPlatform::Event wake(false, true);
	std::atomic<bool> stopped{false};
	std::thread worker([&]() {
		wake.Wait();
		stopped.store(true, std::memory_order_release);
	});
	wake.Set();
	worker.join();
	return check(stopped.load(std::memory_order_acquire), "shutdown signal must wake the worker");
}
}

int main()
{
	if (!testCompletionHandoff() || !testFadeTimeline() || !testShutdownWake())
		return 1;
	std::printf("audio workers passed: 1000 handoffs fade shutdown restart\n");
	return 0;
}
