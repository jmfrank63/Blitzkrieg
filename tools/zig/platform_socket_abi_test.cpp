#include "PlatformABI/platform_c.h"

#include <cstdio>
#include <cstring>

static bool Check(bool value, const char *message) {
    if (!value) std::fprintf(stderr, "platform socket ABI test failed: %s\n", message);
    return value;
}

int main() {
    const BkPlatformApi *api = bk_platform_get_api(BK_PLATFORM_ABI_VERSION);
    if (!Check(api != nullptr && api->struct_size >= sizeof(BkPlatformApi), "API table")) return 1;

    BkPlatformCreateInfo info = {};
    info.struct_size = sizeof(info);
    info.requested_abi_version = BK_PLATFORM_ABI_VERSION;
    if (!Check(api->runtime_create(&info) == BK_PLATFORM_OK, "runtime create")) return 2;
    if (!Check(api->socket_runtime_init() == BK_PLATFORM_OK, "socket init")) return 3;
    if (!Check(api->socket_runtime_init() == BK_PLATFORM_OK, "socket init refcount")) return 4;

    BkPlatformSocketHandle listener = 0;
    if (!Check(api->socket_open_tcp(&listener) == BK_PLATFORM_OK && listener != 0, "open listener")) return 5;
    BkPlatformSocketAddress bound = {};
    if (!Check(api->socket_bind(listener, &bound, 39123) == BK_PLATFORM_OK, "bind listener")) return 6;
    if (!Check(api->socket_listen(listener, 1) == BK_PLATFORM_OK, "listen")) return 7;

    BkPlatformSocketHandle client = 0;
    if (!Check(api->socket_open_tcp(&client) == BK_PLATFORM_OK, "open client")) return 8;
    BkPlatformSocketAddress loopback = {};
    if (!Check(api->socket_resolve_ipv4("127.0.0.1", 39123, &loopback) == BK_PLATFORM_OK, "resolve loopback")) return 9;
    if (!Check(api->socket_connect(client, &loopback) == BK_PLATFORM_OK, "connect")) return 10;

    BkPlatformSocketHandle accepted = 0;
    if (!Check(api->socket_accept(listener, nullptr, &accepted) == BK_PLATFORM_OK, "accept")) return 11;
    const char payload[] = "platform-socket-abi";
    char received[sizeof(payload)] = {};
    if (!Check(api->socket_send(client, payload, static_cast<int32_t>(sizeof(payload))) == static_cast<int32_t>(sizeof(payload)), "send")) return 12;
    if (!Check(api->socket_receive(accepted, received, static_cast<int32_t>(sizeof(received))) == static_cast<int32_t>(sizeof(payload)) && std::memcmp(payload, received, sizeof(payload)) == 0, "receive")) return 13;
    if (!Check(api->socket_set_nonblocking(accepted, 1) == BK_PLATFORM_OK, "nonblocking")) return 14;
    if (!Check(api->socket_receive(accepted, received, static_cast<int32_t>(sizeof(received))) < 0 && api->socket_last_error() == BK_PLATFORM_SOCKET_ERROR_WOULD_BLOCK, "would block")) return 15;

    const BkPlatformSocketHandle stale = client;
    if (!Check(api->socket_close(accepted) == BK_PLATFORM_OK, "close accepted")) return 16;
    if (!Check(api->socket_close(client) == BK_PLATFORM_OK, "close client")) return 17;
    if (!Check(api->socket_close(stale) == BK_PLATFORM_ERROR_INVALID_ARGUMENT, "stale generation rejected")) return 18;
    if (!Check(api->socket_close(listener) == BK_PLATFORM_OK, "close listener")) return 19;
    if (!Check(api->socket_runtime_done() == BK_PLATFORM_OK, "socket done refcount")) return 20;
    if (!Check(api->socket_runtime_done() == BK_PLATFORM_OK, "socket done")) return 21;
    if (!Check(api->socket_open_tcp(&client) == BK_PLATFORM_ERROR_NOT_INITIALIZED, "open after done rejected")) return 22;

    api->runtime_destroy();
    std::puts("platform socket ABI passed: opaque generational handles");
    return 0;
}
