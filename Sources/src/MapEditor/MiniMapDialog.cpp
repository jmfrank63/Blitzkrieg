// MiniMapDialog.cpp : implementation file
//

#include "stdafx.h"
#include "editor.h"
#include "frames.h"
#include "..\RandomMapGen\IB_Types.h"

#include "MiniMapDialog.h"

#include "TemplateEditorFrame1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define LAYER_DIMENSION ( 6 )
#define FRAME_OFFSET ( 2 )
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CMiniMapDialog::vID[] = 
{
	IDC_MINIMAP_VIEW,						//0
	IDC_MINIMAP_EDITOR,					//1
	IDC_MINIMAP_GAME,						//2
	IDC_MINIMAP_GAME_CREATE,		//3
	IDC_MINIMAP_CLOSE,					//4
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMiniMapDialog::CMiniMapDialog( CWnd* pParent )
	: CDialog( CMiniMapDialog::IDD, pParent ), 
		m_frame( 0 ), m_isCreating( true ),
		gameScreenFrame( PS_SOLID, 2, RGB( 0x00, 0x00, 0x00 ) ),
		fireRangeAreas( PS_SOLID, 1, PS_SOLID, 1 ),
		miniMapTerrainGrid( PS_DOT, 1, RGB( 0x00, 0x00, 0x00 ) )
{
	//{{AFX_DATA_INIT(CMiniMapDialog)
	//}}AFX_DATA_INIT
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CMiniMapDialog)
	//}}AFX_DATA_MAP
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP( CMiniMapDialog, CDialog )
	//{{AFX_MSG_MAP(CMiniMapDialog)
	ON_BN_CLICKED( IDC_MINIMAP_CLOSE, OnMinimapClose )
	ON_WM_SIZE()
	ON_WM_SIZING()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_PAINT()
	ON_WM_MOVE()
	ON_BN_CLICKED(IDC_MINIMAP_EDITOR, OnMinimapUpdate)
	ON_BN_CLICKED(IDC_MINIMAP_GAME, OnMinimapGame)
	ON_BN_CLICKED(IDC_MINIMAP_GAME_CREATE, OnMinimapGameCreate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMinimapClose() 
{
	SendMessage( WM_CLOSE );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMiniMapDialog::OnInitDialog() 
{
	m_isCreating = true;
	CDialog ::OnInitDialog();
	
	if ( m_frame && 
		   ( m_frame->m_minimapDialogRect.Width() > 0 ) &&
			 ( m_frame->m_minimapDialogRect.Height() > 0 ) )
	{
		MoveWindow( m_frame->m_minimapDialogRect.left,
								m_frame->m_minimapDialogRect.top,
								m_frame->m_minimapDialogRect.Width(),
								m_frame->m_minimapDialogRect.Height() );
	}
	else
	{
		CRect mainWndRect;
		GetClientRect( &mainWndRect );
		CRect newWndRect;
		GetWindowRect( &newWndRect );

		MoveWindow( 100,
								100,
								GetMinimumXDimension() + newWndRect.Width() - mainWndRect.Width(),
								GetMinimumYDimension() + newWndRect.Height() - mainWndRect.Height() );
	}
	UpdateMinimap( false );
	UpdateControls();
	m_isCreating = false;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnSize( UINT nType, int cx, int cy ) 
{
	CDialog::OnSize( nType, cx, cy );

	int nXRange = 7;
	int nYRange = 6;
	int nButtonWidth = 60;
	int nButtonHeigth = 22;

	CWnd* pwnd = 0;
	if ( pwnd = GetDlgItem( IDC_MINIMAP_VIEW ) )
	{
		pwnd->MoveWindow( 0,
											0,
											cx,
											cy - nYRange - nButtonHeigth );
	}

	if ( pwnd = GetDlgItem( IDC_MINIMAP_EDITOR ) )
	{
		pwnd->MoveWindow( cx - nButtonWidth,
											cy - nButtonHeigth,
											nButtonWidth,
											nButtonHeigth );
	}

	if ( pwnd = GetDlgItem( IDC_MINIMAP_GAME ) )
	{
		pwnd->MoveWindow( nButtonWidth + nXRange,
											cy - nButtonHeigth,
											nButtonWidth,
											nButtonHeigth );
	}
	if ( pwnd = GetDlgItem( IDC_MINIMAP_GAME_CREATE ) )
	{
		pwnd->MoveWindow( 0,
											cy - nButtonHeigth,
											nButtonWidth,
											nButtonHeigth );
	}
  InvalidateRect(NULL, TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnSizing( UINT fwSide, LPRECT pRect ) 
{
	CDialog::OnSizing( fwSide, pRect );
	
	CRect mainWndRect;
	GetClientRect( &mainWndRect );
	CRect newWndRect;
	GetWindowRect( &newWndRect );

	LONG dwXSize = mainWndRect.Width();
	LONG dwYSize = mainWndRect.Height();
	dwXSize = ( pRect->right - pRect->left ) - ( ( newWndRect.Width() ) - dwXSize );
	dwYSize = ( pRect->bottom - pRect->top ) - ( ( newWndRect.Height() ) - dwYSize );

	if ( dwXSize < GetMinimumXDimension() )
	{
	if ( ( fwSide == WMSZ_LEFT ) ||
		   ( fwSide == WMSZ_TOPLEFT ) ||
		   ( fwSide == WMSZ_BOTTOMLEFT ) )
		{
			pRect->left = pRect->right - GetMinimumXDimension() - ( ( pRect->right - pRect->left ) - dwXSize );
		}
		else
		{
			pRect->right = pRect->left + GetMinimumXDimension() + ( ( pRect->right - pRect->left ) - dwXSize );
		}
	}
	if ( dwYSize < GetMinimumYDimension() )
	{
		if ( (fwSide == WMSZ_TOP ) ||
		     ( fwSide == WMSZ_TOPLEFT ) ||
		     ( fwSide == WMSZ_TOPRIGHT ) )
		{
			pRect->top = pRect->bottom - GetMinimumYDimension() - ( ( pRect->bottom - pRect->top ) - dwYSize );
		}
		else
		{
			pRect->bottom = pRect->top + GetMinimumYDimension() + ( ( pRect->bottom - pRect->top ) - dwYSize );
		}
	}
	if ( !m_isCreating && m_frame )
	{
		m_frame->m_minimapDialogRect.left = pRect->left;
		m_frame->m_minimapDialogRect.top = pRect->top;
		m_frame->m_minimapDialogRect.right = pRect->right;
		m_frame->m_minimapDialogRect.bottom = pRect->bottom;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMove( int x, int y ) 
{
	CDialog ::OnMove(x, y);
	if ( !m_isCreating && m_frame )
	{
		CRect wndRect;
		GetWindowRect( &wndRect );

		m_frame->m_minimapDialogRect.left = wndRect.left;
		m_frame->m_minimapDialogRect.top = wndRect.top;
		m_frame->m_minimapDialogRect.right = wndRect.right;
		m_frame->m_minimapDialogRect.bottom = wndRect.bottom;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DrawMiniMapTerrain( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect, true );

	CBitmapMiniMapDrawTool bitmapTool( miniMapRect, CPoint( 0, 0 ), pDC );
	miniMapTerrain.Draw( &bitmapTool );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DrawGameScreenFrame( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect, true );

	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pScene = GetSingleton<IScene>();
	if ( pCamera && pScene )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			CXORDoubleLinesMiniMapDrawTool xorLinesTool( miniMapRect,
																									 CPoint( pTerrain->GetSizeX() * fWorldCellSize,
																													 pTerrain->GetSizeY() * fWorldCellSize ),
																									 pDC );
			gameScreenFrame.Draw( &xorLinesTool );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DrawFireRangeAreas( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect, true );

	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pScene = GetSingleton<IScene>();
	if ( pCamera && pScene )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			CXORLinesMiniMapDrawTool xorLinesTool( miniMapRect,
																						 CPoint( pTerrain->GetSizeX() * fWorldCellSize,
																										 pTerrain->GetSizeY() * fWorldCellSize ),
																						 pDC );
			fireRangeAreas.Draw( &xorLinesTool );
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DrawUnitsSelection( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect, true );

	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pScene = GetSingleton<IScene>();
	if ( pCamera && pScene )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			CXORLinesMiniMapDrawTool xorLinesTool( miniMapRect,
																						 CPoint( pTerrain->GetSizeX() * 2,
																										 pTerrain->GetSizeY() * 2 ),
																						 pDC );
			unitsSelection.Draw( &xorLinesTool );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::DrawMiniMapTerrainGrid( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	return;

	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect, true );

	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pScene = GetSingleton<IScene>();
	if ( pCamera && pScene )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			CXORLinesMiniMapDrawTool xorLinesTool( miniMapRect,
																						 CPoint( pTerrain->GetSizeX(),
																										 pTerrain->GetSizeY() ),
																						 pDC );
			miniMapTerrainGrid.Draw( &xorLinesTool );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnLButtonDown( UINT nFlags, CPoint point ) 
{
	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect );
	if ( miniMapRect.PtInRect( point ) )
	{
		ICamera *pCamera = GetSingleton<ICamera>();
		IScene *pScene = GetSingleton<IScene>();
		if ( pCamera && pScene )
		{
			if (ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CVec3 newCamera3;
				newCamera3.x = ( point.x - miniMapRect.left ) * pTerrain->GetSizeX() * fWorldCellSize / miniMapRect.Width();
				newCamera3.y = ( miniMapRect.Height() - ( point.y - miniMapRect.top ) - 1 ) * pTerrain->GetSizeY() * fWorldCellSize / miniMapRect.Height();
				newCamera3.z = 0;

				const CVec3 center3 = m_frame->GetScreenCenter();
				const CVec3 camera3 = pCamera->GetAnchor();
				newCamera3 += camera3 - center3;
				
				m_frame->NormalizeCamera( &newCamera3 );
				m_frame->RedrawWindow();
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnLButtonUp( UINT nFlags, CPoint point ) 
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( nFlags & MK_LBUTTON )
	{
		OnLButtonDown( nFlags, point );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::GetMiniMapRect( CRect* pRect, bool onlyDimensions )
{
	NI_ASSERT_SLOW_T( pRect != 0,
										NStr::Format( "Wrong parameters: %x", pRect ) );
	if( CWnd* pwnd = GetDlgItem( IDC_MINIMAP_VIEW ) )
	{
		pwnd->GetWindowRect( pRect );
		pRect->left += FRAME_OFFSET;
		pRect->top += FRAME_OFFSET;
		pRect->right -= FRAME_OFFSET;
		pRect->bottom -= FRAME_OFFSET;
		ScreenToClient( pRect );
		if ( onlyDimensions )
		{
			pRect->right -= pRect->left;
			pRect->left = 0;
			pRect->bottom -= pRect->top;
			pRect->top = 0;
		}
		return;
	}
	pRect->SetRectEmpty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::InvalidateMiniMapRect()
{
	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect );
	InvalidateRect( &miniMapRect, false );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::UpdateScreenFrame()
{
	CDC* pDC = GetDC();
	if( pDC )
	{ 
		//miniMapTerrain.Update( pDC );
		fireRangeAreas.Update( pDC );
		gameScreenFrame.Update( pDC );
		miniMapTerrainGrid.Update( pDC );
		//unitsSelection.Update( pDC );
		ReleaseDC( pDC );
	}

	InvalidateMiniMapRect();
	UpdateWindow();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnPaint() 
{
	//CDialog::OnPaint();
	CPaintDC paintDC(this);

	//����� ������� ������� ������
	CRect miniMapRect;
	GetMiniMapRect( &miniMapRect );

	CDC dc;
	int nRes = dc.CreateCompatibleDC( &paintDC );
	CBitmap bmp;
	nRes = bmp.CreateCompatibleBitmap( &paintDC, miniMapRect.Width(), miniMapRect.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );
	dc.FillSolidRect( 0, 0, miniMapRect.Width(), miniMapRect.Height(), RGB( 0, 0, 0 ) );
	
	DrawMiniMapTerrain( &dc );
	DrawFireRangeAreas( &dc );
	DrawUnitsSelection( &dc );
	DrawGameScreenFrame( &dc );
	DrawMiniMapTerrainGrid( &dc );

	paintDC.BitBlt( miniMapRect.left, miniMapRect.top, miniMapRect.Width(), miniMapRect.Height(), &dc, 0, 0, SRCCOPY );
	dc.SelectObject( pOldBitmap );

	/**
	CRect clientRect;
	GetClientRect( &clientRect );

	clientRect.left = clientRect.right - ::GetSystemMetrics( SM_CXHSCROLL );
	clientRect.top = clientRect.bottom - ::GetSystemMetrics( SM_CYVSCROLL );

	paintDC.DrawFrameControl( clientRect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP );
	/**/
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::UpdateMinimap( bool bUpdateUnits ) 
{
	UpdateControls();
	CDC* pDC = GetDC();
	if( pDC )
	{ 
		miniMapTerrain.UpdateColor();
		miniMapTerrain.Update( pDC );
		if ( bUpdateUnits )
		{
			unitsSelection.Update( pDC );
		}
		ReleaseDC( pDC );
		UpdateScreenFrame();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::UpdateMinimapEditor( bool bUpdateUnits ) 
{
	miniMapTerrain.SetInGame( false );
	UpdateMinimap( bUpdateUnits );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMinimapUpdate() 
{
	miniMapTerrain.SetInGame( false );
	UpdateMinimap( true );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMinimapGame() 
{
	miniMapTerrain.SetInGame( true );
	UpdateMinimap( true );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::OnMinimapGameCreate() 
{
	g_frameManager.GetTemplateEditorFrame()->CreateMiniMap();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapDialog::UpdateControls()
{
	bool bEnable = false;
	if( IScene *pScene = GetSingleton<IScene>() )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			bEnable = ( g_frameManager.GetTemplateEditorFrame() && !g_frameManager.GetTemplateEditorFrame()->m_currentMapName.empty() );
		}
	}
	CWnd *pWnd = 0;
	if ( pWnd = GetDlgItem( IDC_MINIMAP_EDITOR ) )
	{
		pWnd->EnableWindow( bEnable );
	}
	if ( pWnd = GetDlgItem( IDC_MINIMAP_GAME_CREATE ) )
	{
		pWnd->EnableWindow( bEnable );
	}
	if ( pWnd = GetDlgItem( IDC_MINIMAP_GAME ) )
	{
		if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
		{
			if ( bEnable )
			{
				bEnable = false;
				std::string szMiniMapImage = g_frameManager.GetTemplateEditorFrame()->m_currentMapName;
				CPtr<IDataStream> pStreamTGA = 0;
				CPtr<IDataStream> pStreamDDS = 0;
				if ( szMiniMapImage.find( pDataStorage->GetName() ) == 0 )
				{
					std::string szShortMiniMapName = szMiniMapImage.substr( strlen( pDataStorage->GetName() ) );
					pStreamTGA = pDataStorage->OpenStream( ( szShortMiniMapName + std::string( ".tga" ) ).c_str(), STREAM_ACCESS_READ );
					pStreamDDS = pDataStorage->OpenStream( ( szShortMiniMapName + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );
				}
				else
				{
					pStreamTGA = OpenFileStream( ( szMiniMapImage + std::string( ".tga" ) ).c_str(), STREAM_ACCESS_READ );
					pStreamDDS = OpenFileStream( ( szMiniMapImage + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );
				}
				if ( pStreamTGA || pStreamDDS )
				{
					bEnable = true;
				}
			}
		}
		pWnd->EnableWindow( bEnable );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	if ( m_frame )
	{
		for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_frame->m_objectsAI.begin(); it != m_frame->m_objectsAI.end(); ++it )
		{ 
			if( it->first)->IsHuman() )
			{
				CVec3 vec3 = (it->first)->pVisObj->GetPosition();


			}
		}
	}
/**/
/**
ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
if ( terra )
{		
	dynamic_cast<ITerrainEditor*>(terra)->SetMarker( NULL, 0 );
}
/**/

/**	
	if( CWnd* pwnd = GetDlgItem( IDC_MINIMAP_VIEW ) )
	{
		CRect wndRect;
		pwnd.GetWindowRect( &wndRect );
		if ( wndRect.PtInRect( point ) )
		{
			CDialog::OnLButtonDown( nFlags, point );
			mouseFrame.GetFrame().Set( point.x, point.y, point.x, point.y );
			DrawXORMouseFrame();
			SetCapture();
		}
	}
/**/
/**
	CDialog::OnLButtonUp( nFlags, point );
	DrawXORMouseFrame();
	ReleaseCapture();

/**/

/**
	if ( nFlags & MK_LBUTTON )
	{
		DrawXORMouseFrame();
		mouseFrame.GetFrame().right = point.x;
		mouseFrame.GetFrame().bottom = point.y;
		DrawXORMouseFrame();
	}
	CDialog::OnMouseMove(nFlags, point);
/**/
