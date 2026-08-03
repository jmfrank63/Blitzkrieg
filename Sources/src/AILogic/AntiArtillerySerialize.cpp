#include "StdAfx.h"

#include "AntiArtillery.h"
#include "AntiArtilleryManager.h"
int CRevealCircle::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &circle );
	saver.AddTypedSuper( 2, static_cast<CLinkObject*>(this) );

	return 0;
}
int CAntiArtillery::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &fMaxRadius );
	saver.Add( 2, &nParty );
	saver.Add( 3, &lastScan );
	saver.Add( 4, &closestEnemyDist2 );
	saver.Add( 5, &lastHeardPos );
	saver.Add( 6, &nHeardShots );
	saver.Add( 7, &lastRevealCenter );
	saver.Add( 8, &lastShotTime );
	saver.Add( 9, &lastRevealCircleTime );
	saver.AddTypedSuper( 11, static_cast<CLinkObject*>(this) );

	return 0;
}
int CAntiArtilleryManager::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &antiArtilleries );

	return 0;
}
