#include "StdAfx.h"

#include "MOBridge.h"

#include "../GameTT/iMission.h"
#include "Actions.h"
#include "PlayEffect.h"
#include "../Formats/fmtTerrain.h"
#include "../Main/CameraTurn.h"
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
	nOwnSpan = nFrameIndex;
	nCameraTurn = NCameraTurn::Step();
	return pSlab != 0;
}
int CMOBridgeSpan::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SBridgeSpanObject*>(this) );
	saver.Add( 2, &nSpanType );
	// A span saved at a turned step: at the 2003 view nothing new is written.
	if ( saver.IsReading() || nCameraTurn != 0 )
	{
		saver.Add( 3, &nCameraTurn );
		saver.Add( 4, &nOwnSpan );
		saver.Add( 5, &vPlacement );
		saver.Add( 6, &wPlacementDir );
	}
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
	vPlacement = vPos;
	wPlacementDir = wDir;
	const SBridgeRPGStats *pRPGStats = static_cast<const SBridgeRPGStats *>( GetRPG() );
	const SBridgeRPGStats::SSpan &span = pRPGStats->GetSpanStats( nIndex, GetBridgeDamageState(pSlab->fHP) );
	pSlab->SetPlacement( vPos, wDir );
	if ( pBackGirder )
		pBackGirder->SetPlacement( vPos + pRPGStats->GetSegmentStats(span.nBackGirder).vRelPos, wDir );
	if ( pFrontGirder )
		pFrontGirder->SetPlacement( vPos + pRPGStats->GetSegmentStats(span.nFrontGirder).vRelPos, wDir );
	if ( nCameraTurn != 0 )
		ApplyCameraTurn( GetSingleton<IGameTimer>()->GetGameTime(), GetSingleton<IVisObjBuilder>() );
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
		nOwnSpan = GetSpanStatsIndex( nNewState );
		const SBridgeRPGStats::SSpan &span = pRPGStats->GetSpanStats( nOwnSpan, nNewState );
		ChangeModel( pSlab, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nSlab), fNewHP, currTime, pVOB );
		ChangeModel( pBackGirder, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nBackGirder), fNewHP, currTime, pVOB );
		ChangeModel( pFrontGirder, pSlab->GetDesc(), pRPGStats->GetSegmentStats(span.nFrontGirder), fNewHP, currTime, pVOB );
	}
	pSlab->fHP = fNewHP;
	if ( nCameraTurn != 0 && nNewState != nOldState )
		ApplyCameraTurn( currTime, pVOB );
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
// The footprint of a span's slab about the span's point (AI units, a tile
// is 32), turned by nStep quarter turns (x, y) -> (-y, x).
static CTRect<float> SpanFootprint( const SBridgeRPGStats *pRPG, const SBridgeRPGStats::SSpan &span, const int nStep )
{
	const SBridgeRPGStats::SSegmentRPGStats &slab = pRPG->GetSegmentStats( span.nSlab );
	CVec2 a( -slab.vOrigin.x, -slab.vOrigin.y );
	CVec2 b( slab.passability.GetSizeX() * 32.0f - slab.vOrigin.x, slab.passability.GetSizeY() * 32.0f - slab.vOrigin.y );
	for ( int k = 0; k < nStep; ++k )
	{
		a = CVec2( -a.y, a.x );
		b = CVec2( -b.y, b.x );
	}
	return CTRect<float>( Min( a.x, b.x ), Min( a.y, b.y ), Max( a.x, b.x ), Max( a.y, b.y ) );
}
static float Overlap( const CTRect<float> &a, const CTRect<float> &b )
{
	const float fWidth = Min( a.x2, b.x2 ) - Max( a.x1, b.x1 ), fHeight = Min( a.y2, b.y2 ) - Max( a.y1, b.y1 );
	if ( fWidth <= 0 || fHeight <= 0 )
		return 0;
	const float fBoth = fWidth * fHeight;
	return fBoth / ( a.Width() * a.Height() + b.Width() * b.Height() - fBoth );
}
// One part of the span drawn as the segment of the bridge kind at pDesc, at
// vPos; where that segment has no shadow the part's shadow is hidden.
static void DrawSegment( SMapObject *pMO, const SGDBObjectDesc *pDesc, const SBridgeRPGStats::SSegmentRPGStats &segment,
												 const CVec3 &vPos, const WORD wDir, const NTimer::STime &currTime, IVisObjBuilder *pVOB )
{
	if ( pMO == 0 )
		return;
	// Placed first: a sprite pack keeps the place it had when its picture was
	// last taken (Update), and the scene does not update a static object.
	pMO->SetPlacement( vPos, wDir );
	const std::string szPartName = pDesc->szPath + "\\" + segment.szModel;
	pVOB->ChangeObject( pMO->pVisObj, szPartName.c_str(), 0, SGVOT_SPRITE );
	GetAnim<ISpriteAnimation>( pMO->pVisObj )->SetFrameIndex( segment.nFrameIndex );
	pMO->pVisObj->Update( currTime, true );
	if ( pMO->pShadow )
	{
		const bool bShadow = pVOB->ChangeObject( pMO->pShadow, (szPartName + "s").c_str(), 0, SGVOT_SPRITE );
		if ( bShadow )
		{
			GetAnim<ISpriteAnimation>( pMO->pShadow )->SetFrameIndex( segment.nFrameIndex );
			pMO->pShadow->Update( currTime, true );
		}
		pMO->pShadow->SetOpacity( bShadow ? 0xff : 0 );
	}
}
// Draws the span for the camera step (Remastered, docs/superpowers/specs/
// 2026-10-26-camera-turn-design.md, S04) with the 2003 pictures of the span
// that shows it turned: at an odd step one of the bridge kind's variant along
// the other axis (`_01` runs along x, `_02` along y), at the half turn one of
// its own, the span whose footprint best covers its own turned by the step (a
// line span exactly; a begin or an end span the other variant's or other end's,
// which may be a tile longer or narrower). Its back girder, slab and front
// girder stand at that span's places turned back by the step, so the girder
// that is far now is drawn behind the slab. Where no span covers half of it,
// the span keeps its 2003 pictures at its turned place. At step 0 it is drawn
// as in 2003.
void CMOBridgeSpan::ApplyCameraTurn( const NTimer::STime &currTime, IVisObjBuilder *pVOB ) const
{
	const SGDBObjectDesc *pDesc = GetDesc();
	const SBridgeRPGStats *pRPG = static_cast<const SBridgeRPGStats*>( GetRPG() );
	const int nState = GetBridgeDamageState( pSlab->fHP );
	int nSpan = nOwnSpan;
	if ( nCameraTurn != 0 )
	{
		const CTRect<float> rcOwn = SpanFootprint( pRPG, pRPG->GetSpanStats( nOwnSpan, nState ), nCameraTurn );
		const SGDBObjectDesc *pTurnedDesc = pDesc;
		const int nLength = pDesc->szKey.size();
		if ( ( nCameraTurn & 1 ) != 0 )
		{
			pTurnedDesc = 0;
			if ( nLength > 3 && pDesc->szKey.compare( nLength - 3, 2, "_0" ) == 0 && ( pDesc->szKey[nLength - 1] == '1' || pDesc->szKey[nLength - 1] == '2' ) )
				pTurnedDesc = GetSingleton<IObjectsDB>()->GetDesc( ( pDesc->szKey.substr( 0, nLength - 1 ) + ( pDesc->szKey[nLength - 1] == '1' ? "2" : "1" ) ).c_str() );
		}
		const SBridgeRPGStats *pTurnedRPG = pTurnedDesc ? NGDB::GetRPGStats<SBridgeRPGStats>( GetSingleton<IObjectsDB>(), pTurnedDesc ) : 0;
		float fBest = 0.5f;
		for ( int i = 0; pTurnedRPG && i < pTurnedRPG->states[nState].spans.size(); ++i )
		{
			const float fOverlap = Overlap( rcOwn, SpanFootprint( pTurnedRPG, pTurnedRPG->states[nState].spans[i], 0 ) );
			if ( fOverlap > fBest )
			{
				fBest = fOverlap;
				pDesc = pTurnedDesc;
				pRPG = pTurnedRPG;
				nSpan = i;
			}
		}
	}
	const SBridgeRPGStats::SSpan &span = pRPG->GetSpanStats( nSpan, nState );
	const SMapObject *parts[3] = { pSlab, pBackGirder, pFrontGirder };
	const int segments[3] = { span.nSlab, span.nBackGirder, span.nFrontGirder };
	for ( int i = 0; i < 3; ++i )
	{
		const SBridgeRPGStats::SSegmentRPGStats &segment = pRPG->GetSegmentStats( segments[i] );
		DrawSegment( const_cast<SMapObject*>( parts[i] ), pDesc, segment, vPlacement + NCameraTurn::ViewOffset( segment.vRelPos, nCameraTurn ), wPlacementDir, currTime, pVOB );
	}
}
void CMOBridgeSpan::SetCameraTurn( const int nTurn, IVisObjBuilder *pVOB )
{
	if ( nTurn == nCameraTurn )
		return;
	nCameraTurn = nTurn;
	ApplyCameraTurn( GetSingleton<IGameTimer>()->GetGameTime(), pVOB );
}
