#include "StdAfx.h"
#include "NetLowest.h"
#include "Streams.h"
#include "Misc/Thread.h"
#include "Platform/Clock.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace NNet {
int nTrafficPackets = 0;
int nTrafficTotalSize = 0;
}

static bool Check(bool value, const char *message) {
    if (!value) std::fprintf(stderr, "network workers test failed: %s\n", message);
    return value;
}

class ReceiveWorker final : public CThread {
    NNet::CLinksManager &links;
    NNet::CNodeAddress peer;
    std::atomic<int> received;

protected:
    void Step() override {
        if (!NPlatform::WaitReadable(links.GetSocket(), 25)) return;
        NNet::CNodeAddress source;
        CMemoryStream packet;
        if (links.Recv(&source, &packet)) {
            received.fetch_add(1, std::memory_order_relaxed);
            const unsigned char responseBytes[] = {0x41, 0x43, 0x4b};
            CMemoryStream response;
            response.SetSize(sizeof(responseBytes));
            std::memcpy(response.GetBufferForWrite(), responseBytes, sizeof(responseBytes));
            links.Send(peer, response);
        }
    }

public:
    ReceiveWorker(NNet::CLinksManager &owner, const NNet::CNodeAddress &remote) : CThread(0), links(owner), peer(remote), received(0) {}
    int Received() const { return received.load(std::memory_order_relaxed); }
};

int main() {
    const unsigned char payload[] = {0x42, 0x4b, 0x2d, 0x57, 0x41, 0x4b, 0x45};
    for (int cycle = 0; cycle < 100; ++cycle) {
        NNet::CLinksManager receiver;
        NNet::CLinksManager sender;
        const int port = 39200 + cycle;
        if (!Check(receiver.Start(port) && sender.Start(0), "start pair")) return 1;
        NNet::CNodeAddressSet addresses;
        NNet::CNodeAddress receiverAddress;
        if (!Check(receiver.GetSelfAddress(&addresses) && addresses.GetAddress(0, &receiverAddress), "receiver address")) return 2;
        NNet::CNodeAddressSet senderAddresses;
        NNet::CNodeAddress senderAddress;
        if (!Check(sender.GetSelfAddress(&senderAddresses) && senderAddresses.GetAddress(0, &senderAddress), "sender address")) return 2;

        ReceiveWorker worker(receiver, senderAddress);
        worker.RunThread();
        CMemoryStream packet;
        packet.SetSize(sizeof(payload));
        std::memcpy(packet.GetBufferForWrite(), payload, sizeof(payload));
        if (!Check(sender.Send(receiverAddress, packet), "send wake packet")) return 3;
        for (int wait = 0; wait < 100 && worker.Received() == 0; ++wait) NPlatform::SleepMilliseconds(1);
        if (!Check(worker.Received() == 1, "incoming packet wake")) return 4;
        if (!Check(NPlatform::WaitReadable(sender.GetSocket(), 100), "peer response wake")) return 4;
        NNet::CNodeAddress responseSource;
        CMemoryStream response;
        if (!Check(sender.Recv(&responseSource, &response) && response.GetSize() == 3 && std::memcmp(response.GetBuffer(), "ACK", 3) == 0, "two-peer response")) return 4;

        sender.Finish();
        worker.StopThread();
        receiver.Finish();
        if (!Check(receiver.GetSocket() == NPlatform::InvalidSocket && sender.GetSocket() == NPlatform::InvalidSocket, "close before restart")) return 5;

        NNet::CLinksManager restart;
        if (!Check(restart.Start(port), "immediate restart")) return 6;
        restart.Finish();
    }
    std::puts("network workers passed: cycles=100 wake/cancel/restart");
    return 0;
}
