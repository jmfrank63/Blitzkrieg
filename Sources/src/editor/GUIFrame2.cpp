#include "StdAfx.h"
#include <Mmsystem.h>

#include "..\Scene\Scene.h"
#include "editor.h"
#include "frames.h"
#include "TreeDockWnd.h"
#include "PropertyDockBar.h"
#include "GameWnd.h"
#include "GUIFrame.h"
#include "GUITreeItem.h"

static const int WIDTH = 3;			//ширина и высота прямоугольника растягивания
static const int MINIMAL = 5;		//минимальная ширина и высота контрола

CTRect<float> CGUIFrame::GetElementRect( IUIElement *pElement )
{
	NI_ASSERT( pElement != 0 );

	int nPositionFlag = pElement->GetPositionFlag();
	CTRect<float> rc;
	IManipulator *pManipulator = pElement->GetManipulator();
	variant_t val;
	pManipulator->GetValue( "Pos.X", &val );
	rc.left = val.intVal;
	pManipulator->GetValue( "Pos.Y", &val );
	rc.top = val.intVal;	
	pManipulator->GetValue( "Size.X", &val );
	rc.right = rc.left + val.intVal;
	pManipulator->GetValue( "Size.Y", &val );
	rc.bottom = rc.top + val.intVal;
	
	IUIContainer *pParent = pElement->GetParent();
	if ( pParent == 0 )
		return rc;
	CTRect<float> rcParent = GetElementRect( pParent );
	
	switch ( nPositionFlag & 0xf )
	{
		case UIPLACE_LEFT:
			rc.x1 = rcParent.x1 + rc.x1;
			rc.x2 = rcParent.x1 + rc.x2;
			break;
		case UIPLACE_RIGHT:
			rc.x1 = rcParent.x2 - rc.x1;
			rc.x2 = rcParent.x2 - rc.x2;
			break;
		case UIPLACE_HMID:
			rc.x1 = rcParent.Width()/2 + rc.x1;
			rc.x2 = rcParent.Width()/2 + rc.x2;
			break;
	}
	
	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			rc.y1 = rcParent.y1 + rc.y1;
			rc.y2 = rcParent.y1 + rc.y2;
			break;
		case UIPLACE_BOTTOM:
			rc.y1 = rcParent.y2 - rc.y1;
			rc.y2 = rcParent.y2 - rc.y2;
			break;
		case UIPLACE_VMID:
			rc.y1 = rcParent.Height()/2 - rc.y1;
			rc.y2 = rcParent.Height()/2 - rc.y2;
			break;
	}

	return rc;
}

void CGUIFrame::SetElementRect( IUIElement *pElement, const CTRect<float> &rc )
{
	NI_ASSERT( pElement != 0 );
	
	IUIContainer *pParent = pElement->GetParent();
	int nPositionFlag = pElement->GetPositionFlag();
	NI_ASSERT( pParent != 0 );
	CTRect<float> rcParent = GetElementRect( pParent );
	
	IManipulator *pManipulator = pElement->GetManipulator();
	variant_t val;
	CVec2 vPos, vSize;
	vSize.x = rc.Width();
	vSize.y = rc.Height();

	switch ( nPositionFlag & 0xf )
	{
		case UIPLACE_LEFT:
			vPos.x = rc.x1 - rcParent.x1;
			break;
		case UIPLACE_RIGHT:
			vPos.x = rcParent.x2 - rc.x2;
			break;
		case UIPLACE_HMID:
			vPos.x = rcParent.x1 + rcParent.Width()/2 + vSize.x/2 - rc.x1;
			break;
	}
	
	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			vPos.y = rc.y1 - rcParent.y1;
			break;
		case UIPLACE_BOTTOM:
			vPos.y = rcParent.y2 - rc.y2;
			break;
		case UIPLACE_VMID:
			vPos.y = rcParent.y1 +rcParent.Height()/2 + vSize.y/2 - rc.y1;
			break;
	}
	
	val.intVal = vPos.x;
	pManipulator->SetValue( "Pos.X", val );
	val.intVal = vPos.y;
	pManipulator->SetValue( "Pos.Y", val );
	val.intVal = vSize.x;
	pManipulator->SetValue( "Size.X", val );
	val.intVal = vSize.y;
	pManipulator->SetValue( "Size.Y", val );
	pElement->Reposition( rcParent );
}

void CGUIFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if ( nChar == VK_DELETE )
	{
		for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
		{
			m_pContainer->RemoveChild( *it );
		}
		m_pHigh = 0;
		m_selectedList.clear();
		pPropertyDockBar->ClearVariables();
		GFXDraw();
	}
	
	CParentFrame::OnKeyDown(nChar, nRepCnt, nFlags);
}

CGUIFrame::EResizeMode CGUIFrame::GetResizeMode( IUIElement *pElement, float x, float y )
{
	CTRect<float> rc = GetElementRect( pElement );
	if ( x >= rc.x1 - WIDTH && x <= rc.x1 + WIDTH && y >= rc.y1 - WIDTH && y <= rc.y1 + WIDTH )
		return R_LEFT_TOP;
	if ( x >= rc.x2 - WIDTH && x <= rc.x2 + WIDTH && y >= rc.y1 - WIDTH && y <= rc.y1 + WIDTH )
		return R_RIGHT_TOP;
	if ( x >= rc.x1 - WIDTH && x <= rc.x1 + WIDTH && y >= rc.y2 - WIDTH && y <= rc.y2 + WIDTH )
		return R_LEFT_BOTTOM;
	if ( x >= rc.x2 - WIDTH && x <= rc.x2 + WIDTH && y >= rc.y2 - WIDTH && y <= rc.y2 + WIDTH )
		return R_RIGHT_BOTTOM;
	
	int cx = (rc.x2 + rc.x1) / 2;
	int cy = (rc.y2 + rc.y1) / 2;
	
	if ( x >= cx - WIDTH && x <= cx + WIDTH && y >= rc.y1 - WIDTH && y <= rc.y1 + WIDTH )
		return R_TOP;
	if ( x >= cx - WIDTH && x <= cx + WIDTH && y >= rc.y2 - WIDTH && y <= rc.y2 + WIDTH )
		return R_BOTTOM;
	if ( x >= rc.x1 - WIDTH && x <= rc.x1 + WIDTH && y >= cy - WIDTH && y <= cy + WIDTH )
		return R_LEFT;
	if ( x >= rc.x2 - WIDTH && x <= rc.x2 + WIDTH && y >= cy - WIDTH && y <= cy + WIDTH )
		return R_RIGHT;
	
	return R_NORESIZE;
}

void CGUIFrame::GFXDrawFrame( const CTRect<float> &rc, DWORD color, float width )
{
	pGFX->SetTexture( 0, 0 );
	SGFXRect2 gfxRC;
	gfxRC.color = color;
	
	CTRect<float> valRC = rc;
	float temp;
	if ( valRC.right < valRC.left )
	{
		temp = valRC.left;
		valRC.left = valRC.right;
		valRC.right = temp;
	}
	if ( valRC.bottom < valRC.top )
	{
		temp = valRC.top;
		valRC.top = valRC.bottom;
		valRC.bottom = temp;
	}
	
	pGFX->SetupDirectTransform();
	gfxRC.rect = CTRect<float> ( valRC.left, valRC.top-width+1, valRC.right-1, valRC.top+1 );
	pGFX->DrawRects( &gfxRC, 1 );
	gfxRC.rect = CTRect<float> ( valRC.right-1, valRC.top, valRC.right+width-1, valRC.bottom-1 );
	pGFX->DrawRects( &gfxRC, 1 );
	gfxRC.rect = CTRect<float> ( valRC.left, valRC.bottom-1, valRC.right, valRC.bottom+width-1 );
	pGFX->DrawRects( &gfxRC, 1 );
	gfxRC.rect = CTRect<float> ( valRC.left-width+1, valRC.top, valRC.left+1, valRC.bottom-1 );
	pGFX->DrawRects( &gfxRC, 1 );
	pGFX->RestoreTransform();
}

void CGUIFrame::GFXDrawFilledRect( const CTRect<float> &rc, DWORD color )
{
	pGFX->SetTexture( 0, 0 );
	SGFXRect2 gfxRC;
	gfxRC.color = color;
	gfxRC.rect = rc;
	pGFX->SetupDirectTransform();
	pGFX->DrawRects( &gfxRC, 1 );
	pGFX->RestoreTransform();
}

