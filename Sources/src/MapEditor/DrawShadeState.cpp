#include "StdAfx.h"
#include "editor.h"
#include "DrawShadeState.h"

#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "../GFX/GFX.h"
#include "../Scene/Terrain.h"
#include "../Image/Image.h"
#include "../Scene/Scene.h"
#include "../Formats/FmtMap.h"
#include "IUndoRedoCmd.h"
#include "PropertieDialog.h"
#include "resource.h"

#include "../RandomMapGen/VSO_Types.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

class SDrawVAPatternFunctional
{
private:
	STerrainInfo::TVertexAltitudeArray2D *pAltitudes;
	CTemplateEditorFrame *pFrame;
	class CSceneDrawTool *pSceneDrawTool;

	std::vector<float> line;
	int nOldX;
	int nLineIndex;

public:
	SDrawVAPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltitudes, CTemplateEditorFrame *_pFrame, CSceneDrawTool *_pSceneDrawTool ) 
		: pAltitudes( _pAltitudes ), pFrame( _pFrame ), pSceneDrawTool( _pSceneDrawTool ), nOldX( -1 ), nLineIndex( 0 )
	{
		NI_ASSERT_T( ( pAltitudes != 0 ) && ( pFrame != 0 ) && ( pSceneDrawTool != 0 ),
								 NStr::Format( "Wrong parameter: %x, %x, %x\n", pAltitudes, pFrame, pSceneDrawTool ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		float fRadius = fWorldCellSize / 8;
		DWORD nParts = 8;
		DWORD dwColor = 0xFFFF0000;

		CVec3 vCurrentPoint( nXIndex * fWorldCellSize,
												 ( pAltitudes->GetSizeY() - nYIndex - 1 ) * fWorldCellSize,
												 ( *pAltitudes )[nYIndex][nXIndex].fHeight );
		if ( fValue > 0 )
		{
			pSceneDrawTool->DrawCircle( vCurrentPoint, fRadius, nParts, dwColor );
		}

		if ( line.empty() )
		{
			line.push_back( ( *pAltitudes )[nYIndex][nXIndex].fHeight );
			nOldX = nXIndex;
			++nLineIndex;
		}
		else
		{
			if ( nXIndex == nOldX )
			{
				pSceneDrawTool->DrawLine( CVec3( vCurrentPoint.x, vCurrentPoint.y + fWorldCellSize, line[nLineIndex - 1] ), vCurrentPoint, dwColor );
			}
			else
			{
				nOldX = nXIndex;
				nLineIndex = 0;
			}
			if ( line.size() == nLineIndex )
			{
				line.push_back( ( *pAltitudes )[nYIndex][nXIndex].fHeight );
			}
			else
			{
				pSceneDrawTool->DrawLine( CVec3( vCurrentPoint.x - fWorldCellSize, vCurrentPoint.y, line[nLineIndex] ), vCurrentPoint, dwColor );
				line[nLineIndex] = vCurrentPoint.z;
			}
			++nLineIndex;
		}
		return true;
	}
};

void CDrawShadeState::Draw( CTemplateEditorFrame* pFrame )
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain =pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				CTPoint<int> terrainTile;
				bool isTileInTerrain = CMapInfo::GetTerrainTileIndices( rTerrainInfo, stateParameter.vLastPos, &terrainTile );
				CTPoint<int> cornerTile( terrainTile.x - ( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeX() / 2 - 1 ),
																 terrainTile.y - ( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeY() / 2 - 1 ) );
				CTRect<int> altitudeRect( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() );

				pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.pos = cornerTile;
				pFrame->m_mapEditorBarPtr->GetShade()->m_currentLevelPattern.pos = cornerTile;
				pFrame->m_mapEditorBarPtr->GetShade()->m_currentUndoLevelPattern.pos = cornerTile;
				
				SDrawVAPatternFunctional functional( &( rTerrainInfo.altitudes ), pFrame, &sceneDrawTool );
				ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentLevelPattern, functional, true );
				
				if ( isTileInTerrain )
				{
					if ( bLeaved )
					{
						sceneDrawTool.ClearMarker();
					}
					else
					{
						sceneDrawTool.AddMarkerTiles( CTRect<int>( terrainTile, terrainTile ), true );
					}
					pFrame->m_lastMouseTileX = terrainTile.x;
					pFrame->m_lastMouseTileY = terrainTile.y;
				}
				else
				{
					pFrame->m_lastMouseTileX = ( -1 );
					pFrame->m_lastMouseTileY = ( -1 );
				}
				
				sceneDrawTool.DrawToScene();
			}
		}
	}
}

