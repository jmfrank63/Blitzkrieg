
#include "stdafx.h"
#include "editor.h"
#include "KeyFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static const int SCROLLBAR_SIZE = 15;		//������ ScrollBar
static const int XS = 25;								//������ �������� ������ �� �����������
static const int YS = 25;								//�� ���������
static const int LEFT = 43;							//������ ����� �� ���� ������ �� �������
static const int BOTTOM = 30;						//����� �� �������
static const int TEXT_SPACE = 4;				//������ ������ �� �������
static const int SELECT_SIZE = 3;				//������� � �������� ������� ���������� ����� ��� ����� ������
static const int NODE_SPACE = 3;				//����������� ���������� ����� ������ �� ������ � ��������


int SIZES[5] = { 5, 10, 20, 50, 100 };	//��������� ��������

CKeyFrameEditor::CKeyFrameEditor()
{
	m_fStepX = 1;
	m_fStepY = 1;
	m_nDragIndex = 0;
	m_nHighNodeIndex = -1;
	m_mode = E_FREE_MODE;

	m_XS = XS;
	m_YS = YS;
	m_bResizeMode = false;
}

CKeyFrameEditor::~CKeyFrameEditor()
{
}


BEGIN_MESSAGE_MAP(CKeyFrameEditor, CWnd)
	ON_WM_SIZE()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_PAINT()
	ON_WM_RBUTTONUP()
	ON_COMMAND(ID_KEYFRAME_ZOOMINX, OnKeyframeZoominx)
	ON_COMMAND(ID_KEYFRAME_ZOOMINY, OnKeyframeZoominy)
	ON_COMMAND(ID_KEYFRAME_ZOOMOUTX, OnKeyframeZoomoutx)
	ON_COMMAND(ID_KEYFRAME_ZOOMOUTY, OnKeyframeZoomouty)
	ON_UPDATE_COMMAND_UI(ID_KEYFRAME_ZOOMINX, OnUpdateKeyframeZoominx)
	ON_UPDATE_COMMAND_UI(ID_KEYFRAME_ZOOMINY, OnUpdateKeyframeZoominy)
	ON_UPDATE_COMMAND_UI(ID_KEYFRAME_ZOOMOUTX, OnUpdateKeyframeZoomoutx)
	ON_UPDATE_COMMAND_UI(ID_KEYFRAME_ZOOMOUTY, OnUpdateKeyframeZoomouty)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()



BOOL CKeyFrameEditor::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	if ( !CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext) )
		return FALSE;

	m_BottomScroll.Create( SBS_HORZ | SBS_TOPALIGN | WS_CHILD, CRect(5,5,100,30), this, 100 );
	m_BottomScroll.ShowScrollBar();

	m_LeftScroll.Create( SBS_VERT | SBS_LEFTALIGN | WS_CHILD, CRect(5,5,30,100), this, 101 );
	m_LeftScroll.ShowScrollBar();

	return TRUE;
}

void CKeyFrameEditor::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	RECT clientRect;
	GetClientRect( &clientRect );

	RECT rc;
	rc.left = clientRect.left + SCROLLBAR_SIZE;
	rc.top = clientRect.bottom - SCROLLBAR_SIZE;
	rc.right = clientRect.right;
	rc.bottom = clientRect.bottom;
	if ( m_BottomScroll.GetSafeHwnd() )
	{
		m_BottomScroll.MoveWindow( &rc );
		SetHDimention( m_fMinValX, m_fMaxValX );
	}

	rc.left = clientRect.left;
	rc.top = clientRect.top;
	rc.right = clientRect.left + SCROLLBAR_SIZE;
	rc.bottom = clientRect.bottom - SCROLLBAR_SIZE;
	if ( m_LeftScroll.GetSafeHwnd() )
	{
		m_LeftScroll.MoveWindow( &rc );
		SetVDimention( m_fMinValY, m_fMaxValY );
	}
}

