#include "StdAfx.h"
#include "NetPeer2Peer.h"
//#define LOG
#ifdef LOG
#include <iostream>
#endif

#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
#define __LOG__
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)

#ifdef __LOG__
#include "..\StreamIO\globals.h"
#endif // __LOG__n
/////////////////////////////////////////////////////////////////////////////////////
namespace NNet
{
/////////////////////////////////////////////////////////////////////////////////////
// CP2PTracker
/////////////////////////////////////////////////////////////////////////////////////
enum EPacket
{
	PKT_ADD_CLIENT,
	PKT_REMOVE_CLIENT,
	PKT_BROADCAST_MSG,
	PKT_DIRECT_MSG,
	PKT_ACK,
	PKT_KICK_ADDR
};
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::AddOutputMessage( EOutMessage msg, const UCID &_from, 
		const CMemoryStream *pData, std::vector<UCID> *pReceived )
{
#ifdef LOG
	cout << "OUT ";
	switch ( msg )
	{
		case NEW_CIENT: cout << "new client " << _from.GetFastName() << endl; break;
		case REMOVE_CLIENT: cout << "del client " << _from.GetFastName() << endl; break;
		case DIRECT: cout << "direct from " << _from.GetFastName() << endl; break;
		case BROADCAST: cout << "broadcast from " << _from.GetFastName() << endl; break;
		default:
			ASSERT( 0 );
			break;
	}
#endif
	output.push_back( SMessage() );
	SMessage &res = output.back();
	res.msg = msg;
	res.from = _from;
	if ( pData )
		res.pkt = *pData;
	if ( pReceived )
		res.received = *pReceived;

#ifdef __LOG__
/*
	switch ( msg )
	{
		case DIRECT:		
			{
				BYTE cMsgID = 0xff;		
				if ( pData )
				{
					pData->Seek( 0 );
					(*pData) >> cMsgID;
				}
				
				GetSingleton<IConsoleBuffer>()->WriteASCII(
					CONSOLE_STREAM_CONSOLE, 
					NStr::Format( "p2p: direct from %s, msg %d", _from.GetFastName().c_str(), (int)cMsgID ), 0xffffff00, true );
			}

			break;
		case NEW_CIENT: 
			GetSingleton<IConsoleBuffer>()->WriteASCII(
				CONSOLE_STREAM_CONSOLE, 
				NStr::Format( "p2p: new client from %s", _from.GetFastName().c_str() ), 0xffffff00, true );

			break;
		case REMOVE_CLIENT: 
			GetSingleton<IConsoleBuffer>()->WriteASCII(
				CONSOLE_STREAM_CONSOLE, 
				NStr::Format( "p2p: del client %s", _from.GetFastName().c_str() ), 0xffffff00, true );

			break;
	}
*/
#endif // __LOG__
}
/////////////////////////////////////////////////////////////////////////////////////
bool CP2PTracker::GetMessage( SMessage *pRes )
{
	if ( output.empty() )
		return false;
	*pRes = output.front();
	output.pop_front();
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
CP2PTracker::SPeer* CP2PTracker::GetClient( const UCID &addr )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		if ( i->addr == addr )
			return &(*i);
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////
CP2PTracker::SPeer* CP2PTracker::GetClient( PEER_ID id )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		if ( i->id == id )
			return &(*i);
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CP2PTracker::IsActive( const UCID &addr )
{
	SPeer *pTest = GetClient( addr );
	if ( !pTest )
		return false;
	return pTest->IsActive();
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::CheckQueuedMessages( SPeer *pWho )
{
	while ( !pWho->messages.empty() )
	{
		SQMessage &b = pWho->messages.front();
		bool bAcked = true;
		for ( std::list<SAck>::iterator i1 = b.acks.begin(); i1 != b.acks.end(); ++i1 )
			bAcked &= i1->bAcked;
		if ( !bAcked )
			break;
		std::vector<UCID> res;
		for ( std::list<SAck>::iterator i2 = b.acks.begin(); i2 != b.acks.end(); ++i2 )
			res.push_back( i2->addr );
		if ( b.bDirect )
			AddOutputMessage( DIRECT, pWho->addr, &b.msg );
		else
			AddOutputMessage( BROADCAST, pWho->addr, &b.msg, &res );
		pWho->messages.pop_front();
	}
	CheckCorpses();
}
/////////////////////////////////////////////////////////////////////////////////////
// check if it is time to remove some inactive clients
void CP2PTracker::CheckCorpses()
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); )
	{
		if ( !i->IsActive() && i->messages.empty() && i->requireKick.empty() )
		{
			AddOutputMessage( REMOVE_CLIENT, i->addr );
			i = clients.erase( i );
		}
		else
			++i;
	}
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::AddKickApprove( const UCID &victim, const UCID &kickFrom )
{
	SPeer *pV = GetClient( victim );
	if ( pV )
	{
		if ( std::find( pV->requireKick.begin(), pV->requireKick.end(), kickFrom ) == pV->requireKick.end() )
			pV->requireKick.push_back( kickFrom );
	}
	else
		ASSERT( 0 );
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ApproveKick( SPeer *pVictim, const UCID &from )
{
	pVictim->requireKick.remove( from );
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ReceiveDirect( SPeer *pWho, CMemoryStream &data )
{
#ifdef LOG
	cout << "RECV direct from " << pWho->addr.GetFastName() << endl;
#endif
	// add to pending list
	pWho->messages.push_back( SQMessage() );
	SQMessage &b = pWho->messages.back();
	b.msg = data;
	b.nID = -1;
	b.bDirect = true;
	CheckQueuedMessages( pWho );	
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ReceiveBroadcast( SPeer *pWho, CMemoryStream &data, int nID )
{
#ifdef LOG
	cout << "RECV broadcast from " << pWho->addr.GetFastName() << " msg " << nID << endl;
#endif
	// add to pending list
	pWho->messages.push_back( SQMessage() );
	SQMessage &b = pWho->messages.back();
	b.msg = data;
	b.nID = nID;
	b.bDirect = false;
	// set pending acks to intersection between active hosts from pWho view and current host view
	for ( std::list<SPeerClient>::iterator i = pWho->clients.begin(); i != pWho->clients.end(); ++i )
	{
		SPeer *pTest = GetClient( i->addr );
		if ( pTest && pTest->IsActive() )
		{
			// add ack request if such client exist
			b.acks.push_back( SAck() );
			SAck &ack = b.acks.back();
			ack.addr = i->addr;
			ack.bAcked = false;
			// seek through fast acks if required ack exists
			for ( std::list<SFastAck>::iterator k = pTest->fastacks.begin(); k != pTest->fastacks.end(); )
			{
				if ( k->addr == pWho->addr && k->nID == nID )
				{
					ack.bAcked = true;
					k = pTest->fastacks.erase( k );
				}
				else
					++k;
			}
			SendAck( *pTest, nID, pWho->id );
		}
	}
	CheckQueuedMessages( pWho ); // can be no acks required
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ReceiveAck( SPeer *pFrom, int nID, PEER_ID id )
{
	const UCID &addr = pFrom->GetAddr( id );
#ifdef LOG
	cout << "RECV ack from " << pFrom->addr.GetFastName() << " msg " << nID << " from " << addr.GetFastName() << endl;
#endif
	// find message in question and remove pending ack from xx
	SPeer *pSender = GetClient( addr );
	if ( pSender )
	{
		bool bFound = false;
		for ( std::list<SQMessage>::iterator i = pSender->messages.begin(); i != pSender->messages.end(); ++i )
		{
			if ( i->nID == nID )
			{
				SQMessage &b = *i;
				bFound = true;
				for ( std::list<SAck>::iterator k = b.acks.begin(); k != b.acks.end(); ++k )
				{
					if ( k->addr == pFrom->addr )
					{
						ASSERT( !k->bAcked );
						k->bAcked = true;
						break;
					}
				}
				break;
			}
		}
		if ( bFound )
			CheckQueuedMessages( pSender );
		else 
		{
			// save ack for the future
			// if sender is inactive no messages from him will be received and ack is useless
			if ( pSender->IsActive() ) 
			{
				pFrom->fastacks.push_back( SFastAck() );
				SFastAck &b = pFrom->fastacks.back();
				b.addr = addr;
				b.nID = nID;
			}
		}
	}
	else
		ASSERT( !pSender ); // sender was erased too early
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ReceiveAddClient( SPeer *pWho, const UCID &who, PEER_ID id, const CMemoryStream &_addrInfo )
{
#ifdef LOG
	cout << "RECV add client " << who.GetFastName() << " from " << pWho->addr.GetFastName() << endl;
#endif
		//if this is client being kicked then respond with kick addr and do nothing more
	SPeer *pTest = GetClient( who );
	if ( pTest )
	{
		if ( !pTest->IsActive() )
		{
			// pWho бредит - этого парня мы как раз кикаем
			SendRemoveClient( *pWho, who );
			return;
		}
	}
	else
		AddNewClient( who, _addrInfo ); // это что-то новое, нужно добавить в свой список
	// now to remove buddy we need to receive kick messages from every client pWho talked about him
	AddKickApprove( who, pWho->addr );
	for ( std::list<SPeerClient>::iterator i = pWho->clients.begin(); i != pWho->clients.end(); ++i )
	{
		SPeer *pT = GetClient( i->addr );
		if ( pT && pT->IsActive() )
			AddKickApprove( who, i->addr );
		else
			ASSERT( pT );
	}
	// client is added to peer tracking record
	pWho->clients.push_back( SPeerClient( who, id ) );
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::ReceiveRemoveClient( SPeer *pWho, const UCID &corpse )
{
#ifdef LOG
	cout << "RECV remove client " << corpse.GetFastName() << " from " << pWho->addr.GetFastName() << endl;
#endif
	bool bHadCorpse = pWho->HasClient( corpse );
	//ASSERT( find( pWho->clients.begin(), pWho->clients.end(), corpse ) != pWho->clients.end() );
	pWho->RemoveClient( corpse );
	KickClient( corpse );

	// every ack for broadcast message from pWho awaiting ack from client corpse 
	// is approved via ack remove
	for ( std::list<SQMessage>::iterator i = pWho->messages.begin(); i != pWho->messages.end(); ++i )
	{
		SQMessage &b = *i;
		for ( std::list<SAck>::iterator k = b.acks.begin(); k != b.acks.end(); )
		{
			if ( k->addr == corpse )
				k = b.acks.erase( k );
			else
				++k;
		}
	}
	CheckQueuedMessages( pWho );
	SPeer *pCorpse = GetClient( corpse );
	if ( pCorpse )
	{
		ApproveKick( pCorpse, pWho->addr );
			ASSERT( !pCorpse->IsActive() );
		// every pending message from corpse awaiting ack from pWho is removed
		for ( std::list<SQMessage>::iterator i = pCorpse->messages.begin(); i != pCorpse->messages.end(); )
		{
			SQMessage &b = *i;
			bool bRemove = false;
			for ( std::list<SAck>::iterator k = b.acks.begin(); k != b.acks.end(); ++k )
			{
				if ( k->addr == pWho->addr && k->bAcked == false )
					bRemove = true;
			}
			if ( bRemove )
			{
#ifdef LOG
				cout << " remove penging message " << i->nID << " from " << pCorpse->addr.GetFastName() << endl;
#endif
				i = pCorpse->messages.erase( i );
			}
			else
				++i;
		}
		CheckQueuedMessages( pCorpse );
	}
	else
		ASSERT( !bHadCorpse ); // was removed too early
}
/////////////////////////////////////////////////////////////////////////////////////
static int nBroadcastID = 1;
void CP2PTracker::SendBroadcast( CMemoryStream &pkt )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		if ( i->IsActive() )
			SendBroadcast( *i, nBroadcastID, pkt );
	}
	++nBroadcastID;
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendDirect( const UCID &addr, CMemoryStream &pkt )
{
	SPeer *pTest = GetClient( addr );
	ASSERT( pTest );
	if ( !pTest )
		return;
	SendDirect( *pTest, pkt );
}
/////////////////////////////////////////////////////////////////////////////////////
CP2PTracker::PEER_ID CP2PTracker::GetUnusedID() const
{
	PEER_ID res = 0;
	for (;;)
	{
		PEER_ID prev = res;
		for ( std::list<SPeer>::const_iterator i = clients.begin(); i != clients.end(); ++i )
		{
			if ( i->id == res )
				res++;
		}
		if ( prev == res )
			break;
	}
	return res;
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::AddNewClient( const UCID &addr, const CMemoryStream &_addrInfo )
{
#ifdef LOG
	cout << "ADD attempt, client " << addr.GetFastName() << endl;
#endif
	SPeer *pTest = GetClient( addr );
	ASSERT( !pTest );
	if ( pTest )
		return;

	PEER_ID newID = GetUnusedID();
#ifdef LOG
	cout << "ADD, client " << addr.GetFastName() << " ID=" << newID << endl;
#endif
	AddOutputMessage( NEW_CIENT, addr, &_addrInfo );
	// new client addr is added and for every existing channel info about that is sent
	for ( std::list<SPeer>::iterator i1 = clients.begin(); i1 != clients.end(); ++i1 )
	{
		if ( i1->IsActive() )
			SendAddClient( *i1, addr, newID, _addrInfo );
	}
	// to client addr info sent about every active SPeer on the moment
	SPeer res;
	res.addr = addr;	
	res.bActive = true;
	res.addrInfo = _addrInfo;
	for ( std::list<SPeer>::iterator i2 = clients.begin(); i2 != clients.end(); ++i2 )
	{
		if ( i2->IsActive() )
		{
			SendAddClient( res, i2->addr, i2->id, i2->addrInfo );
			res.requireKick.push_back( i2->addr );
			AddKickApprove( i2->addr, addr );
		}
	}
	res.id = newID;
	clients.push_back( res );
}
/////////////////////////////////////////////////////////////////////////////////////
// mark client as corpse and inform everybody about it
// no messages from this address will be received anymore
void CP2PTracker::KickClient( const UCID &addr )
{
	SPeer *pVictim = GetClient( addr );
	//cout << "KICK attempt, client " << addr.GetFastName() << endl;
	if ( pVictim && pVictim->IsActive() )
	{
#ifdef LOG
		cout << "KICK, client " << addr.GetFastName() << endl;
#endif
		pVictim->bActive = false;
		for ( std::list<SPeer>::iterator i1 = clients.begin(); i1 != clients.end(); ++i1 )
		{
			ApproveKick( &(*i1), addr ); // мертвые не кусаются и вряд ли пришлют подтверждение о kick
			if ( i1->IsActive() )
				SendRemoveClient( *i1, pVictim->addr );
		}
		// every fast ack received about messages from victim should be deleted
		for ( std::list<SPeer>::iterator i2 = clients.begin(); i2 != clients.end(); ++i2 )
		{
			for ( std::list<SFastAck>::iterator k = i2->fastacks.begin(); k != i2->fastacks.end(); )
			{
				if ( k->addr == addr )
					k = i2->fastacks.erase( k );
				else
					++k;
			}
		}
	}
	CheckCorpses();
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendRemoveClient( const SPeer &dest, const UCID &whom )
{
#ifdef LOG
	cout << "SEND delclient to " << dest.addr.GetFastName() << " client " << whom.GetFastName() << endl;
#endif
	CMemoryStream pkt;
	pkt << (char)PKT_REMOVE_CLIENT;
	pkt << whom;
	packets.push_back( SPacket( dest.addr, pkt ) );
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendAddClient( const SPeer &dest, const UCID &whom, PEER_ID id, const CMemoryStream &addrInfo )
{
#ifdef LOG
	cout << "SEND addclient to " << dest.addr.GetFastName() << " client " << whom.GetFastName() << endl;
#endif
	CMemoryStream pkt;
	pkt << (char)PKT_ADD_CLIENT;
	pkt << whom;
	pkt << id;
	int nSize = addrInfo.GetSize();
	pkt << nSize;
	pkt.Write( addrInfo.GetBuffer(), nSize );
	packets.push_back( SPacket( dest.addr, pkt ) );
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendBroadcast( const SPeer &dest, int nID, CMemoryStream &data )
{
#ifdef LOG
	cout << "SEND broadcast to " << dest.addr.GetFastName() << " msg " << nID << endl;
#endif
	CMemoryStream pkt;
	pkt << (char)PKT_BROADCAST_MSG;
	pkt << nID;
	pkt.WriteFrom( data );
	packets.push_back( SPacket( dest.addr, pkt ) );
	
#ifdef __LOG__
	data.Seek( 0 );
	
	BYTE cMsgID;
	data >> cMsgID;
	if ( cMsgID != 9 )
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "p2p: send broadcast, msg %d", (int)cMsgID ), 0xffffff00, true );
#endif
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendDirect( const SPeer &dest, CMemoryStream &data )
{
#ifdef LOG
	cout << "SEND direct to " << dest.addr.GetFastName() << endl;
#endif

	CMemoryStream pkt;
	pkt << (char)PKT_DIRECT_MSG;
	pkt.WriteFrom( data );
	packets.push_back( SPacket( dest.addr, pkt ) );

#ifdef __LOG__
/*
	data.Seek( 0 );
	
	BYTE cMsgID;
	data >> cMsgID;
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "p2p: send direct to %s, msg %d", dest.addr.GetFastName().c_str(), (int)cMsgID ), 0xffffff00, true );
*/
#endif
}
/////////////////////////////////////////////////////////////////////////////////////
void CP2PTracker::SendAck( const SPeer &dest, int nID, PEER_ID from )
{
#ifdef LOG
	SPeer *pShow = GetClient( from );
	ASSERT( pShow );
	cout << "SEND ack to " << dest.addr.GetFastName() << " about msg " << nID << " from " << pShow->addr.GetFastName() << endl;
#endif
	CMemoryStream pkt;
	pkt << (char)PKT_ACK;
	pkt << nID;
	pkt << from;
	packets.push_back( SPacket( dest.addr, pkt ) );
}
/////////////////////////////////////////////////////////////////////////////////////
static void ReadRest( CMemoryStream &src, CMemoryStream *pDst )
{
	src.ReadTo( *pDst, src.GetSize() - src.GetPosition() );
}
void CP2PTracker::ProcessPacket( const UCID &addr, CMemoryStream &pkt )
{
	SPeer *pWho = GetClient( addr );
	if ( !pWho || !pWho->IsActive() )
		return;
	pkt.Seek(0);
	EPacket t = (EPacket)0;
	pkt.Read( &t, 1 );
	switch ( t )
	{
		case PKT_ADD_CLIENT:
			{
				UCID clientAddr;
				PEER_ID id;
				CMemoryStream addrInfo;
				pkt >> clientAddr;
				pkt >> id;
				int nSize;
				pkt >> nSize;
				if ( nSize < 1024 )
				{
					addrInfo.SetSize( nSize );
					pkt.Read( addrInfo.GetBufferForWrite(), nSize );
					addrInfo.Seek(0);
				}
				ReceiveAddClient( pWho, clientAddr, id, addrInfo );
			}
			break;
		case PKT_REMOVE_CLIENT:
			{
				UCID clientAddr;
				pkt >> clientAddr;
				ReceiveRemoveClient( pWho, clientAddr );
			}
			break;
		case PKT_BROADCAST_MSG:
			{
				CMemoryStream data;
				int nID;
				pkt >> nID;
				ReadRest( pkt, &data );
				ReceiveBroadcast( pWho, data, nID );
			}
			break;
		case PKT_DIRECT_MSG:
			{
				CMemoryStream data;
				ReadRest( pkt, &data );
				ReceiveDirect( pWho, data );
			}
			break;
		case PKT_ACK:
			{
				int nID;
				PEER_ID addr;
				pkt >> nID;
				pkt >> addr;
				ReceiveAck( pWho, nID, addr );
			}
			break;
		default:
			ASSERT( 0 );
			break;
	}
}
/////////////////////////////////////////////////////////////////////////////////////
}