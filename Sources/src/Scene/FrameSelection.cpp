#include "StdAfx.h"

#include "FrameSelection.h"
#include "SceneScreenScale.h"
bool CFrameSelection::Draw( IGFX *pGFX )
{
	static CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	const SHMatrix &matTransform = mstack();
	CVec3 vScrBegin, vScrEnd;
	matTransform.RotateHVector( &vScrBegin, vBegin );
	matTransform.RotateHVector( &vScrEnd, vEnd );
	mstack.Pop( 3 );
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	NSceneScreenScale::ScaleGameplayScreenPoint( &vScrBegin.x, &vScrBegin.y, rcScreen );
	NSceneScreenScale::ScaleGameplayScreenPoint( &vScrEnd.x, &vScrEnd.y, rcScreen );
	pGFX->SetTexture( 0, 0 );
	pGFX->SetShadingEffect( 3 );
	SGFXRect2 rect;
	rect.rect.Set( vScrBegin.x, vScrBegin.y, vScrEnd.x, vScrEnd.y );
	rect.rect.Normalize();
	rect.maps.SetEmpty();
	rect.color = 0x50000000;
	pGFX->DrawRects( &rect, 1, true );
	rect.color = 0xffffffff;
	pGFX->DrawRects( &rect, 1, false );
	
	return true;
}
void CFrameSelection::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