void CGUIFrame::GFXDraw()
{
	const DWORD GREY_COLOR = 0xFFE0E0E0;
	const DWORD BLUE_COLOR = 0xFF0000FF;
	const DWORD RED_COLOR  = 0xFFFF0000;

	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );

	if ( bRunning )
	{
		pGFX->SetShadingEffect( 2 );
		m_pScreen->Draw( pGFX );
		GetSingleton<ICursor>()->Draw( pGFX );
		
		g_frameManager.GetGameWnd()->ValidateRect( 0 );
		pGFX->EndScene();
		pGFX->Flip();
		return;
	}
	
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

/*
	SGFXRect2 rc;
	rc.rect = CTRect<float> ( 100, 100, 200, 200 );
	rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
	pGFX->DrawRects( &rc, 1 );
*/

	m_pScreen->Draw( pGFX );
	if ( m_pHigh )
	{
		CTRect<float> rc = GetElementRect( m_pHigh );
		GFXDrawFrame( rc, RED_COLOR, 1 );
	}

	for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
	{
		CTRect<float> rc = GetElementRect( *it );
		GFXDrawFrame( rc, BLUE_COLOR, 2 );
	}

	if ( m_selectedList.size() == 1 )
	{
		CTRect<float> rc = GetElementRect( m_selectedList.front() );
		int cx = ( rc.x2 + rc.x1 ) / 2;
		int cy = ( rc.y2 + rc.y1 ) / 2;

		CTRect<float> mirc;
		mirc.x1 = rc.x1 - WIDTH;
		mirc.y1 = rc.y1 - WIDTH;
		mirc.x2 = rc.x1 + WIDTH;
		mirc.y2 = rc.y1 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = rc.x2 - WIDTH;
		mirc.x2 = rc.x2 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = cx - WIDTH;
		mirc.x2 = cx + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.y1 = rc.y2- WIDTH;
		mirc.y2 = rc.y2 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = rc.x1 - WIDTH;
		mirc.x2 = rc.x1 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = rc.x2 - WIDTH;
		mirc.x2 = rc.x2 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = rc.x1 - WIDTH;
		mirc.y1 = cy - WIDTH;
		mirc.x2 = rc.x1 + WIDTH;
		mirc.y2 = cy + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );

		mirc.x1 = rc.x2 - WIDTH;
		mirc.x2 = rc.x2 + WIDTH;
		GFXDrawFilledRect( mirc, BLUE_COLOR );
	}

/*
	for ( CRectList::iterator it=m_dragRectList.begin(); it!=m_dragRectList.end(); ++it )
	{
		GFXDrawFrame( *it, GREY_COLOR, 1 );
	}
*/

	pGFX->EndScene();
	pGFX->Flip();
}

void CGUIFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( bRunning )
	{
		CParentFrame::OnMouseMove(nFlags, point);
		return;
	}

	if ( nFlags & MK_LBUTTON && !m_selectedList.empty() && m_mode == MODE_FREE )
	{
		if ( m_selectedList.size() == 1 )
		{
			m_resizeMode = GetResizeMode( m_selectedList.front(), point.x, point.y );
			if ( m_resizeMode != R_NORESIZE )
			{
				SetChangedFlag( true );
				m_undoStack.push_back( new CSaveAllUndo( m_pContainer ) );
				m_mode = MODE_RESIZE;
				m_beginDrag.x = point.x;
				m_beginDrag.y = point.y;

				const char *pCursorName = IDC_ARROW;
				switch ( m_resizeMode )
				{
				case R_LEFT:
				case R_RIGHT:
					pCursorName = IDC_SIZEWE;
					break;
				case R_TOP:
				case R_BOTTOM:
					pCursorName = IDC_SIZENS;
					break;
				case R_LEFT_TOP:
				case R_RIGHT_BOTTOM:
					pCursorName = IDC_SIZENWSE;
					break;
				case R_RIGHT_TOP:
				case R_LEFT_BOTTOM:
					pCursorName = IDC_SIZENESW;
					break;
				}
				SetCursor( LoadCursor(0, pCursorName) );

				g_frameManager.GetGameWnd()->SetCapture();
				CParentFrame::OnMouseMove(nFlags, point);
				return;
			}
			else
				SetCursor( LoadCursor(0, IDC_ARROW) );
		}

		CWindowList::iterator it=m_selectedList.begin();
		for ( ; it!=m_selectedList.end(); ++it )
		{
			CTRect<float> rc = GetElementRect( *it );
			if ( rc.IsInside( point.x, point.y ) )
				break;
		}
		if ( it != m_selectedList.end() )
		{
			SetChangedFlag( true );
			m_undoStack.push_back( new CSaveAllUndo( m_pContainer ) );
			m_mode = MODE_SELECT;
			m_beginDrag.x = point.x;
			m_beginDrag.y = point.y;
			
			g_frameManager.GetGameWnd()->SetCapture();
		}
	}
	else if ( nFlags & MK_LBUTTON && !m_selectedList.empty() && m_mode == MODE_SELECT )
	{
		CTRect<float> clientRc = GetElementRect( m_pContainer );		//перетаскивание осуществляется только внутри этого прямоугольника
		if ( m_selectedList.front().GetPtr() == m_pContainer.GetPtr() )
			clientRc = GetElementRect( m_pScreen );

		CTRect<float> maxRC = GetElementRect( m_selectedList.front() );
		for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
		{
			CTRect<float> rc = GetElementRect( *it );
			if ( rc.left < maxRC.left )
				maxRC.left = rc.left;
			if ( rc.right > maxRC.right )
				maxRC.right = rc.right;
			if ( rc.top < maxRC.top )
				maxRC.top = rc.top;
			if ( rc.bottom > maxRC.bottom )
				maxRC.bottom = rc.bottom;
		}
		
		float fDeltaX = point.x - m_beginDrag.x;
		float fDeltaY = point.y - m_beginDrag.y;
		bool bFlagX = true, bFlagY = true;
		if ( maxRC.left + fDeltaX < clientRc.left )
		{
			fDeltaX = clientRc.left - maxRC.left;
			bFlagX = false;
		}
		if ( maxRC.right + fDeltaX > clientRc.right )
		{
			fDeltaX = clientRc.right - maxRC.right;
			bFlagX = false;
		}
		if ( maxRC.top + fDeltaY < clientRc.top )
		{
			fDeltaY = clientRc.top - maxRC.top;
			bFlagY = false;
		}
		if ( maxRC.bottom + fDeltaY > clientRc.bottom )
		{
			fDeltaY = clientRc.bottom - maxRC.bottom;
			bFlagY = false;
		}
		
		for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
		{
			CTRect<float> rc = GetElementRect( *it );
			rc.left += fDeltaX;
			rc.right += fDeltaX;
			rc.top += fDeltaY;
			rc.bottom += fDeltaY;
			SetElementRect( *it, rc );
		}

		if ( bFlagX )
			m_beginDrag.x = point.x;
		if ( bFlagY )
			m_beginDrag.y = point.y;
		GFXDraw();
	}
	else if ( nFlags & MK_LBUTTON && !m_selectedList.empty() && m_mode == MODE_RESIZE )
	{
		CTRect<float> clientRc = GetElementRect( m_pContainer );		//ресайзирование осуществляется только внутри этого прямоугольника
		if ( m_selectedList.front().GetPtr() == m_pContainer.GetPtr() )
			clientRc = GetElementRect( m_pScreen );

		CTRect<float> rc = GetElementRect( m_selectedList.front() );
		switch ( m_resizeMode )
		{
		case R_LEFT_TOP:
			if ( rc.x1 + point.x - m_beginDrag.x + 5 < rc.x2 )
			{
				if ( rc.x1 + point.x - m_beginDrag.x >= clientRc.x1 )
				{
					rc.x1 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x1 = clientRc.x1;
					m_beginDrag.x = clientRc.x1;
				}
			}
			if ( rc.y1 + point.y - m_beginDrag.y + 5 < rc.y2 )
			{
				if ( rc.y1 + point.y - m_beginDrag.y >= clientRc.y1 )
				{
					rc.y1 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y1 = clientRc.y1;
					m_beginDrag.y = clientRc.y1;
				}
			}
			break;
		case R_RIGHT_TOP:
			if ( rc.x2 + point.x - m_beginDrag.x > rc.x1 + 5 )
			{
				if ( rc.x2 + point.x - m_beginDrag.x <= clientRc.x2 )
				{
					rc.x2 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x2 = clientRc.x2;
					m_beginDrag.x = clientRc.x2;
				}
			}
			if ( rc.y1 + point.y - m_beginDrag.y + 5 < rc.y2 )
			{
				if ( rc.y1 + point.y - m_beginDrag.y >= clientRc.y1 )
				{
					rc.y1 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y1 = clientRc.y1;
					m_beginDrag.y = clientRc.y1;
				}
			}
			break;
		case R_LEFT_BOTTOM:
			if ( rc.x1 + point.x - m_beginDrag.x + 5 <= rc.x2 )
			{
				if ( rc.x1 + point.x - m_beginDrag.x >= clientRc.x1 )
				{
					rc.x1 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x1 = clientRc.x1;
					m_beginDrag.x = clientRc.x1;
				}
			}
			if ( rc.y2 + point.y - m_beginDrag.y >= rc.y1 + 5 )
			{
				if ( rc.y2 + point.y - m_beginDrag.y <= clientRc.y2 )
				{
					rc.y2 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y2 = clientRc.y2;
					m_beginDrag.y = clientRc.y2;
				}
			}
			break;
		case R_RIGHT_BOTTOM:
			if ( rc.x2 + point.x - m_beginDrag.x > rc.x1 + 5 )
			{
				if ( rc.x2 + point.x - m_beginDrag.x <= clientRc.x2 )
				{
					rc.x2 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x2 = clientRc.x2;
					m_beginDrag.x = clientRc.x2;
				}
			}
			if ( rc.y2 + point.y - m_beginDrag.y >= rc.y1 + 5 )
			{
				if ( rc.y2 + point.y - m_beginDrag.y <= clientRc.y2 )
				{
					rc.y2 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y2 = clientRc.y2;
					m_beginDrag.y = clientRc.y2;
				}
			}
			break;
		case R_LEFT:
			if ( rc.x1 + point.x - m_beginDrag.x + 5 <= rc.x2 )
			{
				if ( rc.x1 + point.x - m_beginDrag.x >= clientRc.x1 )
				{
					rc.x1 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x1 = clientRc.x1;
					m_beginDrag.x = clientRc.x1;
				}
			}
			break;
		case R_TOP:
			if ( rc.y1 + point.y - m_beginDrag.y + 5 < rc.y2 )
			{
				if ( rc.y1 + point.y - m_beginDrag.y >= clientRc.y1 )
				{
					rc.y1 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y1 = clientRc.y1;
					m_beginDrag.y = clientRc.y1;
				}
			}			break;
		case R_RIGHT:
			if ( rc.x2 + point.x - m_beginDrag.x > rc.x1 + 5 )
			{
				if ( rc.x2 + point.x - m_beginDrag.x <= clientRc.x2 )
				{
					rc.x2 += point.x - m_beginDrag.x;
					m_beginDrag.x = point.x;
				}
				else
				{
					rc.x2 = clientRc.x2;
					m_beginDrag.x = clientRc.x2;
				}
			}
			break;
		case R_BOTTOM:
			if ( rc.y2 + point.y - m_beginDrag.y >= rc.y1 + 5 )
			{
				if ( rc.y2 + point.y - m_beginDrag.y <= clientRc.y2 )
				{
					rc.y2 += point.y - m_beginDrag.y;
					m_beginDrag.y = point.y;
				}
				else
				{
					rc.y2 = clientRc.y2;
					m_beginDrag.y = clientRc.y2;
				}
			}
			break;
		}

		SetElementRect( m_selectedList.front(), rc );
		
		GFXDraw();
	}
	else if ( m_mode == MODE_FREE )
	{
		IUIElement *pNewHigh = 0;
		pNewHigh = m_pContainer->PickElement( CVec2(point.x, point.y), 1 );
		if ( !pNewHigh )
		{
			m_pHigh = pNewHigh;
			GFXDraw();
			SetCursor( LoadCursor(0, IDC_ARROW) );
			return;
		}
		
		if ( pNewHigh == m_pContainer )
		{
			m_pHigh = 0;
			GFXDraw();

			if ( m_selectedList.size() == 1 && m_selectedList.front().GetPtr() == m_pContainer.GetPtr() )
			{
				if ( pTemplatePropsItem == 0 )
				{
					int nResizeMode = GetResizeMode( pNewHigh, point.x, point.y );
					const char *pCursorName = IDC_SIZEALL;
					switch ( nResizeMode )
					{
					case R_LEFT:
					case R_RIGHT:
						pCursorName = IDC_SIZEWE;
						break;
					case R_TOP:
					case R_BOTTOM:
						pCursorName = IDC_SIZENS;
						break;
					case R_LEFT_TOP:
					case R_RIGHT_BOTTOM:
						pCursorName = IDC_SIZENWSE;
						break;
					case R_RIGHT_TOP:
					case R_LEFT_BOTTOM:
						pCursorName = IDC_SIZENESW;
						break;
					}
					SetCursor( LoadCursor(0, pCursorName) );
					return;				
				}
				else
				{
					SetCursor( LoadCursor(0, IDC_ARROW) );
					return;
				}
			}
		}

		if ( !m_selectedList.empty() )
		{
			if ( m_selectedList.size() == 1 )
			{
				int nResizeMode = GetResizeMode( m_selectedList.front(), point.x, point.y );
				const char *pCursorName = IDC_SIZEALL;
				if ( nResizeMode == R_NORESIZE && !m_selectedList.front()->IsInside( CVec2(point.x, point.y) ) )
					pCursorName = IDC_ARROW;
				switch ( nResizeMode )
				{
				case R_LEFT:
				case R_RIGHT:
					pCursorName = IDC_SIZEWE;
					break;
				case R_TOP:
				case R_BOTTOM:
					pCursorName = IDC_SIZENS;
					break;
				case R_LEFT_TOP:
				case R_RIGHT_BOTTOM:
					pCursorName = IDC_SIZENWSE;
					break;
				case R_RIGHT_TOP:
				case R_LEFT_BOTTOM:
					pCursorName = IDC_SIZENESW;
					break;
				}
				SetCursor( LoadCursor(0, pCursorName ) );
			}
			else
				SetCursor( LoadCursor(0, IDC_ARROW ) );
		}

		if ( pNewHigh == m_pHigh || pNewHigh == m_pContainer )
			return;				//этот элемент уже был подсвечен, не надо его выделять

		m_pHigh = pNewHigh;
		GFXDraw();
		CParentFrame::OnMouseMove(nFlags, point);
		return;
	}

	CParentFrame::OnMouseMove(nFlags, point);
}

void CGUIFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
/*
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//Если проект не был создан
		return;
*/
	SetFocus();
	if ( bRunning )
	{
		CParentFrame::OnLButtonDown(nFlags, point);
		return;
	}
	
	{
		IUIElement *pNewSelected;
		if ( IsCtrlKeyDown() )
			pNewSelected = m_pContainer->PickElement( CVec2(point.x, point.y), 10 );
		else
			pNewSelected = m_pContainer->PickElement( CVec2(point.x, point.y), 1 );
		
		if ( pNewSelected == m_pContainer && m_selectedList.size() == 1 )
		{
			m_resizeMode = GetResizeMode( m_selectedList.front(), point.x, point.y );
			if ( m_resizeMode != R_NORESIZE )
				return;
		}

		if ( pNewSelected == m_pContainer && pTemplatePropsItem != 0 )
		{
			m_selectedList.clear();
			pPropertyDockBar->ClearVariables();
			return;
		}

		CWindowList::iterator it=m_selectedList.begin();
		for ( ; it!=m_selectedList.end(); ++it )
		{
			if ( pNewSelected == *it )
			{
				if ( IsShiftKeyDown() )
				{
					m_selectedList.erase( it );
					GFXDraw();
					SetCursor( LoadCursor(0, IDC_ARROW ) );
				}

				CParentFrame::OnLButtonDown(nFlags, point);
				return;
			}
		}

		if ( m_selectedList.size() == 1 && pNewSelected == 0 )
		{
			m_resizeMode = GetResizeMode( m_selectedList.front(), point.x, point.y );
			if ( m_resizeMode == R_NORESIZE )
			{
				if ( !IsShiftKeyDown() )
				{
					m_selectedList.clear();
					pPropertyDockBar->ClearVariables();
					GFXDraw();
				}
			}
			else
			{
				const char *pCursorName = IDC_SIZEALL;
				switch ( m_resizeMode )
				{
				case R_LEFT:
				case R_RIGHT:
					pCursorName = IDC_SIZEWE;
					break;
				case R_TOP:
				case R_BOTTOM:
					pCursorName = IDC_SIZENS;
					break;
				case R_LEFT_TOP:
				case R_RIGHT_BOTTOM:
					pCursorName = IDC_SIZENWSE;
					break;
				case R_RIGHT_TOP:
				case R_LEFT_BOTTOM:
					pCursorName = IDC_SIZENESW;
					break;
				}
				SetCursor( LoadCursor(0, pCursorName ) );
			}
			CParentFrame::OnLButtonDown(nFlags, point);
			return;
		}

		if ( it == m_selectedList.end() )			//если не нажат уже выделенный элемент
		{
			if ( !IsShiftKeyDown() )
			{
				m_selectedList.clear();
				pPropertyDockBar->ClearVariables();
				if ( pNewSelected )
				{
					m_selectedList.push_back( pNewSelected );
					SetCursor( LoadCursor(0, IDC_SIZEALL ) );

					pPropertyDockBar->AddObjectWithProp( pNewSelected->GetManipulator() );
				}
			}
			else
			{
				if ( pNewSelected )
				{
					m_selectedList.push_back( pNewSelected );
					SetCursor( LoadCursor(0, IDC_SIZEALL ) );
					pPropertyDockBar->AddObjectWithProp( pNewSelected->GetManipulator() );
				}
			}
			
			GFXDraw();
		}

		CParentFrame::OnLButtonDown(nFlags, point);
		return;
	}

	CParentFrame::OnLButtonDown(nFlags, point);
}


void CGUIFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if ( bRunning )
	{
		CParentFrame::OnLButtonUp(nFlags, point);
		return;
	}
	ReleaseCapture();
	
/*
	if ( m_mode == MODE_SELECT )
	{
		NI_ASSERT( m_selectedList.size() > 0 );

		CRectList::iterator currentRect = m_dragRectList.begin();
		for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
		{
			SetElementRect( *it, *currentRect );
			++currentRect;
		}
		m_dragRectList.clear();
		GFXDraw();
	}
	else if ( m_mode == MODE_RESIZE )
	{
		SetElementRect( m_selectedList.front(), m_dragRectList.front() );
		m_dragRectList.clear();
		GFXDraw();
	}

	else 
*/
	if ( m_mode == MODE_FREE && !m_selectedList.empty() )
	{
		IUIElement *pNewSelected = 0;
		pNewSelected = m_pContainer->PickElement( CVec2(point.x, point.y), 1 );
		CWindowList::iterator it=m_selectedList.begin();
		for ( ; it!=m_selectedList.end(); ++it )
		{
			if ( pNewSelected == *it )
			{
				if ( !IsShiftKeyDown() )
				{
					m_selectedList.clear();
					m_selectedList.push_back( pNewSelected );
				}
				SetCursor( LoadCursor(0, IDC_SIZEALL ) );
				GFXDraw();
				CParentFrame::OnLButtonUp(nFlags, point);
				return;
			}
		}
	}
	if ( m_selectedList.empty() && pTemplatePropsItem != 0 )
	{
		if ( m_pScreen != m_pContainer && !m_pContainer->IsInside( CVec2(point.x, point.y) ) )
			return;

		IUIElement *pWindow = GUICreateElement();
		if ( pTemplatePropsItem->GetWindowType() == UI_DIALOG )
		{
			if ( m_pContainer.GetPtr() == m_pScreen.GetPtr() )
			{
				int nRes = IDYES;
				if ( !m_pScreen->IsEmpty() )
					nRes = AfxMessageBox( "Adding dialog will remove all created childs, confirm?", MB_YESNO );
				if ( nRes == IDYES )
				{
					SetChangedFlag( true );
					m_pContainer = static_cast<IUIContainer *> ( pWindow );
					m_pScreen->RemoveAllChildren();
					m_pScreen->AddChild( m_pContainer );
				}
				else
					return;			//нажата NO
			}
			else
				return;		//создался второй диалог
/*
				else if ( m_pContainer)
				{
				m_pContainer = static_cast<IUIContainer *> ( pWindow );
				}
*/
		}
		else
		{
			SetChangedFlag( true );
			m_undoStack.push_back( new CSaveAllUndo( m_pContainer ) );
			m_pContainer->AddChild( pWindow );
		}

		IManipulator *pManipulator = pWindow->GetManipulator();
		variant_t var;
		
		CTRect<float> clientRc = GetElementRect( m_pContainer );		//перетаскивание осуществляется только внутри этого прямоугольника
		if ( pTemplatePropsItem->GetWindowType() == UI_DIALOG )
			clientRc = GetElementRect( m_pScreen );
		CVec2 vPos, vSize;
		CTRect<float> wndRc;
		pWindow->GetWindowPlacement( &vPos, &vSize, &wndRc );

		var.intVal = point.x - clientRc.x1;
		if ( var.intVal + vSize.x > clientRc.right - clientRc.left )
			var.intVal = clientRc.right - clientRc.left - vSize.x;
		pManipulator->SetValue( "Pos.X", var );
		var.intVal = point.y - clientRc.y1;
		if ( var.intVal + vSize.y > clientRc.bottom -clientRc.top )
			var.intVal = clientRc.bottom - clientRc.top - vSize.y;
		pManipulator->SetValue( "Pos.Y", var );
		
		m_selectedList.push_back( pWindow );
		pPropertyDockBar->AddObjectWithProp( pManipulator );

		CTRect<float> rcScreen = GetElementRect( m_pScreen );
		m_pScreen->Reposition( rcScreen );
		GFXDraw();
	}

	m_mode = MODE_FREE;

	CParentFrame::OnLButtonUp(nFlags, point);
}

void CGUIFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	CParentFrame::OnRButtonDown(nFlags, point);
}


void CGUIFrame::OnRButtonUp(UINT nFlags, CPoint point) 
{
	if ( bRunning )
	{
		CParentFrame::OnRButtonUp(nFlags, point);
		return;
	}

	if ( m_selectedList.size() == 1 )
	{
		CTRect<float> rc = GetElementRect( m_selectedList.front() );
		if ( rc.IsInside( point.x, point.y ) )
		{
			ClientToScreen( &point );
			CMenu menu;
			menu.LoadMenu( IDR_ADD_TEMPLATE_MENU );
			CMenu *popup = menu.GetSubMenu( 0 );
			popup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this );
		}
	}
	
	CParentFrame::OnRButtonUp(nFlags, point);
}

CTreeItem *CGUIFrame::GetParentTreeItemForWindowType( int nWindowType )
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( !pTree )
		return 0;
	CTreeItem *pRoot = pTree->GetRootItem();
	if ( !pTree )
		return 0;

	for ( CTreeItem::CTreeItemList::const_iterator it=pRoot->GetBegin(); it!=pRoot->GetEnd(); ++it )
	{
		CTemplatesTreeItem *pTemplatesTreeItem = dynamic_cast<CTemplatesTreeItem *> ( it->GetPtr() );
		if ( !pTemplatesTreeItem )
			continue;
		if ( pTemplatesTreeItem->GetWindowType() == nWindowType )
			return pTemplatesTreeItem;
	}

	return 0;
}