void CKeyFrameEditor::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int minpos;
	int maxpos;
	pScrollBar->GetScrollRange(&minpos, &maxpos); 
	maxpos = pScrollBar->GetScrollLimit();
	
	int curpos = pScrollBar->GetScrollPos();
	
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;
		
	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;
		
	case SB_ENDSCROLL:   // End scroll.
		break;
		
	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;
		
	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;
		
	case SB_PAGELEFT:    // Scroll one page left.
		{
      SCROLLINFO   info;
      pScrollBar->GetScrollInfo(&info, SIF_ALL);
			
      if (curpos > minpos)
				curpos = max(minpos, curpos - (int) info.nPage);
		}
		break;
		
	case SB_PAGERIGHT:      // Scroll one page right.
		{
      SCROLLINFO   info;
      pScrollBar->GetScrollInfo(&info, SIF_ALL);
			
      if (curpos < maxpos)
				curpos = min(maxpos, curpos + (int) info.nPage);
		}
		break;
		
	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;
		
	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}
	
	pScrollBar->SetScrollPos(curpos);
	Invalidate();
	
	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CKeyFrameEditor::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int minpos;
	int maxpos;
	pScrollBar->GetScrollRange(&minpos, &maxpos); 
	maxpos = pScrollBar->GetScrollLimit();
	
	int curpos = pScrollBar->GetScrollPos();
	
	switch (nSBCode)
	{
	case SB_TOP:      // Scroll to far left.
		curpos = minpos;
		break;
		
	case SB_BOTTOM:      // Scroll to far right.
		curpos = maxpos;
		break;
		
	case SB_ENDSCROLL:   // End scroll.
		break;
		
	case SB_LINEUP:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;
		
	case SB_LINEDOWN:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;
		
	case SB_PAGEUP:    // Scroll one page left.
		{
      SCROLLINFO   info;
      pScrollBar->GetScrollInfo(&info, SIF_ALL);
			
      if (curpos > minpos)
				curpos = max(minpos, curpos - (int) info.nPage);
		}
		break;
		
	case SB_PAGEDOWN:      // Scroll one page right.
		{
      SCROLLINFO   info;
      pScrollBar->GetScrollInfo(&info, SIF_ALL);
			
      if (curpos < maxpos)
				curpos = min(maxpos, curpos + (int) info.nPage);
		}
		break;
		
	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;
		
	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}
	
	pScrollBar->SetScrollPos(curpos);
	Invalidate();
	
	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CKeyFrameEditor::OnPaint() 
{
	CPaintDC paintDC(this); // device context for painting
	RECT rc;
	GetClientRect( &rc );

	CDC dc;
	int nRes = dc.CreateCompatibleDC( &paintDC );
	CBitmap bmp;
	nRes = bmp.CreateCompatibleBitmap( &paintDC, rc.right, rc.bottom );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );


	dc.FillSolidRect( rc.left+SCROLLBAR_SIZE, rc.top, rc.right-rc.left-SCROLLBAR_SIZE, rc.bottom-rc.top-SCROLLBAR_SIZE, RGB(255,255,255) );
	paintDC.FillSolidRect( rc.left, rc.bottom-SCROLLBAR_SIZE, SCROLLBAR_SIZE, SCROLLBAR_SIZE, RGB(255,255,255) );
	
	if ( !m_BottomScroll.IsWindowVisible() )
	{
		paintDC.FillSolidRect( rc.left+SCROLLBAR_SIZE, rc.bottom-SCROLLBAR_SIZE, rc.right-rc.left-SCROLLBAR_SIZE, SCROLLBAR_SIZE, RGB(255,255,255) );
	}

	if ( !m_LeftScroll.IsWindowVisible() )
	{
		paintDC.FillSolidRect( rc.left, rc.top, SCROLLBAR_SIZE, rc.bottom-rc.top-SCROLLBAR_SIZE, RGB(255,255,255) );
	}
	

