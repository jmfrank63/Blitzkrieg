#include "stdafx.h"
#include "editor.h"
#include "VectorStripeObjectsState.h"

#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "frames.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

const int			CVSOState::INVALID_INDEX				= ( -1 );
const float		CVSOState::CONTROL_POINT_RADIUS	= fWorldCellSize / 5.0f;
const int			CVSOState::CONTROL_POINT_PARTS	= 8;
const float		CVSOState::KEY_POINT_RADIUS			= fWorldCellSize / 3.0f;
const int			CVSOState::KEY_POINT_PARTS			= 8;
const SColor	CVSOState::CONTROL_POINT_COLOR	= SColor( 0xFF, 0xFF, 0x80, 0x80 );
const SColor	CVSOState::KEY_POINT_COLOR			= SColor( 0xFF, 0xFF, 0xFF, 0x80 );
const SColor	CVSOState::POINT_COLOR					= SColor( 0xFF, 0x20, 0xFF, 0x20 );
const SColor	CVSOState::POLYGON_COLOR				= SColor( 0xFF, 0x20, 0xFF, 0x20 );
const float		CVSOState::OPACITY_DELIMITER		= 100.0f;


void CVSOSelectState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
	}
}

void CVSOSelectState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		pParentState->selectedIndices.clear();
		if ( CMapInfo::TerrainHitTest( rTerrainInfo,
																	 pParentState->stateParameter.vLastPos,
																	 pParentState->GetTerrainHitTestType(),
																	 &( pParentState->selectedIndices ) ) )
		{
			if ( !pParentState->selectedIndices.empty() )
			{
				pParentState->nSelectedIndex = 0;
				pParentState->nCurrentVSO = pParentState->selectedIndices[pParentState->nSelectedIndex];
			}
			else
			{
				pParentState->addPoints.clear();
				std::string szVSODescName;
				if ( pParentState->GetTabVOVSODialog( pFrame )->GetDescriptionName( &szVSODescName ) )
				{
					pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );
					pParentState->SetActiveState( CVSOState::STATE_ADD );
				}
			}
			pFrame->RedrawWindow();
		}
	}
}

void CVSOSelectState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		if ( !pParentState->selectedIndices.empty() )
		{
			pParentState->ValidateSelectedIndex();
			pParentState->nCurrentVSO = pParentState->selectedIndices[pParentState->nSelectedIndex];
			pParentState->selectedIndices.clear();
			pParentState->nSelectedIndex = CVSOState::INVALID_INDEX;
			
			pParentState->SetActiveState( CVSOState::STATE_EDIT );

			pFrame->RedrawWindow();
		}
	}
}

void CVSOSelectState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		if ( !pParentState->selectedIndices.empty() )
		{
			++( pParentState->nSelectedIndex );
			pParentState->ValidateSelectedIndex();
			pParentState->nCurrentVSO = pParentState->selectedIndices[pParentState->nSelectedIndex];

			pFrame->RedrawWindow();
		}
	}
}

void CVSOSelectState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	/**
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		if ( !pParentState->selectedIndices.empty() )
		{
			pParentState->ValidateSelectedIndex();
			pParentState->nCurrentVSO = pParentState->selectedIndices[pParentState->nSelectedIndex];
		}
	}
	/**/
}

