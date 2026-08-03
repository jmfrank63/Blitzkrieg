#include "StdAfx.h"

#include "GSQueryReportingDriver.h"

CGSQueryReportingDriver::CGSQueryReportingDriver()
{
}

CGSQueryReportingDriver::~CGSQueryReportingDriver()
{
}

bool CGSQueryReportingDriver::Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly )
{
	return true;
}

INetDriver::EState CGSQueryReportingDriver::GetState() const
{
	return ACTIVE;
}

INetDriver::EReject CGSQueryReportingDriver::GetRejectReason() const
{
	return NONE;
}

void CGSQueryReportingDriver::StartGame()
{
}

void CGSQueryReportingDriver::StartGameInfoSend( const SGameInfo &_gameInfo )
{
	gameInfo = _gameInfo;
}

void CGSQueryReportingDriver::StopGameInfoSend()
{
}

void CGSQueryReportingDriver::StartNewPlayerAccept()
{
	gameInfo.eGameMode = ESGM_OPENPLAYING;
}

void CGSQueryReportingDriver::StopNewPlayerAccept()
{
	gameInfo.eGameMode = ESGM_CLOSEDPLAYING;
}
