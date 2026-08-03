#include "Socket.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

namespace
{
int Native(NPlatform::SocketHandle socket) { return static_cast<int>( socket ); }
void FromNative(const sockaddr_in &native, NPlatform::SocketAddress *address) { address->family = static_cast<std::uint16_t>( native.sin_family ); std::memcpy( address->data, &native.sin_addr.s_addr, 4 ); std::memcpy( address->data + 4, &native.sin_port, 2 ); }
sockaddr_in ToNative(const NPlatform::SocketAddress &address) { sockaddr_in native{}; native.sin_family = static_cast<sa_family_t>( address.family ); std::memcpy( &native.sin_addr.s_addr, address.data, 4 ); std::memcpy( &native.sin_port, address.data + 4, 2 ); return native; }
}
namespace NPlatform
{
bool SocketRuntimeInit() { return true; }
void SocketRuntimeDone() {}
SocketHandle OpenTcpSocket() { return socket( AF_INET, SOCK_STREAM, 0 ); }
SocketHandle OpenUdpSocket() { return socket( AF_INET, SOCK_DGRAM, 0 ); }
bool Bind(SocketHandle socketHandle, SocketAddress *address, std::uint16_t port) { sockaddr_in native = address ? ToNative(*address) : sockaddr_in{}; native.sin_family = AF_INET; if (!address) native.sin_addr.s_addr = htonl(INADDR_ANY); native.sin_port = htons(port); bool result = ::bind(Native(socketHandle), reinterpret_cast<sockaddr*>(&native), sizeof(native)) == 0; if (result && address) FromNative(native,address); return result; }
bool Listen(SocketHandle socketHandle, int backlog) { return ::listen(Native(socketHandle), backlog) == 0; }
bool Connect(SocketHandle socketHandle, const SocketAddress &address) { sockaddr_in native = ToNative(address); return ::connect(Native(socketHandle), reinterpret_cast<sockaddr*>(&native), sizeof(native)) == 0; }
SocketHandle Accept(SocketHandle socketHandle, SocketAddress *address) { sockaddr_in native{}; socklen_t length = sizeof(native); int result = ::accept(Native(socketHandle), reinterpret_cast<sockaddr*>(&native), &length); if (result >= 0 && address) FromNative(native,address); return result; }
int Send(SocketHandle socketHandle, const void *data, int size) { return static_cast<int>( ::send(Native(socketHandle), data, size, MSG_NOSIGNAL) ); }
int Receive(SocketHandle socketHandle, void *data, int size) { return static_cast<int>( ::recv(Native(socketHandle), data, size, 0) ); }
int SendTo(SocketHandle socketHandle, const SocketAddress &address, const void *data, int size) { sockaddr_in native = ToNative(address); return static_cast<int>( ::sendto(Native(socketHandle), data, size, MSG_NOSIGNAL, reinterpret_cast<sockaddr*>(&native), sizeof(native)) ); }
int ReceiveFrom(SocketHandle socketHandle, SocketAddress *address, void *data, int size) { sockaddr_in native{}; socklen_t length = sizeof(native); int result = static_cast<int>( ::recvfrom(Native(socketHandle), data, size, 0, reinterpret_cast<sockaddr*>(&native), &length) ); if (result >= 0 && address) FromNative(native,address); return result; }
bool SetNonBlocking(SocketHandle socketHandle, bool enabled) { int flags = fcntl(Native(socketHandle), F_GETFL, 0); return flags >= 0 && fcntl(Native(socketHandle), F_SETFL, enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) == 0; }
bool WaitReadable(SocketHandle socketHandle, int timeoutMilliseconds) { fd_set set; FD_ZERO(&set); FD_SET(Native(socketHandle),&set); timeval timeout{timeoutMilliseconds/1000,(timeoutMilliseconds%1000)*1000}; return select(Native(socketHandle)+1,&set,0,0,&timeout) > 0; }
bool ResolveIPv4(const char *host, std::uint16_t port, SocketAddress *address) { addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM; addrinfo *result=0; char service[16]; std::snprintf(service,sizeof(service),"%u",port); if(getaddrinfo(host,service,&hints,&result)!=0 || !result) return false; FromNative(*reinterpret_cast<sockaddr_in*>(result->ai_addr),address); freeaddrinfo(result); return true; }
SocketError LastError() { if (errno == EWOULDBLOCK) return SocketError::wouldBlock; if (errno == EAGAIN) return SocketError::wouldBlock; switch(errno) { case EINTR: return SocketError::interrupted; case ECONNRESET: return SocketError::connectionReset; case ECONNREFUSED: return SocketError::connectionRefused; case ETIMEDOUT: return SocketError::timedOut; case EADDRINUSE: return SocketError::addressInUse; default: return SocketError::unknown; } }
void Close(SocketHandle socketHandle) { if (socketHandle != InvalidSocket) close(Native(socketHandle)); }
}
#endif
