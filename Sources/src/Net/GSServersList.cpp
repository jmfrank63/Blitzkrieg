#include "StdAfx.h"

#include "GSServersList.h"

CGSServersListDriver::CGSServersListDriver()
{
}

CGSServersListDriver::~CGSServersListDriver()
{
}

bool CGSServersListDriver::Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly )
{
	GetSingleton<IConsoleBuffer>()->WriteASCII(
		CONSOLE_STREAM_CONSOLE,
		"Open internet server browser backend is not implemented yet.",
		0xffffff00,
		true
	);
	return true;
}

INetDriver::EState CGSServersListDriver::GetState() const
{
	return ACTIVE;
}

INetDriver::EReject CGSServersListDriver::GetRejectReason() const
{
	return NONE;
}

bool CGSServersListDriver::GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo )
{
	return false;
}

void CGSServersListDriver::RefreshServersList()
{
}
