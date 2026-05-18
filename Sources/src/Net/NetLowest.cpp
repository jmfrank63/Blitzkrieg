#include "StdAfx.h"
#include "NetLowest.h"
#include "Streams.h"
using namespace std;
namespace NNet
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNodeAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNodeAddress::SetInetName( const char *pszHost, int nDefaultPort )
{
	int nIdx, nPort = nDefaultPort;
	memset( &addr, 0, sizeof( addr ) );
	ASSERT( sizeof(addr) >= sizeof( sockaddr_in) );
  sockaddr_in &nameRemote = *(sockaddr_in*)&addr;
	
	nameRemote.sin_family = AF_INET;
	// extract port number from address
	string szAddr = pszHost;
	nIdx = szAddr.find( ':' );
	if ( nIdx != -1 )
	{
		nPort = atoi( string( szAddr, nIdx + 1 ).c_str() );
		szAddr = string( szAddr, 0, nIdx );
	}
	// determine host
	nameRemote.sin_addr.S_un.S_addr = inet_addr( szAddr.c_str() ); 
	if( nameRemote.sin_addr.S_un.S_addr == INADDR_NONE )  // not resolved?
	{
		hostent *he;
		he = gethostbyname( szAddr.c_str() ); // m.b. it is string comp.domain
		if( he == NULL )
			return false;
		nameRemote.sin_addr.S_un.S_addr = *( unsigned long* )( he->h_addr_list[0] );
	}
	nameRemote.sin_port = htons( nPort );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
string CNodeAddress::GetName( bool bResolve ) const
{
  sockaddr_in &nameRemote = *(sockaddr_in*)&addr;
	
	hostent *he = 0;
	if ( bResolve )
		he = gethostbyaddr( (const char*)&addr, sizeof(addr), AF_INET ); // m.b. it is string comp.domain
	char szBuf[1024];
	if( he == 0 || he->h_name == 0 )
	{
		in_addr &ia = nameRemote.sin_addr;
		sprintf( szBuf, "%i.%i.%i.%i:%i", 
			(int) ia.S_un.S_un_b.s_b1,
			(int) ia.S_un.S_un_b.s_b2,
			(int) ia.S_un.S_un_b.s_b3,
			(int) ia.S_un.S_un_b.s_b4,
			(int) ntohs( nameRemote.sin_port ) );
	}
	else
	{
		sprintf( szBuf, "%s:%i", 
			(const char*) he->h_name,
			(int) ntohs( nameRemote.sin_port ) );
	}
	return szBuf;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNodeAddressSet::GetAddress( int n, CNodeAddress *pRes ) const
{
	pRes->Clear();
	if ( n < 0 || n >= N_MAX_HOST_HOMES || ips[n] == 0 )
		return false;
	sockaddr_in *p = (sockaddr_in*)pRes->GetSockAddr();
	p->sin_family = AF_INET;
	p->sin_port = nPort;
	p->sin_addr.S_un.S_addr = ips[n];
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLinksManager
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinksManager::CLinksManager()
{
	WORD wVersionRequested = MAKEWORD( 1, 1 );
	WSADATA wsaData;
 
	int bRv = WSAStartup( wVersionRequested, &wsaData ) == 0;
	ASSERT( bRv );

	s = INVALID_SOCKET;
	// get host for broadcast addresses formation
	char szHost[1024];
	if ( gethostname( szHost, 1000 ) )
	{
		ASSERT(0);
		return;
	}
	hostent *he;
	he = gethostbyname( szHost ); // m.b. it is string comp.domain
	if ( he == NULL )
 	{
		ASSERT(0);
		return;
	}
	// form addresses
	CNodeAddress addr;
  sockaddr_in &name = *(sockaddr_in*)&addr;
	name.sin_family = AF_INET;
	// hostent are broken for some unknown reason, only one address is valid
	unsigned long *pAddr = (unsigned long*)( he->h_addr_list[0] );
	//for ( ; *pAddr; pAddr++ )
	{
		name.sin_addr.S_un.S_addr = pAddr[0];
		unsigned char bClass = name.sin_addr.S_un.S_un_b.s_b1;
		if ( bClass >= 1 && bClass <= 126 )
		{
			name.sin_addr.S_un.S_un_b.s_b2 = 255;
			name.sin_addr.S_un.S_un_b.s_b3 = 255;
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		}
		if ( bClass >= 128 && bClass <= 191 )
		{
			name.sin_addr.S_un.S_un_b.s_b3 = 255;
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		}
		if ( bClass >= 192 && bClass <= 223 )
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		broadcastAddr = addr;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinksManager::~CLinksManager()
{
	Finish();
	WSACleanup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::Start( int nPort )
{
	Finish();
	s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( s == INVALID_SOCKET )
		return false;
	sockaddr_in name;
	memset( &name, 0, sizeof(name) );
	name.sin_family = AF_INET;
	name.sin_addr.S_un.S_addr = INADDR_ANY;
	name.sin_port = htons( nPort );
	if ( nPort > 0 )
	{
		if ( bind( s, (sockaddr*)&name, sizeof( name ) ) != 0 )
		{
			closesocket( s );
			s = INVALID_SOCKET;
			return false;
		}
	}
	DWORD	dwOpt = 1;
	ioctlsocket( s, FIONBIO, &dwOpt ); // no block
	setsockopt( s, SOL_SOCKET, SO_BROADCAST, (const char*)&dwOpt, 4 );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinksManager::Finish()
{
	if ( s != INVALID_SOCKET )
		closesocket( s );
	s = INVALID_SOCKET;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::MakeBroadcastAddr( CNodeAddress *pRes, int nPort ) const
{
	*pRes = broadcastAddr;
  sockaddr_in &name = *(sockaddr_in*)&pRes->addr;
	name.sin_port = htons( nPort );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::IsLocalAddr( const CNodeAddress &test ) const
{
	const sockaddr_in &nt = *(sockaddr_in*)&test.addr;
	if ( nt.sin_addr.S_un.S_addr == 0x0100007f )
		return true;
	//for ( int i = 0; i < broadcastAddr.size(); ++i )
	{
		const CNodeAddress &broad = broadcastAddr;//[ i ];
	  const sockaddr_in &nb = *(sockaddr_in*)&broad.addr;
		DWORD dwB = nb.sin_addr.S_un.S_addr;
		DWORD dwT = nt.sin_addr.S_un.S_addr;
		DWORD dwMask = 0;
		for ( int k = 3; k >= 0; k-- )
		{
			DWORD dwTestMask = 0xFF << k*8;
			if ( (dwB & dwTestMask) == dwTestMask )
				dwMask |= dwTestMask;
			else
				break;
		}
		dwMask = ~dwMask;
		bool bTest = ( dwB & dwMask ) == ( dwT & dwMask );
		if ( bTest )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRAP{
extern int nTrafficPackets;
extern int nTrafficTotalSize;
// CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
#ifdef NET_TEST_APPLICATION
	static vector<SPacket> pktQueue;
	if ( bEmulateWeakNetwork )
	{
		if ( rand() <= RAND_MAX * fLostRate )
			return true;
		pktQueue.push_back( SPacket() );
		pktQueue.back().addr = dst;
		pktQueue.back().pkt = pkt;
		while ( pktQueue.size() > 3 )
		{
			int nPkt = rand() % pktQueue.size();
			SPacket &p = pktQueue[nPkt];
			int nSize = p.pkt.GetSize();
			int nRv = sendto( s, (const char*)p.pkt.GetBuffer(), nSize, 0, &p.addr.addr, sizeof( p.addr.addr ) );
			pktQueue.erase( pktQueue.begin() + nPkt );
		}
		return true;
	}
#endif
	int nSize = pkt.GetSize();
	int nRv = sendto( s, (const char*)pkt.GetBuffer(), nSize, 0, &dst.addr, sizeof( dst.addr ) );

	// CRAP{
	if ( nRv >= 0 )
	{
		++nTrafficPackets;
		nTrafficTotalSize += nRv;
	}
	// CRAP}
//	printf( "send to %s\n", dst.GetFastName().c_str() );

	return nRv == nSize;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::Recv( CNodeAddress *pSrc, CMemoryStream *pPkt ) const
{
	ASSERT( pSrc );
	ASSERT( pPkt );
	int nAddrSize;
	pPkt->Seek( 2048 );
	nAddrSize = sizeof( pSrc->addr );
	int nRes = recvfrom( s, (char*)pPkt->GetBufferForWrite(), 2048, 0, &pSrc->addr, &nAddrSize );
	if ( nRes >= 0 )
	{
		pSrc->addr.sa_family = AF_INET;       // somehow this gets spoiled on win2k
		memset( pSrc->addr.sa_data + 6, 0, 8 );
		pPkt->SetSize( nRes );
	}

	// CRAP{
	if ( nRes >=0 )
	{
		++nTrafficPackets;
		nTrafficTotalSize += nRes;
	}
	// CRAP}
//	if ( nRes >= 0 )
//		printf( "rect from %s\n", pSrc->GetFastName().c_str() );

	return nRes >= 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::GetSelfAddress( CNodeAddressSet *pRes ) const
{
	pRes->Clear();
	sockaddr_in addr;
	int nBufLeng = sizeof(sockaddr_in);
	if ( getsockname( s, (sockaddr*)&addr, &nBufLeng ) != 0 )
		return false;
	pRes->nPort = addr.sin_port;
	char szHostName[10000];
	gethostname( szHostName, 9999 );
	hostent *p = gethostbyname( szHostName );
	if ( !p || p->h_addrtype != AF_INET || p->h_length != 4 )
		return false;
	for ( int k = 0; k < N_MAX_HOST_HOMES; ++k )
	{
		if ( !p->h_addr_list[k] )
			break;
		pRes->ips[k] = *(int*)p->h_addr_list[k];
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SOCKET CLinksManager::GetSocket() const
{	
	return s;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
