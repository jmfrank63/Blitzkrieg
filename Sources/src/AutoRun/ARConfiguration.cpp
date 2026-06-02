#include "StdAfx.h"
#include "ARConfiguration.h"

bool CARConfiguration::Load( const std::vector<BYTE> &rData )
{
	return iniFile.Open( rData, TABLE_ACCESS_READ );
}
