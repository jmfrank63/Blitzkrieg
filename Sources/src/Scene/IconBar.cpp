#include "StdAfx.h"

#include "IconBar.h"
CIconBar::CIconBar() 
: vSize( 20, 5 ), vPos( 0, 0, 20 ), fPercentage( 1 ), bEnable( true ), bHorizontal( true )
{ 
	info.specular = 0xff000000; 
	info.color = 0xffffffff;
	info.pTexture = 0;
	info.maps.SetEmpty();
}
int CIconBar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vPos );
	saver.Add( 2, &vSize );
	saver.Add( 3, &bHorizontal );
	saver.Add( 4, &info.color );
	saver.Add( 5, &info.pos );
	saver.Add( 6, &info.rect );
	saver.Add( 7, &fPercentage );
	saver.Add( 8, &bEnable );
	return 0;
}
void CIconBar::CalcSpriteInfo()
{
	info.rect.x1 = -vSize.x/2.0f;
	info.rect.y1 = -vSize.y/2.0f;
	if ( bHorizontal )
	{
		info.rect.x2 = info.rect.x1 + vSize.x*fPercentage;
		info.rect.y2 = vSize.y/2.0f;
	}
	else
	{
		info.rect.x2 = vSize.x/2.0f;
		info.rect.y2 = info.rect.y1 + vSize.y*fPercentage;
	}
}
void CIconBar::SetSize( const CVec2 &_vSize, bool _bHorizontal ) 
{ 
	vSize = _vSize; 
	bHorizontal = _bHorizontal;
	CalcSpriteInfo();
}
void CIconBar::SetLength( float _fPercentage ) 
{ 
	fPercentage = _fPercentage; 
	CalcSpriteInfo();
}
bool CIconBar::Draw( IGFX *pGFX )
{
	return false;
}
void CIconBar::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bEnable )
		pVisitor->VisitSprite( &info, SGVOGT_ICON, 0 );
}
