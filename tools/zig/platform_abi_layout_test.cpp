#include "PlatformABI/platform_c.h"

#include <stddef.h>

static_assert(BK_PLATFORM_ABI_VERSION == 1u);
static_assert(sizeof(BkPlatformHandle) == 8);
static_assert(sizeof(BkPlatformResult) == 4);
static_assert(offsetof(BkPlatformUtf8Span, struct_size) == 0);
static_assert(offsetof(BkPlatformAllocatorCallbacks, struct_size) == 0);
static_assert(offsetof(BkPlatformCreateInfo, struct_size) == 0);
static_assert(offsetof(BkPlatformApi, abi_version) == 0);
static_assert(offsetof(BkPlatformApi, struct_size) == sizeof(uint32_t));

int main() {
    return 0;
}
