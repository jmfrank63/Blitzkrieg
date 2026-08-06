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

BkPlatformResult Client::LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept {
    if (g_api == nullptr || g_api->get_last_error == nullptr) return BK_PLATFORM_ERROR_NOT_INITIALIZED;
    return g_api->get_last_error(dst, capacity, required);
}

}