void CVSOEditState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		if ( ( nFlags & MK_LBUTTON ) &&
				 pParentState->activePoint.isValid && 
				 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].isValid )
		{
			pFrame->SetMapModified();
			SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();
			
			if ( pParentState->activePoint.bControlPoint )
			{
				pParentState->backupKeyPoints.LoadKeyPoints( pCurrentVSO );
				switch( pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.nParameters[0] )
				{
					case CTabVOVSODialog::CW_SINGLE:
					default:
					{
						pCurrentVSO->controlpoints[pParentState->activePoint.nIndex] = pParentState->stateParameter.vLastPos - 
																																					 pParentState->activePoint.vDifference;
						break;
					}
					case CTabVOVSODialog::CW_MULTI:
					{
						if ( ( nFlags & MK_CONTROL ) == 0 )
						{
							for ( int nPointIndex = pParentState->activePoint.nIndex; nPointIndex < pCurrentVSO->controlpoints.size(); ++nPointIndex )
							{
								pCurrentVSO->controlpoints[nPointIndex] += pParentState->stateParameter.vLastPos -
																													 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].vPos;
							}
						}
						else
						{
							for ( int nPointIndex = 0; nPointIndex <= pParentState->activePoint.nIndex; ++nPointIndex )
							{
								pCurrentVSO->controlpoints[nPointIndex] += pParentState->stateParameter.vLastPos -
																													 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].vPos;
							}
						}
						pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame );
						break;
					}
					case CTabVOVSODialog::CW_ALL:
					{
						for ( int nPointIndex = 0; nPointIndex < pCurrentVSO->controlpoints.size(); ++nPointIndex )
						{
							pCurrentVSO->controlpoints[nPointIndex] += pParentState->stateParameter.vLastPos -
																												 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].vPos;
						}
						pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame );
						break;
					}
				}
			}
			else	
			{
				CVec3 vShift = pParentState->stateParameter.vLastPos - pCurrentVSO->points[pParentState->activePoint.nIndex].vPos - pParentState->activePoint.vDifference;
				float fNewWidth = fabs( pCurrentVSO->points[pParentState->activePoint.nIndex].vNorm * ( vShift * pCurrentVSO->points[pParentState->activePoint.nIndex].vNorm ) );
				switch( pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.nParameters[0] )
				{
					case CTabVOVSODialog::CW_SINGLE:
					default:
					{
						pParentState->backupKeyPoints.LoadKeyPoints( pCurrentVSO );
						pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth = fNewWidth;
						break;
					}
					case CTabVOVSODialog::CW_MULTI:
					{
						pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth = fNewWidth;
						pParentState->backupKeyPoints.SaveKeyPoints( *pCurrentVSO );
						break;
					}
					case CTabVOVSODialog::CW_ALL:
					{
						for ( int nPointsIndex = 0; nPointsIndex < pCurrentVSO->points.size(); ++nPointsIndex )
						{
							pCurrentVSO->points[nPointsIndex].fWidth = fNewWidth;
						}
						pParentState->backupKeyPoints.SaveKeyPoints( *pCurrentVSO );
						break;
					}
				}
				pParentState->GetTabVOVSODialog( pFrame )->SetWidth( int( 0.5f + ( fNewWidth * 2.0f / fWorldCellSize ) ) );
			}
			
			CVSOBuilder::Update( pCurrentVSO,
													 true,
													 CVSOBuilder::DEFAULT_STEP,
													 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
													 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
			CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, pCurrentVSO );
			pParentState->UpdateVSO( pCurrentVSO->nID );
		}
		else if ( ( nFlags & MK_RBUTTON ) &&
						  pParentState->activePoint.isValid && 
						  pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].isValid )
		{
			if ( !pParentState->activePoint.bControlPoint )
			{
				SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();

				int nShift = pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_RBUTTONDOWN].point.y - pParentState->stateParameter.lastPoint.y;
				if ( pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.nParameters[0] == CTabVOVSODialog::CW_ALL )
				{
					for ( int nPointIndex = 0; nPointIndex < pCurrentVSO->points.size(); ++nPointIndex )
					{
						pCurrentVSO->points[nPointIndex].fOpacity = pParentState->activePoint.fOpacity + ( nShift / CVSOState::OPACITY_DELIMITER );
						if ( pCurrentVSO->points[nPointIndex].fOpacity < 0.0f )
						{
							pCurrentVSO->points[nPointIndex].fOpacity = 0.0f;
						}
						else if ( pCurrentVSO->points[nPointIndex].fOpacity > 1.0f )
						{
							pCurrentVSO->points[nPointIndex].fOpacity = 1.0f;
						}
					}
				}
				else
				{
					pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity = pParentState->activePoint.fOpacity + ( nShift / CVSOState::OPACITY_DELIMITER );
					if ( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity < 0.0f )
					{
						pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity = 0.0f;
					}
					else if ( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity > 1.0f )
					{
						pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity = 1.0f;
					}
				}
				
				pParentState->GetTabVOVSODialog( pFrame )->SetOpacity( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity );
				pParentState->UpdateVSO( pCurrentVSO->nID );
			}
		}
		
		pFrame->RedrawWindow();
	}
}