void CDrawShadeState::Update( CTemplateEditorFrame* pFrame, bool bUpdateTileHeight )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	if ( bUpdateTileHeight )
	{
		CTPoint<int> terrainTile;
		if( CMapInfo::GetTerrainTileIndices( rTerrainInfo, stateParameter.vLastPos, &terrainTile ) )
		{
			fTileHeight = ( rTerrainInfo.altitudes[terrainTile.y + 0][terrainTile.x + 0].fHeight +
											rTerrainInfo.altitudes[terrainTile.y + 1][terrainTile.x + 0].fHeight +
											rTerrainInfo.altitudes[terrainTile.y + 1][terrainTile.x + 1].fHeight +
											rTerrainInfo.altitudes[terrainTile.y + 0][terrainTile.x + 1].fHeight ) / 4.0f;
			isTileHeightValid = true;
		}
		else
		{
			fTileHeight = 0.0f;
			isTileHeightValid = false;
		}
		fAverageHeight = pFrame->m_mapEditorBarPtr->GetShade()->m_currentLevelPattern.GetAverageHeight( rTerrainInfo.altitudes );

		switch( pFrame->m_mapEditorBarPtr->GetShade()->resizeDialogOptions.nParameters[1] )
		{
			case CShadeEditorWnd::LEVEL_TO_0:
			{
				pFrame->m_mapEditorBarPtr->GetShade()->SetDlgItemText( IDC_LEVEL_LABEL, NStr::Format( "Level: %.2f", 0.0f ) );
				break;
			}
			case CShadeEditorWnd::LEVEL_TO_1:
			{
				pFrame->m_mapEditorBarPtr->GetShade()->SetDlgItemText( IDC_LEVEL_LABEL, NStr::Format( "Level: %.2f", fTileHeight / fWorldCellSize ) );
				break;
			}
			case CShadeEditorWnd::LEVEL_TO_2:
			case CShadeEditorWnd::LEVEL_TO_3:
			{
				pFrame->m_mapEditorBarPtr->GetShade()->SetDlgItemText( IDC_LEVEL_LABEL, NStr::Format( "Level: %.2f", fAverageHeight / fWorldCellSize ) );
				break;
			}
		}
	}
}

