#include "stdafx.h"

#include "NetDriverConsts.h"
int SNetDriverConsts::F_TIMEOUT = 60;
float SNetDriverConsts::FP_SERVER_LIST_TIMEOUT = 20.0f;
void SNetDriverConsts::Load()
{
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	F_TIMEOUT = constsTbl.GetInt( "Net", "TimeOut", 60 );
	FP_SERVER_LIST_TIMEOUT = constsTbl.GetFloat( "Net", "ServerListTimeOut", 20.0f );
}
