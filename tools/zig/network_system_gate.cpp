#include "Socket.h"
#include "System.h"

#include <cstdio>
#include <cstring>

static bool capturedError = false;
static bool CaptureError(const char *, const char *) { capturedError = true; return true; }
static bool CaptureOpen(const char *, const char *) { return true; }
static unsigned int FixtureHash(const char *text)
{
	unsigned int hash = 2166136261u;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) { hash ^= *p; hash *= 16777619u; }
	return hash;
}
static bool Check(bool value, const char *message) { if (!value) std::fprintf(stderr, "network/system gate failed: %s\n", message); return value; }

int main(int argc, char **argv)
{
	if (argc > 1 && std::strcmp(argv[1], "--child") == 0) return 23;
	using namespace NPlatform;
	if (!Check(FixtureHash("BK-NET-FIXTURE-v1") == 0xc70d495eu, "fixture hash")) return 1;
	if (!Check(SocketRuntimeInit(), "socket init")) return 1;
	SocketHandle listener = OpenTcpSocket();
	SocketAddress any{};
	if (!Check(listener != InvalidSocket && Bind(listener, &any, 39092) && Listen(listener, 1), "TCP setup")) return 1;
	SocketHandle udp = OpenUdpSocket();
	if (!Check(udp != InvalidSocket, "UDP setup")) return 1;
	SocketHandle client = OpenTcpSocket();
	SocketAddress loopback{};
	if (!Check(ResolveIPv4("127.0.0.1", 39092, &loopback) && Connect(client, loopback), "TCP connect")) return 1;
	SocketHandle accepted = Accept(listener, nullptr);
	const char payload[] = "gate";
	if (!Check(accepted != InvalidSocket && Send(client, payload, sizeof(payload)) == sizeof(payload), "TCP exchange")) return 1;
	char received[8] = {};
	if (!Check(Receive(accepted, received, sizeof(received)) == sizeof(payload), "TCP receive")) return 1;
	if (!Check(SetNonBlocking(accepted, true) && Receive(accepted, received, sizeof(received)) < 0 && LastError() == SocketError::wouldBlock, "timeout normalization")) return 1;
	SetUiHandlers(CaptureError, CaptureOpen);
	if (!Check(ShowError("gate", "injected") && capturedError, "injected dialog")) return 1;
	int exitCode = -1;
	const std::string child = ExecutablePath() +
#if defined(_WIN32) || defined(_WIN64)
		"network-system-gate.exe";
#else
		"network-system-gate";
#endif
	if (!Check(RunProcess({ child, "--child" }, std::string(), &exitCode) && exitCode == 23, "child exit code")) return 1;
	Close(accepted); Close(client); Close(udp); Close(listener); Close(listener); SocketRuntimeDone(); SocketRuntimeDone();
	std::puts("network/system gate passed: fixture=c70d495e tcp=4 child=23");
	return 0;
}
