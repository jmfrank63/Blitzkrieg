#include "PlatformABI/PlatformState.h"
#include "Platform/Clock.h"

#include <cstdio>
#include <cstring>

namespace {
BkPlatformResult set_error(BkPlatformResult result, const char *message) {
    BkPlatformState &state = bk_platform_state();
    std::snprintf(state.last_error, sizeof(state.last_error), "%s", message);
    return result;
}

BkPlatformResult BK_PLATFORM_CALL runtime_create(const BkPlatformCreateInfo *create_info) {
    BkPlatformState &state = bk_platform_state();
    if (create_info == nullptr || create_info->struct_size < sizeof(BkPlatformCreateInfo))
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid create info");
    if (create_info->requested_abi_version != BK_PLATFORM_ABI_VERSION)
        return set_error(BK_PLATFORM_ERROR_UNSUPPORTED_VERSION, "unsupported ABI version");
    if (state.initialized)
        return set_error(BK_PLATFORM_ERROR_ALREADY_INITIALIZED, "runtime already initialized");
    state.initialized = true;
    state.generation += 1;
    state.log = create_info->log;
    state.user_data = create_info->user_data;
    state.last_error[0] = 0;
    return BK_PLATFORM_OK;
}

void BK_PLATFORM_CALL runtime_destroy() {
    BkPlatformState &state = bk_platform_state();
    state.initialized = false;
    state.log = nullptr;
    state.user_data = nullptr;
    state.last_error[0] = 0;
}

BkPlatformResult BK_PLATFORM_CALL get_last_error(char *dst, uint32_t capacity, uint32_t *required) {
    BkPlatformState &state = bk_platform_state();
    const uint32_t needed = static_cast<uint32_t>(std::strlen(state.last_error) + 1);
    if (required != nullptr) *required = needed;
    if (dst == nullptr || capacity < needed)
        return BK_PLATFORM_ERROR_BUFFER_TOO_SMALL;
    std::memcpy(dst, state.last_error, needed);
    return BK_PLATFORM_OK;
}

uint64_t BK_PLATFORM_CALL get_runtime_generation() {
    return bk_platform_state().generation;
}

uint64_t BK_PLATFORM_CALL get_monotonic_nanoseconds() { return NPlatform::MonotonicNanoseconds(); }
void BK_PLATFORM_CALL sleep_milliseconds(uint32_t milliseconds) { NPlatform::SleepMilliseconds(milliseconds); }
uint32_t BK_PLATFORM_CALL atomic_exchange_u32(uint32_t *value, uint32_t replacement) { return NPlatform::AtomicExchangeU32(value, replacement); }
uint32_t BK_PLATFORM_CALL atomic_increment_u32(uint32_t *value) { return NPlatform::AtomicIncrementU32(value); }
uint32_t BK_PLATFORM_CALL atomic_decrement_u32(uint32_t *value) { return NPlatform::AtomicDecrementU32(value); }
uint32_t BK_PLATFORM_CALL atomic_compare_exchange_u32(uint32_t *value, uint32_t expected, uint32_t replacement) {
    return NPlatform::AtomicCompareExchangeU32(value, expected, replacement);
}

const BkPlatformApi api = {
    BK_PLATFORM_ABI_VERSION,
    sizeof(BkPlatformApi),
    &runtime_create,
    &runtime_destroy,
    &get_last_error,
    &get_runtime_generation,
    &get_monotonic_nanoseconds,
    &sleep_milliseconds,
    &atomic_exchange_u32,
    &atomic_increment_u32,
    &atomic_decrement_u32,
    &atomic_compare_exchange_u32,
};
}

BkPlatformState &bk_platform_state() {
    static BkPlatformState state = { false, 0, nullptr, nullptr, { 0 } };
    return state;
}

extern "C" BK_PLATFORM_EXPORT const BkPlatformApi *BK_PLATFORM_CALL bk_platform_get_api(uint32_t requested_version) {
    if (requested_version != BK_PLATFORM_ABI_VERSION) return nullptr;
    return &api;
}
