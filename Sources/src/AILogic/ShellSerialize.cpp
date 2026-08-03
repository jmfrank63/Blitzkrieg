#include "StdAfx.h"

#include "Shell.h"
int CHitInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pWeapon );
	saver.Add( 2, &wShell );
	saver.Add( 3, &wDir );
	saver.Add( 4, &pVictim );
	saver.Add( 5, &explCoord );
	saver.AddTypedSuper( 6, static_cast<CLinkObject*>(this) );

	return 0;
}
int CFakeBallisticTraj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &startTime );
	saver.Add( 2, &explTime );
	saver.Add( 3, &point );
	saver.Add( 4, &v );
	saver.Add( 5, &A1 );
	saver.Add( 6, &A2 );
	saver.Add( 7, &wDir );

	return 0;
}
int CBombBallisticTraj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &point );
	saver.Add( 2, &v );
	saver.Add( 3, &wDir );
	saver.Add( 4, &startTime );
	saver.Add( 5, &explTime );
	saver.Add( 6, &vRandAcc );

	return 0;
}
int CBallisticTraj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &vStart3D );
	saver.Add( 3, &fVx );
	saver.Add( 4, &fVy );
	saver.Add( 5, &wDir );
	saver.Add( 6, &vDir );
	saver.Add( 7, &startTime );
	saver.Add( 8, &explTime );
	saver.Add( 9, &eType );
	saver.Add( 10, &fG );
	saver.Add( 13, &wAngle );
	return 0;
}
int CExplosion::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nShellType );
	saver.Add( 2, &pWeapon );
	saver.Add( 3, &pUnit );
	saver.Add( 4, &explCoord );
	saver.Add( 5, &z );
	saver.Add( 6, &attackDir );
	saver.Add( 7, &nPlayerOfShoot );
	saver.Add( 8, &pHitToSend );

	return 0;
}
int CBurstExpl::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.AddTypedSuper( 1, static_cast<CExplosion*>(this) ); 
	saver.Add( 2, &nArmorDir );

	return 0;
}
int CCumulativeExpl::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CExplosion*>(this) );
	saver.Add( 2, &nArmorDir );

	return 0;
}
int CMomentShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &expl );
	return 0;
}
int CShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &explTime );
	saver.Add( 2, &expl );
	saver.Add( 3, &nGun );
	saver.Add( 6, &vStartVisZ );
	saver.Add( 7, &vFinishVisZ );

	return 0;
}
int CInvisShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CShell*>(this) ); 
	
	return 0;
}
int CVisShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CShell*>(this) ); 
	saver.Add( 2, &pTraj );
	saver.Add( 3, &center );
	saver.Add( 4, &speed );
	saver.AddTypedSuper( 5, static_cast<CLinkObject*>(this) );
	saver.Add( 6, &bVisible );

	return 0;
}
int CShellsStore::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &invisShells );
	saver.Add( 3, &visShells );
	return 0;
}
