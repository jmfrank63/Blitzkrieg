#include "StdAfx.h"
#include "IB_Types.h"
#include "Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool SEnumFolderStructureParameter::IsFolderRelative( const std::string &rszFolder, const std::string &rszRelativeFolder )
{
	return IsFolderRelative( folders, rszFolder, rszRelativeFolder );
}

void SEnumFolderStructureParameter::SetRelativeFolder( const std::string &rszFolder, const std::string &rszRelativeFolder )
{
	SetRelativeFolder( &folders, rszFolder, rszRelativeFolder );
}
