#include "stdafx.h"

#include "Graph.h"
int CGraph::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nodes );
	saver.Add( 2, &n );
	saver.Add( 3, &dst );
	saver.Add( 4, &pred );
	saver.Add( 5, &v1 );
	saver.Add( 6, &v2 );
	saver.Add( 7, &graphComponent );

	return 0;
}
