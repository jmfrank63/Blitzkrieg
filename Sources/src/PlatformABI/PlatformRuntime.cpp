#include "PlatformABI/PlatformState.h"

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

const BkPlatformApi api = {
    BK_PLATFORM_ABI_VERSION,
    sizeof(BkPlatformApi),
    &runtime_create,
    &runtime_destroy,
    &get_last_error,
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
