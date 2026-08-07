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

uint64_t Client::MonotonicNanoseconds() noexcept {
    return g_api != nullptr && g_api->get_monotonic_nanoseconds != nullptr ? g_api->get_monotonic_nanoseconds() : 0;
}

void Client::SleepMilliseconds(uint32_t milliseconds) noexcept {
    if (g_api != nullptr && g_api->sleep_milliseconds != nullptr) g_api->sleep_milliseconds(milliseconds);
}

uint32_t Client::AtomicExchangeU32(uint32_t *value, uint32_t replacement) noexcept {
    return g_api != nullptr && g_api->atomic_exchange_u32 != nullptr ? g_api->atomic_exchange_u32(value, replacement) : 0;
}

uint32_t Client::AtomicIncrementU32(uint32_t *value) noexcept {
    return g_api != nullptr && g_api->atomic_increment_u32 != nullptr ? g_api->atomic_increment_u32(value) : 0;
}

uint32_t Client::AtomicDecrementU32(uint32_t *value) noexcept {
    return g_api != nullptr && g_api->atomic_decrement_u32 != nullptr ? g_api->atomic_decrement_u32(value) : 0;
}

uint32_t Client::AtomicCompareExchangeU32(uint32_t *value, uint32_t expected, uint32_t replacement) noexcept {
    return g_api != nullptr && g_api->atomic_compare_exchange_u32 != nullptr ? g_api->atomic_compare_exchange_u32(value, expected, replacement) : 0;
}

BkPlatformResult Client::EventCreate(uint32_t initial_state, uint32_t manual_reset, BkPlatformHandle *out_handle) noexcept { return g_api != nullptr && g_api->event_create != nullptr ? g_api->event_create(initial_state, manual_reset, out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::EventDestroy(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->event_destroy != nullptr ? g_api->event_destroy(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::EventSet(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->event_set != nullptr ? g_api->event_set(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::EventReset(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->event_reset != nullptr ? g_api->event_reset(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::EventWait(BkPlatformHandle handle, uint32_t timeout_milliseconds) noexcept { return g_api != nullptr && g_api->event_wait != nullptr ? g_api->event_wait(handle, timeout_milliseconds) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::MutexCreate(BkPlatformHandle *out_handle) noexcept { return g_api != nullptr && g_api->mutex_create != nullptr ? g_api->mutex_create(out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::MutexDestroy(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->mutex_destroy != nullptr ? g_api->mutex_destroy(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::MutexLock(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->mutex_lock != nullptr ? g_api->mutex_lock(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::MutexUnlock(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->mutex_unlock != nullptr ? g_api->mutex_unlock(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
uint32_t Client::LiveSyncHandles() noexcept { return g_api != nullptr && g_api->get_live_sync_handles != nullptr ? g_api->get_live_sync_handles() : 0; }
BkPlatformResult Client::DiagnosticWrite(uint32_t level, BkPlatformUtf8Span message) noexcept { return g_api != nullptr && g_api->diagnostic_write != nullptr ? g_api->diagnostic_write(level, message) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
bool Client::IsDebuggerAttached() noexcept { return g_api != nullptr && g_api->is_debugger_attached != nullptr && g_api->is_debugger_attached() != 0; }

BkPlatformResult Client::LastError(char *dst, uint32_t capacity, uint32_t *required) noexcept {
    if (g_api == nullptr || g_api->get_last_error == nullptr) return BK_PLATFORM_ERROR_NOT_INITIALIZED;
    return g_api->get_last_error(dst, capacity, required);
}

BkPlatformResult Client::SocketRuntimeInit() noexcept { return g_api != nullptr && g_api->socket_runtime_init != nullptr ? g_api->socket_runtime_init() : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketRuntimeDone() noexcept { return g_api != nullptr && g_api->socket_runtime_done != nullptr ? g_api->socket_runtime_done() : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketOpenTcp(BkPlatformSocketHandle *out_handle) noexcept { return g_api != nullptr && g_api->socket_open_tcp != nullptr ? g_api->socket_open_tcp(out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketOpenUdp(BkPlatformSocketHandle *out_handle) noexcept { return g_api != nullptr && g_api->socket_open_udp != nullptr ? g_api->socket_open_udp(out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketBind(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, uint16_t port) noexcept { return g_api != nullptr && g_api->socket_bind != nullptr ? g_api->socket_bind(handle, address, port) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketListen(BkPlatformSocketHandle handle, int32_t backlog) noexcept { return g_api != nullptr && g_api->socket_listen != nullptr ? g_api->socket_listen(handle, backlog) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketConnect(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address) noexcept { return g_api != nullptr && g_api->socket_connect != nullptr ? g_api->socket_connect(handle, address) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketAccept(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, BkPlatformSocketHandle *out_handle) noexcept { return g_api != nullptr && g_api->socket_accept != nullptr ? g_api->socket_accept(handle, address, out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
int32_t Client::SocketSend(BkPlatformSocketHandle handle, const void *data, int32_t size) noexcept { return g_api != nullptr && g_api->socket_send != nullptr ? g_api->socket_send(handle, data, size) : -1; }
int32_t Client::SocketReceive(BkPlatformSocketHandle handle, void *data, int32_t size) noexcept { return g_api != nullptr && g_api->socket_receive != nullptr ? g_api->socket_receive(handle, data, size) : -1; }
int32_t Client::SocketSendTo(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address, const void *data, int32_t size) noexcept { return g_api != nullptr && g_api->socket_send_to != nullptr ? g_api->socket_send_to(handle, address, data, size) : -1; }
int32_t Client::SocketReceiveFrom(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, void *data, int32_t size) noexcept { return g_api != nullptr && g_api->socket_receive_from != nullptr ? g_api->socket_receive_from(handle, address, data, size) : -1; }
BkPlatformResult Client::SocketSetNonblocking(BkPlatformSocketHandle handle, uint32_t enabled) noexcept { return g_api != nullptr && g_api->socket_set_nonblocking != nullptr ? g_api->socket_set_nonblocking(handle, enabled) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketWaitReadable(BkPlatformSocketHandle handle, int32_t timeout_milliseconds) noexcept { return g_api != nullptr && g_api->socket_wait_readable != nullptr ? g_api->socket_wait_readable(handle, timeout_milliseconds) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::SocketResolveIPv4(const char *host, uint16_t port, BkPlatformSocketAddress *address) noexcept { return g_api != nullptr && g_api->socket_resolve_ipv4 != nullptr ? g_api->socket_resolve_ipv4(host, port, address) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformSocketError Client::SocketLastError() noexcept { return g_api != nullptr && g_api->socket_last_error != nullptr ? g_api->socket_last_error() : BK_PLATFORM_SOCKET_ERROR_UNKNOWN; }
BkPlatformResult Client::SocketClose(BkPlatformSocketHandle handle) noexcept { return g_api != nullptr && g_api->socket_close != nullptr ? g_api->socket_close(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::LibraryOpen(BkPlatformUtf8Span path, BkPlatformHandle *out_handle) noexcept { return g_api != nullptr && g_api->library_open != nullptr ? g_api->library_open(path, out_handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::LibrarySymbol(BkPlatformHandle handle, BkPlatformUtf8Span name, void **out_symbol) noexcept { return g_api != nullptr && g_api->library_symbol != nullptr ? g_api->library_symbol(handle, name, out_symbol) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }
BkPlatformResult Client::LibraryClose(BkPlatformHandle handle) noexcept { return g_api != nullptr && g_api->library_close != nullptr ? g_api->library_close(handle) : BK_PLATFORM_ERROR_NOT_INITIALIZED; }

}