void CVSOEditState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		pParentState->activePoint.isValid = false;
		pParentState->backupKeyPoints.Clear();

		SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();

		for ( int nControlPointsIndex = 0; nControlPointsIndex < pCurrentVSO->controlpoints.size(); ++nControlPointsIndex )
		{
			pParentState->activePoint.vDifference = pParentState->stateParameter.vLastPos - pCurrentVSO->controlpoints[nControlPointsIndex];
			if ( fabs( pParentState->activePoint.vDifference ) <= CVSOState::CONTROL_POINT_RADIUS )
			{
				pParentState->backupKeyPoints.SaveKeyPoints( *pCurrentVSO );

				pParentState->activePoint.nIndex = nControlPointsIndex;
				pParentState->activePoint.bControlPoint = true;
				pParentState->activePoint.isValid = true;

				if ( pParentState->NeedUpdateAI() )
				{
					if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
					{
						pAIEditor->DeleteRiver( *pCurrentVSO );
					}
				}
				pFrame->RedrawWindow();
				return;
			}
		}

		for ( int nPointsIndex = 0; nPointsIndex < pCurrentVSO->points.size(); ++nPointsIndex )
		{
			if ( pCurrentVSO->points[nPointsIndex].bKeyPoint )
			{
				CVec3 vBegin = pCurrentVSO->points[nPointsIndex].vPos + pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vBegin );
				pParentState->activePoint.vDifference = pParentState->stateParameter.vLastPos - vBegin;
				if ( fabs( pParentState->activePoint.vDifference ) <= CVSOState::KEY_POINT_RADIUS )
				{
					pParentState->backupKeyPoints.SaveKeyPoints( *pCurrentVSO );

					pParentState->activePoint.nIndex = nPointsIndex;
					pParentState->activePoint.fOpacity = pCurrentVSO->points[nPointsIndex].fOpacity;
					pParentState->activePoint.bControlPoint = false;
					pParentState->activePoint.bBegin = true;
					pParentState->activePoint.isValid = true;

					pParentState->GetTabVOVSODialog( pFrame )->SetOpacity( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity );
					pParentState->GetTabVOVSODialog( pFrame )->SetWidth( int( 0.5f + ( pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth * 2.0f / fWorldCellSize ) ) );

					if ( pParentState->NeedUpdateAI() )
					{
						if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
						{
							pAIEditor->DeleteRiver( *pCurrentVSO );
						}
					}
					pFrame->RedrawWindow();
					return;
				}

				CVec3 vEnd = pCurrentVSO->points[nPointsIndex].vPos - pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vEnd );
				pParentState->activePoint.vDifference = pParentState->stateParameter.vLastPos - vEnd;
				if ( fabs( pParentState->activePoint.vDifference ) <= CVSOState::KEY_POINT_RADIUS )
				{
					pParentState->backupKeyPoints.SaveKeyPoints( *pCurrentVSO );
					
					pParentState->activePoint.nIndex = nPointsIndex;
					pParentState->activePoint.fOpacity = pCurrentVSO->points[nPointsIndex].fOpacity;
					pParentState->activePoint.bControlPoint = false;
					pParentState->activePoint.bBegin = false;
					pParentState->activePoint.isValid = true;

					pParentState->GetTabVOVSODialog( pFrame )->SetOpacity( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity );
					pParentState->GetTabVOVSODialog( pFrame )->SetWidth( int( 0.5f + ( pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth * 2.0f / fWorldCellSize ) ) );

					if ( pParentState->NeedUpdateAI() )
					{
						if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
						{
							pAIEditor->DeleteRiver( *pCurrentVSO );
						}
					}
					pFrame->RedrawWindow();
					return;
				}
			}
		}

		pParentState->selectedIndices.clear();
		if ( CMapInfo::TerrainHitTest( rTerrainInfo,
																	 pParentState->stateParameter.vLastPos,
																	 pParentState->GetTerrainHitTestType(),
																	 &( pParentState->selectedIndices ) ) )
		{
			if ( !pParentState->selectedIndices.empty() )
			{
				bool isCurrentVSONotPresent = true;
				for ( int nSelectedVSOIndex = 0; nSelectedVSOIndex < pParentState->selectedIndices.size(); ++nSelectedVSOIndex )
				{					
					if ( pParentState->nCurrentVSO == pParentState->selectedIndices[nSelectedVSOIndex] )
					{
						isCurrentVSONotPresent = false;
						break;
					}
				}	
				
				if ( isCurrentVSONotPresent )
				{
					pParentState->nSelectedIndex = 0;
					pParentState->nCurrentVSO = pParentState->selectedIndices[pParentState->nSelectedIndex];
					pParentState->SetActiveState( CVSOState::STATE_SELECT );

					pFrame->RedrawWindow();
				}
			}
		}
	}
}

