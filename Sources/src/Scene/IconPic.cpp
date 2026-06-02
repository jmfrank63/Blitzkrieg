#include "StdAfx.h"

#include "IconPic.h"
CIconPic::CIconPic()
: bEnable( true )
{
	info.specular = 0xff000000;
	info.color = 0xffffffff;
}
int CIconPic::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vPos );
	saver.Add( 2, &pTexture );
	saver.Add( 3, &bEnable );
	saver.Add( 4, &info.pos );
	saver.Add( 5, &info.rect );
	saver.Add( 6, &info.maps );
	saver.Add( 7, &info.color );
	if ( saver.IsReading() )
		info.pTexture = pTexture;
	return 0;
}
bool CIconPic::Draw( IGFX *pGFX )
{
	return false;
}
void CIconPic::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bEnable || (nType == 0x80000000) )
		pVisitor->VisitSprite( &info, SGVOGT_ICON, 0 );
}
