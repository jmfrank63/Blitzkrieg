#include "Socket.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

static_assert( NPlatform::InvalidSocket == static_cast<NPlatform::SocketHandle>(-1), "invalid socket value" );
static_assert( sizeof(NPlatform::SocketAddress) == 16, "IPv4 sockaddr-compatible size" );
static_assert( offsetof(NPlatform::SocketAddress, family) == 0, "address family offset" );
static_assert( offsetof(NPlatform::SocketAddress, data) == 2, "address payload offset" );

int main()
{
	if ( static_cast<int>(NPlatform::SocketError::wouldBlock) == static_cast<int>(NPlatform::SocketError::none) ) return 1;
	if ( static_cast<int>(NPlatform::SocketError::addressInUse) == static_cast<int>(NPlatform::SocketError::unknown) ) return 1;
	std::puts( "portable socket types passed" );
	return 0;
}
