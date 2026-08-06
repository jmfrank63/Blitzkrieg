#include "StdAfx.h"
#include "NetLowest.h"
#include "Streams.h"

using namespace std;

namespace
{
uint16_t AddressPort( const NPlatform::SocketAddress &address )
{
	return static_cast<uint16_t>( ( static_cast<uint16_t>( address.data[4] ) << 8 ) | address.data[5] );
}

void SetAddressPort( NPlatform::SocketAddress *address, uint16_t port )
{
	address->data[4] = static_cast<uint8_t>( port >> 8 );
	address->data[5] = static_cast<uint8_t>( port & 0xff );
}

void SetLoopbackIfUnspecified( NPlatform::SocketAddress *address )
{
	if ( address->data[0] == 0 && address->data[1] == 0 && address->data[2] == 0 && address->data[3] == 0 )
	{
		address->data[0] = 127;
		address->data[1] = 0;
		address->data[2] = 0;
		address->data[3] = 1;
	}
}
}

namespace NNet
{
bool CNodeAddress::SetInetName( const char *pszHost, int nDefaultPort )
{
	if ( pszHost == nullptr || nDefaultPort < 0 || nDefaultPort > 65535 ) return false;
	string host = pszHost;
	int port = nDefaultPort;
	const string::size_type separator = host.find( ':' );
	if ( separator != string::npos )
	{
		port = atoi( host.substr( separator + 1 ).c_str() );
		host.resize( separator );
	}
	if ( port < 0 || port > 65535 ) return false;
	return NPlatform::ResolveIPv4( host.c_str(), static_cast<uint16_t>( port ), &addr );
}

string CNodeAddress::GetName( bool ) const
{
	char buffer[64];
	snprintf( buffer, sizeof(buffer), "%u.%u.%u.%u:%u", static_cast<unsigned int>( addr.data[0] ), static_cast<unsigned int>( addr.data[1] ), static_cast<unsigned int>( addr.data[2] ), static_cast<unsigned int>( addr.data[3] ), static_cast<unsigned int>( AddressPort( addr ) ) );
	return buffer;
}

bool CNodeAddressSet::GetAddress( int n, CNodeAddress *pRes ) const
{
	if ( pRes == nullptr ) return false;
	pRes->Clear();
	if ( n < 0 || n >= N_MAX_HOST_HOMES || ips[n] == 0 ) return false;
	NPlatform::SocketAddress *address = pRes->GetSockAddr();
	address->family = 2;
	memcpy( address->data, &ips[n], 4 );
	memcpy( address->data + 4, &nPort, 2 );
	return true;
}

CLinksManager::CLinksManager() : s( NPlatform::InvalidSocket )
{
	if ( !NPlatform::SocketRuntimeInit() ) return;
	NPlatform::SocketAddress *broadcast = broadcastAddr.GetSockAddr();
	if ( !NPlatform::ResolveIPv4( "127.0.0.1", 0, broadcast ) ) *broadcast = NPlatform::SocketAddress{};
	broadcast->data[0] = 127;
	broadcast->data[1] = 0;
	broadcast->data[2] = 0;
	broadcast->data[3] = 255;
}

CLinksManager::~CLinksManager()
{
	Finish();
	NPlatform::SocketRuntimeDone();
}

bool CLinksManager::Start( int nPort )
{
	Finish();
	if ( nPort < 0 || nPort > 65535 ) return false;
	s = NPlatform::OpenUdpSocket();
	if ( s == NPlatform::InvalidSocket ) return false;
	if ( !NPlatform::Bind( s, nullptr, static_cast<uint16_t>( nPort ) ) || !NPlatform::SetNonBlocking( s, true ) || !NPlatform::SetBroadcast( s, true ) )
	{
		Finish();
		return false;
	}
	return true;
}

void CLinksManager::Finish()
{
	if ( s != NPlatform::InvalidSocket ) NPlatform::Close( s );
	s = NPlatform::InvalidSocket;
}

bool CLinksManager::MakeBroadcastAddr( CNodeAddress *pRes, int nPort ) const
{
	if ( pRes == nullptr || nPort < 0 || nPort > 65535 ) return false;
	*pRes = broadcastAddr;
	SetAddressPort( pRes->GetSockAddr(), static_cast<uint16_t>( nPort ) );
	return true;
}

bool CLinksManager::IsLocalAddr( const CNodeAddress &test ) const
{
	if ( test.addr.data[0] == 127 ) return true;
	const NPlatform::SocketAddress *broadcast = broadcastAddr.GetSockAddr();
	return test.addr.data[0] == broadcast->data[0] && test.addr.data[1] == broadcast->data[1] && test.addr.data[2] == broadcast->data[2];
}

extern int nTrafficPackets;
extern int nTrafficTotalSize;
#ifdef NET_TEST_APPLICATION
bool bEmulateWeakNetwork = false;
float fLostRate = 0.7f;
struct SPacket
{
	CNodeAddress addr;
	CMemoryStream pkt;
};
#endif

bool CLinksManager::Send( const CNodeAddress &dst, CMemoryStream &pkt ) const
{
	if ( s == NPlatform::InvalidSocket ) return false;
#ifdef NET_TEST_APPLICATION
	static vector<SPacket> pktQueue;
	if ( bEmulateWeakNetwork )
	{
		if ( rand() <= RAND_MAX * fLostRate ) return true;
		pktQueue.push_back( SPacket() );
		pktQueue.back().addr = dst;
		pktQueue.back().pkt = pkt;
		while ( pktQueue.size() > 3 )
		{
			const size_t index = rand() % pktQueue.size();
			SPacket &queued = pktQueue[index];
			const int result = NPlatform::SendTo( s, *queued.addr.GetSockAddr(), queued.pkt.GetBuffer(), queued.pkt.GetSize() );
			pktQueue.erase( pktQueue.begin() + index );
			if ( result < 0 ) return false;
		}
		return true;
	}
#endif
	const int size = pkt.GetSize();
	const int result = NPlatform::SendTo( s, *dst.GetSockAddr(), pkt.GetBuffer(), size );
	if ( result >= 0 )
	{
		++nTrafficPackets;
		nTrafficTotalSize += result;
	}
	return result == size;
}

bool CLinksManager::Recv( CNodeAddress *pSrc, CMemoryStream *pPkt ) const
{
	ASSERT( pSrc );
	ASSERT( pPkt );
	if ( pSrc == nullptr || pPkt == nullptr || s == NPlatform::InvalidSocket ) return false;
	pPkt->Seek( 2048 );
	const int result = NPlatform::ReceiveFrom( s, pSrc->GetSockAddr(), pPkt->GetBufferForWrite(), 2048 );
	if ( result >= 0 )
	{
		pSrc->addr.family = 2;
		memset( pSrc->addr.data + 6, 0, 8 );
		pPkt->SetSize( result );
		++nTrafficPackets;
		nTrafficTotalSize += result;
	}
	return result >= 0;
}

bool CLinksManager::GetSelfAddress( CNodeAddressSet *pRes ) const
{
	if ( pRes == nullptr || s == NPlatform::InvalidSocket ) return false;
	pRes->Clear();
	NPlatform::SocketAddress local{};
	if ( !NPlatform::GetLocalAddress( s, &local ) ) return false;
	SetLoopbackIfUnspecified( &local );
	pRes->nPort = 0;
	memcpy( &pRes->nPort, local.data + 4, 2 );
	memcpy( &pRes->ips[0], local.data, 4 );
	return true;
}

NPlatform::SocketHandle CLinksManager::GetSocket() const
{
	return s;
}
}
