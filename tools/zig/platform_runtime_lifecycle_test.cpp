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
    if (api->event_destroy(event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 12;
    if (api->event_set(event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 13;
    if (api->event_reset(event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 14;
    if (api->event_wait(event, 0) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 15;

    BkPlatformHandle reused_event = 0;
    if (api->event_create(0, 0, &reused_event) != BK_PLATFORM_OK || reused_event == 0 || reused_event == event) return 16;
    if (api->event_set(event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 17;
    if (api->event_set(reused_event) != BK_PLATFORM_OK || api->event_wait(reused_event, 10) != BK_PLATFORM_OK) return 18;

    BkPlatformHandle mutex = 0;
    if (api->mutex_create(&mutex) != BK_PLATFORM_OK || api->mutex_lock(mutex) != BK_PLATFORM_OK) return 19;
    if (api->mutex_unlock(mutex) != BK_PLATFORM_OK) return 20;
    if (api->event_set(mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 21;
    if (api->event_destroy(mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 22;
    if (api->mutex_destroy(mutex) != BK_PLATFORM_OK) return 23;
    if (api->mutex_destroy(mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 24;
    if (api->mutex_lock(mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 25;
    if (api->mutex_unlock(mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 26;
    if (api->mutex_destroy(event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 27;
    if (api->event_destroy(reused_event) != BK_PLATFORM_OK) return 28;

    const BkPlatformHandle invalid = UINT64_C(0xffffffffffffffff);
    if (api->event_set(0) != BK_PLATFORM_ERROR_INVALID_ARGUMENT || api->event_set(invalid) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 29;
    if (api->event_destroy(0) != BK_PLATFORM_ERROR_INVALID_ARGUMENT || api->event_destroy(invalid) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 30;
    if (api->mutex_lock(0) != BK_PLATFORM_ERROR_INVALID_ARGUMENT || api->mutex_lock(invalid) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 31;
    if (api->mutex_destroy(0) != BK_PLATFORM_ERROR_INVALID_ARGUMENT || api->mutex_destroy(invalid) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 32;
    if (api->get_live_sync_handles() != 0) return 33;

    BkPlatformHandle teardown_event = 0;
    BkPlatformHandle teardown_mutex = 0;
    if (api->event_create(1, 1, &teardown_event) != BK_PLATFORM_OK || api->mutex_create(&teardown_mutex) != BK_PLATFORM_OK) return 34;
    if (api->get_live_sync_handles() != 2) return 35;
    api->runtime_destroy();
    if (api->get_live_sync_handles() != 0) return 36;
    if (api->event_set(teardown_event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 37;
    if (api->event_destroy(teardown_event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 38;
    if (api->mutex_lock(teardown_mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 39;
    if (api->mutex_destroy(teardown_mutex) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 40;

    if (api->runtime_create(&info) != BK_PLATFORM_OK) return 41;
    BkPlatformHandle post_teardown_event = 0;
    if (api->event_create(0, 0, &post_teardown_event) != BK_PLATFORM_OK || post_teardown_event == teardown_event) return 42;
    if (api->event_destroy(teardown_event) != BK_PLATFORM_ERROR_INVALID_ARGUMENT) return 43;
    if (api->event_destroy(post_teardown_event) != BK_PLATFORM_OK) return 44;
    if (api->get_live_sync_handles() != 0) return 45;
    api->runtime_destroy();
    api->runtime_destroy();
    return 0;
}

int main() {
    if (bk_platform_get_api(0) != nullptr) return 1;
    if (bk_platform_get_api(BK_PLATFORM_ABI_VERSION + 1) != nullptr) return 2;
    const BkPlatformApi *api = bk_platform_get_api(BK_PLATFORM_ABI_VERSION);
    return check_cycle(api);
}
