#include "stdafx.h"

#include "AIUnitInfoForGeneral.h"
#include "AIUnit.h"
#include "Diplomacy.h"
#include "General.h"
#include "SerializeOwner.h"
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CSupremeBeing theSupremeBeing;
BASIC_REGISTER_CLASS( CAIUnitInfoForGeneral );
CAIUnitInfoForGeneral::CAIUnitInfoForGeneral( CAIUnit *_pOwner )
: pOwner( _pOwner ),
	lastVisibleTime( 0 ), vLastVisiblePosition( VNULL2 ),
	lastVisibleAntiArtTime( 0 ), vLastVisibleAntiArtCenter( VNULL2 ), fDistToLastVisibleAntiArt( -1.0f ),
	fWeight( 0 ), vLastRegisteredGeneralPos( VNULL2 )
{
	nextTimeToReportGeneral = curTime + Random( 2000, 5000 );
}
void CAIUnitInfoForGeneral::Segment()
{
	if ( curTime >= nextTimeToReportGeneral && theDipl.GetNeutralPlayer() != pOwner->GetPlayer() )
	{
		nextTimeToReportGeneral = curTime + Random( 2000, 5000 );

		const int nEnemyParty = 1 - pOwner->GetParty();
		if ( pOwner->IsVisible( nEnemyParty ) )
		{
			lastVisibleTime = curTime;
			vLastVisiblePosition = pOwner->GetCenter();
		}

		theSupremeBeing.UpdateEnemyUnitInfo(
			this,
			curTime - lastVisibleTime, vLastVisiblePosition,
			curTime - lastVisibleAntiArtTime, vLastVisibleAntiArtCenter, fDistToLastVisibleAntiArt 
		);
	}
}
void CAIUnitInfoForGeneral::UpdateVisibility( bool bVisible )
{
	lastVisibleTime = curTime;
	vLastVisiblePosition = pOwner->GetCenter();
}
void CAIUnitInfoForGeneral::UpdateAntiArtFire( const NTimer::STime lastHeardTime, const CVec2 &vAntiArtCenter )
{
	if ( curTime - lastHeardTime <= 1000 && lastHeardTime != 0 )
	{
		lastVisibleAntiArtTime = curTime;
		vLastVisibleAntiArtCenter = vAntiArtCenter;
		fDistToLastVisibleAntiArt = fabs( vAntiArtCenter - pOwner->GetCenter() );
	}
}
void CAIUnitInfoForGeneral::Die()
{
	theSupremeBeing.UnitDied( this );
}
int CAIUnitInfoForGeneral::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &lastVisibleTime );
	saver.Add( 3, &vLastVisiblePosition );
	saver.Add( 4, &lastVisibleAntiArtTime );
	saver.Add( 5, &vLastVisibleAntiArtCenter );
	saver.Add( 6, &fDistToLastVisibleAntiArt );
	saver.Add( 7, &nextTimeToReportGeneral );
	saver.Add( 8, &vLastRegisteredGeneralPos );

	return 0;
}
