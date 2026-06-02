#ifndef __NETPEER2PEER_H_
#define __NETPEER2PEER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <vector>
#include <list>
#include "NetLowest.h"
#include "Streams.h"
namespace NNet
{
class CP2PTracker
{
public:
	typedef unsigned short UCID;
	enum EOutMessage
	{
		NEW_CIENT,
		REMOVE_CLIENT,
		DIRECT,
		BROADCAST
	};
	struct SMessage
	{
		EOutMessage msg;
		UCID from;
		std::vector<UCID> received;
		CMemoryStream pkt;
	};
	struct SPacket
	{
		UCID addr;
		CMemoryStream pkt;
		
		SPacket() {}
		SPacket( const UCID &_addr, CMemoryStream &_pkt ): addr(_addr), pkt(_pkt) {}
	};
	std::vector<SPacket> packets;

	void SendBroadcast( CMemoryStream &pkt );
	void SendDirect( const UCID &addr, CMemoryStream &pkt );
	bool GetMessage( SMessage *pRes );
	void ProcessPacket( const UCID &addr, CMemoryStream &pkt );
	void AddNewClient( const UCID &addr, const CMemoryStream &addrInfo );
	void KickClient( const UCID &addr );
	bool IsActive( const UCID &addr );
private:
	struct SAck
	{
		UCID addr;
		bool bAcked;
	};
	struct SQMessage
	{
		int nID;
		bool bDirect;
		CMemoryStream msg;
		std::list<SAck> acks;
	};
	struct SFastAck
	{
		int nID;
		UCID addr; // broadcast sender address
	};
	typedef char PEER_ID;
	struct SPeerClient
	{
		UCID addr;
		PEER_ID id;

		SPeerClient() {}
		SPeerClient( const UCID &_addr, PEER_ID _id ): addr(_addr), id(_id) {}
	};
	struct SPeer
	{
		UCID addr;
		bool bActive;    // not active when peer is being dropped
		std::list<SPeerClient> clients; // current clients from peer view
		std::list<SQMessage> messages;
		std::list<SFastAck> fastacks; // acks received from this peer before message itself
		std::list<UCID> requireKick;
		PEER_ID id;
		CMemoryStream addrInfo;

		bool IsActive() const { return bActive; }
		bool HasClient( const UCID &addr ) const
		{
			for ( std::list<SPeerClient>::const_iterator i = clients.begin(); i != clients.end(); ++i )
				if ( i->addr == addr )
					return true;
			return false;
		}
		const UCID& GetAddr( PEER_ID id )
		{
			for ( std::list<SPeerClient>::iterator i = clients.begin(); i != clients.end(); ++i )
			{
				if ( i->id == id )
					return i->addr;
			}
			ASSERT( 0 );
			return clients.front().addr;
		}
		void RemoveClient( const UCID &addr )
		{
			for ( std::list<SPeerClient>::iterator i = clients.begin(); i != clients.end(); )
			{
				if ( i->addr == addr )
					i = clients.erase( i );
				else
					++i;
			}
		}
	};
	std::list<SMessage> output;
	std::list<SPeer> clients;

	SPeer* GetClient( const UCID &addr );
	SPeer* GetClient( PEER_ID id );
	void AddOutputMessage( EOutMessage msg, const UCID &_from, 
		const CMemoryStream *pData = 0, std::vector<UCID> *pReceived = 0 );
	void AddKickApprove( const UCID &victim, const UCID &kickFrom );
	void ApproveKick( SPeer *pVictim, const UCID &from );
	PEER_ID GetUnusedID() const;
	void ReceiveBroadcast( SPeer *pWho, CMemoryStream &data, int nID );
	void ReceiveDirect( SPeer *pWho, CMemoryStream &data );
	void ReceiveAddClient( SPeer *pWho, const UCID &who, PEER_ID id, const CMemoryStream &addrInfo );
	void ReceiveRemoveClient( SPeer *pWho, const UCID &whom );
	void ReceiveAck( SPeer *pWho, int nID, PEER_ID id );
	void CheckQueuedMessages( SPeer *pWho );
	void CheckCorpses();
	void SendRemoveClient( const SPeer &dest, const UCID &whom );
	void SendAddClient( const SPeer &dest, const UCID &whom, PEER_ID id, const CMemoryStream &addrInfo );
	void SendBroadcast( const SPeer &dest, int nID, CMemoryStream &data );
	void SendDirect( const SPeer &dest, CMemoryStream &pkt );
	void SendAck( const SPeer &dest, int nID, PEER_ID id );
};	
}
#endif
