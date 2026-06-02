#include "stdafx.h"
#include "editor.h"
#include "StateAIGeneral.h"

#include "..\AILogic\AIConsts.h"

#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "PropertieDialog.h"
#include "TabAIGeneralSetPositionTypeDialog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

const int CAIGState::INVALID_INDEX = -1;
const float CAIGState::PARCEL_POINT_RADIUS = fWorldCellSize * 4.0f;
const float CAIGState::PARCEL_POINT_RADIUS_ARROW = fWorldCellSize;
const int CAIGState::PARCEL_POINT_PARTS = 64;

const float CAIGState::PARCEL_CENTER_POINT_RADIUS = fWorldCellSize;
const int CAIGState::PARCEL_CENTER_POINT_PARTS = 16;

const float CAIGState::PARCEL_ARROW_POINT_RADIUS = fWorldCellSize / 2.0f;
const int CAIGState::PARCEL_ARROW_POINT_PARTS = 16;

const float CAIGState::POSITION_POINT_RADIUS = fWorldCellSize * 1.5f;
const float CAIGState::POSITION_POINT_RADIUS_ARROW = fWorldCellSize;
const int CAIGState::POSITION_POINT_PARTS = 32;

const float CAIGState::POSITION_CENTER_POINT_RADIUS = fWorldCellSize / 1.5f;
const int CAIGState::POSITION_CENTER_POINT_PARTS = 16;

const float CAIGState::POSITION_ARROW_POINT_RADIUS = fWorldCellSize / 2.0f;
const int CAIGState::POSITION_ARROW_POINT_PARTS = 16;

const SColor CAIGState::PARCEL_COLORS[3] = { SColor( 0xFF, 0xFF, 0x80, 0x80 ), SColor( 0xFF, 0x80, 0xFF, 0x80 ), SColor( 0xFF, 0xFF, 0xFF, 0x80 ) };
const SColor CAIGState::POSITION_COLORS[3] = { SColor( 0xFF, 0xFF, 0x20, 0x20 ), SColor( 0xFF, 0x20, 0xFF, 0x20 ), SColor( 0xFF, 0xFF, 0xFF, 0x20 ) };

