#include "StdAfx.h"

#include "MOBridge.h"

#include "../GameTT/iMission.h"
#include "Actions.h"
#include "PlayEffect.h"
#include "../Formats/fmtTerrain.h"
inline int GetBridgeDamageState( float fHP )
{
	if ( fHP > 0.5f )
		return 0;
	else if ( fHP > 0.0f )
		return 1;
	else
		return 2;
}
bool CMOBridgeSpan::Create( IRefCount *_pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	const SBridgeRPGStats *pRPG = NGDB::GetRPGStats<SBridgeRPGStats>( pGDB, pDesc );
	if ( pRPG->IsBeginIndex(nFrameIndex) ) 
		nSpanType = 0;
	else if ( pRPG->IsLineIndex(nFrameIndex) ) 
		nSpanType = 1;
	else if ( pRPG->IsEndIndex(nFrameIndex) ) 
		nSpanType = 2;
	const SBridgeRPGStats::SSpan &span = pRPG->GetSpanStats( nFrameIndex );
	pSlab = CreateObject<SMapObject>( MISSION_MO_OBJECT );
	if ( pSlab->Create(0, pDesc, nSeason, span.nSlab, fHP, pVOB, pGDB) == false ) 
		pSlab = 0;
	if ( pSlab && pSlab->pVisObj ) 
		checked_cast<IObjVisObj*>(pSlab->pVisObj.GetPtr())->SetPriority( 90 );

	pBackGirder = CreateObject<SMapObject>( MISSION_MO_OBJECT );
	if ( pBackGirder->Create(0, pDesc, nSeason, span.nBackGirder, fHP, pVOB, pGDB) == false )
		pBackGirder = 0;

	pFrontGirder = CreateObject<SMapObject>( MISSION_MO_OBJECT );
	if ( pFrontGirder->Create(0, pDesc, nSeason, span.nFrontGirder, fHP, pVOB, pGDB) == false )
		pFrontGirder = 0;
	pAIObj = _pAIObj;
	nIndex = nFrameIndex;
	return pSlab != 0;
}
int CMOBridgeSpan::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SBridgeSpanObject*>(this) );
	saver.Add( 2, &nSpanType );
	return 0;
}
int CMOBridgeSpan::GetSpanStatsIndex( const int nDamageState ) const
{
	const SBridgeRPGStats *pRPGStats = static_cast<const SBridgeRPGStats *>( GetRPG() );
	switch ( nSpanType ) 
	{
		case 0: return pRPGStats->GetRandomBeginIndex( -1, nDamageState );
		case 1: return pRPGStats->GetRandomLineIndex( -1, nDamageState );
		case 2: return pRPGStats->GetRandomEndIndex( -1, nDamageState );
	}
	return -1;
}
const SBridgeRPGStats::SSpan& CMOBridgeSpan::GetSpanStats( const int nDamageState ) const
{
	const SBridgeRPGStats *pRPGStats = static_cast<const SBridgeRPGStats *>( GetRPG() );
	return pRPGStats->GetSpanStats( GetSpanStatsIndex(nDamageState), nDamageState );
}
void CMOBridgeSpan::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	const SBridgeRPGStats *pRPGStats = static_cast<const SBridgeRPGStats *>( GetRPG() );
	const SBridgeRPGStats::SSpan &span = pRPGStats->GetSpanStats( nIndex, GetBridgeDamageState(pSlab->fHP) );
	pSlab->SetPlacement( vPos, wDir );
	if ( pBackGirder )
		pBackGirder->SetPlacement( vPos + pRPGStats->GetSegmentStats(span.nBackGirder).vRelPos, wDir );
	if ( pFrontGirder )
		pFrontGirder->SetPlacement( vPos + pRPGStats->GetSegmentStats(span.nFrontGirder).vRelPos, wDir );
}
void CMOBridgeSpan::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	if ( pSlab ) 
		pSlab->GetPlacement( pvPos, pwDir );
}
const SGDBObjectDesc* CMOBridgeSpan::GetDesc() const { return pSlab->GetDesc(); }
const SHPObjectRPGStats* CMOBridgeSpan::GetRPG() const { return pSlab->GetRPG(); }
IRefCount* CMOBridgeSpan::GetAIObj() { return pAIObj; }
IRefCount* CMOBridgeSpan::GetParentAIObj() { return 0; }
bool CMOBridgeSpan::CanSelect() const { return false; }
void CMOBridgeSpan::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pSlab->GetStatus( pStatus );
}
void CMOBridgeSpan::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	pSlab->GetActions( pActions, eActions );
}
void CMOBridgeSpan::AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	pSlab->AIUpdatePlacement( placement, currTime, pScene );
	if ( pBackGirder ) 
		pBackGirder->AIUpdatePlacement( placement, currTime, pScene );
	if ( pFrontGirder ) 
		pFrontGirder->AIUpdatePlacement( placement, currTime, pScene );
}
template <class TAnim>
TAnim* GetAnim( IVisObj *pVisObj )
{
	return static_cast<TAnim*>( static_cast<IObjVisObj*>(pVisObj)->GetAnimation() );
}
void ChangeModel( SMapObject *pMO, const SGDBObjectDesc *pDesc, const SBridgeRPGStats::SSegmentRPGStats &segment, 
								  const float fNewHP, const NTimer::STime &currTime, IVisObjBuilder *pVOB )
{
	if ( pMO == 0 ) 
		return;
	const std::string szPartName = pDesc->szPath + "\\" + segment.szModel;
	pVOB->ChangeObject( pMO->pVisObj, szPartName.c_str(), 0, SGVOT_SPRITE );
	GetAnim<ISpriteAnimation>(pMO->pVisObj)->SetFrameIndex( segment.nFrameIndex );
	if ( fNewHP == 1 ) 
		pMO->pVisObj->SetOpacity( 0xff );
	pMO->pVisObj->Update( currTime, true );
	if ( pMO->pShadow ) 
	{
		if ( pVOB->ChangeObject(pMO->pShadow , (szPartName + "s").c_str(), 0, SGVOT_SPRITE) != false )
		{
			GetAnim<ISpriteAnimation>(pMO->pShadow)->SetFrameIndex( segment.nFrameIndex );
			pMO->pShadow->Update( currTime, true );
		}
		else
		{
			GetSingleton<IScene>()->RemoveObject( pMO->pShadow );
			pMO->pShadow = 0;
		}
	}
}
void CMOBridgeSpan::UpdateModelWithHP( const float fNewHP, const NTimer::STime &currTime, IVisObjBuilder *pVOB ) const
{
	const int nOldState = GetBridgeDamageState( pSlab->fHP );
	const int nNewState = GetBridgeDamageState( fNewHP );
	if ( nNewState != nOldState )
	{
		const SBridgeRPGStats *pRPGStats = static_cast<const SBridgeRPGStats *>( GetRPG() );
		const SBridgeRPGStats::SSpan &span = GetSpanStats( nNewState );
		ChangeModel( pSlab, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nSlab), fNewHP, currTime, pVOB );
		ChangeModel( pBackGirder, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nBackGirder), fNewHP, currTime, pVOB );
		ChangeModel( pFrontGirder, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nFrontGirder), fNewHP, currTime, pVOB );
	}
	pSlab->fHP = fNewHP;
}
bool CMOBridgeSpan::AIUpdateRPGStats( const struct SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene *pScene )
{
	UpdateModelWithHP( stats.fHitPoints / GetRPG()->fMaxHP, GetSingleton<IGameTimer>()->GetGameTime(), pVOB );
	return true;
}
void CMOBridgeSpan::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( const std::string *pEffectName = GetHitEffect(hit, hit.pWeapon->shells[hit.wShell]) ) 
	{
		CVec3 vPos = hit.explCoord;
		AI2Vis( &vPos );
		PlayEffect( *pEffectName, vPos, currTime, false, pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
	}
}
void CMOBridgeSpan::Visit( IMapObjVisitor *pVisitor )
{
	pSlab->Visit( pVisitor );
	if ( pBackGirder ) 
		pBackGirder->Visit( pVisitor );
	if ( pFrontGirder ) 
		pFrontGirder->Visit( pVisitor );
}
