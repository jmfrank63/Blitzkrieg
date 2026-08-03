#ifndef BLITZKRIEG_PLATFORM_SOCKET_H
#define BLITZKRIEG_PLATFORM_SOCKET_H

#include <cstdint>

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

bool SocketRuntimeInit();
void SocketRuntimeDone();
SocketHandle OpenTcpSocket();
SocketHandle OpenUdpSocket();
bool Bind(SocketHandle socket, SocketAddress *address, std::uint16_t port);
bool Listen(SocketHandle socket, int backlog);
bool Connect(SocketHandle socket, const SocketAddress &address);
SocketHandle Accept(SocketHandle socket, SocketAddress *address);
int Send(SocketHandle socket, const void *data, int size);
int Receive(SocketHandle socket, void *data, int size);
int SendTo(SocketHandle socket, const SocketAddress &address, const void *data, int size);
int ReceiveFrom(SocketHandle socket, SocketAddress *address, void *data, int size);
bool SetNonBlocking(SocketHandle socket, bool enabled);
bool WaitReadable(SocketHandle socket, int timeoutMilliseconds);
bool ResolveIPv4(const char *host, std::uint16_t port, SocketAddress *address);
SocketError LastError();
void Close(SocketHandle socket);
}

#endif
