#include "StdAfx.h"
#include "FileUtils.h"
#include "ARSound.h"

bool CARSound::Load( const std::vector<BYTE> &rData )
{
	sound = rData;
	return true;
}