void CGUIFrame::OnCreatenewtemplate() 
{
	if ( m_selectedList.size() != 1 )
		return;
	
	int nClassTypeID = 0;
	nClassTypeID = GetCommonFactory()->GetObjectTypeID( m_selectedList.front() );
	
	string szFileName = theApp.GetEditorDataDir();
	szFileName += "editor\\UI\\";
	szFileName += GetDirectoryFromWindowType( nClassTypeID );

	string szShortFileName = GetDirectoryFromWindowType( nClassTypeID );
	szShortFileName = szShortFileName.substr( 0, szShortFileName.size() - 2 );
	
	string szTempFileName;
	bool bFound = false;
	for ( int i=0; i<100; i++ )
	{
		szTempFileName = szFileName;
		szTempFileName += szShortFileName;
		szTempFileName += NStr::Format( "%.2d.xml", i );
		if ( _access( szTempFileName.c_str(), 00 ) )
		{
			bFound = true;
			szFileName = szTempFileName;
			szShortFileName += NStr::Format( "%.2d", i );;
			break;
		}
	}
	NI_ASSERT( bFound != false );

	CPtr<IDataStream> pStream = CreateFileStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	CTreeAccessor tree = pDT;
	tree.Add( "ClassTypeID", &nClassTypeID );
	m_selectedList.front()->operator&( *pDT );

	int nTemplateItemType = GetTreeItemTypeByWindowType( nClassTypeID );
	CTemplatePropsTreeItem *pTemplatePropsItem = ( CTemplatePropsTreeItem *) GetCommonFactory()->CreateObject( nTemplateItemType );
	pTemplatePropsItem->SetWindowType( nClassTypeID );
	pTemplatePropsItem->SetXMLFile( szFileName.c_str() );
	pTemplatePropsItem->SetItemName( szShortFileName.c_str() );

	CTreeItem *pParentTreeItem = GetParentTreeItemForWindowType( nClassTypeID );
	NI_ASSERT( pParentTreeItem != 0 );
	pParentTreeItem->AddChild( pTemplatePropsItem );

/*
	CPtr<IDataStream> *pStream = CreateFileStream( )
		CTreeAccessor
*/
}