void CVSOEditState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_RBUTTONDOWN, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		pParentState->activePoint.isValid = false;
		pParentState->backupKeyPoints.Clear();

		SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();

		for ( int nPointsIndex = 0; nPointsIndex < pCurrentVSO->points.size(); ++nPointsIndex )
		{
			if ( pCurrentVSO->points[nPointsIndex].bKeyPoint )
			{
				CVec3 vBegin = pCurrentVSO->points[nPointsIndex].vPos + pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vBegin );
				pParentState->activePoint.vDifference = pParentState->stateParameter.vLastPos - vBegin;
				if ( fabs( pParentState->activePoint.vDifference ) <= CVSOState::KEY_POINT_RADIUS )
				{
					pParentState->activePoint.nIndex = nPointsIndex;
					pParentState->activePoint.fOpacity = pCurrentVSO->points[nPointsIndex].fOpacity;
					pParentState->activePoint.bControlPoint = false;
					pParentState->activePoint.bBegin = true;
					pParentState->activePoint.isValid = true;
					
					pParentState->GetTabVOVSODialog( pFrame )->SetOpacity( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity );
					pParentState->GetTabVOVSODialog( pFrame )->SetWidth( int( 0.5f + ( pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth * 2.0f / fWorldCellSize ) ) );
					
					pFrame->RedrawWindow();
					return;
				}

				CVec3 vEnd = pCurrentVSO->points[nPointsIndex].vPos - pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vEnd );
				pParentState->activePoint.vDifference = pParentState->stateParameter.vLastPos - vEnd;
				if ( fabs( pParentState->activePoint.vDifference ) <= CVSOState::KEY_POINT_RADIUS )
				{
					pParentState->activePoint.nIndex = nPointsIndex;
					pParentState->activePoint.fOpacity = pCurrentVSO->points[nPointsIndex].fOpacity;
					pParentState->activePoint.bControlPoint = false;
					pParentState->activePoint.bBegin = false;
					pParentState->activePoint.isValid = true;

					pParentState->GetTabVOVSODialog( pFrame )->SetOpacity( pCurrentVSO->points[pParentState->activePoint.nIndex].fOpacity );
					pParentState->GetTabVOVSODialog( pFrame )->SetWidth( int( 0.5f + ( pCurrentVSO->points[pParentState->activePoint.nIndex].fWidth * 2.0f / fWorldCellSize ) ) );

					pFrame->RedrawWindow();
					return;
				}
			}
		}
	}
}

void CVSOEditState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		if ( pParentState->activePoint.isValid )
		{
			if ( pParentState->NeedUpdateAI() )
			{
				SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();
				if ( pCurrentVSO )
				{
					if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
					{
						pAIEditor->AddRiver( *pCurrentVSO );
					}
				}
			}
		}
		pParentState->activePoint.isValid = false;
		pParentState->backupKeyPoints.Clear();

		pFrame->RedrawWindow();
	}
}

void CVSOEditState::OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_RBUTTONUP, rMousePoint, pFrame ) )
	{
		pParentState->activePoint.isValid = false;
		pParentState->backupKeyPoints.Clear();

		pFrame->RedrawWindow();
	}
}

void CVSOEditState::OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDBLCLK, rMousePoint, pFrame ) )
	{
		pParentState->activePoint.isValid = false;
		pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
		pParentState->SetActiveState( CVSOState::STATE_SELECT );
		
		pFrame->RedrawWindow();
	}
}