/*
	dc.FillSolidRect( rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, RGB(255,255,255) );
*/
	CFont font;
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));       // zero out structure
	lf.lfHeight = 12;                      // request a 12-pixel-height font
	strcpy(lf.lfFaceName, "Arial");        // request a face name "Arial"
	VERIFY(font.CreateFontIndirect(&lf));  // create the font
	
	CFont* def_font = dc.SelectObject(&font);
	dc.SetBkMode( TRANSPARENT );

	CPen grayPen( PS_SOLID, 1, RGB(200,200,200) );
	dc.SelectObject( &grayPen );

	int nMin = 0, nMax = 0;
	float fMinValX = m_fMaxValX, fMaxValX = m_fMinValX;			//��� ���������� � ����������� ������� ���������� �� ��� X �� ������
	float fMinValY = m_fMaxValY, fMaxValY = m_fMinValY;			//�� ��� Y

	if ( m_BottomScroll.IsWindowVisible() )
	{
		GetVisibleX( &nMin, &nMax );
	}
	else
	{
		nMin = 0;
		nMax = (rc.right - rc.left - LEFT)/m_XS;
	}

	fMinValX = m_fMinValX + nMin * m_fStepX;
	fMaxValX = m_fMinValX + nMax * m_fStepX;
	if ( fMaxValX > m_fMaxValX )
		fMaxValX = m_fMaxValX;

	float maxTop, maxRight;
	{
		GetScreenByValue( m_fMaxValX, m_fMaxValY, &maxRight, &maxTop );
		if ( maxTop < rc.top )
			maxTop = rc.top;
		if ( maxRight > rc.right )
			maxRight = rc.right;
	}
	for ( int x=nMin; x<=nMax; x++ )
	{
		float fVal = m_fMinValX + x * m_fStepX;
		if ( fVal > m_fMaxValX )
			break;

		int nDrawX = (x - nMin)*m_XS + LEFT;
		dc.MoveTo( nDrawX, rc.bottom - BOTTOM );
		dc.LineTo( nDrawX, maxTop );
		
		RECT textRC;
		textRC.left = nDrawX - m_XS;
		textRC.right = nDrawX + m_XS;
		textRC.top = rc.bottom - BOTTOM + TEXT_SPACE;
		textRC.bottom = rc.bottom - SCROLLBAR_SIZE;

		CString szStr;

		szStr.Format( "%g", fVal );
		dc.DrawText( szStr, &textRC, DT_CENTER | DT_BOTTOM );
	}


	if ( m_LeftScroll.IsWindowVisible() )
	{
		GetVisibleY( &nMin, &nMax );
	}
	else
	{
		nMin = 0;
		nMax = (rc.bottom - rc.top - BOTTOM)/m_YS;
	}

	fMinValY = m_fMinValY + nMin * m_fStepY;
	fMaxValY = m_fMinValY + nMax * m_fStepY;
	for ( int y=nMin; y<=nMax; y++ )
	{
		float fVal = m_fMinValY + y * m_fStepY;
		if ( fVal > m_fMaxValY )
			break;
		
		int nDrawY = rc.bottom - BOTTOM - (y - nMin)*m_YS;
		dc.MoveTo( rc.left+LEFT, nDrawY );
		dc.LineTo( maxRight, nDrawY );
		
		RECT textRC;
		textRC.left = rc.left;
		textRC.right = rc.left + LEFT - TEXT_SPACE;
		textRC.top = nDrawY - 5;
		textRC.bottom = nDrawY + 5;
		
		CString szStr;
		szStr.Format( "%g", fVal );
		dc.DrawText( szStr, &textRC, DT_RIGHT | DT_BOTTOM );
	}
	
	CPen blackPen( PS_SOLID, 1, RGB(0,0,0) );
	CPen redPen( PS_SOLID, 1, RGB(255,0,0) );
	CPen bluePen( PS_SOLID, 1, RGB(0,0,255) );
	dc.SelectObject( &blackPen );

	if ( framesList.size() > 0 )
	{
		int i = 0;
		CFramesList::iterator it=framesList.begin();
		float fPrevX = LEFT, fPrevY = rc.bottom - BOTTOM;
		bool bFlag = false;
		for ( ; it!=framesList.end(); ++it )
		{
			float x = it->first;
			float y = it->second;
			
			float fScreenX;
			float fScreenY;
			GetScreenByValue( x, y, &fScreenX, &fScreenY );
			
			if ( bFlag )
			{
				dc.SelectObject( &blackPen );
				float x1 = fPrevX, y1 = fPrevY;
				float x2 = fScreenX, y2 = fScreenY;
				if ( x2 < LEFT )
				{
					fPrevX = fScreenX;
					fPrevY = fScreenY;
					i++;
					continue;
				}
				
				if ( fPrevX < LEFT )
				{
					y1 = (fScreenY-fPrevY)/(fScreenX-fPrevX)*(LEFT-fPrevX)+fPrevY;
					x1 = LEFT;
				}
				if ( y1 > rc.bottom - BOTTOM )
				{
					x1 = (fScreenX-fPrevX)/(fScreenY-fPrevY)*(rc.bottom-BOTTOM-fPrevY)+fPrevX;
					y1 = rc.bottom - BOTTOM;
				}
				if ( fScreenY >rc.bottom - BOTTOM )
				{
					x2 = (fScreenX-fPrevX)/(fScreenY-fPrevY)*(rc.bottom-BOTTOM-fPrevY)+fPrevX;
					y2 = rc.bottom - BOTTOM;
				}
				
				dc.MoveTo( x1, y1 );
				dc.LineTo( x2, y2 );
			}

			if ( fScreenX >= LEFT && fScreenY <= rc.bottom-BOTTOM )
			{
				if ( i == m_nDragIndex && i != m_nHighNodeIndex )
				{
					dc.SelectObject( &redPen );
					dc.Rectangle( fScreenX-SELECT_SIZE, fScreenY-SELECT_SIZE, fScreenX+SELECT_SIZE, fScreenY+SELECT_SIZE );
					dc.SelectObject( &blackPen );
				}
				else if ( i == m_nHighNodeIndex )
					dc.FillSolidRect( fScreenX-SELECT_SIZE, fScreenY-SELECT_SIZE, 2*SELECT_SIZE, 2*SELECT_SIZE, RGB(0,0,255) );
				else
					dc.Rectangle( fScreenX-SELECT_SIZE, fScreenY-SELECT_SIZE, fScreenX+SELECT_SIZE, fScreenY+SELECT_SIZE );
			}

			fPrevX = fScreenX;
			fPrevY = fScreenY;
			bFlag = true;
			i++;
		}

		if ( bFlag )
		{
			dc.MoveTo( fPrevX, fPrevY );
			float fSX, fSY;
			GetScreenByValue( m_fMaxValX, m_fMaxValY, &fSX, &fSY );
			if ( fSX < rc.right )
			{
				dc.LineTo( fSX, fPrevY );
			}
			else
			{
				dc.LineTo( rc.right, fPrevY );
			}
		}
	}

	dc.SelectObject(def_font);
	font.DeleteObject();
	
	paintDC.BitBlt( SCROLLBAR_SIZE, 0, rc.right-SCROLLBAR_SIZE, rc.bottom-SCROLLBAR_SIZE, &dc, SCROLLBAR_SIZE, 0, SRCCOPY );
	dc.SelectObject( pOldBitmap );
}

