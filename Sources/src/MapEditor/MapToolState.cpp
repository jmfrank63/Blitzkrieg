
#include "StdAfx.h"
#include <math.h>
#include "editor.h"
#include "MapToolState.h"

#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "../GFX/GFX.H"
#include "../GFX/GFXHelper.h"
#include "../Scene/Terrain.h"
#include "../Image/Image.h"
#include "../Scene/Scene.h"
#include "IUndoRedoCmd.h"
#include "PropertieDialog.h"
#include "resource.h"
#include "TabToolsDialog.h"
#include "AreaNameDialog.h"
#include "DrawingTools.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void CMapToolState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame *pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		return;
	}
	CTPoint<int> mousePoint( rMousePoint );
	RECT r; 
	g_frameManager.GetGameWnd()->GetClientRect( &r );
	mousePoint.x -= r.left;
	mousePoint.y -= r.top;
	CVec3 v;
	CVec2 p ( mousePoint.x , mousePoint.y );
	GetSingleton<IScene>()->GetPos3( &v, p );	
	pFrame->m_firstPoint.x = v.x;
	pFrame->m_firstPoint.y = v.y;

	if ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nRepair )
	{
		std::pair<IVisObj*, CVec2> *pObjects;
		int num;
		GetSingleton<IScene>()->Pick( p, &pObjects, &num, SGVOGT_UNKNOWN );

		if( num != 0 )
		{
			SMapObject *pTmp= pFrame->FindByVis( pObjects[0].first );

			float hpAdded = pFrame->m_mapEditorBarPtr->GetToolsTab()->resizeDialogOptions.nParameters[0] / 100.0f;
			float fMinHP = 0.0f;
			if ( pTmp->IsTechnics() || pTmp->IsHuman() )
			{
				fMinHP = 0.01f;
			}
			
			if ( ( pTmp->fHP - hpAdded ) > 1.0f )
			{
				hpAdded = pTmp->fHP - 1.0f;
			}
			else if ( ( pTmp->fHP - hpAdded ) < fMinHP )
			{
				hpAdded = pTmp->fHP - fMinHP;
			}
			
			hpAdded *= pTmp->pRPG->fMaxHP;
			if ( pTmp->pAIObj )
			{
				GetSingleton<IAIEditor>()->DamageObject( pTmp->pAIObj, hpAdded );
				IGameTimer *pTimer = GetSingleton<IGameTimer>();
				int time = pTimer->GetGameTime( );
				pFrame->Update( time );
				pFrame->RedrawWindow();	
				pFrame->SetMapModified();
			}
		}
	}
	
	if(  pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nArea )
	{
	}
}

void CMapToolState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame *pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		return;
	}
	CTPoint<int> mousePoint( rMousePoint );
	RECT r; 
	g_frameManager.GetGameWnd()->GetClientRect( &r );
	mousePoint.x -= r.left;
	mousePoint.y -= r.top;
	CVec3 v;
	CVec2 p ( mousePoint.x , mousePoint.y );
	GetSingleton<IScene>()->GetPos3( &v, p );	
	pFrame->m_firstPoint.x = v.x;
	pFrame->m_firstPoint.y = v.y;

	if ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nRepair )
	{
		std::pair<IVisObj*, CVec2> *pObjects;
		int num;
		GetSingleton<IScene>()->Pick( p, &pObjects, &num, SGVOGT_UNKNOWN );

		if( num != 0 )
		{
			SMapObject *pTmp= pFrame->FindByVis( pObjects[0].first );
			float fMinHP = 0.0f;
			if ( pTmp->IsTechnics() || pTmp->IsHuman() )
			{
				fMinHP = 0.01f;
			}

			float hpAdded = ( -1 ) * ( pFrame->m_mapEditorBarPtr->GetToolsTab()->resizeDialogOptions.nParameters[0] / 100.0f );
			
			if ( ( pTmp->fHP - hpAdded ) > 1.0f )
			{
				hpAdded = pTmp->fHP - 1.0f;
			}
			else if ( ( pTmp->fHP - hpAdded ) < fMinHP )
			{
				hpAdded = pTmp->fHP - fMinHP;
			}
			
			hpAdded *= pTmp->pRPG->fMaxHP;
			if ( pTmp->pAIObj )
			{
				GetSingleton<IAIEditor>()->DamageObject( pTmp->pAIObj, hpAdded );
				IGameTimer *pTimer = GetSingleton<IGameTimer>();
				int time = pTimer->GetGameTime( );
				pFrame->Update( time );
				pFrame->RedrawWindow();	
				pFrame->SetMapModified();
			}
		}
	}
}

