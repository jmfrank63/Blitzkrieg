#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "MOEntrenchment.h"
#include "StdAfx.h"

#include "MOObject.h"

#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../GameTT/iMission.h"
#include "../Formats/fmtTerrain.h"
#include "Season.h"
bool CMOEntrenchmentSegment::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int nSeason, int nFrameIndex, 
																	   float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	pDesc = pDescLocal;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	const SEntrenchmentRPGStats::SSegmentRPGStats &segment = GetRPGStats()->GetSegmentStats( nFrameIndex );
	const std::string szTextureName = NStr::Format( "\\1%s", GetSeasonApp2(nSeason) );
	pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel).c_str(), (pDesc->szPath + szTextureName).c_str(), pDesc->eVisType );
	NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create object \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	pAIObj = pAIObjLocal;
	UpdateModelWithHP( fNewHP / pRPG->fMaxHP, pVOB );
	fHP = fNewHP / pRPG->fMaxHP;
	return true;
}
void CMOEntrenchmentSegment::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitMesh( pVisObj, pDesc->eGameType, pDesc->eVisType );
}
int CMOEntrenchmentSegment::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &pLocalName );
	return 0;
}
void CMOEntrenchmentSegment::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	pVisObj->SetPlacement( vPos, wDir );
}
void CMOEntrenchmentSegment::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition();
	*pwDir = pVisObj->GetDirection();
}
void CMOEntrenchmentSegment::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pStatus->nScenarioIndex = -1;
	pStatus->dwIconsStatus = 0;
	pStatus->dwPlayer = -1;
	pStatus->params[0] = PackParams( MINT(fHP * pRPG->fMaxHP), MINT(pRPG->fMaxHP) );
	pStatus->params[1] = 0;
	pStatus->params[2] = 0;
	pStatus->params[3] = 0;
	if ( IText *pName = GetLocalName() ) 
		NPlatform::CopyWordStringToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), pName->GetString() );
	else
	{
		static std::wstring szName;
		NStr::ToUnicode( &szName, pDesc->szKey );
		NPlatform::CopyWideToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), szName.c_str() );
	}
	Zero( pStatus->weaponstats );
	Zero( pStatus->armors );
}
void CMOEntrenchmentSegment::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	if ( eActions == IMapObj::ACTIONS_WITH ) 
	{
		pActions->SetAction( USER_ACTION_BOARD );
		pActions->SetAction( USER_ACTION_MOVE );
		pActions->SetAction( USER_ACTION_ATTACK );
		pActions->SetAction( USER_ACTION_UNKNOWN );
	}
}
void CMOEntrenchmentSegment::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	pVisObj->SetDirection( placement.dir );
	pScene->MoveObject( pVisObj, vPos );
	pVisObj->Update( currTime, true );
}
void CMOEntrenchmentSegment::UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB )
{
}
bool CMOEntrenchmentSegment::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene )
{
	const float fNewHP = stats.fHitPoints / pRPG->fMaxHP;
	UpdateModelWithHP( fNewHP, pVOB );
	fHP = fNewHP;
	return fNewHP > 0;
}
void CMOEntrenchmentSegment::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
}
