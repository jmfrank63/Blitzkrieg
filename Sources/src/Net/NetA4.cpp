#include "StdAfx.h"

#include "Streams.h"
#include "NetLowest.h"
#include "NetAcks.h"
#include "NetStream.h"
#include "NetServerInfo.h"
#include "NetLogin.h"
#include "NetPeer2Peer.h"
#include "..\Misc\HPTimer.h"
#include "NetA4.h"
#include "..\StreamIO\StreamIOHelper.h"
#include "..\StreamIO\StreamIOTypes.h"
#include "NetDriverConsts.h"

// for testing
#include "..\Main\GameTimer.h"
#include "..\StreamIO\Globals.h"

#ifdef _USE_GAMESPY
extern "C"
{
#include "peerA4Ext.h"
}
#endif
//#define LOG
#ifdef LOG
#include <iostream>
#endif

namespace NNet
{
using namespace NWin32Helper;
	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// interaction with master server is accomplished with different object
// if so then it should be possible to start
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPacket
{
	NORMAL,
	LOGIN,
	REQUEST_SERVER_INFO,
	SERVER_INFO,
	ACCEPTED,
	REJECTED,
	LOGOUT,
	TRY_SHORTCUT,
	NOP,
	GAMESPY = '\\'
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_APPLICATIONID = 0x45143100;
class CSendPacket
{
	const CLinksManager &links;
	const CNodeAddress &addr;
	static CMemoryStream pkt;
	static CBitLocker bits;
	static bool bLastPacket;
public:
	CSendPacket( const CNodeAddress &_addr, EPacket packet, const CLinksManager &_links ): addr(_addr), links(_links)
	{
		pkt.Seek(0);
		bits.LockWrite( pkt, N_MAX_PACKET_SIZE + 1024 );
		bits.Write( &packet, 1 );
	}
	CSendPacket( const CNodeAddress &_addr, EPacket packet, CP2PTracker::UCID clientID, const CLinksManager &_links ): addr(_addr), links(_links)
	{
		pkt.Seek(0);
		bits.LockWrite( pkt, N_MAX_PACKET_SIZE + 1024 );
		bits.Write( &packet, 1 );
		bits.Write( &clientID, sizeof(clientID) );
	}
	~CSendPacket()
	{
		bits.Free();
		pkt.SetSize( pkt.GetPosition() );
		bLastPacket = links.Send( addr, pkt );
	}
	CBitStream* GetBits() { return &bits; }
	static bool GetResult() { return bLastPacket; }
};
CMemoryStream CSendPacket::pkt;
CBitLocker CSendPacket::bits;
bool CSendPacket::bLastPacket;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// packet to/from stream
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanReadPacket( CRingBuffer<N_STREAM_BUFFER> &buf )
{
	if ( buf.GetSize() < 4 )
		return false;
	int nSize;
	buf.Peek( &nSize, 4 );
	if ( nSize & 1 )
		nSize &= 0xff;
	nSize >>= 1;
//	if ( nSize > 7100 ) cout << "SIZE PIZDOS " << nSize << endl;
	if ( buf.GetSize() >= nSize + (nSize >= 128 ? 4 : 1) )
		return true;
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void WritePacket( std::list<CMemoryStream> *pDst, CMemoryStream &pkt )
{
	NI_ASSERT_T( pkt.GetSize() < N_STREAM_BUFFER - 1000, NStr::Format( "Wrong memory stream size (%d)", pkt.GetSize() ) );
	pDst->push_back( CMemoryStream() );
	CMemoryStream &b = pDst->back();
	int nSize = pkt.GetSize();
	if ( nSize >= 128 )
	{
		nSize <<= 1;
		b.Write( &nSize, 4 );
	}
	else
	{
		nSize <<= 1;
		nSize |= 1;
		b.Write( &nSize, 1 );
	}
	b.Write( pkt.GetBuffer(), pkt.GetSize() );
}
static void ReadPacket( CRingBuffer<N_STREAM_BUFFER> &src, CMemoryStream *pDst )
{
	NI_ASSERT_T( CanReadPacket( src ), "Can't read a packet" );
	int nSize = 0;
	src.Read( &nSize, 1 );
	if ( (nSize & 1) == 0 )
		src.Read( ((char*)&nSize) + 1, 3 );
	nSize >>= 1;
//	if ( nSize > 7100 )	cout << "SIZE PIZDOS " << nSize << endl;
	pDst->SetSizeDiscard( nSize );
	src.Read( pDst->GetBufferForWrite(), nSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** net driver
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI TheThreadProc( LPVOID lpParameter )
{
	// run finction
	CNetDriver *pNet = reinterpret_cast<CNetDriver*>(lpParameter);
	pNet->StartThread();

	while ( pNet->CanWork()  )
	{
		Sleep(100);
		pNet->Step();
	}

	pNet->FinishThread();

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StartThread()
{
	ResetEvent( hFinishReport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::CanWork()
{
	return WaitForSingleObject( hStopCommand, 0 ) != WAIT_OBJECT_0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::FinishThread()
{
	SetEvent( hFinishReport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CNetDriver::CNetDriver() 
: serverInfo( 0 ), login( 0 ), bMultiChannel( false ), state( INACTIVE )
{
	hThread = 0;
	hFinishReport = CreateEvent( 0, true, false, 0 );
	hStopCommand = CreateEvent( 0, true, false, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CNetDriver::~CNetDriver()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	switch ( state )
	{
		case ACTIVE:
			{
				for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
				{
					CSendPacket p( i->currentAddr, LOGOUT, i->clientID, links );
				}
				break;
			}
		case CONNECTING:
			{
				CSendPacket p( login.GetLoginTarget(), LOGOUT, -1, links );
			}
			break;
	}

	if ( hThread )
	{
		// ���� ���� �� �����������
		if ( WaitForSingleObject( hFinishReport,0 ) != WAIT_OBJECT_0 )
		{
			SetEvent( hStopCommand );
			criticalSectionLock.Leave();
			WaitForSingleObject( hFinishReport, INFINITE );
			criticalSectionLock.Enter();
		}

		CloseHandle( hThread );
		hThread = 0;
	}

	ResetEvent( hStopCommand );
	ResetEvent( hFinishReport );
	CloseHandle( hFinishReport );
	CloseHandle( hStopCommand );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly )
{
	bMultiChannel = false;
	
	SNetDriverConsts::Load();	
	serverInfo.Init( _nApplicationID );
	login.Init( _nApplicationID );
	nGamePort = _nGamePort;
	//
	state = INACTIVE;
	lastReject = NONE;
	bAcceptNewClients = true;
	NHPTimer::GetTime( &lastTime );
	//
	const bool bReturn = _bClientOnly ? links.Start( 0 ) : links.Start( nGamePort );

#ifdef __TEST_LAGS__
	lastSendTime = 0;
	lastReceiveTime = 0;
	nMsgCanReceive = 0;
	lagPeriod = 0;
	bPaused = false;
	bSendNow = false;
	bReceiveNow = false;
#endif // __TEST_LAGS__

	DWORD dwThreadId;
	ResetEvent( hStopCommand );
	ResetEvent( hFinishReport );
	hThread = CreateThread( 0, 1024*1024, TheThreadProc, reinterpret_cast<LPVOID>(this), 0, &dwThreadId );
	
	return bReturn;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CNetDriver::SPeer* CNetDriver::GetClientByAddr( const CNodeAddress &addr )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		if ( i->currentAddr == addr )
			return &(*i);
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////
CNetDriver::SPeer* CNetDriver::GetClient( CP2PTracker::UCID nID )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		if ( i->clientID == nID )
			return &(*i);
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::AddClient( const SClientAddressInfo &addr, CP2PTracker::UCID clientID )
{
	clients.push_back( SPeer() );
	SPeer &peer = clients.back();
	peer.currentAddr = addr.inetAddress;
	peer.clientID = clientID;
	peer.addrInfo = addr;
	CNodeAddress test;
	addr.localAddress.GetAddress( 0, &test );
	peer.bTryShortcut = !addr.inetAddress.SameIP( test );
	peer.bTryShortcut |= !addr.localAddress.GetAddress( 1, &test );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::AddNewP2PClient( const SClientAddressInfo &addr, CP2PTracker::UCID clientID )
{
	CMemoryStream addrInfo;
	addrInfo.Write( &addr, sizeof( addr ) );
	p2p.AddNewClient( clientID, addrInfo );
}
/////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::RemoveClient( CP2PTracker::UCID nID )
{
	for ( std::list<SPeer>::iterator i = clients.begin(); i != clients.end(); )
	{
		if ( i->clientID == nID )
			i = clients.erase( i );
		else
			++i;
	}
}
/////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::SendBroadcast( IDataStream *pPkt )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

#ifdef __TEST_LAGS__
	if ( !bSendNow )
	{
		IDataStream *pCopy = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		pPkt->Seek( 0, STREAM_SEEK_SET );
		pPkt->CopyTo( pCopy, pPkt->GetSize() );
		
		msgToSendBroadcast.push_back( pCopy );
		return true;
	}
#endif // __TEST_LAGS__
	
	if ( state != ACTIVE )
		return false;
	//

	// CRAP{ for traffic measurement
	nSent += pPkt->GetSize();
	// CRAP}

	static CMemoryStream pkt;
	pkt.SetSize( pPkt->GetSize() );
	pPkt->Seek( 0, STREAM_SEEK_SET );
	pPkt->Read( pkt.GetBufferForWrite(), pkt.GetSize() );

	//
	p2p.SendBroadcast( pkt );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::SendDirect( int nClient, IDataStream *pPkt )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

#ifdef __TEST_LAGS__
	if ( !bSendNow )
	{
		IDataStream *pCopy = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		pPkt->Seek( 0, STREAM_SEEK_SET );
		pPkt->CopyTo( pCopy, pPkt->GetSize() );

		msgToSendDirect.push_back( std::pair< int, CPtr<IDataStream> >( nClient, pCopy ) );
		return true;
	}
#endif // __TEST_LAGS__

	// CRAP{ for traffic measurement
	nSent += pPkt->GetSize();
	// CRAP}

	SPeer *pDst = GetClient( nClient );
	if ( pDst == 0 || state != ACTIVE )
		return false;

	if ( pDst )
	{
		static CMemoryStream pkt;
		pkt.SetSize( pPkt->GetSize() );
		pPkt->Seek( 0, STREAM_SEEK_SET );
		pPkt->Read( pkt.GetBufferForWrite(), pkt.GetSize() );
		//
		p2p.SendDirect( pDst->clientID, pkt );

/*		
		// for debug
		pPkt->Seek( 0, STREAM_SEEK_SET );
		BYTE cMsgID;
		CStreamAccessor stream = pPkt;
		stream >> cMsgID;
*/
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::Kick( int nClient )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	SPeer *pDst = GetClient( nClient );
//	NI_ASSERT_T( pDst != 0, "pDst == 0 " );
	NI_ASSERT_T( state == ACTIVE, "Wrong state of the game" );
	if ( pDst )
		p2p.KickClient( nClient );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __TEST_LAGS__
bool CNetDriver::AnalyzeLags()
{
	if ( bPaused )
		return false;

	if ( bReceiveNow )
		return true;
	
	if ( GetSingleton<IGameTimer>() )
	{
		const NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
		// all messages are received and it's lag now
		if ( nMsgCanReceive == 0 && lastReceiveTime + lagPeriod > curTime )
			return false;
		// lag finished
		if ( lastReceiveTime + lagPeriod <= curTime )
		{
			lastReceiveTime = curTime;
			if ( bMultiChannel )
			{
				for ( int i = 0; i < channelMsgs.size(); ++i )
					nMsgCanReceive += channelMsgs[i].size();
			}
			else
				nMsgCanReceive += msgQueue.size();
		}

		if ( nMsgCanReceive > 0 )
			--nMsgCanReceive;
	}

	return true;
}
#endif // __TEST_LAGS__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt )
{
	if ( bMultiChannel )
		return GetChannelMessage( pMsg, pClientID, received, pPkt, 0 );
	else
	{
		CCriticalSectionLock criticalSectionLock( criticalSection );

#ifdef __TEST_LAGS__
	if ( !AnalyzeLags() )
		return false;
#endif // __TEST_LAGS__
		
		if ( msgQueue.empty() )
			return false;
		else
		{
			SMessage &msg = msgQueue.front();
			*pMsg = msg.msg;
			*pClientID = msg.nClientID;
			if ( !msg.received.empty() )
				memcpy( received, &(msg.received[0]), msg.received.size()*sizeof(msg.received[0]) );
			received[msg.received.size()] = -1;
			pPkt->SetSize( 0 );
			pPkt->Write( msg.pkt.GetBuffer(), msg.pkt.GetSize() );
			pPkt->Seek( 0, STREAM_SEEK_SET );
			//
			msgQueue.pop_front();
		}

		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::ProcessLogin( const CNodeAddress &addr, CBitStream &bits )
{
	// if can accept login requests only
	CLoginSupport::SLoginInfo info;
	if ( login.ProcessLogin( addr, bits, &info ) )
	{
		bool bAddp2pClient = false;		
		EReject reject = NONE;
		if ( info.bWrongVersion )
			reject = WRONG_VERSION;
		else
		{
			SPeer *pPeer = GetClientByAddr( addr );
			if ( pPeer )
			{
				if ( !login.HasAccepted( addr, info ) )
					return; // ignore obsolete or too new request
			}
			else if ( info.pwd.GetSize() ) // wrong password
				reject = PASSWORD_FAILED;
			else
			{
				if ( !bAcceptNewClients )
					reject = FORBIDDEN;
				else
					bAddp2pClient = true;
			}
		}
		if ( reject != NONE )
		{
			CSendPacket pkt( addr, REJECTED, links );
			login.RejectLogin( pkt.GetBits(), info, (int)reject );
		}
		else
		{
			int nClientID;
			CSendPacket pkt( addr, ACCEPTED, links );
			CNodeAddressSet localAddr;
			bool bGetSelf = links.GetSelfAddress( &localAddr );
			ASSERT( bGetSelf );
			login.AcceptLogin( addr, pkt.GetBits(), info, &nClientID, localAddr );
			if ( bAddp2pClient )
				AddNewP2PClient( SClientAddressInfo( addr, info.localAddr ), nClientID );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::ProcessNormal( const CNodeAddress &addr, CBitStream &bits )
{
	CP2PTracker::UCID clientID = -1;
	bits.Read( &clientID, sizeof(clientID) );
	if ( !p2p.IsActive( clientID ) )
		return;
	SPeer *pPeer = GetClient( clientID );
	if ( pPeer )
	{
		pPeer->currentAddr = addr;
		pPeer->bTryShortcut = false;
		std::vector<PACKET_ID> acked;
		NI_ASSERT_T( pPeer->data.CanReadMsg(), "data polling is not perfect" ); // data polling is not perfect
		if ( pPeer->data.CanReadMsg() && pPeer->acks.ReadAcks( &acked, bits ) )
		{
			pPeer->data.ReadMsg( bits );
			pPeer->data.Commit( acked );
			PollMessages( pPeer );			
		}
	}
	else
	{
		//if (pCSLog) (*pCSLog) << "normal packet from non client received from " << addr.GetFastName() << endl;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::ProcessIncomingMessages()
{
	// process incoming packets
//	CNodeAddress addr;
	static CMemoryStream pkt;
	while ( links.Recv( &addr, &pkt ) )
	{
		if ( pkt.GetSize() == 0 )
		{
			//if (pCSLog) (*pCSLog) << "ZERO length packet received from " << addr.GetFastName() << endl;
			continue;
		}
		EPacket cmd = (EPacket)0;
		pkt.Read( &cmd, 1 );
		CBitStream bits( pkt.GetBufferForWrite() + 1, CBitStream::read, pkt.GetSize() - 1 );
		switch ( cmd )
		{
			case NORMAL:
				ProcessNormal( addr, bits );
				break;
			case LOGIN:
				ProcessLogin( addr, bits );
				break;
			case REQUEST_SERVER_INFO:
				if ( serverInfo.DoReplyRequest() )
				{
					CSendPacket pkt( addr, SERVER_INFO, links );
					serverInfo.ReplyServerInfoRequest( bits, pkt.GetBits() );
				}
				break;
			case SERVER_INFO:
				serverInfo.ProcessServerInfo( addr, bits );
				break;
			case ACCEPTED:
				login.ProcessAccepted( addr, bits );
				break;
			case REJECTED:
				login.ProcessRejected( addr, bits );
				break;
			case LOGOUT:
				{
					CP2PTracker::UCID clientID = -1;
					bits.Read( &clientID, sizeof( clientID ) );
					p2p.KickClient( clientID );
					SPeer *pPeer = GetClientByAddr( addr );
					if ( pPeer )
						p2p.KickClient( pPeer->clientID );
				}

				break;
			case TRY_SHORTCUT:
				{
					CP2PTracker::UCID clientID = -1;
					bits.Read( &clientID, sizeof(clientID) );
					CLoginSupport::TServerID uniqueServerID;
					bits.Read( uniqueServerID );
					if ( uniqueServerID == login.GetUniqueServerID() )
					{
						if ( !p2p.IsActive( clientID ) )
							return;
						SPeer *pPeer = GetClient( clientID );
						if ( pPeer )
						{
							pPeer->currentAddr = addr;
							pPeer->bTryShortcut = false;
						}
					}
				}
				break;
			case NOP:
				break;
#ifdef _USE_GAMESPY
			case GAMESPY:
			{
				g_szGameSpyPeerMessage[0] = GAMESPY;
				pkt.Read( &g_szGameSpyPeerMessage[1], pkt.GetSize() - 1 );
				g_szGameSpyPeerMessage[pkt.GetSize()] = 0;
				break;
			}
#endif
			default:
				NI_ASSERT_T( 0 && "Unknown command", "Unknown command" );
				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StepInactive()
{
	std::vector<CNodeAddress> dest;
	CNodeAddress broadcast;
	links.MakeBroadcastAddr( &broadcast, nGamePort );
	if ( serverInfo.CanSendRequest( broadcast, &dest ) )
	{
		for ( int i = 0; i < dest.size(); ++i )
		{
			CSendPacket pkt( dest[i], REQUEST_SERVER_INFO, links );
			serverInfo.WriteRequest( pkt.GetBits() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StepConnecting()
{
	switch ( login.GetState() )
	{
		case CLoginSupport::INACTIVE:
			lastReject = TIMEOUT;
			state = INACTIVE;
			break;
		case CLoginSupport::LOGIN:
			if ( login.CanSend() )
			{
				CNodeAddressSet localAddr;
				if ( !links.GetSelfAddress( &localAddr ) )
				{
					CSendPacket pkt( login.GetLoginTarget(), NOP, links );
				}
				bool bGetSelf = links.GetSelfAddress( &localAddr );
				ASSERT( bGetSelf );
				CSendPacket pkt( login.GetLoginTarget(), LOGIN, links );
				login.WriteLogin( pkt.GetBits(), localAddr );
			}
			break;
		case CLoginSupport::ACCEPTED:
			AddNewP2PClient( SClientAddressInfo( login.GetLoginTarget(), login.GetTargetLocalAddr() ), 0 );
			state = ACTIVE;
			break;
		case CLoginSupport::REJECTED:
			lastReject = (EReject)login.GetRejectReason();
			state = INACTIVE;
			break;
		default:
			NI_ASSERT_T( 0, "Unknown message" );
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::AddOutputMessage( EMessage msg, const CP2PTracker::UCID &_from, 
		CMemoryStream &data, const std::vector<CP2PTracker::UCID> &received )
{
	SPeer *pPeer = GetClient( _from );
	NI_ASSERT_T( pPeer != 0, "NULL peer" );
	if ( !pPeer )
		return;
	msgQueue.push_back( SMessage() );
	SMessage &res = msgQueue.back();
	res.msg = msg;
	res.pkt = data;
	res.nClientID = pPeer->clientID;
	for ( int i = 0; i < received.size(); ++i )
	{
		SPeer *pTest = GetClient( received[i] );
		NI_ASSERT_T( pTest != 0, "NULL pTest" );
		if ( pTest )
			res.received.push_back( pTest->clientID );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::PollMessages( SPeer *pPeer )
{
	// seek for packets through incoming traffic
	while ( CanReadPacket( pPeer->data.channelInBuf ) )
	{
		CMemoryStream pkt;
		ReadPacket( pPeer->data.channelInBuf, &pkt );
#ifdef LOG
		cout << "receive packet from " << pPeer->addr.GetFastName() << " size=" << pkt.GetSize() << endl;
#endif
		p2p.ProcessPacket( pPeer->clientID, pkt );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StepActive( float fDeltaTime )
{
	// rollback outdated packets
	for ( CPeerList::iterator i = clients.begin(); i != clients.end(); ++i )
	{
		std::vector<PACKET_ID> rolled, erased;
		i->acks.Step( &rolled, &erased, fDeltaTime );
		i->data.Rollback( rolled );
		i->data.Erase( erased );
		PollMessages( &(*i) );
	}
	// process peer2peer messages
	CP2PTracker::SMessage msg;
	while ( p2p.GetMessage( &msg ) )
	{
		switch ( msg.msg )
		{
			case CP2PTracker::NEW_CIENT:
				{
					SClientAddressInfo addr;
					msg.pkt.Seek(0);
					msg.pkt.Read( &addr, sizeof(addr) );
					AddClient( addr, msg.from );
					AddOutputMessage( NEW_CLIENT, msg.from, msg.pkt, msg.received );
				}

				break;
			case CP2PTracker::REMOVE_CLIENT:
				AddOutputMessage( REMOVE_CLIENT, msg.from, msg.pkt, msg.received );
				
				if ( msg.from == 0 )
					AddOutputMessage( SERVER_DEAD, msg.from, msg.pkt, msg.received );

				RemoveClient( msg.from );

				break;
			case CP2PTracker::DIRECT:
				AddOutputMessage( DIRECT, msg.from, msg.pkt, msg.received );
				break;
			case CP2PTracker::BROADCAST:
				AddOutputMessage( BROADCAST, msg.from, msg.pkt, msg.received );
				break;
		}
	}
	// form output packets
	for ( int pi = 0; pi < p2p.packets.size(); ++pi )
	{
		CP2PTracker::SPacket &p = p2p.packets[pi];
		SPeer *pPeer = GetClient( p.addr );
		if ( pPeer )
		{
			WritePacket( &pPeer->data.outList, p.pkt );
#ifdef LOG
			cout << "output packet to " << p.addr.GetFastName() << " size=" << p.pkt.GetSize() << endl;
#endif
		}
		else
		{
#ifdef LOG
			cout << "DISCARD packet to " << p.addr.GetFastName() << " size=" << p.pkt.GetSize() << endl;
#endif
		}
	}
	p2p.packets.resize( 0 );
	// send updates & check for timeouts
	for ( CPeerList::iterator it = clients.begin(); it != clients.end(); ++it )
	{
		if ( it->acks.GetTimeSinceLastRecv() > SNetDriverConsts::F_TIMEOUT )
			p2p.KickClient( it->clientID );
		if ( p2p.IsActive( it->clientID ) )
		{
/*
			if ( !it->data.HasOutData() )
			{
				// ����� ���������? ������
				CMemoryStream shit;
				static int nShit = 0;
				shit.SetSize( 7000 );//15000 );
				sprintf( (char*)shit.GetBufferForWrite(), "info %d", nShit++ );
				p2p.SendDirect( it->addr, shit );
#ifdef LOG
				cout << "add direct msg for " << it->addr.GetFastName() << " msg " << nShit << endl;
#endif
			}*/
			while ( ( it->acks.CanSend() && it->data.HasOutData() ) || it->acks.NeedSend() )
			{
				if ( it->bTryShortcut )
				{
					for ( int k = 0; 1; ++k )
					{
						CNodeAddress dest;
						if ( !it->addrInfo.localAddress.GetAddress( k, &dest ) )
							break;
						CSendPacket pkt( dest, TRY_SHORTCUT, login.GetSelfClientID(), links );
						pkt.GetBits()->Write( login.GetUniqueServerID() );
					}
				}

				PACKET_ID pktID;
				{
					CSendPacket pkt( it->currentAddr, NORMAL, login.GetSelfClientID(), links );
					pktID = it->acks.WrtieAcks( pkt.GetBits(), 220 ); // CRAP - packet size limits??
					it->data.WriteMsg( pktID, pkt.GetBits(), 220 );
				}
				if ( !CSendPacket::GetResult() )
				{
					it->acks.PacketLost( pktID );
					std::vector<PACKET_ID> roll;
					roll.push_back( pktID );
					it->data.Rollback( roll );
					break;
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRAP{
int nTrafficPackets;
int nTrafficTotalSize;
// CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::Step()
{
	{
		CCriticalSectionLock criticalSectionLock( criticalSection );

		// CRAP{
		/*
		// for traffic to winsock measurement
		NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
		if ( lastTrafficCheckTime > curTime )
			lastTrafficCheckTime = curTime;

		if ( lastTrafficCheckTime + 1000 < curTime )
		{
			lastTrafficCheckTime = curTime;
			GetSingleton<IConsoleBuffer>()->WriteASCII
				( CONSOLE_STREAM_CHAT,
					NStr::Format( "Packets %d, total size %d, sent to driver %d", nTrafficPackets, nTrafficTotalSize, nSent ),
					0xffff0000, true
				);

			nTrafficPackets = 0;
			nTrafficTotalSize = 0;
			nSent = 0;
		}
		*/
		// CRAP}
		//

		float fSeconds = NHPTimer::GetTimePassed( &lastTime );
		serverInfo.Step( fSeconds );
		login.Step( fSeconds );
		ProcessIncomingMessages();
		//
		switch ( state )
		{
			case INACTIVE:
				StepInactive();
				break;
			case ACTIVE:
				StepActive( fSeconds );
				break;
			case CONNECTING:
				StepConnecting();
				break;
		}
	}

	if ( bMultiChannel )
		StepMultiChannel();

#ifdef __TEST_LAGS__
	CCriticalSectionLock criticalSectionLock( criticalSection );

	if ( !bPaused )
	{
		if ( GetSingleton<IGameTimer>() )
		{
			const NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
			if ( lastSendTime + lagPeriod <= curTime )
			{
				lastSendTime = curTime;

				bSendNow = true;

				for ( std::list< CPtr<IDataStream> >::iterator iter = msgToSendBroadcast.begin(); iter != msgToSendBroadcast.end(); ++iter )
				{
					criticalSectionLock.Leave();
					SendBroadcast( *iter );
					criticalSectionLock.Enter();
				}
				msgToSendBroadcast.clear();

				for( std::list< std::pair< int, CPtr<IDataStream> > >::iterator iter = msgToSendDirect.begin(); iter != msgToSendDirect.end(); ++iter )
				{
					criticalSectionLock.Leave();
					SendDirect( iter->first, iter->second );
					criticalSectionLock.Enter();
				}
				msgToSendDirect.clear();

				bSendNow = false;
			}
		}
	}
#endif // __TEST_LAGS__
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StartGame()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	//	NI_ASSERT_T( state == INACTIVE, "Wrong state of the game" );
	state = ACTIVE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	state = CONNECTING;
	static CMemoryStream pwd;
	pwd.SetSize( pPwd->GetSize() );
	pPwd->Seek( 0, STREAM_SEEK_SET );
	pPwd->Read( pwd.GetBufferForWrite(), pwd.GetSize() );
	login.StartLogin( static_cast<const CNodeAddressWrap*>(pAddr)->GetCNodeAddress(), pwd );

	gameHostAddress.Clear();
	gameHostAddress.SetInetName( pAddr->GetName(), 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StartGameInfoSend( IDataStream *pData )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	static CMemoryStream data;
	data.SetSize( pData->GetSize() );
	pData->Seek( 0, STREAM_SEEK_SET );
	pData->Read( data.GetBufferForWrite(), data.GetSize() );
	//
	serverInfo.StartReply( data );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StartGameInfoSend( const SGameInfo &gameInfo )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	CPtr<IDataStream> pStream = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	SGameInfoWrapper gameInfoWrapper( gameInfo );
	gameInfoWrapper.Pack( pStream );

	StartGameInfoSend( pStream );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StopGameInfoSend()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	serverInfo.StopReply();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	const CServerInfoSupport::CServerInfoList &servers = serverInfo.GetServers();
	CServerInfoSupport::CServerInfoList::const_iterator k = servers.begin();
	
	for ( ; k != servers.end(); ++k, --nIdx )
	{
		if ( nIdx == 0 )
		{
			static_cast<CNodeAddressWrap*>(pAddr)->GetCNodeAddress() = k->addr;
			*pWrongVersion = k->bWrongVersion;
			*pPing = k->fPing;

			CStreamAccessor info = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
			info->SetSize( 0 );
			info->Write( k->info.GetBuffer(), k->info.GetSize() );
			info->Seek( 0, STREAM_SEEK_SET );

			SGameInfoWrapper gameInfoWrapper;
			gameInfoWrapper.Unpack( info );
			*pGameInfo = gameInfoWrapper;

			return true;
		}
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StartNewPlayerAccept() 
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	bAcceptNewClients = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StopNewPlayerAccept()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	bAcceptNewClients = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SOCKET CNetDriver::GetSocket()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	return links.GetSocket();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
sockaddr *CNetDriver::GetSockAddr()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	return addr.GetSockAddr();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INetDriver* MakeDriver( const APPLICATION_ID nApplicationID, int nGamePort, bool bClientOnly )
{
	CNetDriver *pRes = new CNetDriver();
	pRes->Init( nApplicationID, nGamePort, bClientOnly );
	return pRes;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// multichannel
//
void CNetDriver::ResizeArrays( const int nNewSize )
{
	channelMsgTypes.resize( nNewSize );
	channelMsgs.resize( nNewSize );
	existingChannels.resize( nNewSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages )
{
	{
		CCriticalSectionLock criticalSectionLock( criticalSection );
		
		if ( nChannelID >= existingChannels.size() )
			ResizeArrays( nChannelID + 1 );
		
		bMultiChannel = true;
		existingChannels[0] = true;

		existingChannels[nChannelID] = 1;
		channelMsgTypes[nChannelID].clear();
		channelMsgTypes[nChannelID].insert( channelMessages.begin(), channelMessages.end() );
	}

	StepMultiChannel();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::RemoveChannel( const int nChannelID )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	NI_ASSERT_T( nChannelID != 0, "Can't remove channel 0" );
	
	if ( nChannelID < existingChannels.size() && existingChannels[nChannelID] == 1 )
		existingChannels[nChannelID] = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::StepMultiChannel()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	//
	// ����� ��� ����� ��������� ���������� ���������
	bMultiChannel = false;
	// �� ���� ���������� ����������

	criticalSectionLock.Leave();
#ifdef __TEST_LAGS__
	bReceiveNow = true;
#endif // __TEST_LAGS__
	while ( GetMessage(&eMsgID, &nClientID, received, pkt) )
	{
		criticalSectionLock.Enter();
		// ������ ���
		BYTE msg;
		int i = 0;
		const int nCurStreamPosition = pkt->GetPos();
		if ( pkt->GetSize() >= sizeof( msg ) )
		{
			pkt >> msg;
			
			// try to find appropriate not-common channel
			i = existingChannels.size() - 1;
			while ( i > 0 && ( existingChannels[i] == 0 || channelMsgTypes[i].find( msg ) == channelMsgTypes[i].end() ) )
				--i;
		}
		pkt->Seek( nCurStreamPosition, STREAM_SEEK_SET );

		channelMsgs[i].push_back( SChannelMessage() );
		channelMsgs[i].back().msg = eMsgID;
		channelMsgs[i].back().nClientID = nClientID;
		channelMsgs[i].back().pPkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		pkt->CopyTo( channelMsgs[i].back().pPkt, pkt->GetSize() );
		channelMsgs[i].back().pPkt->Seek( 0, STREAM_SEEK_SET );

		pkt->SetSize( 0 );

		criticalSectionLock.Leave();
	}

#ifdef __TEST_LAGS__
	bReceiveNow = false;
#endif // __TEST_LAGS__

	bMultiChannel = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNetDriver::GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

#ifdef __TEST_LAGS__
	if ( !AnalyzeLags() )
		return false;
#endif // __TEST_LAGS__

	NI_ASSERT_T( nChannel < existingChannels.size() && existingChannels[nChannel] == 1, NStr::Format( "Channel %d doesn't exist", nChannel ) );

	if ( channelMsgs[nChannel].empty() )
		return false;
	else
	{
		*pMsg = channelMsgs[nChannel].front().msg;
		*pClientID = channelMsgs[nChannel].front().nClientID;

		pPkt->SetSize( 0 );		
		IDataStream *pMemStream = channelMsgs[nChannel].front().pPkt;
		// ����������� 'nLength' ���� �� ������� ������� ������ � ������ ������� 'pDstStream' ������
		pMemStream->CopyTo( pPkt, pMemStream->GetSize() );
		pPkt->Seek( 0, STREAM_SEEK_SET );

		channelMsgs[nChannel].pop_front();

		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CNetDriver::GetPing( const int nClientID )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	CPeerList::const_iterator iter = clients.begin();
	while ( iter != clients.end() && iter->clientID != nClientID )
		++iter;

	if ( iter == clients.end() )
		return -1.0f;
	else
		return iter->acks.GetPing();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CNetDriver::GetTimeSinceLastRecv( const int nClientID )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	
	CPeerList::const_iterator iter = clients.begin();
	while ( iter != clients.end() && iter->clientID != nClientID )
		++iter;

	if ( iter == clients.end() )
		return -1.0f;
	else
		return iter->acks.GetTimeSinceLastRecv();	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
void CNetDriver::SGameInfoWrapper::Pack( IDataStream *pDataStream )
{
	CStreamAccessor stream( pDataStream );
	stream << wszServerName << nHostPort << wszMapName << szGameType << nCurPlayers << nMaxPlayers << eGameMode << bPasswordRequired << szModName << szModVersion;

	pGameSettings->Seek( 0, STREAM_SEEK_SET );
	pGameSettings->CopyTo( pDataStream, pGameSettings->GetSize() );
	pGameSettings->Seek( 0, STREAM_SEEK_SET );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::SGameInfoWrapper::Unpack( IDataStream *pDataStream )
{
	CStreamAccessor stream( pDataStream );
	stream >> wszServerName >> nHostPort >> wszMapName >> szGameType >> nCurPlayers >> nMaxPlayers >> eGameMode >> bPasswordRequired >> szModName >> szModVersion;

	pGameSettings = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pDataStream->CopyTo( pGameSettings, pDataStream->GetSize() - pDataStream->GetPos() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for debug
const char* CNetDriver::GetAddressByClientID( const int nClientID ) const
{
	CPeerList::const_iterator iter = clients.begin();
	while ( iter != clients.end() && iter->clientID != nClientID )
		++iter;

	if ( iter == clients.end() )
		return NStr::Format( "Invalid client %d", nClientID );
	else
		return iter->currentAddr.GetFastName().c_str();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::PauseNet()
{
#ifdef __TEST_LAGS__
	bPaused = true;
#endif // __TEST_LAGS__
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::UnpauseNet()
{
#ifdef __TEST_LAGS__
	bPaused = false;
#endif // __TEST_LAGS__
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNetDriver::SetLag( const NTimer::STime period )
{
#ifdef __TEST_LAGS__
	lagPeriod = period;
#endif // __TEST_LAGS__
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
