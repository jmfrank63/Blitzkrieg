#include "StdAfx.h"

#include "MOProjectile.h"

#include "..\Common\Actions.h"
#include "..\Common\Icons.h"
#include "..\GameTT\iMission.h"
#include "..\Formats\fmtTerrain.h"
CMOProjectile::CMOProjectile()
{
	wMoveSoundID = 0;
	timeLastTime = 0;
}
CMOProjectile::~CMOProjectile()
{
	if ( wMoveSoundID != 0 )
	{
		GetSingleton<IScene>()->RemoveSound( wMoveSoundID );
		wMoveSoundID = 0;
	}
}
bool CMOProjectile::Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, 
													  float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	NI_ASSERT_T( false, "Can't create projectile from DBID - u must create it from the name" );
	return false;
}
bool CMOProjectile::Create( IRefCount *pAIObjLocal, const char *pszName, interface IVisObjBuilder *pVOB )
{
	pVisObj = pVOB->BuildObject( pszName, 0, SGVOT_EFFECT );
	pAIObj = pAIObjLocal;
	return pVisObj != 0;
}
void CMOProjectile::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitEffect( pVisObj, SGVOGT_EFFECT, SGVOT_EFFECT );
}
void CMOProjectile::Init( const NTimer::STime &_timeStart, const NTimer::STime &_timeDuration, const CVec3 &_delta )
{
	fTimeStart = _timeStart;
	fTimeDuration = _timeDuration;
	delta = _delta;
	timeLastTime = _timeStart;
	GetVisObj()->SetStartTime( _timeStart );
	GetVisObj()->CalibrateDuration( _timeDuration );
}
void CMOProjectile::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	pVisObj->SetPlacement( vPos, wDir );
	if ( pVisObj && (wMoveSoundID == 0) ) 
	{
		const std::string &szSoundName = static_cast_ptr<IEffectVisObj*>(pVisObj)->GetSoundEffect();
		if ( !szSoundName.empty() ) 
		{
			wMoveSoundID = GetSingleton<IScene>()->AddSound( szSoundName.c_str(), vPos, 
																											 SFX_MIX_IF_TIME_EQUALS, SAM_NEED_ID, ESCT_GENERIC, 1, 100 );
		}
	}
	vLastPos = vPos;
}
void CMOProjectile::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition();
	*pwDir = pVisObj->GetDirection();
}
void CMOProjectile::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	const float dt = float( currTime - fTimeStart );
	if ( dt > fTimeDuration )
		return;
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	for ( NTimer::STime time = timeLastTime; time < currTime; time += 20 )
	{
		const float dt = float( time - fTimeStart );
		CVec3 vTempPos = vLastPos + ( vPos - vLastPos ) * float( time - timeLastTime ) / float( currTime - timeLastTime );
		vTempPos += ( 1 - dt/fTimeDuration )*delta;
		pVisObj->SetPosition( vTempPos );
		pVisObj->Update( time, true );
	}
	timeLastTime = currTime;
	vLastPos = vPos;
	vPos += ( 1 - dt/fTimeDuration )*delta;
	pVisObj->SetDirection( placement.dir );
	pScene->MoveObject( pVisObj, vPos );
	pVisObj->Update( currTime, true );
	if ( wMoveSoundID != 0 )
		pScene->SetSoundPos( wMoveSoundID, vPos );
}
int CMOProjectile::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &fTimeStart );
	saver.Add( 3, &fTimeDuration );
	saver.Add( 4, &delta );
	saver.Add( 5, &wMoveSoundID );
	saver.Add( 6, &vLastPos );
	saver.Add( 7, &timeLastTime );
	return 0;
}
int CMOProjectile::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager ) 
{ 
	int nRetVal = 0;
	switch ( action.typeID )
	{
		case ACTION_NOTIFY_CHANGE_VISIBILITY:
			GetVisObj()->SetSuspendedState( !bool(action.nParam) );
			break;
		default:
			break;
	}
	return nRetVal;
}
