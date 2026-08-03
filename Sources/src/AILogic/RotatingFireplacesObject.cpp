#include "StdAfx.h"

#include "RotatingFireplacesObject.h"
#include "Soldier.h"
#include "Guns.h"
extern NTimer::STime curTime;
void CRotatingFireplacesObject::AddUnit( CSoldier *pSoldier, const int nFireplace )
{
	std::list<SUnitInfo>::iterator iter = units.begin();
	while ( iter != units.end() && iter->pSoldier != pSoldier )
		++iter;

	if ( iter == units.end() )
	{
		units.push_back( SUnitInfo() );
		iter = units.end();
		--iter;
	}

	iter->pSoldier = pSoldier;
	
	if ( pSoldier->IsInFirePlace() )
	{
		iter->nLastFireplace = nFireplace;
		NI_ASSERT_T( iter->nLastFireplace < GetNFirePlaces(), NStr::Format( "Wrong number of fireplace (%d), number of fireplaces (%d)", iter->nLastFireplace, GetNFirePlaces() ) );
	}
	else
	{
		const int nFireplaces = GetNFirePlaces();
		if ( nFireplaces > 1 )
			iter->nLastFireplace = Random( 0, nFireplaces - 1 );
		else
			iter->nLastFireplace = 0;
	}

	iter->lastFireplaceChange = curTime + Random( 0, 20 * SConsts::AI_SEGMENT_DURATION );
}
void CRotatingFireplacesObject::DeleteUnit( CSoldier *pSoldier )
{
	std::list<SUnitInfo>::iterator iter = units.begin();
	while ( iter != units.end() && iter->pSoldier != pSoldier )
		++iter;


	if ( iter != units.end() )
		units.erase( iter );
}
bool CRotatingFireplacesObject::IsBetterToGoToFireplace( CSoldier *pSoldier, const int nFireplace ) const
{
	CSoldier *pFireplaceSoldier = GetSoldierInFireplace( nFireplace );

	if ( pFireplaceSoldier == pSoldier )
		return false;
	else if ( pFireplaceSoldier == 0 )
		return true;
	else if ( !pFireplaceSoldier->IsAlive() || pSoldier->IsInFirePlace() )
		return false;
	else if ( pSoldier->GetStats()->type != RPG_TYPE_OFFICER && pFireplaceSoldier->GetStats()->type == RPG_TYPE_OFFICER )
		return false;
	else if ( pSoldier->GetStats()->type == RPG_TYPE_OFFICER && pFireplaceSoldier->GetStats()->type != RPG_TYPE_OFFICER )
		return true;
	else
	{
		const int nSoldierMainAmmo = pSoldier->GetNAmmo( 0 );
		const int nFireplaceSoldierMainAmmo = pFireplaceSoldier->GetNAmmo( 0 );

		if ( nSoldierMainAmmo != 0 || nFireplaceSoldierMainAmmo != 0 )
		{
			if ( nSoldierMainAmmo == 0 )
				return false;
			if ( nFireplaceSoldierMainAmmo == 0 )
				return true;

			const float fSoldierFireRange = pSoldier->GetGun( 0 )->GetFireRange( 0 );
			const float fFireplaceSoldierFireRange = pFireplaceSoldier->GetGun( 0 )->GetFireRange( 0 );
			if ( fSoldierFireRange > fFireplaceSoldierFireRange )
				return true;
			if ( fSoldierFireRange < fFireplaceSoldierFireRange )
				return false;

			const float fSoldierDamageSpeed = pSoldier->GetGun( 0 )->GetFireRate() * pSoldier->GetGun( 0 )->GetDamage();
			const float fFireplaceSoldierDamageSpeed = pFireplaceSoldier->GetGun( 0 )->GetFireRate() * pFireplaceSoldier->GetGun( 0 )->GetDamage();

			return ( fSoldierDamageSpeed > fFireplaceSoldierDamageSpeed );
		}
		else
		{
			int nSoldierAmmo = 0;
			for ( int i = 0; i < pSoldier->GetNCommonGuns(); ++i )
				nSoldierAmmo += pSoldier->GetNAmmo( i );
			int nFireplaceSoldierAmmo = 0;
			for ( int i = 0; i < pFireplaceSoldier->GetNCommonGuns(); ++i )
				nFireplaceSoldierAmmo += pFireplaceSoldier->GetNAmmo( i );

			return ( nSoldierAmmo > nFireplaceSoldierAmmo );
		}
	}
}
void CRotatingFireplacesObject::Segment()
{
	if ( GetNFirePlaces() != 0 )
	{
		std::list<SUnitInfo>::iterator iter = units.begin();
		bool bChanged = false;
		while ( !bChanged && iter != units.end() )
		{
			CSoldier *pSoldier = iter->pSoldier;
			if ( pSoldier->IsAlive() )
			{
				if ( !pSoldier->IsInEntrenchment() )
				{
					bChanged = true;
					units.erase( iter );
				}
				else if ( pSoldier->IsAlive() )
				{
					if ( curTime >= iter->lastFireplaceChange && curTime - iter->lastFireplaceChange >= 3000 )
					{
						iter->lastFireplaceChange = curTime + Random( 0, 20 * SConsts::AI_SEGMENT_DURATION ) ;
						if ( CanRotateSoldier( pSoldier ) )
						{
							iter->nLastFireplace = ( iter->nLastFireplace + 1 ) % GetNFirePlaces();	

							if ( IsBetterToGoToFireplace( pSoldier, iter->nLastFireplace ) )
								ExchangeUnitToFireplace( pSoldier, iter->nLastFireplace );
							
							bChanged = true;
						}
					}
				}
			}

			if ( !bChanged )
				++iter;
		}
	}
}