void CDrawShadeState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
		{
			if ( ( pFrame->m_mapEditorBarPtr->GetShade()->m_tickCount - GetTickCount() ) > pFrame->m_mapEditorBarPtr->GetShade()->m_refreshRate )
			{
				pFrame->m_mapEditorBarPtr->GetShade()->m_tickCount = GetTickCount();
				if ( ( nFlags & MK_LBUTTON ) || ( nFlags & MK_RBUTTON ) || ( nFlags & MK_MBUTTON ) )
				{

					pFrame->SetMapModified();
					IScene *pScene = GetSingleton<IScene>();
					ITerrain *pTerrain = pScene->GetTerrain();
					ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
					STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				
					CTPoint<int> terrainTile;
					CMapInfo::GetTerrainTileIndices( rTerrainInfo, stateParameter.vLastPos, &terrainTile );
					CTPoint<int> cornerTile( terrainTile.x - ( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeX() / 2 - 1 ),
																	 terrainTile.y - ( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeY() / 2 - 1 ) );

					CTRect<int> altitudeRect( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() );
					CTRect<int> updateRect( cornerTile.x - 1,
																	cornerTile.y - 1 ,
																	cornerTile.x + 1 + pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeX(),
																	cornerTile.y + 1 + pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.heights.GetSizeY() );
				
					CTRect<int> affectedRect;

					if ( ( nFlags & MK_MBUTTON ) || 
							 ( ( nFlags & MK_LBUTTON ) && ( nFlags & MK_RBUTTON ) ) )
					{
						float fLevelToHeight = 0.0f;
						
						switch( pFrame->m_mapEditorBarPtr->GetShade()->resizeDialogOptions.nParameters[1] )
						{
							case CShadeEditorWnd::LEVEL_TO_1:
							{
								if ( isTileHeightValid )
								{
									fLevelToHeight = fTileHeight;
								}
								else
								{
									fLevelToHeight = 0.0f;
								}
								break;
							}
							case CShadeEditorWnd::LEVEL_TO_2:
							{
								fLevelToHeight = pFrame->m_mapEditorBarPtr->GetShade()->m_currentLevelPattern.GetAverageHeight( rTerrainInfo.altitudes );
								pFrame->m_mapEditorBarPtr->GetShade()->SetDlgItemText( IDC_LEVEL_LABEL, NStr::Format( "Level: %.2f", fLevelToHeight / fWorldCellSize ) );
								break;
							}
							case CShadeEditorWnd::LEVEL_TO_3:
							{
								fLevelToHeight = fAverageHeight;
								break;
							}
							default:
							{
								fLevelToHeight = 0.0f;
								break;
							}
						}
						
						if ( isTileHeightValid || ( pFrame->m_mapEditorBarPtr->GetShade()->resizeDialogOptions.nParameters[1] != CShadeEditorWnd::LEVEL_TO_1 ) )
						{
							{
								pFrame->m_mapEditorBarPtr->GetShade()->m_currentUndoLevelPattern.heights.SetZero();
								SVALevelAndCreateUndoPatternFunctional functional( &( rTerrainInfo.altitudes ), fLevelToHeight, pFrame->m_mapEditorBarPtr->GetShade()->resizeDialogOptions.fParameters[1], &( pFrame->m_mapEditorBarPtr->GetShade()->m_currentUndoLevelPattern ) );
								ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentLevelPattern, functional, true );
							}
							if ( ( ( nFlags & MK_CONTROL ) == 0 ) && ( !CVertexAltitudeInfo::IsValidHeight( rTerrainInfo.altitudes, updateRect ) ) )
							{
								SVASubstractPatternFunctional functional( &( rTerrainInfo.altitudes ) );
								ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentUndoLevelPattern, functional, true );
							}
							else
							{
								if ( pFrame->m_bNeedUpdateUnitHeights )
								{
									pAIEditor->ApplyPattern( pFrame->m_mapEditorBarPtr->GetShade()->m_currentUndoLevelPattern );
									pFrame->UpdateObjectsZ( updateRect );
								}
								
								CMapInfo::UpdateTerrainShades( &rTerrainInfo, updateRect, CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pFrame->GetSeason() ) ) );
								pTerrainEditor->Update( CTRect<int>( 0, 0, 0, 0 ) );
							}
						}
					}
					else if ( nFlags & MK_LBUTTON )
					{
						{
							SVAAddPatternFunctional functional( &( rTerrainInfo.altitudes ) );
							ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern, functional, true );
						}
						if ( ( ( nFlags & MK_CONTROL ) == 0 ) && ( !CVertexAltitudeInfo::IsValidHeight( rTerrainInfo.altitudes, updateRect ) ) )
						{
							SVASubstractPatternFunctional functional( &( rTerrainInfo.altitudes ) );
							ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern, functional, true );
						}
						else
						{
							if ( pFrame->m_bNeedUpdateUnitHeights )
							{
								pAIEditor->ApplyPattern( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern );
								pFrame->UpdateObjectsZ( updateRect );
							}

							CMapInfo::UpdateTerrainShades( &rTerrainInfo, updateRect, CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pFrame->GetSeason() ) ) );
							pTerrainEditor->Update( CTRect<int>( 0, 0, 0, 0 ) );
						}
					}
					else if ( nFlags & MK_RBUTTON )
					{
						{
							SVASubstractPatternFunctional functional( &( rTerrainInfo.altitudes ) );
							ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern, functional, true );
						}
						if ( ( ( nFlags & MK_CONTROL ) == 0 ) && ( !CVertexAltitudeInfo::IsValidHeight( rTerrainInfo.altitudes, updateRect ) ) )
						{
							SVAAddPatternFunctional functional(&( rTerrainInfo.altitudes ) );
							ApplyVAPattern( altitudeRect, pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern, functional, true );
						}
						else
						{
							if ( pFrame->m_bNeedUpdateUnitHeights )
							{
								pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.fRatio = -1;
								pAIEditor->ApplyPattern( pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern );
								pFrame->m_mapEditorBarPtr->GetShade()->m_currentPattern.fRatio = 1;
								pFrame->UpdateObjectsZ( updateRect );
							}

							CMapInfo::UpdateTerrainShades( &rTerrainInfo, updateRect, CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pFrame->GetSeason() ) ) );
							pTerrainEditor->Update( CTRect<int>( 0, 0, 0, 0 ) );
						}
					}
				}		
				else
				{
					Update( pFrame );
				}
			}
			pFrame->RedrawWindow();
		}
	}
}

void CDrawShadeState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		Update( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::UpdateZ( CTemplateEditorFrame* pFrame )
{
}

void CDrawShadeState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		Update( pFrame );

		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
		}
		
		UpdateZ( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		Update( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_RBUTTONUP, rMousePoint, pFrame ) )
	{
		Update( pFrame );

		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
		}

		UpdateZ( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::OnMButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_MBUTTONDOWN, rMousePoint, pFrame ) )
	{
		Update( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::OnMButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_MBUTTONUP, rMousePoint, pFrame ) )
	{
		Update( pFrame );

		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
		}
		
		UpdateZ( pFrame );
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		
		Update( pFrame, false );
	}
}

void CDrawShadeState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
		}
	
		bLeaved = false;
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::Leave()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain =pScene->GetTerrain() )
			{
				if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
				{
					pTerrainEditor->SetMarker( 0, 0 );
				}
			}
		}
		bLeaved = true;
		pFrame->RedrawWindow();
	}
}

void CDrawShadeState::Update()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}
