
#include "StdAfx.h"
#include "editor.h"
#include "TileDrawState.h"

#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "../GFX/GFX.H"
#include "../Scene/Terrain.h"
#include "../Image/Image.h"
#include "../Scene/Scene.h"
#include "IUndoRedoCmd.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void NormalizeRange( std::vector<SMainTileDesc> *pTiles );
int  GetRandomTile( const std::vector<SMainTileDesc> &tiles );


void CTileDrawState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
		return;
	}
	CTPoint<int> mousePoint( rMousePoint );
	if ( nFlags & MK_LBUTTON )
	{
		RECT r;
		g_frameManager.GetGameWnd()->GetClientRect( &r );
		mousePoint.x -= r.left;
		mousePoint.y -= r.top;
		CVec3 v;
		CVec2 p ( mousePoint.x , mousePoint.y );
		GetSingleton<IScene>()->GetPos3( &v, p );
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		int tileX,tileY;
		if ( terra )
		{
			ITerrainEditor* pTerrainEditor = dynamic_cast< ITerrainEditor* >( terra );
			const STerrainInfo &terrainInfo = pTerrainEditor->GetTerrainInfo();
			
			GRect terrainRect( 0,
												 0,
												 terrainInfo.patches.GetSizeX() * 16 - 1,
												 terrainInfo.patches.GetSizeY() * 16 - 1 );
			pTerrainEditor->GetTileIndex( v, &tileX, &tileY );
			int nTileIndex = pFrame->m_pTabTileEditDialog->m_TilesList.GetNextItem( -1, LVNI_SELECTED );
			if ( nTileIndex >= 0 )
			{
				nTileIndex = pFrame->m_pTabTileEditDialog->m_TilesList.GetItemData( nTileIndex );
				if ( terrainRect.contains( tileX, tileY ) )
				{
					pFrame->m_lastMouseTileX = tileX;
					pFrame->m_lastMouseTileY = tileY;

					std::vector<STileRedoCmdInfo> tmpVec;
					for ( int i = 0; i != pFrame->m_brushDY; i++ )
					{
						for ( int j = 0; j != pFrame->m_brushDX; j++ )
						{
							int nMapIndex = pFrame->descrTile.terrtypes[nTileIndex].GetMapsIndex();
							if ( terrainRect.contains( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 ) ) ) 
							{
								tmpVec.push_back( STileRedoCmdInfo( tileX + j - ( pFrame->m_brushDX >> 1 ),
																										tileY + i - ( pFrame->m_brushDY >> 1 ),
																										nMapIndex,
																										pTerrainEditor->GetTile( tileX + j - ( pFrame->m_brushDX >> 1 ),
																																						 tileY + i - ( pFrame->m_brushDY >> 1 )	) ) );
							}
						}
					}

					pFrame->AddTileCmd( tmpVec );
					CTRect<int> rTilepdate( ( tileX - 3 - ( pFrame->m_brushDX >> 1 ) ) / 16 ,( tileY - 3 - ( pFrame->m_brushDY >> 1 ) ) / 16 , ( tileX + pFrame->m_brushDX + 3 - ( pFrame->m_brushDX >> 1 )  ) / 16 , ( tileY + pFrame->m_brushDY + 3 - ( pFrame->m_brushDY >> 1 ) ) / 16 );
					ValidateIndices( CTRect<int>( 0, 0, terrainInfo.patches.GetSizeX(), terrainInfo.patches.GetSizeY() ), &rTilepdate );
					if ( rTilepdate.maxx >= terrainInfo.patches.GetSizeX() )
					{
						rTilepdate.maxx = terrainInfo.patches.GetSizeX() - 1;
					}
					if ( rTilepdate.maxy >= terrainInfo.patches.GetSizeY() )
					{
						rTilepdate.maxy = terrainInfo.patches.GetSizeY() - 1;
					}
					pTerrainEditor->Update( rTilepdate );
					
					if ( GRect(r).contains( mousePoint.x , mousePoint.y ) )
					{
						std::vector< CTPoint<int> > pointsForUpdate;
						for ( int i = 0; i != pFrame->m_brushDY; i++ )
						{
							for ( int j = 0; j != pFrame->m_brushDX; j++ )
							{
								if ( terrainRect.contains( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 ) ) )
									pointsForUpdate.push_back( CTPoint<int> ( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 ) ) );
							}
						}
						if ( pointsForUpdate.size() )
						{
							pTerrainEditor->SetMarker( &pointsForUpdate[0], pointsForUpdate.size() );
						}
					}
					else
					{
						pTerrainEditor->SetMarker( NULL, 0);
					}
					pFrame->SetMapModified();
				}
			}
		}
		pFrame->RedrawWindow();
	}
	else
	{
		if ( ITerrain *terra = GetSingleton<IScene>()->GetTerrain() )
		{
			RECT r;
			g_frameManager.GetGameWnd()->GetClientRect( &r );
			RECT r2;
			g_frameManager.GetGameWnd()->GetWindowRect( &r2 );
	
			mousePoint.x -= r.left;
			mousePoint.y -= r.top;
			CVec3 v;
			CVec2 p ( mousePoint.x , mousePoint.y );
			GetSingleton<IScene>()->GetPos3( &v, p );
			int tileX,tileY;
			const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
			GRect terrainRect( 0, 0 , terrainInfo.patches.GetSizeX() * 16 - 1, terrainInfo.patches.GetSizeY() * 16 - 1);
			dynamic_cast< ITerrainEditor* >(terra)->GetTileIndex( v, &tileX, &tileY );	
			
			CPoint pt;
			GetCursorPos(&pt);
      pFrame->ScreenToClient(&pt);
			pFrame->ScreenToClient(&r2);
			if ( GRect(r2).contains( pt.x , pt.y ) )
			{
				pFrame->m_lastMouseTileX = tileX;
				pFrame->m_lastMouseTileY = tileY;

				std::vector< CTPoint<int> > pointsForUpdate;
				for ( int i = 0; i != pFrame->m_brushDY; i++ )
				{
					for ( int j = 0; j != pFrame->m_brushDX; j++ )
					{
						if ( terrainRect.contains( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 )) )
							pointsForUpdate.push_back( CTPoint<int> ( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 ) ) );
					}
				}
				if ( pointsForUpdate.size() )
					dynamic_cast<ITerrainEditor*>(terra)->SetMarker( &pointsForUpdate[0], pointsForUpdate.size() );
			}
			else
				dynamic_cast<ITerrainEditor*>(terra)->SetMarker( NULL, 0);

			pFrame->RedrawWindow();
		}
	}
}
void CTileDrawState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
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
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
	int tileX,tileY;
	if ( terra )
	{
		ITerrainEditor* pTerrainEditor = dynamic_cast< ITerrainEditor* >( terra );
		const STerrainInfo &terrainInfo = pTerrainEditor->GetTerrainInfo();
			
		GRect terrainRect( 0,
											 0,
											 terrainInfo.patches.GetSizeX() * 16 - 1,
											 terrainInfo.patches.GetSizeY() * 16 - 1 );
		pTerrainEditor->GetTileIndex( v, &tileX, &tileY );
		int nTileIndex = pFrame->m_pTabTileEditDialog->m_TilesList.GetNextItem( -1, LVNI_SELECTED );
		if ( nTileIndex >= 0 )
		{
			nTileIndex = pFrame->m_pTabTileEditDialog->m_TilesList.GetItemData( nTileIndex );
			if ( terrainRect.contains( tileX, tileY ) )
			{
				std::vector<STileRedoCmdInfo> tmpVec;
				for ( int i = 0; i != pFrame->m_brushDY; i++ )
				{
					for ( int j = 0; j != pFrame->m_brushDX; j++ )
					{
						int nMapIndex = pFrame->descrTile.terrtypes[nTileIndex].GetMapsIndex();
						if ( terrainRect.contains( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 )) ) 
						{
							tmpVec.push_back( STileRedoCmdInfo( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 ) , nMapIndex, 
								pTerrainEditor->GetTile( tileX + j - ( pFrame->m_brushDX >> 1 ), tileY + i - ( pFrame->m_brushDY >> 1 )	) ) );
						} 
					}
				}	

				pFrame->AddTileCmd( tmpVec );
				CTRect<int> rTilepdate( ( tileX - 3 - ( pFrame->m_brushDX >> 1 ) ) / 16 ,( tileY - 3 - ( pFrame->m_brushDY >> 1 ) ) / 16 , ( tileX + pFrame->m_brushDX + 3 - ( pFrame->m_brushDX >> 1 )  ) / 16 , ( tileY + pFrame->m_brushDY + 3 - ( pFrame->m_brushDY >> 1 ) ) / 16 );
				ValidateIndices( CTRect<int>( 0, 0, terrainInfo.patches.GetSizeX(), terrainInfo.patches.GetSizeY() ), &rTilepdate );
				if ( rTilepdate.maxx >= terrainInfo.patches.GetSizeX() )
				{
					rTilepdate.maxx = terrainInfo.patches.GetSizeX() - 1;
				}
				if ( rTilepdate.maxy >= terrainInfo.patches.GetSizeY() )
				{
					rTilepdate.maxy = terrainInfo.patches.GetSizeY() - 1;
				}
				pTerrainEditor->Update( rTilepdate );
				pFrame->SetMapModified();
			}
		}
	}
	pFrame->RedrawWindow();
}

void CTileDrawState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, pFrame ) )
	{
		return;
	}
	if ( g_frameManager.GetMiniMapWindow() )
	{
		g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
	}
}
void CTileDrawState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( false );
		}
		pFrame->RedrawWindow();
	}
}

void CTileDrawState::Leave()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CTileDrawState::Update()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}
