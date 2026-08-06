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
// Fixed-width, borrowed window identity. The value is owned and interpreted
// by the platform runtime; callers must not treat it as an SDL_Window pointer.
typedef uint64_t BkPlatformWindowHandle;
typedef uint64_t BkPlatformSocketHandle;
typedef uint32_t BkPlatformResult;

#define BK_PLATFORM_OK ((BkPlatformResult)0u)
#define BK_PLATFORM_ERROR_INVALID_ARGUMENT ((BkPlatformResult)1u)
#define BK_PLATFORM_ERROR_UNSUPPORTED_VERSION ((BkPlatformResult)2u)
#define BK_PLATFORM_ERROR_NOT_INITIALIZED ((BkPlatformResult)3u)
#define BK_PLATFORM_ERROR_ALREADY_INITIALIZED ((BkPlatformResult)4u)
#define BK_PLATFORM_ERROR_BUFFER_TOO_SMALL ((BkPlatformResult)5u)
#define BK_PLATFORM_ERROR_TIMEOUT ((BkPlatformResult)6u)
#define BK_PLATFORM_ERROR_BUSY ((BkPlatformResult)7u)

typedef struct BkPlatformSocketAddress {
    uint16_t family;
    uint8_t data[14];
} BkPlatformSocketAddress;

typedef uint32_t BkPlatformSocketError;
#define BK_PLATFORM_SOCKET_ERROR_NONE ((BkPlatformSocketError)0u)
#define BK_PLATFORM_SOCKET_ERROR_WOULD_BLOCK ((BkPlatformSocketError)1u)
#define BK_PLATFORM_SOCKET_ERROR_INTERRUPTED ((BkPlatformSocketError)2u)
#define BK_PLATFORM_SOCKET_ERROR_CONNECTION_RESET ((BkPlatformSocketError)3u)
#define BK_PLATFORM_SOCKET_ERROR_CONNECTION_REFUSED ((BkPlatformSocketError)4u)
#define BK_PLATFORM_SOCKET_ERROR_TIMED_OUT ((BkPlatformSocketError)5u)
#define BK_PLATFORM_SOCKET_ERROR_ADDRESS_IN_USE ((BkPlatformSocketError)6u)
#define BK_PLATFORM_SOCKET_ERROR_UNKNOWN ((BkPlatformSocketError)7u)

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
    BkPlatformResult (BK_PLATFORM_CALL *event_create)(uint32_t initial_state, uint32_t manual_reset, BkPlatformHandle *out_handle);
    BkPlatformResult (BK_PLATFORM_CALL *event_destroy)(BkPlatformHandle handle);
    BkPlatformResult (BK_PLATFORM_CALL *event_set)(BkPlatformHandle handle);
    BkPlatformResult (BK_PLATFORM_CALL *event_reset)(BkPlatformHandle handle);
    BkPlatformResult (BK_PLATFORM_CALL *event_wait)(BkPlatformHandle handle, uint32_t timeout_milliseconds);
    BkPlatformResult (BK_PLATFORM_CALL *mutex_create)(BkPlatformHandle *out_handle);
    BkPlatformResult (BK_PLATFORM_CALL *mutex_destroy)(BkPlatformHandle handle);
    BkPlatformResult (BK_PLATFORM_CALL *mutex_lock)(BkPlatformHandle handle);
    BkPlatformResult (BK_PLATFORM_CALL *mutex_unlock)(BkPlatformHandle handle);
    uint32_t (BK_PLATFORM_CALL *get_live_sync_handles)(void);
    BkPlatformResult (BK_PLATFORM_CALL *diagnostic_write)(uint32_t level, BkPlatformUtf8Span message);
    uint32_t (BK_PLATFORM_CALL *is_debugger_attached)(void);
    BkPlatformResult (BK_PLATFORM_CALL *socket_runtime_init)(void);
    BkPlatformResult (BK_PLATFORM_CALL *socket_runtime_done)(void);
    BkPlatformResult (BK_PLATFORM_CALL *socket_open_tcp)(BkPlatformSocketHandle *out_handle);
    BkPlatformResult (BK_PLATFORM_CALL *socket_open_udp)(BkPlatformSocketHandle *out_handle);
    BkPlatformResult (BK_PLATFORM_CALL *socket_bind)(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, uint16_t port);
    BkPlatformResult (BK_PLATFORM_CALL *socket_listen)(BkPlatformSocketHandle handle, int32_t backlog);
    BkPlatformResult (BK_PLATFORM_CALL *socket_connect)(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address);
    BkPlatformResult (BK_PLATFORM_CALL *socket_accept)(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, BkPlatformSocketHandle *out_handle);
    int32_t (BK_PLATFORM_CALL *socket_send)(BkPlatformSocketHandle handle, const void *data, int32_t size);
    int32_t (BK_PLATFORM_CALL *socket_receive)(BkPlatformSocketHandle handle, void *data, int32_t size);
    int32_t (BK_PLATFORM_CALL *socket_send_to)(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address, const void *data, int32_t size);
    int32_t (BK_PLATFORM_CALL *socket_receive_from)(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, void *data, int32_t size);
    BkPlatformResult (BK_PLATFORM_CALL *socket_set_nonblocking)(BkPlatformSocketHandle handle, uint32_t enabled);
    BkPlatformResult (BK_PLATFORM_CALL *socket_wait_readable)(BkPlatformSocketHandle handle, int32_t timeout_milliseconds);
    BkPlatformResult (BK_PLATFORM_CALL *socket_resolve_ipv4)(const char *host, uint16_t port, BkPlatformSocketAddress *address);
    BkPlatformSocketError (BK_PLATFORM_CALL *socket_last_error)(void);
    BkPlatformResult (BK_PLATFORM_CALL *socket_close)(BkPlatformSocketHandle handle);
} BkPlatformApi;

#ifdef __cplusplus
extern "C" {
#endif

BK_PLATFORM_EXPORT const BkPlatformApi *BK_PLATFORM_CALL bk_platform_get_api(uint32_t requested_version);

#ifdef __cplusplus
}
#endif

#endif
