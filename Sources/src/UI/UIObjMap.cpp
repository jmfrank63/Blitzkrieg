#include "StdAfx.h"

#include "..\gfx\gfxhelper.h"
#include "UIObjMap.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIObjMap::Init()
{
	const CTRect<float> screenRC = GetScreenRect();
	vertices.resize( 4 );
	{
		const float fW = screenRC.Width() / 2;
		const float fH = screenRC.Height() / 2;
		const DWORD dwColor = GetGlobalVar( "Scene.Colors.ObjMap.InterMission.Color", int(0xffffffff) );
		vertices[0].Setup( screenRC.left + fW, screenRC.top + 3, 0, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( screenRC.right - 5, screenRC.top + fH, 0, 1, dwColor, 0xff000000, 1, 0 );
		vertices[2].Setup( screenRC.left + fW, screenRC.bottom - 3, 0, 1, dwColor, 0xff000000, 1, 1 );
		vertices[3].Setup( screenRC.left + 5, screenRC.top + fH, 0, 1, dwColor, 0xff000000, 0, 1 );
	}
	
	indices.resize( 6 );
	{
		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 0;
		indices[4] = 3;
		indices[5] = 2;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIObjMap::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;

	CSimpleWindow::Visit( pVisitor );
	pVisitor->VisitUICustom( dynamic_cast<IUIElement*>(this) );
	// ������ �����
	for ( CWindowList::reverse_iterator ri=childList.rbegin(); ri!=childList.rend(); ri++ )
		(*ri)->Visit( pVisitor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIObjMap::Draw( IGFX *pGFX )
{
	// ������ �����
	if ( pMapTexture )
	{
		pGFX->SetShadingEffect( 3 );
		pGFX->SetTexture( 0, pMapTexture );
		if ( !vertices.empty() && !indices.empty() )
			DrawTemp( pGFX, vertices, indices );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////