#include "Socket.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>

namespace
{
SOCKET Native(NPlatform::SocketHandle socket) { return static_cast<SOCKET>( socket ); }
NPlatform::SocketHandle Portable(SOCKET socket) { return static_cast<NPlatform::SocketHandle>( socket ); }
void FromNative(const sockaddr_in &native, NPlatform::SocketAddress *address)
{
	address->family = static_cast<std::uint16_t>( native.sin_family );
	std::memcpy( address->data, &native.sin_addr.s_addr, 4 );
	std::memcpy( address->data + 4, &native.sin_port, 2 );
}
sockaddr_in ToNative(const NPlatform::SocketAddress &address)
{
	sockaddr_in native{};
	native.sin_family = static_cast<ADDRESS_FAMILY>( address.family );
	std::memcpy( &native.sin_addr.s_addr, address.data, 4 );
	std::memcpy( &native.sin_port, address.data + 4, 2 );
	return native;
}
}

namespace NPlatform
{
bool SocketRuntimeInit() { WSADATA data{}; return WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0; }
void SocketRuntimeDone() { WSACleanup(); }
SocketHandle OpenTcpSocket() { return Portable( socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ); }
SocketHandle OpenUdpSocket() { return Portable( socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ); }
bool Bind(SocketHandle socketHandle, SocketAddress *address, std::uint16_t port)
{
	sockaddr_in native = address ? ToNative( *address ) : sockaddr_in{};
	native.sin_family = AF_INET; native.sin_addr.s_addr = address ? native.sin_addr.s_addr : htonl( INADDR_ANY ); native.sin_port = htons( port );
	const bool result = ::bind( Native(socketHandle), reinterpret_cast<const sockaddr *>( &native ), sizeof(native) ) == 0;
	if ( result && address ) FromNative( native, address );
	return result;
}
bool GetLocalAddress(SocketHandle socketHandle, SocketAddress *address)
{
	if ( address == nullptr ) return false;
	sockaddr_in native{}; int length = sizeof(native);
	if ( getsockname( Native(socketHandle), reinterpret_cast<sockaddr *>( &native ), &length ) != 0 ) return false;
	FromNative( native, address );
	return true;
}
bool Listen(SocketHandle socketHandle, int backlog) { return ::listen( Native(socketHandle), backlog ) == 0; }
bool Connect(SocketHandle socketHandle, const SocketAddress &address) { const sockaddr_in native = ToNative(address); return ::connect( Native(socketHandle), reinterpret_cast<const sockaddr *>( &native ), sizeof(native) ) == 0; }
SocketHandle Accept(SocketHandle socketHandle, SocketAddress *address)
{
	sockaddr_in native{}; int length = sizeof(native); SOCKET result = ::accept( Native(socketHandle), reinterpret_cast<sockaddr *>( &native ), &length );
	if ( result != INVALID_SOCKET && address ) FromNative( native, address );
	return Portable( result );
}
int Send(SocketHandle socketHandle, const void *data, int size) { return ::send( Native(socketHandle), static_cast<const char *>(data), size, 0 ); }
int Receive(SocketHandle socketHandle, void *data, int size) { return ::recv( Native(socketHandle), static_cast<char *>(data), size, 0 ); }
int SendTo(SocketHandle socketHandle, const SocketAddress &address, const void *data, int size) { const sockaddr_in native = ToNative(address); return ::sendto( Native(socketHandle), static_cast<const char *>(data), size, 0, reinterpret_cast<const sockaddr *>( &native ), sizeof(native) ); }
int ReceiveFrom(SocketHandle socketHandle, SocketAddress *address, void *data, int size)
{
	sockaddr_in native{}; int length = sizeof(native); int result = ::recvfrom( Native(socketHandle), static_cast<char *>(data), size, 0, reinterpret_cast<sockaddr *>( &native ), &length );
	if ( result == SOCKET_ERROR && WSAGetLastError() == WSAEMSGSIZE ) result = size;
	if ( result >= 0 && address ) FromNative( native, address );
	return result;
}
bool SetNonBlocking(SocketHandle socketHandle, bool enabled) { u_long mode = enabled ? 1 : 0; return ioctlsocket( Native(socketHandle), FIONBIO, &mode ) == 0; }
bool SetBroadcast(SocketHandle socketHandle, bool enabled) { const char value = enabled ? 1 : 0; return setsockopt( Native(socketHandle), SOL_SOCKET, SO_BROADCAST, &value, sizeof(value) ) == 0; }
bool WaitReadable(SocketHandle socketHandle, int timeoutMilliseconds)
{
	fd_set set; FD_ZERO( &set ); FD_SET( Native(socketHandle), &set ); timeval timeout{ timeoutMilliseconds / 1000, (timeoutMilliseconds % 1000) * 1000 }; return select( 0, &set, 0, 0, &timeout ) > 0;
}
bool ResolveIPv4(const char *host, std::uint16_t port, SocketAddress *address)
{
	addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; addrinfo *result = 0;
	char service[16]; std::snprintf( service, sizeof(service), "%u", port );
	if ( getaddrinfo( host, service, &hints, &result ) != 0 || !result ) return false;
	FromNative( *reinterpret_cast<sockaddr_in *>( result->ai_addr ), address ); freeaddrinfo( result ); return true;
}
SocketError LastError()
{
	switch ( WSAGetLastError() ) { case WSAEWOULDBLOCK: return SocketError::wouldBlock; case WSAEINTR: return SocketError::interrupted; case WSAECONNRESET: return SocketError::connectionReset; case WSAECONNREFUSED: return SocketError::connectionRefused; case WSAETIMEDOUT: return SocketError::timedOut; case WSAEADDRINUSE: return SocketError::addressInUse; default: return SocketError::unknown; }
}
void Close(SocketHandle socketHandle) { if ( socketHandle != InvalidSocket ) closesocket( Native(socketHandle) ); }
}
#endif
