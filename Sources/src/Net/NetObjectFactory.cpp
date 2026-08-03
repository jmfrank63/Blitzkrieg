#include "StdAfx.h"

#include "NetObjectFactory.h"

#include "NetA4.h"

#ifndef BK_ENABLE_GAMESPY
#define BK_ENABLE_GAMESPY 0
#endif

#if BK_ENABLE_GAMESPY
#include "GSQueryReportingDriver.h"
#include "GSServersList.h"
#endif
static CNetObjectFactory theNetObjectFactory;
CNetObjectFactory::CNetObjectFactory()
{
	REGISTER_CLASS( this, NET_NET_DRIVER, NNet::CNetDriver );
	REGISTER_CLASS( this, NET_NODE_ADDRESS, NNet::CNodeAddressWrap );
#if BK_ENABLE_GAMESPY
	REGISTER_CLASS( this, NET_GS_QUERY_REPORTING_DRIVER, CGSQueryReportingDriver );
	REGISTER_CLASS( this, NET_GS_SERVERS_LIST_DIRVER, CGSServersListDriver );
#endif
}
static SModuleDescriptor theModuleDescriptor( "Network", NET_NET, 0x0100, &theNetObjectFactory, 0 );
extern "C" BK_EXPORT const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
