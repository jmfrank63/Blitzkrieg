#include "StdAfx.h"

#include "RndRunUpToEnemy.h"
#include "Soldier.h"
#include "PathFinder.h"
#include "Guns.h"
#include "SerializeOwner.h"
#include "Path.h"
extern NTimer::STime curTime;
CRndRunUpToEnemy::CRndRunUpToEnemy( CAIUnit *pOwner, CAIUnit *pEnemy )
{
	Init( pOwner, pEnemy );
}
void CRndRunUpToEnemy::Init( CAIUnit *_pOwner, CAIUnit *_pEnemy )
{
	pEnemy = _pEnemy;
	bRunningToEnemy = false;
	bForceStaying = false;
	vLastOwnerPos = _pOwner->GetCenter();
	checkTime = 0;

	if ( _pOwner->GetStats()->IsInfantry() && _pOwner->GetStats()->type != RPG_TYPE_SNIPER && _pOwner->GetBehaviour().moving != SBehaviour::EMHoldPos )
	{
		bCheck = true;
		pOwner = static_cast<CSoldier*>( _pOwner );
	}
	else
	{
		pOwner = 0;
		bCheck = false;
	}
}
void CRndRunUpToEnemy::SendOwnerToRandomRun()
{
	NI_ASSERT_T( !bForceStaying, "Wrong force staying value ( false expected )" );
	
	const CVec2 vDirToEnemy = pEnemy->GetCenter() - pOwner->GetCenter();
	const WORD wDirToEnemy = GetDirectionByVector( vDirToEnemy );

	const WORD wRandomAngle = Random( 0, 65536 / 5 );
	WORD wResultDir;
	if ( Random( 0.0f, 1.0f ) < 0.5f )
		wResultDir = wDirToEnemy - wRandomAngle;
	else
		wResultDir = wDirToEnemy + wRandomAngle;

	float fRandomDist;
	if ( Random( 0.0f, 1.0f ) < 0.7f )
	{
		fRandomDist = Random( float( 0.4f * SConsts::TILE_SIZE ), float( 2.0f * SConsts::TILE_SIZE ) );
		bForceStaying = false;
	}
	else
	{
		fRandomDist = Random( float( 2.0f * SConsts::TILE_SIZE ), float( 4.0f * SConsts::TILE_SIZE ) );
		bForceStaying = true;
	}

	const CVec2 vPointToRunUp = pOwner->GetCenter() + GetVectorByDirection( wResultDir ) * fRandomDist;

	if ( IStaticPath *pStaticPath = CreateStaticPathToPoint( vPointToRunUp, VNULL2, pOwner, true ) )
	{
		if ( ( bForceStaying && pStaticPath->GetLength() <= 5 ||
 				   !bForceStaying && pStaticPath->GetLength() <= 3 ) &&
				 fabs2( pStaticPath->GetFinishPoint() - vPointToRunUp ) < sqr( 3.0f * SConsts::TILE_SIZE / 4.0f ) )
		{
			bRunningToEnemy = true;

			if ( bForceStaying )
				pOwner->AllowLieDown( false );

			pOwner->SendAlongPath( pStaticPath, VNULL2, false );
		}
		else
		{
			bRunningToEnemy = false;	
			bForceStaying = false;
		}
	}
	else
	{
		bRunningToEnemy = false;	
		bForceStaying = false;
	}
}
void CRndRunUpToEnemy::Segment()
{
	if (
			 bCheck &&
		   pOwner->IsFree() && IsValidObj( pEnemy ) &&
			 ( bRunningToEnemy || fabs2( pOwner->GetCenter() - pEnemy->GetCenter() ) >= sqr(0.7f) * sqr(pOwner->GetGun( 0 )->GetFireRange( 0 )) ) 
		 )
	{
		if ( !bRunningToEnemy )
		{
			if ( curTime >= checkTime )
			{
				if ( pOwner->IsIdle() && vLastOwnerPos == pOwner->GetCenter() && Random( 0.0f, 1.0f ) <= 0.7f )
					SendOwnerToRandomRun();

				if ( !bRunningToEnemy )
				{
					vLastOwnerPos = pOwner->GetCenter();
					checkTime = curTime + Random( 2000, 5000 );
				}
			}
		}
		else if ( pOwner->IsIdle() )
		{
			bRunningToEnemy = false;
			vLastOwnerPos = pOwner->GetCenter();
			checkTime = curTime + Random( 2000, 5000 );

			if ( bForceStaying )
			{
				pOwner->AllowLieDown( true );
				bForceStaying = false;
			}
		}
	}

	NI_ASSERT_T( bRunningToEnemy || !bForceStaying, "Wrong force staying value ( false expected )" );
}
void CRndRunUpToEnemy::Finish()
{
	if ( pOwner )
		pOwner->AllowLieDown( true );
}
int CRndRunUpToEnemy::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &pEnemy );
	saver.Add( 3, &vLastOwnerPos );
	saver.Add( 4, &checkTime );
	saver.Add( 5, &bRunningToEnemy );
	saver.Add( 6, &bForceStaying );
	saver.Add( 8, &bCheck );

	return 0;
}