void CAIGSelectState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		
		pParentState->activePoint.isValid = false;
		
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		int nSide = pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->m_nSide;
		if ( ( nSide >= 0 ) && ( nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[nSide];
	
			for ( int nParcelIndex = 0; nParcelIndex < rAIGeneralSideInfo.parcels.size(); ++nParcelIndex )
			{
				const SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[nParcelIndex];

				CVec3 vParcelBegin = CVec3( rAIGeneralParcelInfo.vCenter, 0.0f );
				AI2Vis( &vParcelBegin );
				float fRadius = rAIGeneralParcelInfo.fRadius * fAITileXCoeff;
				float fParcelAngle = ( rAIGeneralParcelInfo.wDefenceDirection * FP_2PI / 0xFFFF );
				CVec3 vParcelEnd = vParcelBegin + CVec3( CreateFromPolarCoord( fRadius, fParcelAngle + FP_PI2 ), 0.0f );
				CVec3 vParcelArrowEnd = vParcelBegin + CVec3( CreateFromPolarCoord( fRadius + CAIGState::PARCEL_POINT_RADIUS_ARROW, fParcelAngle + FP_PI2 ), 0.0f );

				CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelBegin.x, vParcelBegin.y, &( vParcelBegin.z ) );
				CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelEnd.x, vParcelEnd.y, &( vParcelEnd.z ) );
				CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelArrowEnd.x, vParcelArrowEnd.y, &( vParcelArrowEnd.z ) );

				for ( int nPositionIndex = 0; nPositionIndex < rAIGeneralParcelInfo.reinforcePoints.size(); ++nPositionIndex )
				{
					const SAIGeneralParcelInfo::SReinforcePointInfo &rPositionInfo = rAIGeneralParcelInfo.reinforcePoints[nPositionIndex];

					CVec3 vBegin = CVec3( rPositionInfo.vCenter, 0.0f );
					AI2Vis( &vBegin );
					float fAngle = ( rPositionInfo.wDir * FP_2PI / 0xFFFF );
					CVec3 vEnd = vBegin + CVec3 ( CreateFromPolarCoord( CAIGState::POSITION_POINT_RADIUS, fAngle + FP_PI2 ), 0.0f );
					CVec3 vArrowEnd = vBegin + CVec3( CreateFromPolarCoord( CAIGState::POSITION_POINT_RADIUS + CAIGState::PARCEL_POINT_RADIUS_ARROW, fAngle + FP_PI2 ), 0.0f );

					RotatePoint( &vBegin, fParcelAngle );
					RotatePoint( &vEnd, fParcelAngle );
					RotatePoint( &vArrowEnd, fParcelAngle );
					vBegin += vParcelBegin;
					vEnd += vParcelBegin;
					vArrowEnd += vParcelBegin;
					
					CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vBegin.x, vBegin.y, &( vBegin.z ) );
					CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vEnd.x, vEnd.y, &( vEnd.z ) );
					CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vArrowEnd.x, vArrowEnd.y, &( vArrowEnd.z ) );

					pParentState->activePoint.vDifference = vEnd - pParentState->stateParameter.vLastPos;
					if ( fabs2( pParentState->activePoint.vDifference ) < fabs2( CAIGState::POSITION_ARROW_POINT_RADIUS ) )
					{
						pParentState->nCurrentParcel = nParcelIndex;
						pParentState->activePoint.isValid = true;
						pParentState->activePoint.nIndex = nPositionIndex;
						pParentState->activePoint.bParcel = false;
						pParentState->activePoint.bArrow = true;
						break;
					}
					else
					{
						pParentState->activePoint.vDifference = vBegin - pParentState->stateParameter.vLastPos;
						if ( fabs2( pParentState->activePoint.vDifference ) < fabs2( CAIGState::POSITION_CENTER_POINT_RADIUS ) )
						{
							pParentState->nCurrentParcel = nParcelIndex;
							pParentState->activePoint.isValid = true;
							pParentState->activePoint.nIndex = nPositionIndex;
							pParentState->activePoint.bParcel = false;
							pParentState->activePoint.bArrow = false;
							break;
						}
					}
				}
				if ( !pParentState->activePoint.isValid )
				{
					pParentState->activePoint.vDifference = vParcelEnd - pParentState->stateParameter.vLastPos;
					if ( fabs2( pParentState->activePoint.vDifference ) < fabs2( CAIGState::PARCEL_ARROW_POINT_RADIUS ) )
					{
						pParentState->nCurrentParcel = nParcelIndex;
						pParentState->activePoint.isValid = true;
						pParentState->activePoint.bParcel = true;
						pParentState->activePoint.bArrow = true;
					}
					else
					{
						pParentState->activePoint.vDifference = vParcelBegin - pParentState->stateParameter.vLastPos;
						if ( fabs2( pParentState->activePoint.vDifference ) < fabs2( CAIGState::PARCEL_CENTER_POINT_RADIUS ) )
						{
							pParentState->nCurrentParcel = nParcelIndex;
							pParentState->activePoint.isValid = true;
							pParentState->activePoint.bParcel = true;
							pParentState->activePoint.bArrow = false;
						}
					}
				}
				if ( pParentState->activePoint.isValid )
				{
					break;
				}
			}

			if ( !pParentState->activePoint.isValid )
			{
				for ( int nParcelIndex = 0; nParcelIndex < rAIGeneralSideInfo.parcels.size(); ++nParcelIndex )
				{
					SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[nParcelIndex];

					CVec3 vPos = CVec3( rAIGeneralParcelInfo.vCenter, 0.0f );
					AI2Vis( &vPos );
					CVec3 vDifference = vPos - pParentState->stateParameter.vLastPos;
					if ( fabs2( vDifference ) < fabs2( rAIGeneralParcelInfo.fRadius * fAITileXCoeff ) )
					{
						rAIGeneralParcelInfo.reinforcePoints.push_back( SAIGeneralParcelInfo::SReinforcePointInfo() );
						rAIGeneralParcelInfo.reinforcePoints.back().vCenter.x = pParentState->stateParameter.vLastPos.x;
						rAIGeneralParcelInfo.reinforcePoints.back().vCenter.y = pParentState->stateParameter.vLastPos.y;
						Vis2AI( &( rAIGeneralParcelInfo.reinforcePoints.back().vCenter ) );
						rAIGeneralParcelInfo.reinforcePoints.back().vCenter -= rAIGeneralParcelInfo.vCenter;
						RotatePoint( &( rAIGeneralParcelInfo.reinforcePoints.back().vCenter ), -( rAIGeneralParcelInfo.wDefenceDirection * FP_2PI / 0xFFFF ) );
						rAIGeneralParcelInfo.reinforcePoints.back().wDir = 0;
						
						pParentState->activePoint.vDifference = VNULL3;
						pParentState->nCurrentParcel = nParcelIndex;
						pParentState->activePoint.isValid = true;
						pParentState->activePoint.nIndex = rAIGeneralParcelInfo.reinforcePoints.size() - 1;
						pParentState->activePoint.bParcel = false;
						pParentState->activePoint.bArrow = false;

						break;
					}
				}
			}
			if ( !pParentState->activePoint.isValid )
			{
				rAIGeneralSideInfo.parcels.push_back( SAIGeneralParcelInfo() );
				SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels.back();

				rAIGeneralParcelInfo.vCenter.x = pParentState->stateParameter.vLastPos.x;
				rAIGeneralParcelInfo.vCenter.y = pParentState->stateParameter.vLastPos.y;
				Vis2AI( &( rAIGeneralParcelInfo.vCenter ) );
				rAIGeneralParcelInfo.eType = SAIGeneralParcelInfo::EPATCH_DEFENCE;
				rAIGeneralParcelInfo.fRadius = CAIGState::PARCEL_POINT_RADIUS / fAITileXCoeff;
				rAIGeneralParcelInfo.wDefenceDirection = 0;

				pParentState->activePoint.vDifference = VNULL3;
				pParentState->nCurrentParcel = rAIGeneralSideInfo.parcels.size() - 1;
				pParentState->activePoint.isValid = true;
				pParentState->activePoint.nIndex = 0;
				pParentState->activePoint.bParcel = true;
				pParentState->activePoint.bArrow = false;

				pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->AddPosition( pParentState->nCurrentParcel );
			}
			else
			{
				pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->UpdatePosition( pParentState->nCurrentParcel );
			}
			pParentState->SetActiveState( CAIGState::STATE_EDIT );
		}
		
		pFrame->RedrawWindow();
	}
}

