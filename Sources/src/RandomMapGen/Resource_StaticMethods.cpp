#include "StdAfx.h"
#include "IB_Types.h"
#include "Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool SEnumFolderStructureParameter::IsFolderRelative( const TEnumFolders &rFolders, const std::string &rszFolder, const std::string &rszRelativeFolder )
{
	TEnumFolders::const_iterator folderIterator = rFolders.find( rszFolder );
	if ( folderIterator != rFolders.end() )
	{
		return ( folderIterator->second.find( rszRelativeFolder ) != folderIterator->second.end() );
	}
	return false;
}

void SEnumFolderStructureParameter::SetRelativeFolder( TEnumFolders *pFolders, const std::string &rszFolder, const std::string &rszRelativeFolder )
{
	NI_ASSERT_T( pFolders != 0, NStr::Format( "Wrong parameter: %x\n", pFolders ) );
	if ( pFolders )
	{
		( *pFolders )[rszFolder].insert( rszRelativeFolder );
	}
}
