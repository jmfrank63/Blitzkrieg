#pragma once

#include <stddef.h>
#include <string.h>

#include "queryreporting/gqueryreporting.h"
#include "cengine/goaceng.h"
#include "../StreamIO/Globals.h"

extern "C" char pi_chat_server_address[64];

inline void CopyGameSpyHostName( char *pszDst, const size_t nDstSize, const char *pszValue )
{
	if ( nDstSize == 0 )
		return;

	strncpy( pszDst, pszValue, nDstSize - 1 );
	pszDst[nDstSize - 1] = 0;
}

inline const char* GetOpenSpyConfiguredHost( const char *pszSpecificVar, const char *pszDefaultHost )
{
	const char *pszOpenSpyHost = GetGlobalVar( "Options.Multiplayer.OpenSpyHost", "" );
	const char *pszHost = GetGlobalVar( pszSpecificVar, "" );

	if ( pszHost && pszHost[0] )
		return pszHost;
	if ( pszOpenSpyHost && pszOpenSpyHost[0] )
		return pszOpenSpyHost;
	return pszDefaultHost;
}

inline void SyncGameSpyEndpointConfig()
{
	CopyGameSpyHostName(
		qr_hostname,
		sizeof(qr_hostname),
		GetOpenSpyConfiguredHost( "Options.Multiplayer.OpenSpyMasterHost", "master.gamespy.com" )
	);
	CopyGameSpyHostName(
		ServerListHostname,
		sizeof(ServerListHostname),
		GetOpenSpyConfiguredHost( "Options.Multiplayer.OpenSpyMasterHost", "master.gamespy.com" )
	);
	CopyGameSpyHostName(
		pi_chat_server_address,
		sizeof(pi_chat_server_address),
		GetOpenSpyConfiguredHost( "Options.Multiplayer.OpenSpyPeerchatHost", "peerchat.gamespy.com" )
	);
}
