#include "StdAfx.h"
#include "../Scene/Scene.h"
#include "ImageFrm.h"
#include "frames.h"
#include "GameWnd.h"
#include "editor.h"
#include "BuildCompose.h"

#define ID_H_SCROLLBAR			2233
#define ID_V_SCROLLBAR			2234
static const int zeroSizeX = 32;
static const int zeroSizeY = 32;
static const float zeroShiftX = 15.4f;
static const float zeroShiftY = 15.4f;

IMPLEMENT_DYNCREATE(CImageFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CImageFrame, CParentFrame)
	ON_WM_CREATE()
END_MESSAGE_MAP()

CImageFrame::CImageFrame()
{
	bShowKrest = false;
}

void CImageFrame::Init( IGFX *_pGFX )
{
	pGFX = _pGFX;
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	pKrestTexture = pTM->GetTexture( "editor\\krest\\1" );
}

int CImageFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
	DWORD dwStyle = WS_CHILD | SBS_HORZ | SBS_TOPALIGN;
	if ( !m_wndHScrollBar.Create( dwStyle, CRect( 0, 600, 800, 20 ), pWndView, ID_H_SCROLLBAR ) )
	{
		TRACE0("Failed to create ScrollBar\n");
		return -1;
	}
	
	dwStyle = WS_CHILD | SBS_VERT | SBS_TOPALIGN;
	if ( !m_wndVScrollBar.Create( dwStyle, CRect( 800, 0, 820, 600 ), pWndView, ID_V_SCROLLBAR ) )
	{
		TRACE0("Failed to create ScrollBar\n");
		return -1;
	}
	
	return 0;
}

void CImageFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
}

void CImageFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	pGFX->SetShadingEffect( 3 );
	SGFXRect2 rc;
	pGFX->SetTexture( 0, pImageTexture );
	rc.rect.x1 = vImagePos.x;
	rc.rect.y1 = vImagePos.y;
	rc.rect.x2 = rc.rect.x1 + vImageSize.x;
	rc.rect.y2 = rc.rect.y1 + vImageSize.y;
	rc.maps = rcImageMap;
	pGFX->SetupDirectTransform();
	pGFX->DrawRects( &rc, 1 );
	pGFX->RestoreTransform();
	
	if ( bShowKrest )
	{
		pGFX->SetTexture( 0, pKrestTexture );
		CVec2 vBegin( vKrestPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vKrestPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
		rc.rect = CTRect<float> ( vBegin.x, vBegin.y, vBegin.x+zeroSizeX, vBegin.y+zeroSizeY );
		rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
		pGFX->SetupDirectTransform();
		pGFX->DrawRects( &rc, 1 );
		pGFX->RestoreTransform();
	}
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CImageFrame::LoadImageTexture( const char *pszFileName )
{
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	pImageTexture = 0;
	string szTempFile = theApp.GetEditorTempDir();
	szTempFile += szComposerName;
	if ( !ComposeImageToTexture( pszFileName, szTempFile.c_str(), false ) )
	{
		if ( (pImageTexture = pTM->GetTexture( pszFileName )) == 0 )
		{
			AfxMessageBox( "Error: ComposeImageToTexture() FAILED" );
			return;
		}
		vImageSize.x = pImageTexture->GetSizeX(0);
		vImageSize.y = pImageTexture->GetSizeY(0);
		rcImageMap.x1 = rcImageMap.y1 = 0;
		rcImageMap.x2 = 1.0f;
		rcImageMap.y2 = 1.0f;
	}
	else
	{
		CTRect<float> mapImageRect = GetImageSize( pszFileName );
		vImageSize.x = mapImageRect.x1;
		vImageSize.y = mapImageRect.y1;
		rcImageMap.x1 = rcImageMap.y1 = 0;
		rcImageMap.x2 = mapImageRect.x2;
		rcImageMap.y2 = mapImageRect.y2;
		
		szTempFile = theApp.GetEditorTempResourceDir();
		szTempFile += "\\";
		szTempFile += szComposerName;
		pImageTexture = pTM->GetTexture( szTempFile.c_str() );
	}
	
	vImagePos = VNULL2;
	
	InitScrollBars();
	GFXDraw();
}

void CImageFrame::InitScrollBars()
{
	if ( vImageSize.x > GAME_SIZE_X )
	{
		m_wndHScrollBar.ShowScrollBar();
		m_wndHScrollBar.EnableScrollBar();
		SCROLLINFO info;
		info.fMask = SIF_PAGE|SIF_RANGE;
		info.nMin = 0;
		info.nMax = vImageSize.x;
		info.nPage = GAME_SIZE_X;
		m_wndHScrollBar.SetScrollInfo( &info );
		m_wndHScrollBar.SetScrollPos( 0 );
	}
	else
		m_wndHScrollBar.ShowScrollBar( FALSE );
	
	if ( vImageSize.y > GAME_SIZE_Y )
	{
		m_wndVScrollBar.ShowScrollBar();
		m_wndVScrollBar.EnableScrollBar();
		SCROLLINFO info;
		info.fMask = SIF_PAGE|SIF_RANGE;
		info.nMin = 0;
		info.nMax = vImageSize.y;
		info.nPage = GAME_SIZE_Y;
		m_wndVScrollBar.SetScrollInfo( &info );
		m_wndVScrollBar.SetScrollPos( 0 );
	}
	else
		m_wndVScrollBar.ShowScrollBar( FALSE );
}

void CImageFrame::UpdateImageCoordinates()
{
	vImagePos.x = -m_wndHScrollBar.GetScrollPos();
	vImagePos.y = -m_wndVScrollBar.GetScrollPos();
	GFXDraw();
}
