#include "stdafx.h"

#include "Commands.h"
int SGroupPathInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPath );
	saver.Add( 2, &pPathFinder );
	saver.Add( 3, &nSubGroup );
	saver.Add( 4, &cTileSize );
	saver.Add( 5, &aiClass );

	return 0;
}
int CAICommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &unitCmd );
	saver.Add( 2, &id );
	saver.Add( 3, &nFlag );

	return 0;
}
