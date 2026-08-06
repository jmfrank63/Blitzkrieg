#include "StdAfx.h"
#include "NetLowest.h"
#include "Streams.h"

#include <cstdio>
#include <cstring>

namespace NNet {
int nTrafficPackets = 0;
int nTrafficTotalSize = 0;
}

static bool Check(bool value, const char *message)
{
	if (!value)
		std::fprintf(stderr, "netlowest test failed: %s\n", message);
	return value;
}

int main()
{
	using namespace NNet;

	CLinksManager receiver;
	if (!Check(receiver.Start(39131), "receiver start"))
		return 1;
	CLinksManager sender;
	if (!Check(sender.Start(39132), "sender start"))
		return 1;

	CNodeAddressSet localAddresses;
	if (!Check(receiver.GetSelfAddress(&localAddresses), "receiver local address"))
		return 1;
	CNodeAddress receiverAddress;
	if (!Check(localAddresses.GetAddress(0, &receiverAddress), "receiver address entry"))
		return 1;
	if (!Check(receiverAddress.GetFastName() == "127.0.0.1:39131", "address and port representation"))
		return 1;
	if (!Check(receiver.IsLocalAddr(receiverAddress), "loopback locality"))
		return 1;

	CNodeAddress emptySource;
	CMemoryStream received;
	if (!Check(!receiver.Recv(&emptySource, &received), "empty nonblocking receive"))
		return 1;

	const unsigned char payload[] = { 0x42, 0x4b, 0x2d, 0x55, 0x44, 0x50, 0x00, 0xff };
	CMemoryStream packet;
	packet.SetSize(sizeof(payload));
	std::memcpy(packet.GetBufferForWrite(), payload, sizeof(payload));
	if (!Check(sender.Send(receiverAddress, packet), "loopback send"))
		return 1;
	if (!Check(NPlatform::WaitReadable(receiver.GetSocket(), 100), "loopback readiness"))
		return 1;
	if (!Check(receiver.Recv(&emptySource, &received), "loopback receive"))
		return 1;
	if (!Check(received.GetSize() == sizeof(payload) && std::memcmp(received.GetBuffer(), payload, sizeof(payload)) == 0, "byte-identical packet"))
		return 1;
	if (!Check(emptySource.GetFastName() == "127.0.0.1:39132", "receive address reporting"))
		return 1;

	CMemoryStream oversized;
	oversized.SetSize(2057);
	for (int i = 0; i < oversized.GetSize(); ++i)
		oversized.GetBufferForWrite()[i] = static_cast<unsigned char>(i * 17 + 3);
	if (!Check(sender.Send(receiverAddress, oversized), "oversize send"))
		return 1;
	if (!Check(NPlatform::WaitReadable(receiver.GetSocket(), 100), "oversize readiness"))
		return 1;
	if (!Check(receiver.Recv(&emptySource, &received), "oversize receive"))
		return 1;
	if (!Check(received.GetSize() == 2048 && std::memcmp(received.GetBuffer(), oversized.GetBuffer(), 2048) == 0, "oversize truncation"))
		return 1;

	receiver.Finish();
	if (!Check(receiver.GetSocket() == NPlatform::InvalidSocket, "close"))
		return 1;
	if (!Check(receiver.Start(39131), "reinitialize"))
		return 1;
	receiver.Finish();
	sender.Finish();

	std::puts("netlowest loopback UDP passed");
	return 0;
}
