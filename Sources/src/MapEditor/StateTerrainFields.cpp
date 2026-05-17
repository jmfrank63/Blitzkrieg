#include "stdafx.h"

#include "editor.h"
#include "StateTerrainFields.h"

#include "frames.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "PropertieDialog.h"
#include "..\image\image.h"

#include "TabTerrainFieldsDialog.h"
#include "TabTerrainFieldsDialog.h"

#include "..\RandomMapGen\Polygons_Types.h"
#include "..\RandomMapGen\VSO_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//fields
//CFieldsSelectState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsSelectState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		pParentState->nCurrentPoint = CFieldsState::INVALID_INDEX;
		
		for( int nPointIndex = 0; nPointIndex < pParentState->points.size(); ++nPointIndex )
		{
			pParentState->vDifference = pParentState->stateParameter.vLastPos - pParentState->points[nPointIndex];
			if ( fabs( pParentState->vDifference ) <= CFieldsState::POINT_RADIUS )
			{
				pParentState->nCurrentPoint = nPointIndex;
				pParentState->SetActiveState( CFieldsState::STATE_EDIT );
				break;
			}
		}
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsSelectState::OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDBLCLK, rMousePoint, pFrame ) )
	{
		pParentState->PlaceField( true );
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsSelectState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		if ( ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) )
		{
			pParentState->PlaceField( true );
			pFrame->RedrawWindow();
		}
		else if ( nChar == VK_ESCAPE )
		{
			pParentState->PlaceField( false );
			pFrame->RedrawWindow();
		}
	}
}

//CFieldsEditState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsEditState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		if ( ( nFlags & MK_LBUTTON ) &&
				 ( pParentState->nCurrentPoint >= 0 ) && 
				 ( pParentState->nCurrentPoint < pParentState->points.size() ) )
		{
			pParentState->points[pParentState->nCurrentPoint] = pParentState->stateParameter.vLastPos - pParentState->vDifference;
			CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( pParentState->points[pParentState->nCurrentPoint] ) );
		}
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsEditState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		pParentState->nCurrentPoint = CFieldsState::INVALID_INDEX;
		pParentState->vDifference = VNULL3;
		pParentState->SetActiveState( CFieldsState::STATE_SELECT );
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsEditState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		if ( ( pParentState->nCurrentPoint >= 0 ) && 
				 ( pParentState->nCurrentPoint < pParentState->points.size() ) )
		{
			if ( nChar == VK_INSERT )
			{
				IScene *pScene = GetSingleton<IScene>();
				ITerrain *pTerrain = pScene->GetTerrain();
				ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				CVec3 vNewPoint = ( pParentState->points[pParentState->nCurrentPoint] + 
														pParentState->points[( pParentState->nCurrentPoint + 1 ) % pParentState->points.size()] ) / 2.0f;
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vNewPoint );
				pParentState->points.insert( pParentState->points.begin() + pParentState->nCurrentPoint + 1, vNewPoint );
				pFrame->RedrawWindow();
			}
			else if ( nChar == VK_DELETE ) 
			{
				pParentState->points.erase( pParentState->points.begin() + pParentState->nCurrentPoint );
				pParentState->nCurrentPoint = CFieldsState::INVALID_INDEX;
				pParentState->vDifference = VNULL3;
				pParentState->SetActiveState( CFieldsState::STATE_SELECT );
				pFrame->RedrawWindow();
			}
		}
		//�� ����
		//pFrame->RedrawWindow();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//CFieldsAddState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsAddState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		if ( !pParentState->points.empty() )
		{
			pParentState->points.pop_back();
		}
		pParentState->points.push_back( pParentState->stateParameter.vLastPos );
		pFrame->RedrawWindow();
	}	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsAddState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		pParentState->points.push_back( pParentState->stateParameter.vLastPos );
		pFrame->RedrawWindow();
	}	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsAddState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		if ( !pParentState->points.empty() )
		{
			pParentState->points.pop_back();
		}
		if ( !pParentState->points.empty() )
		{
			pParentState->points.pop_back();
		}
		pParentState->points.push_back( pParentState->stateParameter.vLastPos );
		pFrame->RedrawWindow();
	}	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsAddState::OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDBLCLK, rMousePoint, pFrame ) )
	{
		UniquePolygon<std::vector<CVec3>, CVec3>( &( pParentState->points ), CFieldsState::POINT_RADIUS );
		if ( pParentState->points.size() > 2 )
		{
			if ( fabs2( GetSignedPolygonSquare( pParentState->points ) ) > fabs2( CFieldsState::POINT_RADIUS ) )
			{
				pParentState->nCurrentPoint = CFieldsState::INVALID_INDEX;
				pParentState->vDifference = VNULL3;
				pParentState->SetActiveState( CFieldsState::STATE_SELECT );
			}
		}
		pFrame->RedrawWindow();
	}	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsAddState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		if ( ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) )
		{
			UniquePolygon<std::vector<CVec3>, CVec3>( &( pParentState->points ), CFieldsState::POINT_RADIUS );
			if ( pParentState->points.size() > 2 )
			{
				if ( fabs2( GetSignedPolygonSquare( pParentState->points ) ) > fabs2( CFieldsState::POINT_RADIUS ) )
				{
					pParentState->nCurrentPoint = CFieldsState::INVALID_INDEX;
					pParentState->vDifference = VNULL3;
					pParentState->SetActiveState( CFieldsState::STATE_SELECT );
					pFrame->RedrawWindow();
				}
			}
		}
		else if ( nChar == VK_ESCAPE )
		{
			if ( !pParentState->points.empty() )
			{
				const CVec3 vLastPoint = pParentState->points.back();
				pParentState->points.clear();
				pParentState->points.push_back( vLastPoint );
				pFrame->RedrawWindow();
			}
		}
		//�� ����
		//pFrame->RedrawWindow();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//CFieldsState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CFieldsState::INVALID_INDEX = ( -1 );