void CKeyFrameEditor::SetXResizeMode( bool bResizeMode )
{
	m_bResizeMode = bResizeMode;
	SetHDimention( m_fMinValX, m_fMaxValX );
}

void CKeyFrameEditor::SetDimentions( float fMinX, float fMaxX, float fStepX, float fMinY, float fMaxY, float fStepY )
{
	m_fStepX = fStepX;
	m_fStepY = fStepY;
	SetHDimention( fMinX, fMaxX );
	SetVDimention( fMinY, fMaxY );
	if ( m_LeftScroll.IsWindowVisible() )
	{
		int nmin, nmax;
		m_LeftScroll.GetScrollRange( &nmin, &nmax );
		m_LeftScroll.SetScrollPos( nmax );
	}
}

void CKeyFrameEditor::SetHDimention( float fMin, float fMax )
{
	m_fMinValX = fMin;
	m_fMaxValX = fMax;

	if ( m_bResizeMode )
	{
		m_BottomScroll.ShowScrollBar( FALSE );

		RECT rc;
		GetClientRect( &rc );
		m_XS = (rc.right-rc.left-LEFT-15)/(m_fMaxValX - m_fMinValX)*m_fStepX;		//��� 15 ��� ������ ������
		Invalidate();
		return;
	}
	else
	{
		m_XS = XS;

		SCROLLINFO scinfo;
		scinfo.cbSize = sizeof(SCROLLINFO);
		scinfo.fMask = SIF_PAGE | SIF_RANGE;
		
		RECT rc;
		GetClientRect( &rc );
		int nNumberInSB = (fMax - fMin) / m_fStepX + 0.5f;
		int nNumberOnTheScreen = (rc.right - rc.left - LEFT - m_XS/2) / m_XS + 0.5f;
		if ( nNumberOnTheScreen > nNumberInSB )
		{
			m_BottomScroll.ShowScrollBar( FALSE );
			return;
		}
		else
		{
			m_BottomScroll.ShowScrollBar();
		}
		
		scinfo.nMin = 0;
		scinfo.nMax = nNumberInSB - 1;
		scinfo.nPage = nNumberOnTheScreen;
		
		m_BottomScroll.SetScrollInfo( &scinfo );
		Invalidate();
	}
}

