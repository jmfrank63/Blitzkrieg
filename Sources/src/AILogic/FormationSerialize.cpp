#include "StdAfx.h"

#include "Formation.h"
#include "SerializeOwner.h"
#include "Path.h"
#include "SaveDBID.h"
int CFormation::SUnitInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );	
	saver.Add( 2, &geoms );
	saver.Add( 3, &nSlotInStats );

	return 0;
}
int CFormation::SVirtualUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &nSlotInStats );

	return 0;
}
int CFormation::CCarryedMortar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bHasMortar );
	saver.Add( 2, &pStats );
	saver.Add( 3, &fHP );
	
	if ( saver.IsReading() )
	{
		LoadDBID( &saver, 5, &nDBID );

		if ( nDBID == -1 )
			saver.Add( 4, &nDBID );
	}
	else
		SaveDBID( &saver, 5, nDBID );

	return 0;
}
int CFormation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CFormationCenter*>(this) );
	saver.Add( 2, &units );
	saver.Add( 4, &id );
	saver.Add( 5, &availCommands );
	saver.Add( 6, &cPlayer );
	saver.Add( 7, &guns );
	saver.Add( 8, &fPass );
	saver.Add( 9, &bWaiting );
	saver.Add( 12, &pStats );
	saver.Add( 13, &timeToCamouflage );
	saver.Add( 14, &nCurGeometry );
	saver.Add( 15, &geomInfo );
	saver.Add( 16, &nUnits );
	saver.Add( 17, &bDisabled );
	saver.Add( 18, &eInsideType );
	SerializeOwner( 19, &pObjInside, &saver );
	saver.Add( 20, &fMaxFireRange );
	saver.Add( 21, &virtualUnits );
	saver.Add( 23, &nVirtualUnits );
	saver.Add( 24, &mortar );
	saver.Add( 25, &bBoredInMoveFormationSent );
	saver.Add( 26, &lastBoredInMoveFormationCheck );
	saver.Add( 27, &bCanBeResupplied );
	saver.Add( 28, &bWithMoraleOfficer );

	return 0;
}
int CFormationCenter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCommonUnit*>(this) );
	saver.Add( 2, &maxSpeed );
	saver.Add( 3, &speed );
	saver.Add( 4, &center );
	saver.Add( 5, &dir );
	saver.Add( 6, &vAABBHalfSize );
	saver.Add( 7, &z );
	saver.Add( 8, &nBoundTileRadius );
	saver.Add( 9, &pSmoothPath );
	saver.Add( 10, &pStaticPath );
	saver.Add( 11, &maxDiff );
	saver.Add( 12, &fSpeedCoeff );
	saver.Add( 13, &lastKnownGoodTile );

	if ( saver.IsReading() && pSmoothPath.IsValid() )
		pSmoothPath->SetOwner( this );
	
	return 0;
}