void CAIGSelectState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		pParentState->activePoint.isValid = false;
		pParentState->SetActiveState( CAIGState::STATE_SELECT );

		pFrame->RedrawWindow();
	}
}

void CAIGSelectState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
	}
}

void CAIGEditState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		pParentState->activePoint.isValid = false;
		pParentState->SetActiveState( CAIGState::STATE_SELECT );

		pFrame->RedrawWindow();
	}
}

void CAIGEditState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		pFrame->SetMapModified();
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		int nSide = pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->m_nSide;
		if ( ( nSide >= 0 ) && ( nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[nSide];
			if ( pParentState->activePoint.isValid &&
					 ( pParentState->nCurrentParcel >= 0 ) && 
					 ( pParentState->nCurrentParcel < rAIGeneralSideInfo.parcels.size() ) )
			{
				SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[pParentState->nCurrentParcel];
				
				CVec3 vNewPos = pParentState->stateParameter.vLastPos + pParentState->activePoint.vDifference;
				Vis2AI( &vNewPos );

				if ( pParentState->activePoint.bParcel )
				{
					if ( pParentState->activePoint.bArrow )
					{
						CVec2 vArrowPos( vNewPos.x, vNewPos.y );						
						float fRadius = GetPolarLength( vArrowPos - rAIGeneralParcelInfo.vCenter );
						float fAlpha = GetPolarAngle( vArrowPos - rAIGeneralParcelInfo.vCenter ) - FP_PI2;
						if ( fAlpha < 0 )
						{
							fAlpha += FP_2PI;
						}
						if ( fRadius < ( CAIGState::PARCEL_POINT_RADIUS / fAITileXCoeff ) )
						{
							fRadius = ( CAIGState::PARCEL_POINT_RADIUS / fAITileXCoeff );
						}
						rAIGeneralParcelInfo.fRadius = fRadius;
						rAIGeneralParcelInfo.wDefenceDirection = static_cast<WORD>( fAlpha * 0xFFFF / FP_2PI );
					}
					else
					{
						rAIGeneralParcelInfo.vCenter.x = vNewPos.x;
						rAIGeneralParcelInfo.vCenter.y = vNewPos.y;
					}
					pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->UpdatePosition( pParentState->nCurrentParcel );
				}
				else
				{
					if ( ( pParentState->activePoint.nIndex >= 0 ) && 
							 ( pParentState->activePoint.nIndex < rAIGeneralParcelInfo.reinforcePoints.size() ) )
					{
						SAIGeneralParcelInfo::SReinforcePointInfo &rPositionInfo = rAIGeneralParcelInfo.reinforcePoints[pParentState->activePoint.nIndex];
						
						CVec2 vPos( vNewPos.x , vNewPos.y );
						vPos -= rAIGeneralParcelInfo.vCenter;
						RotatePoint( &vPos, -( rAIGeneralParcelInfo.wDefenceDirection * FP_2PI / 0xFFFF ) );
						
						if ( pParentState->activePoint.bArrow )
						{
							float fAlpha = GetPolarAngle( vPos - rPositionInfo.vCenter ) - FP_PI2;	
							rPositionInfo.wDir = static_cast<WORD>( fAlpha * 0xFFFF / FP_2PI );
						}
						else
						{
							rPositionInfo.vCenter = vPos;
						}
						pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->UpdatePosition( pParentState->nCurrentParcel );
					}
				}
			}
			else
			{
				pParentState->activePoint.isValid = false;
				pParentState->SetActiveState( CAIGState::STATE_SELECT );
			}
		}
		pFrame->RedrawWindow();
	}
}

