#include "StdAfx.h"

#include "CombatEstimator.h"
#include "AIUnit.h"
CCombatEstimator theCombatEstimator;
extern NTimer::STime curTime;
int CCombatEstimator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fDamage );
	saver.Add( 5, &registeredMechUnits );
	saver.Add( 3, &shellTimes );
	saver.Add( 6, &registeredInfantry );
	return 0;
}
CCombatEstimator::CCombatEstimator()
: fDamage( 0 )
{
}
void CCombatEstimator::AddShell( NTimer::STime time, float _fDamage )
{
	fDamage += _fDamage;
	shellTimes.push_back( SShellInfo(time + SConsts::DIRECT_HIT_TIME_COMBAT_SITUATION,_fDamage) );
}
void CCombatEstimator::Segment()
{
	for ( CShellTimes::iterator it = shellTimes.begin(); 
				it != shellTimes.end() && curTime > it->time;  )
	{
		fDamage -= it->fDamage;
		it = shellTimes.erase( it );
	}
}
void CCombatEstimator::Clear()
{
	fDamage = 0.0f;
	shellTimes.clear();
	registeredInfantry.clear();
	registeredMechUnits.clear();
}
bool CCombatEstimator::IsCombatSituation() const 
{ 
	const int nMechs = registeredMechUnits.size();
	const int nInfantry = registeredInfantry.size();
	return fDamage > SConsts::DIRECT_HIT_DAMAGE_COMBAT_SITUATION ||
			registeredMechUnits.size() > SConsts::NUMBER_ENEMY_MECH_MOVING_TO_COMBAT_SITUATION ||
			registeredInfantry.size() > SConsts::NUMBER_ENEMY_INFANTRY_MOVING_TO_COMBAT_SITUATION;

} 
void CCombatEstimator::AddUnit( CAIUnit *pUnit )
{
	if ( pUnit->GetStats()->IsInfantry() )
		registeredInfantry.insert( pUnit->GetUniqueId() );
	else
		registeredMechUnits.insert( pUnit->GetUniqueId() );
}
void CCombatEstimator::DelUnit( CAIUnit *pUnit )
{
	if ( pUnit->GetStats()->IsInfantry() )
		registeredInfantry.erase( pUnit->GetUniqueId() );
	else
		registeredMechUnits.erase( pUnit->GetUniqueId() );
}
