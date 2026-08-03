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
}

#endif
