#include "PlatformABI/platform_c.h"

#include <cstring>

static int check_cycle(const BkPlatformApi *api) {
    if (api == nullptr) return 1;
    if (api->runtime_create(nullptr) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 2;
    BkPlatformCreateInfo info = {};
    info.struct_size = sizeof(info);
    info.requested_abi_version = BK_PLATFORM_ABI_VERSION;
    if (api->runtime_create(&info) != BK_PLATFORM_OK) return 3;
    if (api->runtime_create(&info) != BK_PLATFORM_ERROR_ALREADY_INITIALIZED) return 4;
    char error[64] = {};
    uint32_t required = 0;
    if (api->get_last_error(error, sizeof(error), &required) != BK_PLATFORM_OK) return 5;
    if (std::strcmp(error, "runtime already initialized") != 0) return 6;
    api->runtime_destroy();
    api->runtime_destroy();
    if (api->runtime_create(&info) != BK_PLATFORM_OK) return 7;
    api->runtime_destroy();
    return 0;
}

int main() {
    if (bk_platform_get_api(0) != nullptr) return 1;
    if (bk_platform_get_api(BK_PLATFORM_ABI_VERSION + 1) != nullptr) return 2;
    const BkPlatformApi *api = bk_platform_get_api(BK_PLATFORM_ABI_VERSION);
    return check_cycle(api);
}
