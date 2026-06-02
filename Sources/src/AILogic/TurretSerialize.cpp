#include "stdafx.h"

#include "Turret.h"
int CTurret::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &hor );
	saver.Add( 2, &ver );

	saver.Add( 9, &bCanReturn );
	saver.Add( 10, &pTracedUnit );
	saver.Add( 11, &pLockingGun );
	saver.Add( 12, &bVerAiming );
	saver.Add( 13, &wDefaultHorAngle );
	saver.Add( 14, &bReturnToNULLVerAngle );
	
	saver.AddTypedSuper( 15, static_cast<CLinkObject*>(this) );
	
	return 0;
}
int CUnitTurret::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CTurret*>(this) );
	saver.Add( 2, &pOwner );
	saver.Add( 3, &nModelPart );
	saver.Add( 4, &dwGunCarriageParts );
	saver.Add( 5, &wHorConstraint );
	saver.Add( 6, &wVerConstraint );
	saver.Add( 7, &bCanRotateTurret );

	return 0;
}
int CMountedTurret::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CTurret*>(this) );
	saver.Add( 2, &center );
	saver.Add( 3, &dir );
	saver.Add( 4, &wHorTurnConstraint );
	saver.Add( 5, &wVerTurnConstraint );

	return 0;
}