void CVSOEditState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		if ( nChar == VK_INSERT ) 
		{
			if ( pParentState->activePoint.isValid && 
					 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].isValid )
			{
				pFrame->SetMapModified();
				if ( pParentState->activePoint.bControlPoint )
				{
					SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();

					CVec3 vBegin = pCurrentVSO->controlpoints[pParentState->activePoint.nIndex];
					if ( pParentState->activePoint.nIndex < ( pCurrentVSO->controlpoints.size() - 1 ) )
					{
						CVec3 vEnd = pCurrentVSO->controlpoints[pParentState->activePoint.nIndex + 1];
						CVec3 vNewControlPoint = ( vBegin + vEnd ) / 2.0f;
						pCurrentVSO->controlpoints.insert( pCurrentVSO->controlpoints.begin() + ( pParentState->activePoint.nIndex + 1 ), vNewControlPoint );
						pParentState->backupKeyPoints.AddKeyPoint( ( pParentState->activePoint.nIndex + 1 ), pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f, pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					}
					else
					{
						CVec3 vEnd = pCurrentVSO->controlpoints[pParentState->activePoint.nIndex - 1];
						CVec3 vNewControlPoint = ( vBegin + vEnd ) / 2.0f;
						pCurrentVSO->controlpoints.insert( pCurrentVSO->controlpoints.begin() + ( pParentState->activePoint.nIndex + 0 ), vNewControlPoint );
						pParentState->backupKeyPoints.AddKeyPoint( ( pParentState->activePoint.nIndex + 0 ), pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f, pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );

						++( pParentState->activePoint.nIndex );
					}

					CVSOBuilder::Update( pCurrentVSO,
															 true,
															 CVSOBuilder::DEFAULT_STEP,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					pParentState->backupKeyPoints.LoadKeyPoints( pCurrentVSO );
					CVSOBuilder::Update( pCurrentVSO,
															 true,
															 CVSOBuilder::DEFAULT_STEP,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, pCurrentVSO );
					pParentState->UpdateVSO( pCurrentVSO->nID );

					pFrame->RedrawWindow();
					return;
				}
			}
		}
		else if ( nChar == VK_DELETE ) 
		{
			SVectorStripeObject *pCurrentVSO = pParentState->GetCurrentVSO();

			if ( pParentState->activePoint.isValid && 
					 pParentState->stateParameter.mouseEvents[CInputStateParameter::ISE_LBUTTONDOWN].isValid )
			{
				pFrame->SetMapModified();
				if ( pParentState->activePoint.bControlPoint && ( pCurrentVSO->controlpoints.size() > 2 ) )
				{
					pCurrentVSO->controlpoints.erase( pCurrentVSO->controlpoints.begin() + pParentState->activePoint.nIndex );
					pParentState->backupKeyPoints.RemoveKeyPoint( pParentState->activePoint.nIndex );

					CVSOBuilder::Update( pCurrentVSO,
															 true,
															 CVSOBuilder::DEFAULT_STEP,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					pParentState->backupKeyPoints.LoadKeyPoints( pCurrentVSO );
					CVSOBuilder::Update( pCurrentVSO,
															 true,
															 CVSOBuilder::DEFAULT_STEP,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, pCurrentVSO );
					pParentState->UpdateVSO( pCurrentVSO->nID );
					
					pParentState->activePoint.isValid = false;
				}
				
				pFrame->RedrawWindow();
				return;
			}
			else
			{
				pParentState->RemoveVSO( pCurrentVSO->nID );
				pCurrentVSO = 0;

				pParentState->activePoint.isValid = false;
				pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
				pParentState->SetActiveState( CVSOState::STATE_SELECT );
				
				pFrame->RedrawWindow();
				return;
			}
		}
		else if ( ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) )
		{
			pParentState->activePoint.isValid = false;
			pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
			pParentState->SetActiveState( CVSOState::STATE_SELECT );
			
			pFrame->RedrawWindow();
			return;
		}

	}
}

void CVSOAddState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		pParentState->addPoints.pop_back();
		pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );

		pFrame->RedrawWindow();
	}
}

void CVSOAddState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );

		pFrame->RedrawWindow();
	}
}

void CVSOAddState::OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDBLCLK, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );
		
		SVectorStripeObject newVSO;
		pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
		if ( pParentState->CreateVSO( &newVSO ) )
		{
			if ( newVSO.controlpoints.size() > 1 )
			{
				pFrame->SetMapModified();
				CVSOBuilder::Update( &newVSO,
														 false,
														 CVSOBuilder::DEFAULT_STEP,
														 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
														 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &newVSO );
				pParentState->AddVSO( newVSO );
			}
		}
		pParentState->addPoints.clear();

		if ( pParentState->nCurrentVSO != CVSOState::INVALID_INDEX )
		{
			pParentState->SetActiveState( CVSOState::STATE_EDIT );
		}
		else
		{
			pParentState->SetActiveState( CVSOState::STATE_SELECT );
		}

		pFrame->RedrawWindow();
	}
}

