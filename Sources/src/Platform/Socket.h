#ifndef BLITZKRIEG_PLATFORM_SOCKET_H
#define BLITZKRIEG_PLATFORM_SOCKET_H

#include <cstdint>
#include "Compiler.h"

#if defined(BK_PLATFORM_RUNTIME_BUILD)
#define BK_PLATFORM_RUNTIME_SOCKET_API BK_EXPORT
#elif defined(_WIN32) || defined(_WIN64)
#define BK_PLATFORM_RUNTIME_SOCKET_API BK_IMPORT
#else
#define BK_PLATFORM_RUNTIME_SOCKET_API BK_EXPORT
#endif

namespace NPlatform
{
using SocketHandle = std::intptr_t;
constexpr SocketHandle InvalidSocket = static_cast<SocketHandle>( -1 );

enum class SocketError
{
	none,
	wouldBlock,
	interrupted,
	connectionReset,
	connectionRefused,
	timedOut,
	addressInUse,
	unknown,
};

enum class SocketResult { success, failed };

#pragma pack(push, 1)
struct SocketAddress
{
	std::uint16_t family;
	std::uint8_t data[14];
};
#pragma pack(pop)

static_assert( sizeof(SocketAddress) == 16, "socket address ABI must match sockaddr" );
static_assert( sizeof(SocketHandle) == sizeof(void *), "socket handles must be pointer-sized" );

BK_PLATFORM_RUNTIME_SOCKET_API bool SocketRuntimeInit();
BK_PLATFORM_RUNTIME_SOCKET_API void SocketRuntimeDone();
BK_PLATFORM_RUNTIME_SOCKET_API SocketHandle OpenTcpSocket();
BK_PLATFORM_RUNTIME_SOCKET_API SocketHandle OpenUdpSocket();
BK_PLATFORM_RUNTIME_SOCKET_API bool Bind(SocketHandle socket, SocketAddress *address, std::uint16_t port);
BK_PLATFORM_RUNTIME_SOCKET_API bool GetLocalAddress(SocketHandle socket, SocketAddress *address);
BK_PLATFORM_RUNTIME_SOCKET_API bool Listen(SocketHandle socket, int backlog);
BK_PLATFORM_RUNTIME_SOCKET_API bool Connect(SocketHandle socket, const SocketAddress &address);
BK_PLATFORM_RUNTIME_SOCKET_API SocketHandle Accept(SocketHandle socket, SocketAddress *address);
BK_PLATFORM_RUNTIME_SOCKET_API int Send(SocketHandle socket, const void *data, int size);
BK_PLATFORM_RUNTIME_SOCKET_API int Receive(SocketHandle socket, void *data, int size);
BK_PLATFORM_RUNTIME_SOCKET_API int SendTo(SocketHandle socket, const SocketAddress &address, const void *data, int size);
BK_PLATFORM_RUNTIME_SOCKET_API int ReceiveFrom(SocketHandle socket, SocketAddress *address, void *data, int size);
BK_PLATFORM_RUNTIME_SOCKET_API bool SetNonBlocking(SocketHandle socket, bool enabled);
BK_PLATFORM_RUNTIME_SOCKET_API bool SetBroadcast(SocketHandle socket, bool enabled);
BK_PLATFORM_RUNTIME_SOCKET_API bool WaitReadable(SocketHandle socket, int timeoutMilliseconds);
BK_PLATFORM_RUNTIME_SOCKET_API bool ResolveIPv4(const char *host, std::uint16_t port, SocketAddress *address);
BK_PLATFORM_RUNTIME_SOCKET_API SocketError LastError();
BK_PLATFORM_RUNTIME_SOCKET_API void Close(SocketHandle socket);
}

#endif
