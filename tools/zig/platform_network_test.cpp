#include "Socket.h"
#include <cstdio>
#include <cstring>

static bool Check(bool value, const char *message) { if (!value) std::fprintf(stderr, "network test failed: %s\n", message); return value; }

int main()
{
	using namespace NPlatform;
	if (!Check(SocketRuntimeInit(), "runtime init")) return 1;
	SocketHandle listener = OpenTcpSocket();
	SocketAddress address{};
	if (!Check(listener != InvalidSocket && Bind(listener, &address, 39091), "TCP bind")) return 1;
	if (!Check(Listen(listener, 1), "TCP listen")) return 1;
	SocketHandle client = OpenTcpSocket();
	SocketAddress loopback{};
	if (!Check(ResolveIPv4("127.0.0.1", 39091, &loopback), "address conversion")) return 1;
	SocketAddress serverAddress = loopback;
	if (!Check(Connect(client, serverAddress), "TCP connect")) return 1;
	SocketHandle accepted = Accept(listener, nullptr);
	if (!Check(accepted != InvalidSocket, "TCP accept")) return 1;
	const char payload[] = "portable";
	if (!Check(Send(client, payload, sizeof(payload)) == sizeof(payload), "TCP send")) return 1;
	char received[32] = {};
	if (!Check(Receive(accepted, received, sizeof(received)) == sizeof(payload) && std::strcmp(received, payload) == 0, "TCP receive")) return 1;
	if (!Check(SetNonBlocking(accepted, true) && Receive(accepted, received, sizeof(received)) < 0 && LastError() == SocketError::wouldBlock, "nonblocking would-block")) return 1;
	Close(accepted); Close(client); Close(listener); Close(listener); SocketRuntimeDone(); SocketRuntimeDone();
	std::puts("portable network loopback passed");
	return 0;
}
