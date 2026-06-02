#include "StdAfx.h"

#include "IconText.h"
int CIconText::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pFont );
	saver.Add( 2, &szText );
	saver.Add( 3, &vPos );
	saver.Add( 4, &vAbsPos );
	saver.Add( 5, &color );
	saver.Add( 6, &bEnable );
	return 0;
}
bool CIconText::Draw( IGFX *pGFX )
{
	return false;
}
void CIconText::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bEnable )
		pVisitor->VisitText( vAbsPos, szText.c_str(), pFont, color );
}