const float CFieldsState::POINT_RADIUS = fWorldCellSize / 4.0f;
const int CFieldsState::POINT_PARTS = 8;
const float CFieldsState::LINE_SEGMENT = fWorldCellSize;
const SColor CFieldsState::POINT_COLOR = SColor( 0xFF, 0xFF, 0x80, 0x80 );
const SColor CFieldsState::LINE_COLOR = SColor( 0xFF, 0xFF, 0x80, 0x80 );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		nCurrentPoint = CFieldsState::INVALID_INDEX;
		points.clear();
		vDifference = VNULL3;
		SetActiveState( STATE_ADD );
		pFrame->RedrawWindow();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsState::Leave()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		nCurrentPoint = CFieldsState::INVALID_INDEX;
		points.clear();
		vDifference = VNULL3;
		SetActiveState( STATE_ADD );
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsState::Draw( CTemplateEditorFrame* pFrame )
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				
				for ( int nPointIndex = 0; nPointIndex < points.size(); ++nPointIndex )
				{
					CVec3 &rvBegin = points[nPointIndex];
					if ( nPointIndex < ( points.size() - 1 ) )
					{
						CVec3 &rvEnd = points[nPointIndex + 1];
						sceneDrawTool.DrawLine3D( rvBegin, rvEnd, LINE_COLOR, rTerrainInfo.altitudes, static_cast<int>( 0.5f + ( fabs( rvBegin - rvEnd ) / LINE_SEGMENT ) ) );
					}
					sceneDrawTool.DrawCircle3D( rvBegin, POINT_RADIUS, POINT_PARTS, POINT_COLOR, rTerrainInfo.altitudes, true );
				}
				if ( GetActiveState() != STATE_ADD )
				{
					CVec3 &rvBegin = points.front();
					CVec3 &rvEnd = points.back();
					sceneDrawTool.DrawLine3D( rvBegin, rvEnd, LINE_COLOR, rTerrainInfo.altitudes, static_cast<int>( 0.5f + ( fabs( rvBegin - rvEnd ) / LINE_SEGMENT ) ) );
				}
				if ( GetActiveState() == STATE_EDIT )
				{
					if ( ( nCurrentPoint >= 0 ) && ( nCurrentPoint < points.size() ) )
					{
						float fRadius = POINT_RADIUS - 1.0f;
						while( fRadius > 0.0f )
						{
							sceneDrawTool.DrawCircle3D( points[nCurrentPoint], fRadius, POINT_PARTS, POINT_COLOR, rTerrainInfo.altitudes, true );
							fRadius -= 1.0f;
						}
					}
				}
				sceneDrawTool.DrawToScene();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsState::Update()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
				{
					pFrame->RedrawWindow();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFieldsState::PlaceField( bool bPlace )
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->BeginWaitCursor();

		bool bFillTerrain = true;
		UniquePolygon<std::vector<CVec3>, CVec3>( &( points ), CFieldsState::POINT_RADIUS );
		if ( bPlace &&
				 ( points.size() > 2 )  && 
				 ( fabs2( GetSignedPolygonSquare( points ) ) > fabs2( CFieldsState::POINT_RADIUS ) ) )
		{
			if ( CTabTerrainFieldsDialog *pTabTerrainFieldsDialog = pFrame->GetMapEditorBar()->GetTerrainFieldsTab() )
			{
				IScene *pScene = GetSingleton<IScene>();
				ITerrain *pTerrain = pScene->GetTerrain();
				ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				IImageProcessor *pImageProcessor = GetImageProcessor();
				
				pTabTerrainFieldsDialog->SetControlsToActualValues();

				std::list<CVec2> listedPolygon;
				for ( int nPointIndex = 0; nPointIndex < points.size(); ++nPointIndex )
				{
					listedPolygon.push_back( CVec2( points[nPointIndex].x, points[nPointIndex].y ) );
				}

				std::list<CVec2> mapVisPointsPolygon;
				mapVisPointsPolygon.push_back( VNULL2 );
				mapVisPointsPolygon.push_back( CVec2( 0.0f, rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) );
				mapVisPointsPolygon.push_back( CVec2( rTerrainInfo.tiles.GetSizeX() * fWorldCellSize, rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) );
				mapVisPointsPolygon.push_back( CVec2( rTerrainInfo.tiles.GetSizeX() * fWorldCellSize, 0.0f  ) );

				std::list<CVec2> cutPolygon;
				CutByPolygonCore<std::list<CVec2>, CVec2>( listedPolygon, mapVisPointsPolygon, &cutPolygon );

				//randomize polygon
				std::list<CVec2> polygon;
				if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[0] > 0 )
				{
					RandomizeEdges<std::list<CVec2>, CVec2>( cutPolygon,
																									 10,
																									 pTabTerrainFieldsDialog->resizeDialogOptions.fParameters[1],
																									 CTPoint<float>( 0.0f, pTabTerrainFieldsDialog->resizeDialogOptions.fParameters[2] ),
																									 &polygon,
																									 pTabTerrainFieldsDialog->resizeDialogOptions.fParameters[0] * fWorldCellSize,
																									 512.0f * fWorldCellSize,
																									 true );
				}
				else
				{
					polygon = cutPolygon;
				}

				//��������� ��� ����� �� ���� ������ ������
				if ( pFrame->dlg ) 
				{
					pFrame->dlg->DestroyWindow();
					delete pFrame->dlg;
					pFrame->dlg = 0;
					pFrame->isStartCommandPropertyActive = false;
				}
				
				pFrame->m_CurentReservePosition.vPos = VNULL2;
				pFrame->m_CurentReservePosition.pArtilleryObject = 0;
				pFrame->m_CurentReservePosition.pTruckObject = 0;
				pFrame->isReservePositionActive = false;

				pFrame->m_currentMovingObjectPtrAI = 0;
				pFrame->m_currentMovingObjectsAI.clear();
				//���������� �����
				//remove objects
				if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[1] > 0 )
				{
					//���� ����������
				}

				SRMFieldSet fieldSet;
				if ( LoadDataResource( pTabTerrainFieldsDialog->resizeDialogOptions.szParameters[0], "", false, 0, RMGC_FIELDSET_XML_NAME, fieldSet ) )
				{
					const int nMapSeason = pFrame->currentMapInfo.GetSelectedSeason();
					const int nFieldSeason = CMapInfo::GetSelectedSeason( fieldSet.nSeason, fieldSet.szSeasonFolder );
					
					if ( nMapSeason != nFieldSeason )
					{
						CString strTitle;
						CString strFormatMessage;
						CString strMessage;
						strTitle.LoadString( IDR_EDITORTYPE );
						strFormatMessage.LoadString( IDS_INVALID_FIELD_SEASON );
						strMessage.Format( strFormatMessage, CMapInfo::SEASON_NAMES[nMapSeason], CMapInfo::SEASON_NAMES[nFieldSeason] );

						bFillTerrain = false;
						
						if ( pFrame->MessageBox( strMessage, strTitle, MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON1 ) == IDYES )
						{
							bFillTerrain = true;
						}
					}

					if ( bFillTerrain )
					{
						std::unordered_map<LPARAM, float> distances;
						const std::list<std::list<CVec2> > exclusivePolygons;
						
						//fill terrain
						if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[2] > 0 )
						{
							fieldSet.ValidateFieldSet( pFrame->descrTile, CMapInfo::MOST_COMMON_TILES[nMapSeason] );

							CMapInfo::FillTileSet( &rTerrainInfo,
																		 pFrame->descrTile,
																		 polygon,
																		 exclusivePolygons,
																		 fieldSet.tilesShells,
																		 &distances );
							pTerrainEditor->Update( CTRect<int>( 0,
																									 0, 
																									 rTerrainInfo.patches.GetSizeX() - 1, 
																									 rTerrainInfo.patches.GetSizeY() - 1 ) );
							pFrame->SetMapModified();
						}

						//fill objects
						if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[3] > 0 )
						{
							CArray2D<BYTE> tileMap( rTerrainInfo.tiles.GetSizeX() * 2, rTerrainInfo.tiles.GetSizeY() * 2 );
							tileMap.Set( RMGC_UNLOCKED );
							CMapInfo mapInfo;
							mapInfo.Create( CTPoint<int>( rTerrainInfo.patches.GetSizeX(),
																						rTerrainInfo.patches.GetSizeY() ),
															CMapInfo::REAL_SEASONS[CMapInfo::SEASON_SUMMER],
															CMapInfo::SEASON_FOLDERS[CMapInfo::SEASON_SUMMER],
															3, 0 );
							CMapInfo::FillObjectSet( &mapInfo,
																			 polygon,
																			 exclusivePolygons,
																			 fieldSet.objectsShells,
																			 &tileMap );
							if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
							{
								for ( int nObjectIndex = 0; nObjectIndex < mapInfo.objects.size(); ++nObjectIndex )
								{
									if ( ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[6] == 0 ) || pAIEditor->CanAddObject( mapInfo.objects[nObjectIndex] ) )
									{
										pFrame->AddObjectByAI( mapInfo.objects[nObjectIndex] );
									}
								}
								pFrame->SetMapModified();
							}
							pFrame->currentMapInfo.objects.clear();
						}

						//fill heights
						if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[4] > 0 )
						{
							if ( fieldSet.fHeight > 0 )
							{
								SVAGradient gradient;
								{
									if ( CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( ( fieldSet.szProfileFileName + ".tga" ).c_str(), STREAM_ACCESS_READ ) )
									{
										if ( CPtr<IImage> pImage = pImageProcessor->LoadImage( pImageStream ) )
										{ 
											gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, fieldSet.fHeight ) );
										}
									}
								}
								if ( !gradient.heights.empty() )
								{
									CMapInfo::FillProfilePattern( &rTerrainInfo,
																								polygon,
																								exclusivePolygons,
																								gradient,
																								fieldSet.patternSize,
																								fieldSet.fPositiveRatio,
																								&distances );
									pFrame->UpdateObjectsZ( CTRect<int>( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() ) );
									CMapInfo::UpdateTerrainShades( &rTerrainInfo,
																								 CTRect<int>( 0,
																															0,
																															rTerrainInfo.altitudes.GetSizeX(),
																															rTerrainInfo.altitudes.GetSizeY() ),
																								 CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pFrame->GetSeason() ) ) );
									pFrame->SetMapModified();
								}
							}
						}
						if ( pTabTerrainFieldsDialog->resizeDialogOptions.nParameters[5] > 0 )
						{
							pFrame->OnButtonUpdate();
						}
						if ( g_frameManager.GetMiniMapWindow() )
						{
							g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
						}
					}
				}
			}
		}
		
		if ( bFillTerrain )
		{
			nCurrentPoint = CFieldsState::INVALID_INDEX;
			vDifference = VNULL3;
			points.clear();
			SetActiveState( CFieldsState::STATE_ADD );
		}
		//
		pFrame->EndWaitCursor();
		pFrame->RedrawWindow();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