void CMapToolState::OnMButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame *pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		return;
	}
	CTPoint<int> mousePoint( rMousePoint );
	RECT r; 
	g_frameManager.GetGameWnd()->GetClientRect( &r );
	mousePoint.x -= r.left;
	mousePoint.y -= r.top;
	CVec3 v;
	CVec2 p ( mousePoint.x , mousePoint.y );
	GetSingleton<IScene>()->GetPos3( &v, p );	
	pFrame->m_firstPoint.x = v.x;
	pFrame->m_firstPoint.y = v.y;

	if ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nRepair )
	{
		std::pair<IVisObj*, CVec2> *pObjects;
		int num;
		GetSingleton<IScene>()->Pick( p, &pObjects, &num, SGVOGT_UNKNOWN );

		if( num != 0 )
		{
			SMapObject *pTmp= pFrame->FindByVis( pObjects[0].first );
			float hpAdded = pTmp->fHP - 1.0f;
			hpAdded *= pTmp->pRPG->fMaxHP;
			if ( pTmp->pAIObj )
			{
				GetSingleton<IAIEditor>()->DamageObject( pTmp->pAIObj, hpAdded );
				IGameTimer *pTimer = GetSingleton<IGameTimer>();
				int time = pTimer->GetGameTime( );
				pFrame->Update( time );
				pFrame->RedrawWindow();	
				pFrame->SetMapModified();
			}
		}
	}
}

void CMapToolState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		return;
	}
	if ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nArea )
	{
		CAreaNameDialog dlg;
		if ( dlg.DoModal() == IDOK && dlg.m_name != "" )
		{
			SScriptArea area;
			if( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_drawType == toolStateConsts::nRectType )
			{
				area.eType = SScriptArea::EAT_RECTANGLE;
				area.center = CVec2( ( pFrame->m_firstPoint.x + pFrame->m_lastPoint.x ) / 2.0f, ( pFrame->m_firstPoint.y + pFrame->m_lastPoint.y ) / 2.0f ) ;
				area.vAABBHalfSize = CVec2( abs( pFrame->m_firstPoint.x - pFrame->m_lastPoint.x ) / 2.0f, abs( pFrame->m_firstPoint.y - pFrame->m_lastPoint.y ) / 2.0f ) ;
			}
			else
			{
				 area.eType = SScriptArea::EAT_CIRCLE;
		  	 area.center = CVec2( pFrame->m_firstPoint.x , pFrame->m_firstPoint.y ) ;
		  	 area.fR = fabs( pFrame->m_firstPoint.x - pFrame->m_lastPoint.x, 
												 pFrame->m_firstPoint.y - pFrame->m_lastPoint.y ); 		
			}
			area.szName = dlg.m_name;
			pFrame->m_scriptAreas.push_back( area );	
			pFrame->CalculateAreas();
			pFrame->RedrawWindow();
			pFrame->SetMapModified();
		}
	}
}

void CMapToolState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		return;
	}
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				CTPoint<int> mousePoint( rMousePoint );
				RECT r; 
				g_frameManager.GetGameWnd()->GetClientRect( &r );
				mousePoint.x -= r.left;
				mousePoint.y -= r.top;
				CVec3 v;
				CVec2 p( mousePoint.x , mousePoint.y );
				GetSingleton<IScene>()->GetPos3( &v, p );	
				pFrame->m_lastPoint.x = v.x;
				pFrame->m_lastPoint.y = v.y;

				pFrame->m_mapEditorBarPtr->GetToolsTab()->UpdateData();

				CSceneDrawTool drawTool;
				DWORD dwColor = 0xff00FF00;

				if ( ( nFlags & MK_LBUTTON ) &&
						 ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nArea ) && 
						 ( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_drawType == toolStateConsts::nRectType ) )
				{
					drawTool.DrawLine3D( CVec2( pFrame->m_firstPoint.x,	pFrame->m_firstPoint.y ), CVec2( pFrame->m_lastPoint.x,		pFrame->m_firstPoint.y ), dwColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( pFrame->m_lastPoint.x,	pFrame->m_firstPoint.y ), CVec2( pFrame->m_lastPoint.x,		pFrame->m_lastPoint.y ), dwColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( pFrame->m_lastPoint.x,	pFrame->m_lastPoint.y ), CVec2( pFrame->m_firstPoint.x,	pFrame->m_lastPoint.y ), dwColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( pFrame->m_firstPoint.x,	pFrame->m_lastPoint.y ), CVec2( pFrame->m_firstPoint.x,	pFrame->m_firstPoint.y ), dwColor, rTerrainInfo.altitudes, 16 );
				}	

				if( ( nFlags & MK_LBUTTON ) &&
						( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_mode == toolStateConsts::nArea ) &&
						( pFrame->m_mapEditorBarPtr->GetToolsTab()->m_drawType == toolStateConsts::nCircleType ) )
				{
					float fRadius = fabs( CVec2( pFrame->m_firstPoint.x - pFrame->m_lastPoint.x, pFrame->m_firstPoint.y - pFrame->m_lastPoint.y ) );
					drawTool.DrawCircle3D( CVec2( pFrame->m_firstPoint.x,	pFrame->m_firstPoint.y ), fRadius, 32, dwColor, rTerrainInfo.altitudes );
				}	
				drawTool.DrawToScene();
				pFrame->RedrawWindow();
			}
		}
	}
}

void CMapToolState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CMapToolState::Leave()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CMapToolState::Update()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}
