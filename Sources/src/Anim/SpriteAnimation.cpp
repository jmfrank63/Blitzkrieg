#include "StdAfx.h"

#include "SpriteAnimation.h"
bool CSpriteAnimation::Init( SSpriteAnimationFormat *_pAnimations ) 
{ 
	pAnimations = _pAnimations; 
	timescales.insert( timescales.begin(), pAnimations->animations.size(), 1.0f );
	return true; 
}
int CSpriteAnimation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pAnimations );
	saver.Add( 2, &nCurrAnim );
	saver.Add( 3, &nCurrDirection );
	saver.Add( 4, &fScale );
	saver.Add( 5, &nFrameIndex );
	saver.Add( 6, &timer );
	saver.Add( 7, &timescales );
	saver.Add( 9, &rect.rect );
	if ( saver.IsReading() )
		pAnimation = 0;
	return 0;
}
const bool CSpriteAnimation::IsHit( const CVec3 &relpos, const CVec2 &point, CVec2 *pShift ) const
{
	const CTRect<float> rcRect( relpos.x + rect.rect.x1, relpos.y + rect.rect.y1,
												      relpos.x + rect.rect.x2, relpos.y + rect.rect.y2 );
	const bool bRetVal = (point.x >= rcRect.x1) && (point.x <= rcRect.x2) && (point.y >= rcRect.y1) && (point.y <= rcRect.y2);
	if ( bRetVal && pShift )
	{
		pShift->x = relpos.x - point.x;
		pShift->y = relpos.y - point.y;
	}
	return bRetVal;
}
const bool CSpriteAnimation::IsHit( const CVec3 &relpos, const CTRect<float> &rcRect ) const
{
	return rcRect.IsInside( CTPoint<float>(relpos.x + (rect.rect.x1 + rect.rect.x2)*0.5f,
		                                     relpos.y + (rect.rect.y1 + rect.rect.y2)*0.5f) );
}
void CSpriteAnimation::Visit( IAnimVisitor *pVisitor )
{
	const SSpriteRect &srRect = GetRect();
	pVisitor->VisitSprite( &srRect );
}
int CComplexSprite::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pSprites );
	saver.Add( 2, &timer );
	saver.Add( 3, &nFrameIndex );
	return 0;
}
const bool CComplexSprite::IsHit( const CVec3 &relpos, const CVec2 &point, CVec2 *pShift ) const
{
	const bool bRetVal = GetSprite().IsInside( CVec2(point.x - relpos.x, point.y - relpos.y) );
	if ( bRetVal && pShift )
	{
		pShift->x = relpos.x - point.x;
		pShift->y = relpos.y - point.y;
	}
	return bRetVal;
}
const bool CComplexSprite::IsHit( const CVec3 &relpos, const CTRect<float> &rcRect ) const
{
	const CTRect<float> &rcBoundBox = GetSprite().GetBoundBox();
	return rcRect.IsInside( CTPoint<float>(relpos.x + (rcBoundBox.x1 + rcBoundBox.x2)*0.5f,
		                                     relpos.y + (rcBoundBox.y1 + rcBoundBox.y2)*0.5f) );
}
void CComplexSprite::Visit( IAnimVisitor *pVisitor )
{
	pVisitor->VisitSprite( &(GetSprite()) );
}
