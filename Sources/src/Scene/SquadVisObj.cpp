#include "StdAfx.h"

#include "SquadVisObj.h"
static const int ICON_SIZE = 32;
static const float HALF_PIXEL = 0.5f / float( ICON_SIZE );
CSquadVisObj::CSquadVisObj()
{
	vPos = VNULL2;
	bSelected = false;
}
int CSquadVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &units );
	saver.Add( 2, &pIcon );
	saver.Add( 3, &vPos );
	saver.Add( 4, &bSelected );
	return 0;
}
bool CSquadVisObj::UpdateData( ISquadVisObj::SData *pObjects, int nNumObjects )
{
	units.resize( nNumObjects );
	if ( nNumObjects ) 
		memcpy( &(units[0]), pObjects, nNumObjects*sizeof(ISquadVisObj::SData) );
	return nNumObjects != 0;
}
void CSquadVisObj::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
bool CSquadVisObj::Draw( interface IGFX *pGFX )
{
	SGFXRect2 rect;
	rect.rect.Set( vPos.x, vPos.y, vPos.x + ICON_SIZE, vPos.y + ICON_SIZE );
	rect.maps.Set( HALF_PIXEL, HALF_PIXEL, 1.0f + HALF_PIXEL, 1.0f + HALF_PIXEL );
	rect.fZ = 0;
	pGFX->SetTexture( 0, pIcon );
	pGFX->DrawRects( &rect, 1 );
	return true;
}
