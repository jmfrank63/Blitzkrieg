#include "StdAfx.h"

#include "NetObjectFactory.h"

#include "NetA4.h"
#include "GSQueryReportingDriver.h"
#include "GSServersList.h"
static CNetObjectFactory theNetObjectFactory;
CNetObjectFactory::CNetObjectFactory()
{
	REGISTER_CLASS( this, NET_NET_DRIVER, NNet::CNetDriver );
	REGISTER_CLASS( this, NET_NODE_ADDRESS, NNet::CNodeAddressWrap );
	REGISTER_CLASS( this, NET_GS_QUERY_REPORTING_DRIVER, CGSQueryReportingDriver );
	REGISTER_CLASS( this, NET_GS_SERVERS_LIST_DIRVER, CGSServersListDriver );
}
static SModuleDescriptor theModuleDescriptor( "Network", NET_NET, 0x0100, &theNetObjectFactory, 0 );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
