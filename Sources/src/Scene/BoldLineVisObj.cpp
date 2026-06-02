#include "StdAfx.h"

#include "BoldLineVisObj.h"
CBoldLineVisObj::CBoldLineVisObj()
{
	vStart = VNULL3;
	vEnd = VNULL3;
	fWidth = 5;
	color = 0xffff0000;
	bSetuped = false;
}
int CBoldLineVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vStart );
	saver.Add( 2, &vEnd );
	saver.Add( 3, &fWidth );
	saver.Add( 4, &color );
	saver.Add( 5, &bSetuped );
	saver.Add( 6, &corners );
	return 0;
}
void CBoldLineVisObj::SetupLocal()
{
	if ( !IsSetuped() )
		return;
	CVec3 vNorm;
	GetLineEq( vStart.x, vStart.y, vEnd.x, vEnd.y, &vNorm.x, &vNorm.y, &vNorm.z );
	vNorm.z = 0;
	vNorm *= fWidth / 2;
	corners[0] = vStart + vNorm;
	corners[1] = vStart - vNorm;
	corners[2] = vEnd - vNorm;
	corners[3] = vEnd + vNorm;
}
void CBoldLineVisObj::Setup( const CVec3 &_vStart, const CVec3 &_vEnd, float _fWidth, DWORD _color )
{
	vStart = _vStart;
	vEnd = _vEnd;
	fWidth = _fWidth;
	color = _color;

	bSetuped = true;
	SetupLocal();
}
void CBoldLineVisObj::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitBoldLine( corners, fWidth, color );
}
