#include "StdAfx.h"

#include "SpriteVisObj.h"
#include "AnimVisitor.h"
#include "..\Misc\Win32Random.h"
DWORD CSpriteVisObj::dwIdleData = 0;
CSpriteVisObj::CSpriteVisObj() 
{ 
	bComplexSprite = false;
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
	if ( dwLastUpdateTime != time || bForced )
	{
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
	return pAnim->IsHit( relpos, point, pShift );
}
bool CSpriteVisObj::IsHit( const SHMatrix &matTransform, const RECT &rect )
{
	CVec3 relpos;
	matTransform.RotateHVector( &relpos, GetPos() );
	return pAnim->IsHit( relpos, rect );
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