void CVSOAddState::OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_RBUTTONUP, rMousePoint, pFrame ) )
	{
		if ( pParentState->addPoints.size() > 1 )
		{
			pParentState->addPoints.pop_back();
			pParentState->addPoints.pop_back();
			pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );
		}
		else
		{
			pParentState->addPoints.clear();
			pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
			pParentState->SetActiveState( CVSOState::STATE_SELECT );
		}

		pFrame->RedrawWindow();
	}
}

void CVSOAddState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		if ( ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) )
		{
			pParentState->addPoints.push_back( pParentState->stateParameter.vLastPos );
			
			SVectorStripeObject newVSO;
			pParentState->nCurrentVSO = CVSOState::INVALID_INDEX;
			if ( pParentState->CreateVSO( &newVSO ) )
			{
				if ( newVSO.controlpoints.size() > 1 )
				{
					pFrame->SetMapModified();
					CVSOBuilder::Update( &newVSO,
															 false,
															 CVSOBuilder::DEFAULT_STEP,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
															 pParentState->GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &newVSO );
					pParentState->AddVSO( newVSO );
				}
			}
			pParentState->addPoints.clear();

			if ( pParentState->nCurrentVSO != CVSOState::INVALID_INDEX )
			{
				pParentState->SetActiveState( CVSOState::STATE_EDIT );
			}
			else
			{
				pParentState->SetActiveState( CVSOState::STATE_SELECT );
			}
		
			pFrame->RedrawWindow();
			return;
		}

	}
}

void CVSOState::Enter()
{
	SetActiveState( STATE_SELECT );
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CVSOState::Leave()
{
	activePoint.isValid = false;
	backupKeyPoints.Clear();

	addPoints.clear();
	nCurrentVSO = INVALID_INDEX;
	SetActiveState( CVSOState::STATE_SELECT );

	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}

}

void CVSOState::Draw( CTemplateEditorFrame* pFrame )
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				/**
				if ( GetActiveState() == STATE_SELECT )
				{
				}
				else if ( GetActiveState() == STATE_EDIT )
				{
				}
				else if ( GetActiveState() == STATE_ADD )
				{
				}
				/**/

				if ( ( GetActiveState() == STATE_SELECT ) || ( GetActiveState() == STATE_EDIT ) )
				{
					if ( IsCurrentVSOValid() )
					{
						SVectorStripeObject *pCurrentVSO = GetCurrentVSO();

						for ( int nPointsIndex = 0; nPointsIndex < pCurrentVSO->points.size(); ++nPointsIndex )
						{
							CVec3 vBegin = pCurrentVSO->points[nPointsIndex].vPos +	pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
							CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vBegin );
							CVec3 vEnd = pCurrentVSO->points[nPointsIndex].vPos - pCurrentVSO->points[nPointsIndex].vNorm * pCurrentVSO->points[nPointsIndex].fWidth;
							CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vEnd );
							if ( pCurrentVSO->points[nPointsIndex].bKeyPoint )
							{
								sceneDrawTool.DrawLine( vBegin, vEnd, KEY_POINT_COLOR );
								sceneDrawTool.DrawCircle( vBegin, KEY_POINT_RADIUS, KEY_POINT_PARTS, KEY_POINT_COLOR );
								sceneDrawTool.DrawCircle( vEnd, KEY_POINT_RADIUS, KEY_POINT_PARTS, KEY_POINT_COLOR );
							}
							else
							{
								sceneDrawTool.DrawLine( vBegin, vEnd, POINT_COLOR );
							}
							if ( nPointsIndex > 0 )
							{
								sceneDrawTool.DrawLine( pCurrentVSO->points[nPointsIndex - 1].vPos, pCurrentVSO->points[nPointsIndex].vPos, POINT_COLOR );
							}
						}
						
						for ( int nControlPointsIndex = 0; nControlPointsIndex < pCurrentVSO->controlpoints.size(); ++nControlPointsIndex )
						{
							sceneDrawTool.DrawCircle( pCurrentVSO->controlpoints[nControlPointsIndex], CONTROL_POINT_RADIUS, CONTROL_POINT_PARTS, CONTROL_POINT_COLOR );
						}
						sceneDrawTool.DrawPolyLine( pCurrentVSO->controlpoints, CONTROL_POINT_COLOR );
						
						if ( activePoint.isValid )
						{
							if ( activePoint.bControlPoint )
							{
								if ( ( activePoint.nIndex ) >= 0 && ( activePoint.nIndex < pCurrentVSO->controlpoints.size() ) )
								{
									float fRadius = CONTROL_POINT_RADIUS - 1.0f;
									while( fRadius > 0.0f )
									{
										sceneDrawTool.DrawCircle( pCurrentVSO->controlpoints[activePoint.nIndex], fRadius, CONTROL_POINT_PARTS, CONTROL_POINT_COLOR );
										fRadius -=1.0f;
									}
								}
							}
							else
							{
								if ( ( activePoint.nIndex ) >= 0 && ( activePoint.nIndex < pCurrentVSO->points.size() ) )
								{		
									float fRadius = KEY_POINT_RADIUS - 1.0f;
									if ( activePoint.bBegin )
									{
										CVec3 vBegin = pCurrentVSO->points[activePoint.nIndex].vPos + pCurrentVSO->points[activePoint.nIndex].vNorm * pCurrentVSO->points[activePoint.nIndex].fWidth;
										CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vBegin );
										while( fRadius > 0.0f )
										{
											sceneDrawTool.DrawCircle( vBegin, fRadius, KEY_POINT_PARTS, KEY_POINT_COLOR );
											fRadius -=1.0f;
										}
									}
									else
									{
										CVec3 vEnd = pCurrentVSO->points[activePoint.nIndex].vPos - pCurrentVSO->points[activePoint.nIndex].vNorm * pCurrentVSO->points[activePoint.nIndex].fWidth;
										CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vEnd );
										while( fRadius > 0.0f )
										{
											sceneDrawTool.DrawCircle( vEnd, fRadius, KEY_POINT_PARTS, KEY_POINT_COLOR );
											fRadius -=1.0f;
										}
									}
								}
							}
						}
					}
				}
				else if ( GetActiveState() == STATE_ADD )
				{
					for ( std::list<CVec3>::const_iterator addPontsIterator = addPoints.begin(); addPontsIterator != addPoints.end(); ++addPontsIterator )
					{
						sceneDrawTool.DrawCircle( *addPontsIterator, CONTROL_POINT_RADIUS, CONTROL_POINT_PARTS, CONTROL_POINT_COLOR );
					}	
					if ( addPoints.size() > 1 )
					{
						sceneDrawTool.DrawPolyLine( addPoints, CONTROL_POINT_COLOR );
					}
				}

				sceneDrawTool.DrawToScene();
			}
		}
	}
}

