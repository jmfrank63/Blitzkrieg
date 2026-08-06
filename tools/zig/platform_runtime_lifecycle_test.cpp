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
    BkPlatformHandle event = 0;
    if (api->event_create(0, 0, &event) != BK_PLATFORM_OK || event == 0) return 8;
    if (api->event_wait(event, 0) != BK_PLATFORM_ERROR_TIMEOUT) return 9;
    if (api->event_set(event) != BK_PLATFORM_OK || api->event_wait(event, 10) != BK_PLATFORM_OK) return 10;
    if (api->event_destroy(event) != BK_PLATFORM_OK) return 11;
    BkPlatformHandle mutex = 0;
    if (api->mutex_create(&mutex) != BK_PLATFORM_OK || api->mutex_lock(mutex) != BK_PLATFORM_OK) return 12;
    if (api->mutex_unlock(mutex) != BK_PLATFORM_OK || api->mutex_destroy(mutex) != BK_PLATFORM_OK) return 13;
    if (api->get_live_sync_handles() != 0) return 14;
    api->runtime_destroy();
    api->runtime_destroy();
    if (api->runtime_create(&info) != BK_PLATFORM_OK) return 15;
    api->runtime_destroy();
    return 0;
}

int main() {
    if (bk_platform_get_api(0) != nullptr) return 1;
    if (bk_platform_get_api(BK_PLATFORM_ABI_VERSION + 1) != nullptr) return 2;
    const BkPlatformApi *api = bk_platform_get_api(BK_PLATFORM_ABI_VERSION);
    return check_cycle(api);
}
