#include "PlatformABI/PlatformClient.h"

namespace {
const BkPlatformApi *g_api = nullptr;
}

namespace BkPlatform {

bool Client::Attach(uint32_t requested_version) noexcept {
    g_api = bk_platform_get_api(requested_version);
    return g_api != nullptr && g_api->struct_size >= sizeof(BkPlatformApi);
}

void Client::Detach() noexcept { g_api = nullptr; }
bool Client::IsAttached() noexcept { return g_api != nullptr; }

BkPlatformResult Client::Create(const BkPlatformCreateInfo &info) noexcept {
    if (g_api == nullptr || g_api->runtime_create == nullptr) return BK_PLATFORM_ERROR_NOT_INITIALIZED;
    return g_api->runtime_create(&info);
}

void Client::Destroy() noexcept {
    if (g_api != nullptr && g_api->runtime_destroy != nullptr) g_api->runtime_destroy();
}

uint64_t Client::Generation() noexcept {
    return g_api != nullptr && g_api->get_runtime_generation != nullptr ? g_api->get_runtime_generation() : 0;
}

uint64_t Client::MonotonicNanoseconds() noexcept {
    return g_api != nullptr && g_api->get_monotonic_nanoseconds != nullptr ? g_api->get_monotonic_nanoseconds() : 0;
}

void Client::SleepMilliseconds(uint32_t milliseconds) noexcept {
    if (g_api != nullptr && g_api->sleep_milliseconds != nullptr) g_api->sleep_milliseconds(milliseconds);
}

uint32_t Client::AtomicExchangeU32(uint32_t *value, uint32_t replacement) noexcept {
    return g_api != nullptr && g_api->atomic_exchange_u32 != nullptr ? g_api->atomic_exchange_u32(value, replacement) : 0;
}

uint32_t Client::AtomicIncrementU32(uint32_t *value) noexcept {
    return g_api != nullptr && g_api->atomic_increment_u32 != nullptr ? g_api->atomic_increment_u32(value) : 0;
}

uint32_t Client::AtomicDecrementU32(uint32_t *value) noexcept {
    return g_api != nullptr && g_api->atomic_decrement_u32 != nullptr ? g_api->atomic_decrement_u32(value) : 0;
}

uint32_t Client::AtomicCompareExchangeU32(uint32_t *value, uint32_t expected, uint32_t replacement) noexcept {
    return g_api != nullptr && g_api->atomic_compare_exchange_u32 != nullptr ? g_api->atomic_compare_exchange_u32(value, expected, replacement) : 0;
}

BkPlatformResult Client::LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept {
    if (g_api == nullptr || g_api->get_last_error == nullptr) return BK_PLATFORM_ERROR_NOT_INITIALIZED;
    return g_api->get_last_error(dst, capacity, required);
}

}
