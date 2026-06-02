#include "stdafx.h"

#include "FreeFireManager.h"
int CFreeFireManager::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &shootInfo );
	saver.Add( 2, &lastCheck );

	return 0;
}
int CFreeFireManager::SShotInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pTarget );
	saver.Add( 2,	&shootingPos );
	saver.Add( 3, &unitDir );
	saver.Add( 4, &gunDir );
	
	return 0;
}
