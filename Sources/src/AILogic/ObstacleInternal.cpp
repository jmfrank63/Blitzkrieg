#include "stdafx.h"

#include "ObstacleInternal.h"
#include "CommonUnit.h"
#include "StaticObject.h"
#include "GroupLogic.h"
extern CGroupLogic theGroupLogic;
BASIC_REGISTER_CLASS( IObstacle );
CBasicGun* CObstacleStaticObject::ChooseGunToShootToSelf( CCommonUnit *pUnit, NTimer::STime *pTime )
{
	return pUnit->ChooseGunForStatObj( pObj, pTime );
}
int CObstacleStaticObject::GetPlayer() const
{
	return pObj->GetPlayer();
}
float CObstacleStaticObject::GetHPPercent() const
{
	return pObj->GetHitPoints() / pObj->GetStats()->fMaxHP;
}
const CVec2 CObstacleStaticObject::GetCenter() const
{
	return pObj->GetCenter();
}
bool CObstacleStaticObject::IsAlive() const 
{ 
	return IsValidObj( pObj );
}
void CObstacleStaticObject::IssueUnitAttackCommand( CCommonUnit *pUnit )
{
	theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_ATTACK_OBJECT, pObj), pUnit );
}
bool CObstacleStaticObject::CanDeleteByMovingOver( CAIUnit *pUnit )
{
	return false;
}
interface IUpdatableObj * CObstacleStaticObject::GetObject() const 
{ 
	return pObj.GetPtr(); 
}