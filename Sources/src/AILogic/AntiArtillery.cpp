#include "stdafx.h"

#include "AntiArtillery.h"
#include "Diplomacy.h"
#include "Updater.h"
#include "Randomize.h"
#include "UnitsIterators2.h"
#include "AntiArtilleryManager.h"
#include "AIUnit.h"
#include "Cheats.h"
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CUpdater updater;
extern CAntiArtilleryManager theAAManager;
extern SCheats theCheats;
BASIC_REGISTER_CLASS( CAntiArtillery );
CAntiArtillery::CAntiArtillery( CAIUnit *pOwner )
{
	SetUniqueId();
}
void CAntiArtillery::Init( const float _fMaxRadius, const int _nParty )
{
	Mem2UniqueIdObjs();
	
	fMaxRadius = _fMaxRadius;
	nParty = _nParty;

	lastScan = 0;

	closestEnemyDist2.resize( 3 );
	lastHeardPos.resize( 3 );
	nHeardShots.resize( 3 );
	lastRevealCenter.resize( 3 );
	
	lastShotTime.resize( 3 );
	lastRevealCircleTime.resize( 3 );
}
void CAntiArtillery::Scan( const CVec2 &center )
{
	memset( &(closestEnemyDist2[0]), 0, closestEnemyDist2.size() );
	
	for ( CUnitsIter<0,3> iter( nParty, EDI_ENEMY, center, fMaxRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		if ( closestEnemyDist2[(*iter)->GetParty()] == 0 ) 
		{
			const float fR2 = fabs2( center - (*iter)->GetCenter() );
			if ( fR2 <= sqr( fMaxRadius ) )
				closestEnemyDist2[ (*iter)->GetParty() ] = fR2;
		}
	}

	lastScan = curTime;
}
float GetRadius( const float nShots, const float fRevealRadius )
{
	float fMax = SConsts::MAX_ANTI_ARTILLERY_RADIUS *
								SConsts::ARTILLERY_REVEAL_COEEFICIENT / fRevealRadius;
	fMax = fMax < SConsts::MIN_ANTI_ARTILLERY_RADIUS ? SConsts::MIN_ANTI_ARTILLERY_RADIUS : fMax;
	
	return fMax - (fMax - SConsts::MIN_ANTI_ARTILLERY_RADIUS)/ SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS * nShots;
	/*return SConsts::ARTILLERY_REVEAL_COEEFICIENT/fRevealRadius * 
	(SConsts::MAX_ANTI_ARTILLERY_RADIUS - ( SConsts::MAX_ANTI_ARTILLERY_RADIUS - SConsts::MIN_ANTI_ARTILLERY_RADIUS ) / SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS * nShots); */
}
void CAntiArtillery::Fired( const float fGunRadius, const CVec2 &center )
{
	if ( curTime - lastScan >= SConsts::ANTI_ARTILLERY_SCAN_TIME )
		Scan( center );
	theAAManager.AddAA( this );

	const float fGunRadius2 = sqr( fGunRadius );
	for ( int i = 0; i < 2; ++i )
	{
		if ( i != nParty && closestEnemyDist2[i] <= fGunRadius2 && closestEnemyDist2[i] != 0.0f )
		{
			if ( fabs2( center - lastHeardPos[i] ) > sqr( SConsts::RELOCATION_RADIUS ) || i == 0 && curTime - lastShotTime[i] > SConsts::AUDIBILITY_TIME )
				nHeardShots[i] = 0;

			CVec2 newCenter;
			const float fCurRadius = GetRadius( Min( (float)nHeardShots[i], (float)SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS ), fGunRadius );

			if ( nHeardShots[i] > SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS )
				newCenter = lastRevealCenter[i];
			else
			{
				const float fOldRadius = GetRadius( nHeardShots[i] - 1, fGunRadius );

				if ( nHeardShots[i] == 0 || lastHeardPos[i] != center || fabs( center - lastRevealCenter[i] ) + fCurRadius <= fOldRadius - fCurRadius )
				{
					RandQuadrInCircle( fCurRadius, &newCenter );
					newCenter += center;
				}
				else if ( fabs( center - lastRevealCenter[i] ) + fOldRadius - fCurRadius <= fCurRadius )
				{
					RandQuadrInCircle( fOldRadius - fCurRadius, &newCenter );
					newCenter += lastRevealCenter[i];
				}
				else
				{
					CVec2 center2old = lastRevealCenter[i] - center;
					Normalize( &center2old );
					center2old.y = -center2old.y;

					CVec2 rotReveal( lastRevealCenter[i] - center );
					rotReveal ^= center2old;

					const float dRadius = fOldRadius - fCurRadius;

					const float r = fabs( rotReveal.x );
					NI_ASSERT_T( r != 0 , "Wrong distance between unit's center and reveal circle" );
					const float x = ( sqr(r) + sqr( fCurRadius ) - sqr( dRadius ) ) / ( 2 * r );

					float y = sqr( fCurRadius ) - sqr( x );
					if ( y < 0.1 )
						y = 0;
					else
						y = sqrt( y );

					const float minX = Max( -fCurRadius, r - dRadius );
					const float maxX = Min( fCurRadius, r + dRadius );
					
					if ( fabs( minX - maxX ) < 0.1 )
					{
						RandQuadrInCircle( fCurRadius, &newCenter );
						newCenter += center;
					}
					else if ( minX > maxX )
						newCenter = center;
					else
					{
						int cnt = 0;
						do
						{
							newCenter.x = RandomCheck( minX, maxX );
							newCenter.y = Random( -1.0f, 1.0f );
						} while ( ++cnt < 20 &&
											( sqr( newCenter.x )	   + sqr( newCenter.y ) >= sqr( fCurRadius ) ||
												sqr( newCenter.x - r ) + sqr( newCenter.y ) >= sqr( dRadius ) ) 
										);

						if ( cnt >= 20 )
						{
							newCenter.x = 0.5f * ( minX + maxX );
							newCenter.y = 0;
						}
						center2old.y = -center2old.y;
						newCenter ^= center2old;
						newCenter += center;
					}
				}
			}

			lastHeardPos[i] = center;
			lastRevealCenter[i] = newCenter;

			if ( nHeardShots[i] <= SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS )
				++nHeardShots[i];

			lastShotTime[i] = curTime;
		}
	}
}
const CCircle CAntiArtillery::GetRevealCircle( const int nParty ) const
{
	return CCircle( lastRevealCenter[nParty], 
									GetRadius( Min( (float)nHeardShots[nParty], (float)SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS ), fMaxRadius ) );
}
const NTimer::STime CAntiArtillery::GetLastHeardTime( const int nParty ) const
{
	return lastShotTime[nParty];
}
void CAntiArtillery::Segment( bool bOwnerVisible )
{
	const int nMyParty = theDipl.GetMyParty();	
	for ( int nIterParty = 0; nIterParty < 2; ++nIterParty )
	{
		const bool bEnemy = theDipl.GetDiplStatusForParties( nParty, nIterParty ) == EDI_ENEMY;
		const bool bHaveToSendCircle =
			lastShotTime[nIterParty] != 0 && curTime - lastShotTime[nIterParty] <= SConsts::AUDIBILITY_TIME && 
			curTime - lastRevealCircleTime[nIterParty] >= SConsts::REVEAL_CIRCLE_PERIOD;

		if ( bEnemy && bHaveToSendCircle )
		{
			lastRevealCircleTime[nIterParty] = curTime;

			CPtr<CRevealCircle> pCircle = new CRevealCircle( GetRevealCircle( 1 - nParty ) );
			
			if ( nMyParty == nIterParty && !bOwnerVisible )
			{
				if ( !theCheats.IsHistoryPlaying() )
					updater.Update( ACTION_NOTIFY_REVEAL_ARTILLERY, pCircle );
			}
		}
	}
}