void CKeyFrameEditor::SetVDimention( float fMin, float fMax )
{
	m_fMinValY = fMin;
	m_fMaxValY = fMax;
	
	SCROLLINFO scinfo;
	scinfo.cbSize = sizeof(SCROLLINFO);
	scinfo.fMask = SIF_PAGE | SIF_RANGE;
	
	RECT rc;
	GetClientRect( &rc );
	int nNumberInSB = (fMax - fMin) / m_fStepY + 0.5f;
	int nNumberOnTheScreen = (rc.bottom - rc.top - BOTTOM - m_YS/4) / m_YS + 0.5f;
	if ( nNumberOnTheScreen >= nNumberInSB )
	{
		m_LeftScroll.ShowScrollBar( FALSE );
		Invalidate();
		return;
	}
	else
	{
		m_LeftScroll.ShowScrollBar();
	}
	
	scinfo.nMin = 0;
	scinfo.nMax = nNumberInSB - 1;
	scinfo.nPage = nNumberOnTheScreen;
	
	m_LeftScroll.SetScrollInfo( &scinfo );
	Invalidate();
}

void CKeyFrameEditor::GetVisibleX( int *nMin, int *nMax )
{
	RECT rc;
	GetClientRect( &rc );

	*nMin = m_BottomScroll.GetScrollPos();
	*nMax = *nMin + (rc.right - rc.left - LEFT - m_XS/2)/m_XS;
}

void CKeyFrameEditor::GetVisibleY( int *nMin, int *nMax )
{
	RECT rc;
	GetClientRect( &rc );

	int minpos;
	int maxpos;
	m_LeftScroll.GetScrollRange(&minpos, &maxpos);
	*nMax = maxpos - m_LeftScroll.GetScrollPos() + 1;
	*nMin = *nMax - (rc.bottom - rc.top - BOTTOM - m_YS/4)/m_YS;
}

void CKeyFrameEditor::GetScreenByValue( float fValX, float fValY, float *pScreenX, float *pScreenY )
{
	int nMin, nMax;
	
	if ( m_BottomScroll.IsWindowVisible() )
		GetVisibleX( &nMin, &nMax );
	else
		nMin = 0;
	float fMinValX = m_fMinValX + nMin * m_fStepX;
	float fScaleX = (float) m_XS / m_fStepX;
	*pScreenX = (fValX - fMinValX) * fScaleX + LEFT;
	
	if ( m_LeftScroll.IsWindowVisible() )
		GetVisibleY( &nMin, &nMax );
	else
		nMin = 0;
	float fMinValY = m_fMinValY + nMin * m_fStepY;
	float fScaleY = (float) m_YS / m_fStepY;

	RECT rc;
	GetClientRect( &rc );
	*pScreenY = rc.bottom - ( (fValY - fMinValY) * fScaleY + BOTTOM );
}

void CKeyFrameEditor::GetValueByScreen( int x, int y, float *pValX, float *pValY )
{
	int nMin, nMax;

	if ( m_BottomScroll.IsWindowVisible() )
		GetVisibleX( &nMin, &nMax );
	else
		nMin = 0;
	float fMinValX = m_fMinValX + nMin * m_fStepX;
	float fScaleX = m_fStepX / m_XS;
	*pValX = fMinValX + (x - LEFT) * fScaleX;

	if ( m_LeftScroll.IsWindowVisible() )
		GetVisibleY( &nMin, &nMax );
	else
		nMin = 0;
	float fMinValY = m_fMinValY + nMin * m_fStepY;
	float fScaleY = m_fStepY / m_YS;
	RECT rc;
	GetClientRect( &rc );
	*pValY = fMinValY + (rc.bottom - y - BOTTOM) * fScaleY;
}