void CVSOState::Update()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				if ( ( GetActiveState() == STATE_EDIT ) && IsCurrentVSOValid() )
				{
					if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
					{
						if ( GetTabVOVSODialog( pFrame )->bWidthChanged )
						{
							if ( GetTabVOVSODialog( pFrame )->resizeDialogOptions.nParameters[0] == CTabVOVSODialog::CW_ALL )
							{
								SVectorStripeObject *pCurrentVSO = GetCurrentVSO();

								for ( int nPointsIndex = 0; nPointsIndex < pCurrentVSO->points.size(); ++nPointsIndex )
								{
									pCurrentVSO->points[nPointsIndex].fWidth = GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f;
									pCurrentVSO->points[nPointsIndex].fOpacity = GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1];
								}
								backupKeyPoints.SaveKeyPoints( *pCurrentVSO );
								CVSOBuilder::Update( pCurrentVSO,
																		 true,
																		 CVSOBuilder::DEFAULT_STEP,
																		 GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[0] * fWorldCellSize / 2.0f,
																		 GetTabVOVSODialog( pFrame )->resizeDialogOptions.fParameters[1] );
								CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, pCurrentVSO );
								UpdateVSO( pCurrentVSO->nID );
							}
						}

						pFrame->RedrawWindow();
					}
				}
			}
		}
	}
}

bool CRoads3DState::CreateVSO( SVectorStripeObject *pRoad3D )
{
	NI_ASSERT_T( pRoad3D != 0,
							 NStr::Format( "Wrong parameter pRoad3D: %x\n", pRoad3D ) );
	
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		std::string szRoad3DDescName;
		if ( !GetTabVOVSODialog( pFrame )->GetDescriptionName( &szRoad3DDescName ) )
		{
			return false;
		}
		CVSOBuilder::CreateVSO( pRoad3D, szRoad3DDescName, addPoints );
		return true;
	}
	return false;
}

