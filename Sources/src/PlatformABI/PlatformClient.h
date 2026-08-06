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
    static BkPlatformResult EventCreate(uint32_t initial_state, uint32_t manual_reset, BkPlatformHandle *out_handle) noexcept;
    static BkPlatformResult EventDestroy(BkPlatformHandle handle) noexcept;
    static BkPlatformResult EventSet(BkPlatformHandle handle) noexcept;
    static BkPlatformResult EventReset(BkPlatformHandle handle) noexcept;
    static BkPlatformResult EventWait(BkPlatformHandle handle, uint32_t timeout_milliseconds) noexcept;
    static BkPlatformResult MutexCreate(BkPlatformHandle *out_handle) noexcept;
    static BkPlatformResult MutexDestroy(BkPlatformHandle handle) noexcept;
    static BkPlatformResult MutexLock(BkPlatformHandle handle) noexcept;
    static BkPlatformResult MutexUnlock(BkPlatformHandle handle) noexcept;
    static uint32_t LiveSyncHandles() noexcept;
    static BkPlatformResult DiagnosticWrite(uint32_t level, BkPlatformUtf8Span message) noexcept;
    static bool IsDebuggerAttached() noexcept;
    static BkPlatformResult LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept;
    static BkPlatformResult SocketRuntimeInit() noexcept;
    static BkPlatformResult SocketRuntimeDone() noexcept;
    static BkPlatformResult SocketOpenTcp(BkPlatformSocketHandle *out_handle) noexcept;
    static BkPlatformResult SocketOpenUdp(BkPlatformSocketHandle *out_handle) noexcept;
    static BkPlatformResult SocketBind(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, uint16_t port) noexcept;
    static BkPlatformResult SocketListen(BkPlatformSocketHandle handle, int32_t backlog) noexcept;
    static BkPlatformResult SocketConnect(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address) noexcept;
    static BkPlatformResult SocketAccept(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, BkPlatformSocketHandle *out_handle) noexcept;
    static int32_t SocketSend(BkPlatformSocketHandle handle, const void *data, int32_t size) noexcept;
    static int32_t SocketReceive(BkPlatformSocketHandle handle, void *data, int32_t size) noexcept;
    static int32_t SocketSendTo(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address, const void *data, int32_t size) noexcept;
    static int32_t SocketReceiveFrom(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, void *data, int32_t size) noexcept;
    static BkPlatformResult SocketSetNonblocking(BkPlatformSocketHandle handle, uint32_t enabled) noexcept;
    static BkPlatformResult SocketWaitReadable(BkPlatformSocketHandle handle, int32_t timeout_milliseconds) noexcept;
    static BkPlatformResult SocketResolveIPv4(const char *host, uint16_t port, BkPlatformSocketAddress *address) noexcept;
    static BkPlatformSocketError SocketLastError() noexcept;
    static BkPlatformResult SocketClose(BkPlatformSocketHandle handle) noexcept;
};

}

#endif
