#ifndef __NETACKS_H_
#define __NETACKS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <winsock.h>
#include <vector>
#include <list>
class CBitStream;
namespace NNet
{
typedef unsigned int UPDATE_ID;
typedef unsigned short PACKET_ID;
class CAckTracker
{
public:
	CAckTracker();
	static void SetMaxRTT( int nRTT ) { nMaxRTT = nRTT; }
	float GetRTT() const { return fAvrgRTT; }
	float GetRTTDisp() const { return sqrt( fabs( sqr( fAvrgRTT ) - fAvrgRTT2 ) ); }
	float GetTimeSinceLastRecv() const { return fTimeSinceLastRecv; }
	const float GetPing() const { return fPing; }
	bool CanSend(); 
	bool NeedSend() const;
	void Step( std::vector<PACKET_ID> *pRolled, std::vector<PACKET_ID> *pErased, double fDeltaTime );
	PACKET_ID WrtieAcks( CBitStream *pBits, int nPktSize );
	bool ReadAcks( std::vector<PACKET_ID> *acked, CBitStream &bits );
	void PacketLost( PACKET_ID pktID );

private:
	static int nMaxRTT;
	PACKET_ID nPktSent;        // is used to order update packets
	UPDATE_ID nPktLastReceived;// number of last received pkt
	UPDATE_ID nPktHighCounter; // high part of packet counter
	std::vector<UPDATE_ID> receivedPkts;
	DWORD dwAckedBits;
	PACKET_ID nAckedLast;
	float fAvrgRTT, fAvrgRTT2;    // RTT statistics

	float fPing;
	long nPingPacketsReceived;		
	float fSumPktRTT4Period;
	float fLastPingUpdateTime;

	float fWindow;
	int nFlyPackets;
	float fTimeSinceLastSend;
	float fTimeSinceLastRecv;     // time elapsed since last receive
	double fCurrentTime;
	class CUpdate 
	{
	public: 
		double fTimeCreated;
		PACKET_ID nPktNumber;
		bool bOnTheWindowEdge;
	};
	std::list< CUpdate > sentUpdates, rolledUpdates;    // unacknowledged updates
	
	void RegisterRTT( float fRTT );
	void SendPktAcks( CBitStream *pBits );
	void ReceiveAck( std::vector<PACKET_ID> *pAcked, PACKET_ID nPkt );
	void ReceivePktAcks( std::vector<PACKET_ID> *pAcked, CBitStream &bits );
	bool CheckRecvPacketNumber( UPDATE_ID nPkt );
};
}
#endif
