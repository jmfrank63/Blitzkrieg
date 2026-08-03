#include "StdAfx.h"

#include "MountedGun.h"
#include "SerializeOwner.h"
int CCommonMountedGun::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pBuildingStats );
	SerializeOwner( 2, &pBuilding, &saver );
	saver.Add( 3, &pTurret );
	saver.Add( 4, &nSlot );

	return 0;
}
