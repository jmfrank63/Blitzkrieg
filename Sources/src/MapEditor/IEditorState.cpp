
#include "StdAfx.h"
#include "editor.h"
#include "IEditorState.h"

#include "..\Scene\Scene.h"
#include "..\Terrain\Terrain.h"

#include "frames.h"
#include "GameWnd.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

bool CStateParameter::Update( STATE_EVENT nType, const CTPoint<int> &rPoint, CTemplateEditorFrame* pFrame )
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{							
				POINT point;
				point.x = rPoint.x;
				point.y = rPoint.y;
				if ( nType == SE_KEYDOWN )
				{
					GetCursorPos( &point );
					g_frameManager.GetGameWnd()->ScreenToClient( &point );
				}
				/**
				else
				{
					g_frameManager.GetGameWnd()->ClientToScreen( &point );
				}
				/**/

				/**
				RECT screenRect;
				g_frameManager.GetGameWnd()->GetClientRect( &screenRect );
				CVec2 v2Point( point.x - screenRect.left, point.y - screenRect.top );
				/**/

				pScene->GetPos3( &vLastPos, CVec2( point.x, point.y ) );
				
				mouseEvents[nType].isValid = true;
				mouseEvents[nType].vPos = vLastPos;

				return true;
			}
		}
	}
	return false;	
}
