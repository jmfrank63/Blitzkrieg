#ifndef __GS_CONSTS_H__
#define __GS_CONSTS_H__
#pragma ONCE
#include "NetDriver.h"
inline const INetDriver::EServerGameMode GetMode( const char *pszMode )
{
	if ( strcmp( pszMode, "wait" ) == 0 )
		return INetDriver::ESGM_WAIT;
	if ( strcmp( pszMode, "settings" ) == 0 )
		return INetDriver::ESGM_SETTINGS;
	if ( strcmp( pszMode, "closedplaying" ) == 0 )
		return INetDriver::ESGM_CLOSEDPLAYING;
	if ( strcmp( pszMode, "openplaying" ) == 0 )
		return INetDriver::ESGM_OPENPLAYING;
	if ( strcmp( pszMode, "debriefing" ) == 0 )
		return INetDriver::ESGM_DEBRIEFING;
	if ( strcmp( pszMode, "exiting" ) == 0 )
		return INetDriver::ESGM_EXITING;

	return INetDriver::ESGM_CLOSEDPLAYING;
}
inline const char* GetMode( const INetDriver::EServerGameMode eMode )
{
	switch ( eMode )
	{
		case INetDriver::ESGM_WAIT:						return "wait";
		case INetDriver::ESGM_SETTINGS:				return "settings";
		case INetDriver::ESGM_CLOSEDPLAYING:	return "closedplaying";
		case INetDriver::ESGM_OPENPLAYING:		return "openplaying";
		case INetDriver::ESGM_DEBRIEFING:			return "debriefing";
		case INetDriver::ESGM_EXITING:				return "exiting";
		default:
			return "closedplaying";
	}
}
#endif // __GS_CONSTS_H__