void CKeyFrameEditor::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	
	CWnd::OnRButtonDown(nFlags, point);
}

void CKeyFrameEditor::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CPoint screen = point;
	ClientToScreen( &screen );
	GetParent()->PostMessage( WM_KEY_FRAME_RCLICK, screen.x, screen.y );

	CWnd::OnRButtonUp(nFlags, point);
}

void CKeyFrameEditor::OnKeyframeZoominx() 
{
/*
	if ( nScaleIndexX > 0 )
	{
		int nPos = m_BottomScroll.GetScrollPos();
		nPos = nPos * fScaleX / SIZES[nScaleIndexX-1];
		
		nScaleIndexX--;
		SetHDimention( m_fMinValX, m_fMaxValX );
		m_BottomScroll.SetScrollPos( nPos );
	}
*/
}

void CKeyFrameEditor::OnKeyframeZoominy() 
{
/*
	if ( nScaleIndexY > 0 )
	{
		int nPos = m_LeftScroll.GetScrollPos();
		nPos = nPos * fScaleY / SIZES[nScaleIndexY-1];
		
		nScaleIndexY--;
		SetVDimention( m_fMinValY, m_fMaxValY );
		m_LeftScroll.SetScrollPos( nPos );
	}
*/
}

void CKeyFrameEditor::OnKeyframeZoomoutx() 
{
/*
	if ( nScaleIndexX < sizeof(SIZES) / sizeof(SIZES[0]) - 1 )
	{
		int nPos = m_BottomScroll.GetScrollPos();
		nPos = nPos * fScaleX / SIZES[nScaleIndexX+1];
		
		nScaleIndexX++;
		SetHDimention( m_fMinValX, m_fMaxValX );
		m_BottomScroll.SetScrollPos( nPos );
	}
*/
}

void CKeyFrameEditor::OnKeyframeZoomouty() 
{
/*
	if ( nScaleIndexY < sizeof(SIZES) / sizeof(SIZES[0]) - 1 )
	{
		int nPos = m_LeftScroll.GetScrollPos();
		nPos = nPos * fScaleY / SIZES[nScaleIndexY+1];
		
		nScaleIndexY++;
		SetVDimention( m_fMinValY, m_fMaxValY );
		m_LeftScroll.SetScrollPos( nPos );
	}
*/
}

void CKeyFrameEditor::OnUpdateKeyframeZoominx(CCmdUI* pCmdUI) 
{
	
}

void CKeyFrameEditor::OnUpdateKeyframeZoominy(CCmdUI* pCmdUI) 
{
	
}

void CKeyFrameEditor::OnUpdateKeyframeZoomoutx(CCmdUI* pCmdUI) 
{
	
}

void CKeyFrameEditor::OnUpdateKeyframeZoomouty(CCmdUI* pCmdUI) 
{
	
}

/*
CFramesList::iterator CKeyFrameEditor::GetNearNodeIndex( int x, int y, int *pIndex )
{
	int i = 0;
	bool bFound = false;
	CFramesList::iterator it=framesList.begin();
	for ( ; it!=framesList.end(); ++it )
	{
		float x = it->first;
		float y = it->second;
		
		float fScreenX;
		float fScreenY;
		GetScreenByValue( x, y, &fScreenX, &fScreenY );
		
		if ( fScreenX >= x-SELECT_SIZE && fScreenX <= x+SELECT_SIZE )
		{
			bFound = true;
			break;
		}
		
		if ( x < fScreenX )
			break;
		
		i++;
	}

	if ( bFound )
		*pIndex = i;
	else
		*pIndex = -1;
	return it;
}
*/