void CAIGEditState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	if ( pParentState->stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), pFrame ) )
	{
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene->GetTerrain();
		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		int nSide = pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->m_nSide;
		if ( ( nSide >= 0 ) && ( nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[nSide];
			if ( pParentState->activePoint.isValid &&
					 ( pParentState->nCurrentParcel >= 0 ) && 
					 ( pParentState->nCurrentParcel < rAIGeneralSideInfo.parcels.size() ) )
			{
				SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[pParentState->nCurrentParcel];

				if ( ( nChar == VK_INSERT ) || ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) ) 
				{
					pFrame->SetMapModified();
					CTabAIGeneralSetPositionTypeDialog setPositionTypeDialog;
					if ( rAIGeneralParcelInfo.eType == SAIGeneralParcelInfo::EPATCH_DEFENCE )
					{
						setPositionTypeDialog.m_Type = 0;
					}
					else
					{
						setPositionTypeDialog.m_Type = 1;
					}
					if ( setPositionTypeDialog.DoModal() == IDOK )
					{
						if ( 	setPositionTypeDialog.m_Type == 0 )
						{
							rAIGeneralParcelInfo.eType = SAIGeneralParcelInfo::EPATCH_DEFENCE;
						}
						else
						{
							rAIGeneralParcelInfo.eType = SAIGeneralParcelInfo::EPATCH_REINFORCE;
						}
						pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->UpdatePosition( pParentState->nCurrentParcel );
					}
					pParentState->activePoint.isValid = false;
					pParentState->SetActiveState( CAIGState::STATE_SELECT );
					pFrame->RedrawWindow();
					return;
				}
				else if ( nChar == VK_DELETE ) 
				{
					pFrame->SetMapModified();
					if ( pParentState->activePoint.bParcel )
					{
						rAIGeneralSideInfo.parcels.erase( rAIGeneralSideInfo.parcels.begin() + pParentState->nCurrentParcel );
						pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->DeletePosition( pParentState->nCurrentParcel );
						pParentState->activePoint.isValid = false;
						pParentState->SetActiveState( CAIGState::STATE_SELECT );
					}
					else 
					{
						if ( ( pParentState->activePoint.nIndex >= 0 ) && 
								 ( pParentState->activePoint.nIndex < rAIGeneralParcelInfo.reinforcePoints.size() ) )
						{
							rAIGeneralParcelInfo.reinforcePoints.erase( rAIGeneralParcelInfo.reinforcePoints.begin() + pParentState->activePoint.nIndex );
							pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->UpdatePosition( pParentState->nCurrentParcel );
						}
					}
					pParentState->activePoint.isValid = false;
					pParentState->SetActiveState( CAIGState::STATE_SELECT );
					pFrame->RedrawWindow();
					return;
				}
			}
			else
			{
				pParentState->activePoint.isValid = false;
				pParentState->SetActiveState( CAIGState::STATE_SELECT );
			}
		}

	}
}

