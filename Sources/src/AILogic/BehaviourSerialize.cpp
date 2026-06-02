#include "stdafx.h"

#include "Behaviour.h"
int CStandartBehaviour::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &camouflateTime );
	saver.Add( 2, &underFireAnalyzeTime );
	saver.Add( 3, &nLastSign );
	saver.Add( 4, &lastTimeOfRotate );
	saver.Add( 5, &fleeTraceEnemyTime );

	return 0;
}