void CKeyFrameEditor::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();


	int i = 0;
	bool bFound = false;
	CFramesList::iterator it=framesList.begin();
	for ( ; it!=framesList.end(); ++it )
	{
		float x = it->first;
		float y = it->second;
		
		float fScreenX;
		float fScreenY;
		GetScreenByValue( x, y, &fScreenX, &fScreenY );
		
		if ( fScreenX >= point.x-SELECT_SIZE && fScreenX <= point.x+SELECT_SIZE )
		{
			bFound = true;
			break;
		}
		
		if ( point.x < fScreenX )
			break;					//� ���� ������ �� ������ �������� ����� ��� ����� it
		
		i++;
	}
	
	if ( bFound )
	{
		float fValX, fValY;
		GetValueByScreen( point.x, point.y, &fValX, &fValY );
		if ( fValY > m_fMaxValY )
			fValY = m_fMaxValY;
		else if ( fValY < m_fMinValY )
			fValY = m_fMinValY;

		it->second = fValY;
		m_beginDrag.x = point.x;
		m_beginDrag.y = point.y;

		m_nDragIndex = i;
		m_mode = E_DRAG_MODE;
		SetCursor( LoadCursor(0, IDC_CROSS ) );
		Invalidate();

		SetCapture();
	}
	else
	{
		float fValX, fValY;
		GetValueByScreen( point.x, point.y, &fValX, &fValY );
		if ( fValX >= m_fMinValX && fValX <= m_fMaxValX && fValY >= m_fMinValY && fValY <= m_fMaxValY )
		{
			pair<float, float> para;
			para.first = fValX;
			para.second = fValY;
			
			framesList.insert( it, para );
			m_beginDrag.x = point.x;
			m_beginDrag.y = point.y;
			m_nDragIndex = i;
			m_mode = E_DRAG_MODE;
			SetCursor( LoadCursor(0, IDC_CROSS ) );
			Invalidate();

			SetCapture();
		}
	}

	CWnd::OnLButtonDown(nFlags, point);
}

void CKeyFrameEditor::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	m_mode = E_FREE_MODE;
	SetCursor( LoadCursor(0, IDC_ARROW ) );
	GetParent()->PostMessage( WM_KEY_FRAME_UPDATE );
	
	CWnd::OnLButtonUp(nFlags, point);
}

void CKeyFrameEditor::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( m_mode == E_FREE_MODE )
	{
		int i = 0;
		float fScreenX;
		float fScreenY;
		bool bFound = false;
		CFramesList::iterator it=framesList.begin();

		for ( ; it!=framesList.end(); ++it )
		{
			float x = it->first;
			float y = it->second;
			
			GetScreenByValue( x, y, &fScreenX, &fScreenY );
			
			if ( fScreenX >= point.x-SELECT_SIZE && fScreenX <= point.x+SELECT_SIZE )
			{
				bFound = true;
				break;
			}
			
			if ( point.x < fScreenX )
				break;					//� ���� ������ �� ������ �������� ����� ��� ����� it
			
			i++;
		}

		if ( bFound )
		{
			m_nHighNodeIndex = i;
			Invalidate();
		}
		else if ( m_nHighNodeIndex != -1 )
		{
			m_nHighNodeIndex = -1;
			Invalidate();
		}

		CWnd::OnMouseMove(nFlags, point);
		return;
	}

	if ( m_mode == E_DRAG_MODE )
	{
		RECT rc;
		GetClientRect( &rc );

		int i = 0;
		CFramesList::iterator it=framesList.begin();
		for ( ; it!=framesList.end(); ++it )
		{
			if ( i == m_nDragIndex )
				break;

			i++;
		}
		NI_ASSERT( it != framesList.end() );

		CFramesList::iterator prev = it, next = it;
		if ( it != framesList.begin() )
		{
			--prev;
			float fMinX, fTemp;
			GetScreenByValue( prev->first, prev->second, &fMinX, &fTemp );
			if ( point.x < fMinX + NODE_SPACE )
				point.x = fMinX + NODE_SPACE;
			else if ( point.x < LEFT )
			{
				if ( m_BottomScroll.IsWindowVisible() )
				{
					int nPos = m_BottomScroll.GetScrollPos();
					if ( nPos > 0 )
						m_BottomScroll.SetScrollPos( nPos-1 );
				}
				point.x = LEFT;
			}
		}

		++next;
		if ( next != framesList.end() )
		{
			float fMaxX;
			{
				float fTemp;
				GetScreenByValue( next->first, next->second, &fMaxX, &fTemp );
			}
			if ( point.x > fMaxX - NODE_SPACE )
				point.x = fMaxX - NODE_SPACE;
			else
			{
				int nMin, nMax;
				GetVisibleX( &nMin, &nMax );
				int nMaxScreenPosX = LEFT + (nMax - nMin)*m_XS;
				if ( point.x > nMaxScreenPosX )
				{
					if ( m_BottomScroll.IsWindowVisible() )
					{
						int minpos, maxpos;
						m_BottomScroll.GetScrollRange( &minpos, &maxpos );
						int nPos = m_BottomScroll.GetScrollPos();
						if ( nPos < maxpos )
							m_BottomScroll.SetScrollPos( nPos+1 );
					}
					point.x = nMaxScreenPosX;
				}
			}
		}

		{
			if ( point.y > rc.bottom - BOTTOM )
			{
				if ( m_LeftScroll.IsWindowVisible() )
				{
					int minpos, maxpos;
					m_LeftScroll.GetScrollRange( &minpos, &maxpos );
					int nPos = m_LeftScroll.GetScrollPos();
					if ( nPos < maxpos )
						m_LeftScroll.SetScrollPos( nPos+1 );
				}
				point.y = rc.bottom - BOTTOM;
			}
			else if ( point.y < m_YS/4 )
			{
				if ( m_LeftScroll.IsWindowVisible() )
				{
					int nPos = m_LeftScroll.GetScrollPos();
					if ( nPos > 0 )
						m_LeftScroll.SetScrollPos( nPos-1 );
				}
				point.y = m_YS/4;
			}
		}

		{
			float fValX, fValY;
			GetValueByScreen( point.x, point.y, &fValX, &fValY );
			if ( fValX > m_fMaxValX )
				fValX = m_fMaxValX;
			if ( fValY > m_fMaxValY )
				fValY = m_fMaxValY;
			if ( m_nDragIndex != 0 )
				it->first = fValX;
			it->second = fValY;
		}

/*
		if ( m_nDragIndex == 0 )
		{
			float fTemp;
			GetValueByScreen( point.x, point.y, &fTemp, &it->second );
		}
		else
			GetValueByScreen( point.x, point.y, &it->first, &it->second );
*/

		m_beginDrag = point;
		Invalidate();
	}
	
	CWnd::OnMouseMove(nFlags, point);
}

