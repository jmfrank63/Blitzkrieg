#include "StdAfx.h"

#include "SpriteVisObj.h"
#include "AnimVisitor.h"
#include "SceneScreenScale.h"
#include "../Misc/Win32Random.h"
DWORD CSpriteVisObj::dwIdleData = 0;
CSpriteVisObj::CSpriteVisObj() 
{ 
	bComplexSprite = false;
	nViewTurn = 0;
	info.color = 0xffffffff; 
	info.specular = 0xff000000; 
	info2.color = 0xffffffff; 
	info2.specular = 0xff000000; 
	nNextIdle = 0;
}
bool CSpriteVisObj::Init( IGFXTexture *_pTexture, ISpriteAnimation *_pAnimation ) 
{ 
	pTexture = _pTexture; 
	pAnim = _pAnimation; 
	if ( dwIdleData == 0 ) 
		dwIdleData = PackDWORD( GetGlobalVar("Scene.InfantryIdle.Interval", 10000), GetGlobalVar("Scene.InfantryIdle.Random", 5000) );
	RetrieveSpriteInfo();
	return true; 
}
static CExtractAnimVisitor animVisitor;
void CSpriteVisObj::RetrieveSpriteInfo()
{
	pAnim->SetDirection( NCameraTurn::ViewDirection( GetDir(), nViewTurn ) );
	pAnim->Visit( &animVisitor );
	if ( const SSpriteRect *pSprite = animVisitor.GetSpriteRect() ) 
	{
		bComplexSprite = false;
		info.maps = pSprite->maps;
		info.rect = pSprite->rect;
		info.pos = GetPos();
		info.pTexture = pTexture;
		info.fDepthLeft = pSprite->fDepthLeft;
		info.fDepthRight = pSprite->fDepthRight;
	}
	else if ( const SSpritesPack::SSprite *pSprite = animVisitor.GetComplexSprite() )
	{
		bComplexSprite = true;
		info2.pSprite = pSprite;
		info2.pos = GetPos();
		info2.pTexture = pTexture;
	}
}
bool CSpriteVisObj::Update( const NTimer::STime &time, bool bForced )
{
	// A camera turn (Remastered) draws the facing that shows the unit or
	// effect as the turned view sees it (NCameraTurn::ViewDirection), at once
	// even while the game time stands still.
	const int nCameraTurn = NCameraTurn::Step();
	if ( dwLastUpdateTime != time || bForced || nCameraTurn != nViewTurn )
	{
		nViewTurn = nCameraTurn;
		pAnim->SetTime( time );
		RetrieveSpriteInfo();
		dwLastUpdateTime = time;
		if ( pAnim->GetAnimation() == 0 && nNextIdle <= time )
		{
			pAnim->SetStartTime( time );
			const int nIdleInterval = dwIdleData >> 16;
			const int nIdleRandom = dwIdleData & 0x0000ffff;
			nNextIdle = time + nIdleInterval + NWin32Random::Random( -nIdleRandom, nIdleRandom );
		}
	}
	return true;
}
bool CSpriteVisObj::IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift )
{
	CVec3 relpos;
	matTransform.RotateHVector( &relpos, GetPos() );
	CVec2 vHitPoint = point;
	NSceneScreenScale::UnscaleGameplaySpritePoint( &vHitPoint.x, &vHitPoint.y, relpos, GetSingleton<IGFX>()->GetScreenRect() );
	// A mirrored or flat-turned sprite (the Remastered camera turn) is hit where
	// it is drawn: the point is taken back into the 2003 picture's frame.
	if ( info.bMirror || info.nFlatTurn != 0 )
	{
		float fX = vHitPoint.x - relpos.x, fY = vHitPoint.y - relpos.y;
		info.UnturnOffset( &fX, &fY );
		vHitPoint.Set( relpos.x + fX, relpos.y + fY );
	}
	const bool bHit = pAnim->IsHit( relpos, vHitPoint, pShift );
	if ( bHit && pShift )
	{
		pShift->x = relpos.x - point.x;
		pShift->y = relpos.y - point.y;
	}
	return bHit;
}
bool CSpriteVisObj::IsHit( const SHMatrix &matTransform, const RECT &rect )
{
	CVec3 relpos;
	matTransform.RotateHVector( &relpos, GetPos() );
	// The picture is drawn scaled about relpos by the gameplay scale (a larger
	// window, a zoom step), so the rectangle is brought back into the
	// picture's own unscaled frame around the same point - the correction the
	// point pick above applies to its point. Unscaling is a positive scale
	// about one point, so the corners keep their order and "the picture's
	// centre inside the rectangle" answers the same as on screen.
	float fLeft = float( rect.left ), fTop = float( rect.top );
	float fRight = float( rect.right ), fBottom = float( rect.bottom );
	const CTRect<float> rcScreen = GetSingleton<IGFX>()->GetScreenRect();
	NSceneScreenScale::UnscaleGameplaySpritePoint( &fLeft, &fTop, relpos, rcScreen );
	NSceneScreenScale::UnscaleGameplaySpritePoint( &fRight, &fBottom, relpos, rcScreen );
	// Turned back as the point pick above; a mirror and quarter turns of the
	// ground keep a rectangle's sides along the screen's axes.
	if ( info.bMirror || info.nFlatTurn != 0 )
	{
		float fX1 = fLeft - relpos.x, fY1 = fTop - relpos.y, fX2 = fRight - relpos.x, fY2 = fBottom - relpos.y;
		info.UnturnOffset( &fX1, &fY1 );
		info.UnturnOffset( &fX2, &fY2 );
		fLeft = relpos.x + Min( fX1, fX2 );
		fRight = relpos.x + Max( fX1, fX2 );
		fTop = relpos.y + Min( fY1, fY2 );
		fBottom = relpos.y + Max( fY1, fY2 );
	}
	return pAnim->IsHit( relpos, CTRect<float>( fLeft, fTop, fRight, fBottom ) );
}
bool CSpriteVisObj::Draw( IGFX *pGFX )
{
	return false;
}
void CSpriteVisObj::SetScale( float sx, float, float )
{
	if ( pAnim )
	{
		pAnim->SetScale( sx );
		RetrieveSpriteInfo();
		RepositionIcons();
	}
}
void CSpriteVisObj::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bComplexSprite ) 
	{
		if ( info2.pSprite == 0 ) 
			RetrieveSpriteInfo();
		pVisitor->VisitSprite( &info2, GetGameType(nType), GetPriority() );
	}
	else
		pVisitor->VisitSprite( &info, GetGameType(nType), GetPriority() );
	VisitIcons( pVisitor );
}
void CSpriteVisObj::RepositionIcons()
{
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
}
void CSpriteVisObj::RepositionIconsLocal( DWORD placement )
{
	CTRect<float> rcRect;
	const int nFrameIndex = pAnim->GetFrameIndex();
	if ( nFrameIndex == -1 )
	{
		pAnim->SetFrameIndex( 0 );
		const SSpriteRect &rect = pAnim->GetRect();
		rcRect.Set( -7, -rect.rect.top, 7, -rect.rect.bottom );
		pAnim->SetFrameIndex( nFrameIndex );
	}
	else
	{
		const SSpriteRect &rect = pAnim->GetRect();
		rcRect.Set( -7, -rect.rect.top, 7, -rect.rect.bottom );
	}
	CObjVisObj::RepositionIconsLocal( placement, rcRect, 10 );
}
int CSpriteVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 20, static_cast<CObjVisObj*>(this) );
	saver.Add( 1, &pTexture );
	saver.Add( 2, &pAnim );
	saver.Add( 3, &info.rect );
	saver.Add( 4, &info.maps );
	saver.Add( 13, &bComplexSprite );
	if ( bComplexSprite ) 
	{
		saver.Add( 5, &info2.color );
		saver.Add( 6, &info2.specular );
		saver.Add( 7, &info2.relpos );
	}
	else
	{
		saver.Add( 5, &info.color );
		saver.Add( 6, &info.specular );
		saver.Add( 7, &info.relpos );
		saver.Add( 14, &info.fDepthLeft );
		saver.Add( 15, &info.fDepthRight );
	}
	saver.Add( 22, &nNextIdle );
	if ( saver.IsReading() )
	{
		info.pos = GetPos();
		info.pTexture = pTexture;
		info2.pos = GetPos();
		info2.pTexture = pTexture;
		if ( dwIdleData == 0 ) 
			dwIdleData = PackDWORD( GetGlobalVar("Scene.InfantryIdle.Interval", 10000), GetGlobalVar("Scene.InfantryIdle.Random", 5000) );
	}
	return 0;
}
