#include "stdafx.h"

#include "ObstacleInternal.h"
#include "CommonUnit.h"
#include "StaticObject.h"

int CObstacle::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fFirePower );
	return 0;
}
int CObstacleStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pObj );
	saver.AddTypedSuper( 2, static_cast<CObstacle*>(this) );
	return 0;
}