void CAIGState::Enter()
{
	SetActiveState( STATE_SELECT );
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->LoadAIGReinforcementsInfo();
		pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->LoadAIGPositionsInfo();
		
		pFrame->RedrawWindow();
	}
}
void CAIGState::Leave()
{
	nCurrentParcel = INVALID_INDEX;
	activePoint.isValid = false;
	SetActiveState( CAIGState::STATE_SELECT );

	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CAIGState::Draw( CTemplateEditorFrame* pFrame )
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

				const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
				int nSide = pFrame->m_mapEditorBarPtr->GetAIGeneralTab()->m_nSide;
				if ( ( nSide >= 0 ) && ( nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
				{
					const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[nSide];

					for ( int nParcelIndex = 0; nParcelIndex < rAIGeneralSideInfo.parcels.size(); ++nParcelIndex )
					{
						const SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[nParcelIndex];
						
						SColor parcelColor;
						SColor positionColor;
						switch( rAIGeneralParcelInfo.eType )
						{
							case SAIGeneralParcelInfo::EPATCH_DEFENCE:
							{
								parcelColor = PARCEL_COLORS[1];
								positionColor = POSITION_COLORS[1];
								break;
							}
							case SAIGeneralParcelInfo::EPATCH_REINFORCE:
							{
								parcelColor = PARCEL_COLORS[2];
								positionColor = POSITION_COLORS[2];
								break;
							}
							default:
							{
								parcelColor = PARCEL_COLORS[0];
								positionColor = POSITION_COLORS[0];
								break;
							}
						}

						CVec3 vParcelBegin = CVec3( rAIGeneralParcelInfo.vCenter, 0.0f );
						AI2Vis( &vParcelBegin );
						float fRadius = rAIGeneralParcelInfo.fRadius * fAITileXCoeff;
						float fParcelAngle = ( rAIGeneralParcelInfo.wDefenceDirection * FP_2PI / 0xFFFF );
						CVec3 vParcelEnd = vParcelBegin + CVec3( CreateFromPolarCoord( fRadius, fParcelAngle + FP_PI2 ), 0.0f );
						CVec3 vParcelArrowEnd = vParcelBegin + CVec3( CreateFromPolarCoord( fRadius + PARCEL_POINT_RADIUS_ARROW, fParcelAngle + FP_PI2 ), 0.0f );

						CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelBegin.x, vParcelBegin.y, &( vParcelBegin.z ) );
						CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelEnd.x, vParcelEnd.y, &( vParcelEnd.z ) );
						CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vParcelArrowEnd.x, vParcelArrowEnd.y, &( vParcelArrowEnd.z ) );

						sceneDrawTool.DrawLine( vParcelBegin, vParcelEnd, parcelColor );
						sceneDrawTool.DrawLine( vParcelEnd, vParcelArrowEnd, parcelColor );
						sceneDrawTool.DrawCircle3D( vParcelBegin, fRadius, PARCEL_POINT_PARTS, parcelColor, rTerrainInfo.altitudes );
						sceneDrawTool.DrawCircle3D( vParcelBegin, PARCEL_CENTER_POINT_RADIUS, PARCEL_CENTER_POINT_PARTS, parcelColor, rTerrainInfo.altitudes );
						sceneDrawTool.DrawCircle3D( vParcelEnd, PARCEL_ARROW_POINT_RADIUS, PARCEL_ARROW_POINT_PARTS, parcelColor, rTerrainInfo.altitudes );

						if ( ( GetActiveState() == STATE_EDIT ) &&
								 ( activePoint.isValid &&
									 activePoint.bParcel &&
									 ( nCurrentParcel == nParcelIndex ) ) )
						{
							if ( activePoint.bArrow )
							{
								float fRadius = PARCEL_ARROW_POINT_RADIUS - 1.0f;
								while( fRadius > 0.0f )
								{
									sceneDrawTool.DrawCircle3D( vParcelEnd, fRadius, PARCEL_ARROW_POINT_PARTS, parcelColor, rTerrainInfo.altitudes );
									fRadius -=1.0f;
								}
							}
							else
							{
								float fRadius = PARCEL_CENTER_POINT_RADIUS - 1.0f;
								while( fRadius > 0.0f )
								{
									sceneDrawTool.DrawCircle3D( vParcelBegin, fRadius, PARCEL_CENTER_POINT_PARTS, parcelColor, rTerrainInfo.altitudes );
									fRadius -=1.0f;
								}
							}
						}

						for ( int nPositionIndex = 0; nPositionIndex < rAIGeneralParcelInfo.reinforcePoints.size(); ++nPositionIndex )
						{
							const SAIGeneralParcelInfo::SReinforcePointInfo &rPositionInfo = rAIGeneralParcelInfo.reinforcePoints[nPositionIndex];

							CVec3 vBegin = CVec3( rPositionInfo.vCenter, 0.0f );
							AI2Vis( &vBegin );
							float fAngle = ( rPositionInfo.wDir * FP_2PI / 0xFFFF );
							CVec3 vEnd = vBegin + CVec3( CreateFromPolarCoord( POSITION_POINT_RADIUS, fAngle + FP_PI2 ), 0.0f );
							CVec3 vArrowEnd = vBegin + CVec3( CreateFromPolarCoord( POSITION_POINT_RADIUS + PARCEL_POINT_RADIUS_ARROW, fAngle + FP_PI2 ), 0.0f );

							RotatePoint( &vBegin, fParcelAngle );
							RotatePoint( &vEnd, fParcelAngle );
							RotatePoint( &vArrowEnd, fParcelAngle );
							vBegin += vParcelBegin;
							vEnd += vParcelBegin;
							vArrowEnd += vParcelBegin;
							
							CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vBegin.x, vBegin.y, &( vBegin.z ) );
							CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vEnd.x, vEnd.y, &( vEnd.z ) );
							CVertexAltitudeInfo::GetHeight( rTerrainInfo.altitudes, vArrowEnd.x, vArrowEnd.y, &( vArrowEnd.z ) );

							sceneDrawTool.DrawLine( vBegin, vEnd, positionColor );
							sceneDrawTool.DrawLine( vEnd, vArrowEnd, positionColor );
							sceneDrawTool.DrawCircle3D( vBegin, POSITION_POINT_RADIUS, POSITION_CENTER_POINT_PARTS, positionColor, rTerrainInfo.altitudes );
							sceneDrawTool.DrawCircle3D( vBegin, POSITION_CENTER_POINT_RADIUS, POSITION_CENTER_POINT_PARTS, positionColor, rTerrainInfo.altitudes );
							sceneDrawTool.DrawCircle3D( vEnd, POSITION_ARROW_POINT_RADIUS, POSITION_ARROW_POINT_PARTS, positionColor, rTerrainInfo.altitudes );

							if ( ( GetActiveState() == STATE_EDIT ) &&
									 ( activePoint.isValid &&
										 ( nCurrentParcel == nParcelIndex ) &&
										 ( !activePoint.bParcel ) &&
										 ( activePoint.nIndex == nPositionIndex ) ) )
							{
								if ( activePoint.bArrow )
								{
									float fRadius = POSITION_ARROW_POINT_RADIUS - 1.0f;
									while( fRadius > 0.0f )
									{
										sceneDrawTool.DrawCircle3D( vEnd, fRadius, POSITION_ARROW_POINT_PARTS, positionColor, rTerrainInfo.altitudes );
										fRadius -=1.0f;
									}
								}
								else
								{
									float fRadius = POSITION_CENTER_POINT_RADIUS - 1.0f;
									while( fRadius > 0.0f )
									{
										sceneDrawTool.DrawCircle3D( vBegin, fRadius, POSITION_CENTER_POINT_PARTS, positionColor, rTerrainInfo.altitudes );
										fRadius -=1.0f;
									}
								}
							}
						}
					}
				}
				sceneDrawTool.DrawToScene();

				
				std::pair<IVisObj*, CVec2> *pPickedVisObjects;
				int nPickedVisObjectsCount = 0;
				
				pScene->Pick( CVec2( stateParameter.lastPoint.x, stateParameter.lastPoint.y ), &pPickedVisObjects, &nPickedVisObjectsCount, SGVOGT_UNIT );
				if ( nPickedVisObjectsCount > 0 )
				{
					SMapObject *pMapObject = pFrame->FindByVis( pPickedVisObjects[0].first );
					if ( pMapObject )
					{
						int nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );

						CVec3 vPos = pMapObject->pVisObj->GetPosition();
						Vis2AI( &vPos );
						int nPlayer = pFrame->GetEditorObjectItem( pMapObject )->nPlayer;
						std::string szMessage;
						if ( pFrame->currentMapInfo.diplomacies.size() > nPlayer )
						{
							szMessage = NStr::Format( "%s, Player: %d, Side: %d, scriptID: %d",
																				pFrame->GetEditorObjectItem( pMapObject )->sDesc.szKey.c_str(),
																				nPlayer,
																				pFrame->currentMapInfo.diplomacies[nPlayer],
																				pFrame->GetEditorObjectItem( pMapObject )->nScriptID );
						}
						else
						{
							szMessage = NStr::Format( "%s, Player: %d, Side: UNKNOWN!, scriptID: %d",
																				pFrame->GetEditorObjectItem( pMapObject )->sDesc.szKey.c_str(),
																				nPlayer,
																				pFrame->GetEditorObjectItem( pMapObject )->nScriptID );
						}
						theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, szMessage.c_str() );
					}
				}
			}
		}
	}
}

void CAIGState::Update()
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
