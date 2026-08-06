#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_NULL
#define MINIAUDIO_IMPLEMENTATION
#include "../../Sources/sdk/miniaudio/miniaudio.h"

namespace
{
struct AllocatorState
{

	std::size_t allocations = 0;
	std::size_t frees = 0;
	std::size_t bytes = 0;
	std::size_t failAfter = static_cast<std::size_t>(-1);
};

void* fixtureMalloc(size_t size, void* userData)
{
	AllocatorState* state = static_cast<AllocatorState*>(userData);
	if (state->allocations >= state->failAfter)
		return nullptr;
	void* result = std::malloc(size == 0 ? 1 : size);
	if (result != nullptr)
	{
		++state->allocations;
		state->bytes += size == 0 ? 1 : size;
	}
	return result;
}

void* fixtureRealloc(void* pointer, size_t size, void* userData)
{
	AllocatorState* state = static_cast<AllocatorState*>(userData);
	if (state->allocations >= state->failAfter)
		return nullptr;
	return std::realloc(pointer, size == 0 ? 1 : size);
}

void fixtureFree(void* pointer, void* userData)
{
	AllocatorState* state = static_cast<AllocatorState*>(userData);
	if (pointer != nullptr)
		++state->frees;
	std::free(pointer);
}

bool check(bool condition, const char* message)
{
	if (!condition)
		std::fprintf(stderr, "audio lifecycle fixture failed: %s\n", message);
	return condition;
}

ma_allocation_callbacks callbacks(AllocatorState* state)
{
	ma_allocation_callbacks result{};
	result.pUserData = state;
	result.onMalloc = fixtureMalloc;
	result.onRealloc = fixtureRealloc;
	result.onFree = fixtureFree;
	return result;
}

bool exerciseAllocator()
{
	AllocatorState state;
	ma_allocation_callbacks allocation = callbacks(&state);
	void* block = allocation.onMalloc(0, allocation.pUserData);
	if (!check(block != nullptr, "zero-size allocation must produce a usable block"))
		return false;
	allocation.onFree(block, allocation.pUserData);
	block = allocation.onMalloc(8, allocation.pUserData);
	if (!check(block != nullptr, "ordinary allocation must produce a usable block"))
		return false;
	std::memset(block, 0x5a, 8);
	block = allocation.onRealloc(block, 32, allocation.pUserData);
	if (!check(block != nullptr, "realloc must preserve a live allocation"))
		return false;
	if (!check(static_cast<unsigned char*>(block)[0] == 0x5a, "realloc must preserve contents"))
		return false;
	const std::size_t freesBeforeNull = state.frees;
	allocation.onFree(nullptr, allocation.pUserData);
	allocation.onFree(block, allocation.pUserData);
	state.failAfter = state.allocations;
	if (!check(allocation.onMalloc(16, allocation.pUserData) == nullptr, "injected allocation failure must be observable"))
		return false;
	return check(freesBeforeNull + 1 == state.frees, "free-null must not alter the allocation balance") &&
		check(state.bytes >= 8, "allocator must account for non-zero requests");
}

bool exerciseMiniaudio()
{
	AllocatorState state;
	ma_allocation_callbacks allocation = callbacks(&state);
	const ma_backend backend[] = { ma_backend_null };
	const ma_backend unsupportedBackend[] = { ma_backend_custom };
	ma_context unsupportedContext{};
	ma_context_config unsupportedConfig = ma_context_config_init();
	unsupportedConfig.allocationCallbacks = allocation;
	if (!check(ma_context_init(unsupportedBackend, 1, &unsupportedConfig, &unsupportedContext) != MA_SUCCESS, "unsupported backend must fail deterministically"))
		return false;

	for (int cycle = 0; cycle != 3; ++cycle)
	{
		ma_context context{};
		ma_context_config contextConfig = ma_context_config_init();
		contextConfig.allocationCallbacks = allocation;
		if (!check(ma_context_init(backend, 1, &contextConfig, &context) == MA_SUCCESS, "null context must initialize"))
			return false;

		ma_engine engine{};
		ma_engine_config engineConfig = ma_engine_config_init();
		engineConfig.pContext = &context;
		engineConfig.channels = 2;
		engineConfig.sampleRate = 48000;
		engineConfig.noDevice = cycle == 0 ? MA_FALSE : MA_TRUE;
		engineConfig.allocationCallbacks = allocation;
		if (!check(ma_engine_init(&engineConfig, &engine) == MA_SUCCESS, "no-device engine must initialize"))
		{
			ma_context_uninit(&context);
			return false;
		}
		ma_engine_uninit(&engine);
		ma_context_uninit(&context);
	}

	if (!check(ma_get_bytes_per_frame(ma_format_unknown, 2) == 0, "unknown format must be rejected by format sizing"))
		return false;

	return check(state.allocations == state.frees, "miniaudio allocations must balance after uninitialization");
}
}

int main()
{
	if (!exerciseAllocator() || !exerciseMiniaudio())
		return 1;
	std::printf("audio lifecycle passed: allocator semantics and null-device restart\n");
	return 0;
}