void CRoads3DState::AddVSO( const SVectorStripeObject &rVSO )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	nCurrentVSO = INVALID_INDEX;
	int nNewRoad3DID = pTerrainEditor->AddRoad( rVSO );
	for ( int nRoad3DIndex = 0; nRoad3DIndex < rTerrainInfo.roads3.size(); ++nRoad3DIndex )
	{
		if ( rTerrainInfo.roads3[nRoad3DIndex].nID == nNewRoad3DID )
		{
			nCurrentVSO = nRoad3DIndex;
			break;
		}
	}
}
	
void CRoads3DState::UpdateVSO( int nID )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );

	pTerrainEditor->UpdateRoad( nID );
}

void CRoads3DState::RemoveVSO( int nID )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	
	pTerrainEditor->RemoveRoad( nID );
}

SVectorStripeObject* CRoads3DState::GetVSO( int nVSOIndex )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return &( rTerrainInfo.roads3[nVSOIndex] );
}
	
SVectorStripeObject* CRoads3DState::GetCurrentVSO()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return &( rTerrainInfo.roads3[nCurrentVSO] );
}

bool CRoads3DState::IsCurrentVSOValid()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return ( ( nCurrentVSO >= 0 ) && ( nCurrentVSO < rTerrainInfo.roads3.size() ) );
}

CTabVOVSODialog* CRoads3DState::GetTabVOVSODialog( CTemplateEditorFrame *pFrame )
{
	return pFrame->m_mapEditorBarPtr->GetRoads3DTab();
}

CMapInfo::TERRAIN_HIT_TEST_TYPE CRoads3DState::GetTerrainHitTestType()
{
	return CMapInfo::THT_ROADS3D;
}

bool CRiversState::CreateVSO( SVectorStripeObject *pRiver )
{
	NI_ASSERT_T( pRiver != 0,
							 NStr::Format( "Wrong parameter: %x\n", pRiver ) );

	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		std::string szRiverDescName;
		if ( !GetTabVOVSODialog( pFrame )->GetDescriptionName( &szRiverDescName ) )
		{
			return false;
		}
		CVSOBuilder::CreateVSO( pRiver, szRiverDescName, addPoints );
		return true;
	}
	return false;
}

void CRiversState::AddVSO( const SVectorStripeObject &rVSO )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
	{
		pAIEditor->AddRiver( rVSO );
	}

	nCurrentVSO = INVALID_INDEX;
	int nNewRiverID = pTerrainEditor->AddRiver( rVSO );
	for ( int nRiverIndex = 0; nRiverIndex < rTerrainInfo.rivers.size(); ++nRiverIndex )
	{
		if ( rTerrainInfo.rivers[nRiverIndex].nID == nNewRiverID )
		{
			nCurrentVSO = nRiverIndex;
			break;
		}
	}
}

void CRiversState::UpdateVSO( int nID )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );

	pTerrainEditor->UpdateRiver( nID );
}

void CRiversState::RemoveVSO( int nID )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	for ( int nRiverIndex = 0; nRiverIndex < rTerrainInfo.rivers.size(); ++nRiverIndex )
	{
		if ( rTerrainInfo.rivers[nRiverIndex].nID == nID )
		{
			if ( CPtr<IAIEditor> pAIEditor = GetSingleton<IAIEditor>() )
			{
				pAIEditor->DeleteRiver( rTerrainInfo.rivers[nRiverIndex] );
			}
			break;
		}
	}
	pTerrainEditor->RemoveRiver( nID );
}

SVectorStripeObject* CRiversState::GetVSO( int nVSOIndex )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return &( rTerrainInfo.rivers[nVSOIndex] );
}
	
SVectorStripeObject* CRiversState::GetCurrentVSO()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return &( rTerrainInfo.rivers[nCurrentVSO] );
}

bool CRiversState::IsCurrentVSOValid()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	return ( ( nCurrentVSO >= 0 ) && ( nCurrentVSO < rTerrainInfo.rivers.size() ) );
}

CTabVOVSODialog* CRiversState::GetTabVOVSODialog( CTemplateEditorFrame *pFrame )
{
	return pFrame->m_mapEditorBarPtr->GetRiversTab();
}
	
CMapInfo::TERRAIN_HIT_TEST_TYPE CRiversState::GetTerrainHitTestType()
{
	return CMapInfo::THT_RIVERS;
}

