#ifndef BK_PLATFORM_CLIENT_H
#define BK_PLATFORM_CLIENT_H

#include "PlatformABI/platform_c.h"

namespace BkPlatform {

class Client final {
public:
    static bool Attach(uint32_t requested_version = BK_PLATFORM_ABI_VERSION) noexcept;
    static void Detach() noexcept;
    static bool IsAttached() noexcept;
    static BkPlatformResult Create(const BkPlatformCreateInfo &info) noexcept;
    static void Destroy() noexcept;
    static uint64_t Generation() noexcept;
    static BkPlatformResult LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept;
};

}

#endif
