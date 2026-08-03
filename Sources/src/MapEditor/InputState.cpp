#include "StdAfx.h"
#include "editor.h"
#include "InputState.h"

#include "..\Scene\Scene.h"
#include "..\Scene\Terrain.h"

#include "frames.h"
#include "GameWnd.h"
#include "mainFrm.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

const int CInputStateParameter::INVALID_STATE = ( -1 );

bool CInputStateParameter::Update( INPUT_STATE_EVENT nType, const CTPoint<int> &rPoint, CTemplateEditorFrame* pFrame, bool bSetZToZero )
{
	NI_ASSERT_TF( ( nType >= 0 ) && ( nType < ISE_COUNT ),
								NStr::Format( "Invalid INPUT_STATE_EVENT: %d\n", nType ),
								return false );

	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{							
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				POINT point;
				point.x = rPoint.x;
				point.y = rPoint.y;
				if ( nType == ISE_KEYDOWN )
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
				/**
				if ( bSetZToZero )
				{
					if ( ( vLastPos.x <= 0.0f ) ||
							 ( vLastPos.y <= 0.0f ) ||
							 ( vLastPos.x >= ( rTerrainInfo.tiles.GetSizeX() * fWorldCellSize ) ) ||
							 ( vLastPos.y >= ( rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) ) )
					{
						const float fRatio = FP_SQRT_3 / FP_SQRT_2;
						vLastPos.x -= vLastPos.z * fRatio;
						vLastPos.y += vLastPos.z * fRatio;
						vLastPos.z = 0.0f;
					}
					else
					{
						CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vLastPos.x, vLastPos.y, &( vLastPos.z ) );
					}
				}
				else
				/**/
				{
					CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vLastPos.x, vLastPos.y, &( vLastPos.z ) );
				}
				
				lastPoint.x = point.x;
				lastPoint.y = point.y;

				mouseEvents[nType].point = lastPoint;
				mouseEvents[nType].vPos = vLastPos;
				mouseEvents[nType].isValid = true;

				UpdateSatusBar( true );
				return true;
			}
		}
	}
	UpdateSatusBar( false );
	return false;	
}

void CInputStateParameter::UpdateSatusBar( bool bValid )
{
	int nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_TILEPOS );
	std::string szMessage;
	/**
	if ( bValid )
	{
		CVec3 vAIPos;
		Vis2AI( &vAIPos, vLastPos );
		szMessage = NStr::Format( "AI:[%d, %d] (%d, %d),VIS:[%d, %d, %d] (%d, %d, %d)",
															static_cast<int>( vAIPos.x ),
															static_cast<int>( vAIPos.y ),
															static_cast<int>( vAIPos.x / SAIConsts::TILE_SIZE ),
															static_cast<int>( vAIPos.y / SAIConsts::TILE_SIZE ),
															static_cast<int>( vLastPos.x ),
															static_cast<int>( vLastPos.y ),
															static_cast<int>( vLastPos.z ),
															static_cast<int>( vLastPos.x / fWorldCellSize ),
															static_cast<int>( vLastPos.y / fWorldCellSize ),
															static_cast<int>( vLastPos.z / fWorldCellSize ) );
	}
	else
	{
		szMessage = NStr::Format( "AI:[-, -] (-, -),VIS:[-, -, -] (-, -, -)" );
	}
	/**/
	if ( bValid )
	{
		CVec3 vAIPos;
		Vis2AI( &vAIPos, vLastPos );
		szMessage = NStr::Format( "VIS: (%.2f, %.2f, %.2f), SCRIPT: (%d, %d)",
															vLastPos.x / fWorldCellSize,
															vLastPos.y / fWorldCellSize,
															vLastPos.z / fWorldCellSize,
															static_cast<int>( vAIPos.x ),
															static_cast<int>( vAIPos.y ) );
	}
	else
	{
		szMessage = NStr::Format( "VIS:(-, -, -), SCRIPT: (-, -)" );
	}
	theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, szMessage.c_str() );
}

bool CInputStateParameter::LastPointInTerrain()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{							
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				if ( ( vLastPos.x > 0.0f ) && 
						 ( vLastPos.y > 0.0f ) && 
						 ( vLastPos.x < ( rTerrainInfo.tiles.GetSizeX() * fWorldCellSize ) ) && 
						 ( vLastPos.y < ( rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) ) )
				{
					return true;	
				}
			}
		}
	}
	return false;
}
