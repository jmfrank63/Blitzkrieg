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
    static uint64_t MonotonicNanoseconds() noexcept;
    static void SleepMilliseconds(uint32_t milliseconds) noexcept;
    static uint32_t AtomicExchangeU32(uint32_t *value, uint32_t replacement) noexcept;
    static uint32_t AtomicIncrementU32(uint32_t *value) noexcept;
    static uint32_t AtomicDecrementU32(uint32_t *value) noexcept;
    static uint32_t AtomicCompareExchangeU32(uint32_t *value, uint32_t expected, uint32_t replacement) noexcept;
    static BkPlatformResult LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept;
};

}

#endif
