#ifndef __NETSERVERINFO_H_
#define __NETSERVERINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <list>
#include <vector>

#include "NetServerInfo.h"
#include "NetLowest.h"
#include "Streams.h"
namespace NNet
{
class CServerInfoSupport
{
public:
	struct SServerInfo
	{
		CNodeAddress addr;
		float fValidTimeLeft;   // how much time left until server info expires//TimeSinceUpdate;
		float fPing;            // ping to server in seconds
		bool bWrongVersion;     // to catch different version
		CMemoryStream info;
	};
	typedef std::list<SServerInfo> CServerInfoList;
private:
	CServerInfoList servers;
	float fTime, fRequestDelay;
	APPLICATION_ID applicationID;
	bool bDoReply;
	CMemoryStream serverInfo;

	SServerInfo& GetInfo( const CNodeAddress &addr );
public:
	CServerInfoSupport( APPLICATION_ID _nApplicationID );
	void Init( APPLICATION_ID _applicationID ) { applicationID = _applicationID; }
	void Step( float fDeltaTime );
	const CServerInfoList& GetServers() const { return servers; }
	bool DoReplyRequest() const { return bDoReply; }
	void StartReply( const CMemoryStream &info ) { bDoReply = true; serverInfo = info; }
	void StopReply() { bDoReply = false; }
	void ReplyServerInfoRequest( CBitStream &bits, CBitStream *pDstBits );
	void ProcessServerInfo( const CNodeAddress &addr, CBitStream &bits );
	void WriteRequest( CBitStream *pBits );
	bool CanSendRequest( const CNodeAddress &broadcast, std::vector<CNodeAddress> *pDest );
};
}
#endif