void CKeyFrameEditor::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch ( nChar )
	{
		case VK_DELETE:
			DeleteActiveNode();
			break;

		case VK_LEFT:
			{
				int nPos = m_BottomScroll.GetScrollPos();
				if ( nPos > 0 )
				{
					m_BottomScroll.SetScrollPos( nPos - 1 );
					Invalidate();
				}
				break;
			}

		case VK_RIGHT:
			{
				int minpos, maxpos;
				m_BottomScroll.GetScrollRange( &minpos, &maxpos );

				int nPos = m_BottomScroll.GetScrollPos();
				if ( nPos < maxpos )
				{
					m_BottomScroll.SetScrollPos( nPos + 1 );
					Invalidate();
				}
				break;
			}

		case VK_UP:
			{
				int nPos = m_LeftScroll.GetScrollPos();
				if ( nPos > 0 )
				{
					m_LeftScroll.SetScrollPos( nPos - 1 );
					Invalidate();
				}
				break;
			}
			
		case VK_DOWN:
			{
				int minpos, maxpos;
				m_LeftScroll.GetScrollRange( &minpos, &maxpos );
				
				int nPos = m_LeftScroll.GetScrollPos();
				if ( nPos < maxpos )
				{
					m_LeftScroll.SetScrollPos( nPos + 1 );
					Invalidate();
				}
				break;
			}
	}

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CKeyFrameEditor::ResetNodes()
{
	if ( framesList.size() > 0 )
	{
		pair<float, float> first = framesList.front();
		framesList.clear();
		framesList.push_back( first );
	}
	Invalidate();
}

void CKeyFrameEditor::DeleteActiveNode()
{
	if ( m_nDragIndex != 0 )
	{
		int i = 0;
		CFramesList::iterator it=framesList.begin();
		for ( ; it!=framesList.end(); ++it )
		{
			if ( i == m_nDragIndex )
				break;
			i++;
		}
		
		if ( it != framesList.end() )
		{
			framesList.erase( it );
			Invalidate();
			GetParent()->PostMessage( WM_KEY_FRAME_UPDATE );
		}
	}
}

