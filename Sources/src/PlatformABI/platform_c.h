#ifndef BK_PLATFORM_C_H
#define BK_PLATFORM_C_H

#include <stdint.h>

#if defined(_WIN32)
#define BK_PLATFORM_CALL __cdecl
#define BK_PLATFORM_EXPORT __declspec(dllexport)
#else
#define BK_PLATFORM_CALL
#define BK_PLATFORM_EXPORT __attribute__((visibility("default")))
#endif

#define BK_PLATFORM_ABI_VERSION 1u

typedef uint64_t BkPlatformHandle;
typedef uint32_t BkPlatformResult;

#define BK_PLATFORM_OK ((BkPlatformResult)0u)
#define BK_PLATFORM_ERROR_INVALID_ARGUMENT ((BkPlatformResult)1u)
#define BK_PLATFORM_ERROR_UNSUPPORTED_VERSION ((BkPlatformResult)2u)
#define BK_PLATFORM_ERROR_NOT_INITIALIZED ((BkPlatformResult)3u)
#define BK_PLATFORM_ERROR_ALREADY_INITIALIZED ((BkPlatformResult)4u)
#define BK_PLATFORM_ERROR_BUFFER_TOO_SMALL ((BkPlatformResult)5u)

typedef struct BkPlatformUtf8Span {
    uint32_t struct_size;
    const char *data;
    uint32_t length;
} BkPlatformUtf8Span;

typedef void *(BK_PLATFORM_CALL *BkPlatformAllocFn)(void *user_data, uint64_t size, uint64_t alignment);
typedef void (BK_PLATFORM_CALL *BkPlatformFreeFn)(void *user_data, void *memory);
typedef void (BK_PLATFORM_CALL *BkPlatformLogFn)(void *user_data, uint32_t level, BkPlatformUtf8Span message);

typedef struct BkPlatformAllocatorCallbacks {
    uint32_t struct_size;
    BkPlatformAllocFn alloc;
    BkPlatformFreeFn free;
    void *user_data;
} BkPlatformAllocatorCallbacks;

typedef struct BkPlatformCreateInfo {
    uint32_t struct_size;
    uint32_t requested_abi_version;
    BkPlatformAllocatorCallbacks allocator;
    BkPlatformLogFn log;
    void *user_data;
} BkPlatformCreateInfo;

typedef struct BkPlatformApi {
    uint32_t abi_version;
    uint32_t struct_size;
    BkPlatformResult (BK_PLATFORM_CALL *runtime_create)(const BkPlatformCreateInfo *create_info);
    void (BK_PLATFORM_CALL *runtime_destroy)(void);
    BkPlatformResult (BK_PLATFORM_CALL *get_last_error)(char *dst, uint32_t capacity, uint32_t *required);
    uint64_t (BK_PLATFORM_CALL *get_runtime_generation)(void);
    uint64_t (BK_PLATFORM_CALL *get_monotonic_nanoseconds)(void);
    void (BK_PLATFORM_CALL *sleep_milliseconds)(uint32_t milliseconds);
    uint32_t (BK_PLATFORM_CALL *atomic_exchange_u32)(uint32_t *value, uint32_t replacement);
    uint32_t (BK_PLATFORM_CALL *atomic_increment_u32)(uint32_t *value);
    uint32_t (BK_PLATFORM_CALL *atomic_decrement_u32)(uint32_t *value);
    uint32_t (BK_PLATFORM_CALL *atomic_compare_exchange_u32)(uint32_t *value, uint32_t expected, uint32_t replacement);
} BkPlatformApi;

#ifdef __cplusplus
extern "C" {
#endif

BK_PLATFORM_EXPORT const BkPlatformApi *BK_PLATFORM_CALL bk_platform_get_api(uint32_t requested_version);

#ifdef __cplusplus
}
#endif

#endif
