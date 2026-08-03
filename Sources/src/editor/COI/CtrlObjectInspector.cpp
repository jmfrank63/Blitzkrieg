
#include "StdAfx.h"
#include "CtrlObjectInspector.h"
#include "OIEdit.h"
#include "OICombo.h"
#include "OIBrowEdit.h"
#include "OIReference.h"
#include "OIColorEdit.h"
#include "../frames.h"

#include "../RefDlg.h"
#include "PropView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int ConvertFromDomenTypeToRef( int nDomenType )
{
	switch ( nDomenType )
	{
		case DT_ANIMATION_REF:
			return E_ANIMATIONS_REF;
			
		case DT_FUNC_PARTICLE_REF:
			return E_FUNC_PARTICLES_REF;

		case DT_EFFECT_REF:
			return E_EFFECTS_REF;

		case DT_WEAPON_REF:
			return E_WEAPONS_REF;

		case DT_SOLDIER_REF:
			return E_SOLDIER_REF;
		
		case DT_ACTION_REF:
			return E_ACTIONS_REF;

		case DT_SCENARIO_MISSION_REF:
			return E_SCENARIO_MISSIONS_REF;

		case DT_TEMPLATE_MISSION_REF:
			return E_TEMPLATE_MISSIONS_REF;

		case DT_CHAPTER_REF:
			return E_CHAPTERS_REF;
			
		case DT_SOUND_REF:
			return E_SOUNDS_REF;
			
		case DT_SETTING_REF:
			return E_SETTING_REF;

		case DT_ASK_REF:
			return E_ASKS_REF;

		case DT_DEATH_REF:
			return E_DEATHHOLE_REF;

		case DT_CRATER_REF:
			return E_CRATER_REF;

		case DT_MAP_REF:
			return E_MAP_REF;
			
		case DT_MUSIC_REF:
			return E_MUSIC_REF;

		case DT_MOVIE_REF:
			return E_MOVIE_REF;

		case DT_PARTICLE_TEXTURE_REF:
			return E_PARTICLE_TEXTURE_REF;

		case DT_WATER_TEXTURE_REF:
			return E_WATER_TEXTURE_REF;

		case DT_ROAD_TEXTURE_REF:
			return E_ROAD_TEXTURE_REF;

	}

	NI_ASSERT( 0 );
	return 0;
}


enum
{
	SUB_CTRL_EDIT   = 1100,
	SUB_CTRL_COMBO  = 1101,
	SUB_CTRL_BUTTON = 1102,
  SUB_CTRL_BEDIT  = 1103,
	SUB_CTRL_CEDIT  = 1104,
	SUB_CTRL_REFERENCE  = 1105,
};

const string STR_TRUE  = "true";
const string STR_FALSE = "false";

CCtrlObjectInspector::CCtrlObjectInspector() 
  : m_pEdit( new COIEdit ), m_pCombo( new COICombo ), 
	  m_pBEdit( new COIBrowseEdit ), m_pReference( new COIReferenceEdit), m_pCEdit( new COIColorEdit )
{
	m_nFirstElem = 0;
	m_nLineHeight = -1;
	m_nCurVirtualLine = -1;
	m_nCurGroup = GroupDefault;
  pActiveWnd = 0;
	bDraggingSplitter = false;
}

CCtrlObjectInspector::~CCtrlObjectInspector()
{
  if ( m_pEdit )
    delete m_pEdit;
  if ( m_pCombo )
    delete m_pCombo;
  if ( m_pBEdit )
    delete m_pBEdit;
	if ( m_pReference )
		delete m_pReference;
	if ( m_pCEdit )
		delete m_pCEdit;
  const_cast<COIEdit*>( m_pEdit ) = 0;
  const_cast<COIBrowseEdit*>( m_pBEdit ) = 0;
	const_cast<COIReferenceEdit*>( m_pReference ) = 0;
  const_cast<COICombo*>( m_pCombo ) = 0;
	const_cast<COIColorEdit*>( m_pCEdit ) = 0;
}


BEGIN_MESSAGE_MAP(CCtrlObjectInspector, CWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()



int CCtrlObjectInspector::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	Init();
	return 0;
}

inline CRect ScaleRect( const CRect &rect, int nHowMore )
{
	return CRect( rect.left - nHowMore, rect.top - nHowMore, rect.right + nHowMore, rect.bottom + nHowMore );
}

CRect CCtrlObjectInspector::GetPaintColPartRect( int nPaintLine, int nCol )
{
	if ( nCol )
		return CRect( m_nSplitterPos, nPaintLine * m_nLineHeight, m_sizeClient.cx, (nPaintLine+1) * m_nLineHeight );
	else
		return CRect( m_nLineHeight, nPaintLine * m_nLineHeight, m_nSplitterPos, (nPaintLine+1) * m_nLineHeight );
}

CRect CCtrlObjectInspector::GetTextColPartRect( int nPaintLine, int nCol, bool bHasIcon )
{
	CRect rect = GetPaintColPartRect( nPaintLine, nCol );
	rect.top+=1;
	rect.bottom+=1;
	if ( bHasIcon && nCol == 0 )
	{
		rect.left += 24;
		rect.left = min( rect.left, rect.right );
	}
	return rect;
}

CRect CCtrlObjectInspector::GetPaintLineRect( int nPaintLine )
{
	return CRect( 0, nPaintLine * m_nLineHeight, m_sizeClient.cx, (nPaintLine+1) * m_nLineHeight );
}

void CCtrlObjectInspector::DrawPlus( CDC* pDC, int nLine, bool isPlus )
{
	CRect rect = GetPlusRect(nLine);
	pDC->Rectangle(rect);
	CPoint ptCenter = rect.CenterPoint();
	int nTall = rect.Width() / 2 - 2;
	pDC->MoveTo( ptCenter.x - nTall, ptCenter.y );
	pDC->LineTo( ptCenter.x + nTall + 1, ptCenter.y );
	if ( isPlus )
	{
		pDC->MoveTo( ptCenter.x, ptCenter.y - nTall );
		pDC->LineTo( ptCenter.x, ptCenter.y + nTall + 1 );
	}
}

void CCtrlObjectInspector::OnPaint()
{
  ASSERT( m_pEdit );
  ASSERT( m_pBEdit );
	ASSERT( m_pReference );
  ASSERT( m_pCombo );

  CPaintDC paintDC(this); // device context for painting
  CDC dc; // back buffer DC
	CBitmap bmp;	
  dc.CreateCompatibleDC( &paintDC );

	CRect rectClient;
	GetClientRect( rectClient );
  bmp.CreateCompatibleBitmap( &paintDC, rectClient.Width(), rectClient.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );
	dc.FillSolidRect( 0, 0, rectClient.Width(), rectClient.Height(), GetSysColor( COLOR_BTNFACE ) );

	TEXTMETRIC sTextMetrics;
	dc.GetTextMetrics(&sTextMetrics);

	CFont *pOldFont = dc.SelectObject( &m_fntDef );
	dc.SetBkMode( TRANSPARENT );

	int nNumber = 1;
	for( CCOIPaintElemVector::const_iterator it = m_aPaintElems.begin() + m_nFirstElem; it != m_aPaintElems.end() && nNumber <= GetLineCount(); ++it, ++nNumber )
	{
		CRect rect = GetPaintLineRect(nNumber);
		rect.bottom = rect.top + 1;
		dc.FillSolidRect( rect, RGB(160,160,160) );

		const SCOIPaintElem &elem = *it;
		if ( elem.pProp )
		{
      const SCOIProperties &prop = *elem.pProp;
      rect = GetTextColPartRect( nNumber, 0, true );
			dc.DrawText( prop.strName.c_str(), prop.strName.length(), rect, DT_LEFT );
			POINT ptPicPos;
			ptPicPos.x = rect.left - 24;
			ptPicPos.y = rect.top;
			POINT ptIconOffset;
			ptIconOffset.x = ptIconOffset.y = 0;
			SIZE iconSize;
			iconSize.cx = iconSize.cy = 16;
			int nIconIndex;
			switch( prop.idDomen )
			{
			case DT_DEC:
				nIconIndex = 1;
				break;
			case DT_HEX:
				nIconIndex = 2;
				break;
			case DT_STR:
				nIconIndex = 3;
				break;
			case DT_BOOL:
				nIconIndex = 4;
				break;
			case DT_BROWSE:
				nIconIndex = 5;
				break;
			case DT_BROWSEDIR:
				nIconIndex = 6;
				break;
			case DT_COMBO:
				nIconIndex = 7;
				break;
			case DT_FLOAT:
				nIconIndex = 8;
				break;
			default:
				nIconIndex = 0;
				break;
			}
			imlIcons.DrawIndirect( &dc, nIconIndex, ptPicPos, iconSize, ptIconOffset, ILD_NORMAL, SRCCOPY, RGB( 255, 255, 255 ) );

			rect = GetTextColPartRect( nNumber, 1 );
			if ( prop.idDomen == DT_HEX )
			{
				CVariant &value = const_cast<CVariant&>( prop.varValue );
				value.SetType( CVariant::VT_INT32 );
			}
			string strVal = prop.varValue;
			if ( DT_COLOR != prop.idDomen )
				dc.DrawText( strVal.c_str(), strVal.length(), rect, DT_RIGHT );
			else
			{
				CBrush brush( atoi( strVal.c_str() ) );
				CRect  r = rect;
				r.DeflateRect( 5, 1, 3, 1 );
				dc.FillRect( &r, &brush );
			}
      if ( m_mapGroups[prop.idGroup].bRadioGroup && m_mapGroups[prop.idGroup].iActiveProp == prop.idProp )
      {
        CBrush brush( RGB( 170, 170, 50 ) );
        CBrush *pBrush = dc.SelectObject( &brush );
        CRect rect = GetPlusRect( nNumber );
        dc.Ellipse( &rect );
        dc.SelectObject( pBrush );
      }
		}
		else
		{
			const SCOIGroup &group = *elem.pGroup;
			DrawPlus( &dc, nNumber, !group.isExpand );
			rect = GetTextColPartRect( nNumber, 0 );
			dc.SelectObject( &m_fntDefBold );
			dc.DrawText( group.strGroupName.c_str(), group.strGroupName.length(), rect, DT_LEFT );
			dc.SelectObject( &m_fntDef );
		}
	}
	CRect rect = GetPaintLineRect( VirtualToPaintLine(m_nCurVirtualLine) );
	rect.left -= N_BORDER;
	rect.right += N_BORDER * 2;
	rect.bottom++;
	if ( m_haveFocus )
		dc.DrawEdge( rect, BDR_SUNKENOUTER, BF_RECT );

	rect = CRect( m_nSplitterPos - 1, 0, m_nSplitterPos + 1, m_sizeClient.cy + 2 );
	dc.DrawEdge( rect, EDGE_ETCHED, BF_RECT );

	rect = GetPaintColPartRect( 0, 0 );
	rect.left = 0;
	rect.bottom += 1;
	dc.DrawEdge( rect, EDGE_RAISED, BF_RECT ); // EDGE_ETCHED
	rect.left += 4;
	rect.top++;
	dc.DrawText( "Properties", rect, DT_LEFT );

	rect = GetPaintColPartRect( 0, 1 );
	rect.bottom += 1;
	dc.DrawEdge( rect, EDGE_RAISED, BF_RECT );
	rect.left += 4;
	rect.top++;
	dc.DrawText( "Value", rect, DT_LEFT );
	
	dc.SelectObject(pOldFont);

  paintDC.BitBlt( 0, 0, rectClient.Width(), rectClient.Height(), &dc, 0, 0, SRCCOPY );
  dc.SelectObject( pOldBitmap );
  bmp.DeleteObject();
}

void CCtrlObjectInspector::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	m_sizeClient.cx = cx;
	m_sizeClient.cy = cy;
	MakePaintList();
	UpdateScrollers();
  if ( pActiveWnd )
  {
    CRect rect;
    pActiveWnd->GetWindowRect( &rect );
    ScreenToClient( &rect );
    rect.left = m_nSplitterPos;
    rect.right = m_sizeClient.cx;
    
    pActiveWnd->MoveWindow( &rect );
  }
}

void CCtrlObjectInspector::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	OnLButtonDown( nFlags, point );
	CWnd::OnLButtonDblClk(nFlags, point);
}

void CCtrlObjectInspector::ExpandGroup( bool needInverse, bool isExpand )
{
	SCOIPaintElem *pElem = GetVirtualElem( m_nCurVirtualLine );
	if ( !( pElem && pElem->pGroup ) )
		return;
	if ( needInverse )
		pElem->pGroup->isExpand = !pElem->pGroup->isExpand;
	else
		pElem->pGroup->isExpand = isExpand;
	MakePaintList();
	UpdateScrollers();
	Invalidate( FALSE );
}

void CCtrlObjectInspector::SelectRow( int nVirtualLine, bool needHide )
{
	if ( nVirtualLine >= m_aPaintElems.size() )
		return;
  if ( nVirtualLine < 0 )
    return;

  SCOIPaintElem *pOldElem = GetVirtualElem( m_nCurVirtualLine );
  if ( pOldElem && pOldElem->pProp && !pOldElem->pProp->bReadOnly )
  {
    if ( pActiveWnd )
      pActiveWnd->ShowWindow( SW_HIDE );
    switch( pOldElem->pProp->idDomen )
    {
			case DT_DEC:
			case DT_FLOAT:
			case DT_HEX:
			case DT_STR:
				{
          if ( !pActiveWnd )
            break;
					CString szTemp;
					pActiveWnd->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
					pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
					CVariant var = GetPropertyValue( pOldElem->pProp->idProp );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
					}
				}
				break;
      case DT_BROWSE:
			case DT_BROWSEDIR:
        {
          CString szTemp;
          m_pBEdit->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
					((CPropView*)GetParent())->UpdateValue( pOldElem->pProp->idProp );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_COLOR:
        {
          CString szTemp;
          m_pCEdit->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_COMBO:
      case DT_BOOL:
        {
          int nSel = m_pCombo->GetCurSel();
          if ( CB_ERR == nSel )
            break;
          CString szTemp;
          m_pCombo->GetLBText( nSel, szTemp );
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
        
			default:
				if ( pOldElem->pProp->idDomen >= DT_ANIMATION_REF && pOldElem->pProp->idDomen < DT_CUSTOM )
				{
          CString szTemp;
          m_pReference->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
				}
				break;
		}
	}
	SCOIPaintElem *pElem = GetVirtualElem( nVirtualLine );
	if ( !pElem || needHide /*|| VirtualToPaintLine(nVirtualLine) == 0*/ )
		return;
	if ( pElem->pProp )
		SetActiveProp( pElem->pProp->idProp );
	if ( pElem->pProp || pElem->pGroup )
	{
		if ( pElem->pProp && !pElem->pProp->bReadOnly )
		{
			switch( pElem->pProp->idDomen )
			{
				case DT_DEC:
				case DT_FLOAT:
				case DT_HEX:
				case DT_STR:
					{
						m_pEdit->SetWindowText( pElem->pProp->varValue );
						m_pEdit->SetSel( 0, -1 );
						m_pEdit->ShowWindow( SW_SHOW );
						CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
						rect.top--;
						m_pEdit->MoveWindow( rect );
						m_pEdit->SetFocus();
            pActiveWnd = m_pEdit;
					}
					break;
        case DT_BROWSE:
				case DT_BROWSEDIR:
          {
            m_pBEdit->SetWindowText( pElem->pProp->varValue );
						if ( pElem->pProp->szStrs.size() > 0 )
							m_pBEdit->SetSourceDir( pElem->pProp->szStrs[0].c_str() );
						if ( pElem->pProp->szStrs.size() > 1 )
							m_pBEdit->SetBrowseFilter( pElem->pProp->szStrs[1].c_str() );
            m_pBEdit->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pBEdit->MoveWindow( rect );
            m_pBEdit->SetFocus();
						if ( pElem->pProp->idDomen == DT_BROWSE )
							m_pBEdit->SetDirectoryFlag( false );
						else
							m_pBEdit->SetDirectoryFlag( true );

            pActiveWnd = m_pBEdit;
          }
          break;
        case DT_COLOR:
          {
            m_pCEdit->SetWindowText( pElem->pProp->varValue );
            m_pCEdit->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pCEdit->MoveWindow( rect );
            m_pCEdit->SetFocus();
            pActiveWnd = m_pCEdit;
          }
          break;
        case DT_BOOL:
        case DT_COMBO:
          {
            m_pCombo->ResetContent();
            for ( int i = 0; i < pElem->pProp->szStrs.size(); ++i )
            {
              const string &szItem = pElem->pProp->szStrs[i];
              int n = m_pCombo->AddString( szItem.c_str() );
				const char* pszValue = pElem->pProp->varValue.operator const char *();
				if ( szItem == pszValue )
                m_pCombo->SetCurSel( n );
            }
            m_pCombo->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            rect.bottom -= 2;
            m_pCombo->MoveWindow( rect );
            m_pCombo->SetItemHeight( -1, rect.Height() );
            m_pCombo->SetFocus();
            pActiveWnd = m_pCombo;
          }
          break;
        default:
				if ( pElem->pProp->idDomen >= DT_ANIMATION_REF && pElem->pProp->idDomen < DT_CUSTOM )
				{
					m_pReference->SetWindowText( pElem->pProp->varValue );
					m_pReference->SetReferenceType( ConvertFromDomenTypeToRef(pElem->pProp->idDomen) );
					if ( pElem->pProp->idDomen == DT_ACTION_REF )
						m_pReference->SetReferenceValue( pElem->pProp->varValue );
					m_pReference->ShowWindow( SW_SHOW );
					CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
					rect.top--;
					m_pReference->MoveWindow( rect );
					m_pReference->SetFocus();
					pActiveWnd = m_pReference;
				}
				break;
			}
		}
		m_nCurVirtualLine = nVirtualLine;
		Invalidate( FALSE );
	}
}

void CCtrlObjectInspector::ProcessKeyInput( UINT nChar )
{
	switch( nChar )
	{
		case VK_LEFT:
			ExpandGroup( false, false );
			break;

		case VK_RIGHT:
			ExpandGroup();
			break;

		case VK_DOWN:
		case VK_RETURN:
			if ( m_nCurVirtualLine == m_nFirstElem + GetLineCount() - 1 && m_nCurVirtualLine < m_aPaintElems.size() - 1 )
				UpdateScrollers( m_nCurVirtualLine + 1 );
			if ( m_nCurVirtualLine + 1 == m_aPaintElems.size() )
				SelectRow( m_nCurVirtualLine );
			else
				SelectRow( m_nCurVirtualLine + 1 );
			break;

		case VK_UP:
			if ( m_nCurVirtualLine == m_nFirstElem && m_nCurVirtualLine > 0 )
				UpdateScrollers( m_nCurVirtualLine - 1 );
			SelectRow( m_nCurVirtualLine - 1 );
			break;

		case VK_ESCAPE:
/*
			switch( pOldElem->pProp->idDomen )
			{
				case 
					if ( pActiveWnd )
						pActiveWnd->ShowWindow( SW_HIDE );
			}
*/
/*			
			CVariant GetPropertyValue( PropID idProp );
			string   GetPropertyName( PropID idProp );
			bool SetPropertiesValue( PropID idProp, const CVariant &var );
			void SelectProperties( PropID nID );
			PropID HitTest( CPoint ptClient );	// PropIDEmpty if not click in properties
			PropID GetActiveProp( int nGroupID );
*/
/*
			int nActive = GetActiveProp();
			CVariant var = GetPropertyValue( nActive );
			m_pEdit->SetWindowText( var );
			m_pEdit->ShowWindow( SW_HIDE );
			*/
/*			
			if ( m_pEdit == pActiveWnd )
			{
				pActiveWnd->ShowWindow( SW_HIDE );
				pActiveWnd = 0;
			}
*/
			if ( pActiveWnd )
			{
				pActiveWnd->ShowWindow( SW_HIDE );
				pActiveWnd = 0;
				SelectRow( 0 );
			}

			break;
	}
}

void CCtrlObjectInspector::OnLButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
	bDraggingSplitter = ( point.x >= m_nSplitterPos - 3 && point.x <= m_nSplitterPos + 3 && point.y >= 0 && point.y <= m_sizeClient.cy );
	if ( bDraggingSplitter )
	{
		SetCapture();
		SetCursor( LoadCursor(0, IDC_SIZEWE) );
		return;
	}
	int nPaintLine = GetPaintLine(point);
	int nVirtualLine = PaintLineToVirtual(nPaintLine);
	int nCol = GetCol(point);
	if ( nPaintLine ==  0)
		return; // CRAP

	SCOIPaintElem *pElem = GetVirtualElem( nVirtualLine );
	CRect rectPlus = GetPlusRect( nPaintLine );
	SelectRow( nVirtualLine );
  if ( pElem && pElem->pProp && m_mapGroups[pElem->pProp->idGroup].bRadioGroup )
  {
    m_mapGroups[pElem->pProp->idGroup].iActiveProp = pElem->pProp->idProp;
  }
  if ( pElem && pElem->pGroup && !pElem->pProp && rectPlus.PtInRect(point) )
		ExpandGroup( true );
	CWnd::OnLButtonDown(nFlags, point);
}

void CCtrlObjectInspector::OnLButtonUp( UINT nFlags, CPoint point )
{
	if ( bDraggingSplitter && GetCapture() == this )
		ReleaseCapture();
	bDraggingSplitter = false;
	CWnd::OnLButtonUp(nFlags, point);
}

void CCtrlObjectInspector::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( (point.x >= m_nSplitterPos - 3 && point.x <= m_nSplitterPos + 3 &&
		point.y >= 0 && point.y <= m_sizeClient.cy) || bDraggingSplitter )
	{
		SetCursor( LoadCursor(0, IDC_SIZEWE) );
		if ( bDraggingSplitter )
		{
			const int nMinSplitterPos = m_nLineHeight + 20;
			const int nMaxSplitterPos = max( nMinSplitterPos, m_sizeClient.cx - 30 );
			m_nSplitterPos = Clamp( int(point.x), nMinSplitterPos, nMaxSplitterPos );
			if ( pActiveWnd )
			{
				CRect rect;
				pActiveWnd->GetWindowRect( &rect );
				ScreenToClient( &rect );
				rect.left = m_nSplitterPos;
				rect.right = m_sizeClient.cx;
				pActiveWnd->MoveWindow( &rect );
			}
			MakePaintList();
			Invalidate( FALSE );
		}
	}
	else
		SetCursor( LoadCursor(0, IDC_ARROW) );
	
	CWnd::OnMouseMove(nFlags, point);
}
BOOL CCtrlObjectInspector::PreTranslateMessage( MSG* pMsg )
{
	if ( pMsg->message == WM_USER + 1 )
	{
		ProcessKeyInput(pMsg->wParam);
		return true;
	}
	else if ( pMsg->message == WM_USER + 2 )
	{
		LooseFocus();
		return true;
	}
	else if ( pMsg->message == WM_KEYDOWN )
	{
		if ( ( pMsg->wParam == 'C' || pMsg->wParam == 'V' || pMsg->wParam == 'X' || pMsg->wParam == VK_INSERT || pMsg->wParam == VK_DELETE ) && IsCtrlKeyDown() )
		{
			TranslateMessage( pMsg );
			DispatchMessage( pMsg );
			return TRUE;
		}

		if ( pMsg->wParam == VK_INSERT && IsShiftKeyDown() )
		{
			TranslateMessage( pMsg );
			DispatchMessage( pMsg );
			return TRUE;
		}

		switch ( pMsg->wParam )
		{
			case VK_LEFT:
			case VK_RIGHT:
				if ( GetFocus() != this )
					break;
			case VK_DOWN:
			case VK_RETURN:
			case VK_UP:
			case VK_ESCAPE:
				ProcessKeyInput( pMsg->wParam );
				return true;
		}
	}
	return CWnd::PreTranslateMessage( pMsg );
}

void CCtrlObjectInspector::LooseFocus()
{
	SelectRow( m_nCurVirtualLine, true );			//ÝÒÀ ÑÒÐÎ×ÊÀ ÍÓÆÍÀ, ÈÍÀ×Å ÍÅ ÎÁÍÎÂËßÅÒÑß ÑÎÄÅÐÆÈÌÎÅ ÏÐÈ ÏÅÐÅÕÎÄÅ ÍÀ ÍÎÂÛÉ ITEM
	Invalidate( FALSE );
}

void CCtrlObjectInspector::OnKillFocus(CWnd* pNewWnd)
{
	CWnd::OnKillFocus(pNewWnd);
	m_haveFocus = ( pNewWnd == pActiveWnd );
	if ( !m_haveFocus )
		LooseFocus();
}

void CCtrlObjectInspector::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);
	m_haveFocus = true;	
}

void CCtrlObjectInspector::Init()
{
/*	// Calculate line height
	CDC *pDC = GetDC();
	TEXTMETRIC sTextMetrics;
	pDC->GetTextMetrics(&sTextMetrics);
	m_nLineHeight = sTextMetrics.tmHeight;
	if ( !(m_nLineHeight % 2) )
		m_nLineHeight--;
	ReleaseDC(pDC);
*/
	bDraggingSplitter = false;
	m_nLineHeight = 18;
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
	lf.lfHeight = 15;							// request a ?-pixel-height font
	strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
	m_fntDef.CreateFontIndirect(&lf);			// create the fonts
	lf.lfWeight = FW_BOLD;
	m_fntDefBold.CreateFontIndirect(&lf);

	CRect rect, rectClient;
	m_pEdit->Create( WS_CHILD | ES_WANTRETURN | ES_MULTILINE | ES_LEFT | ES_AUTOHSCROLL, rect, this, SUB_CTRL_EDIT );
	m_pEdit->ModifyStyleEx( 0, WS_EX_STATICEDGE );
	m_pEdit->SetFont( &m_fntDef );

  m_pBEdit->Create( 0, "Browse Edit", WS_CHILD, rect, this, SUB_CTRL_BEDIT );
  m_pReference->Create( 0, "Reference", WS_CHILD, rect, this, SUB_CTRL_REFERENCE );
	m_pCEdit->Create( 0, "Color Edit", WS_CHILD, rect, this, SUB_CTRL_CEDIT );
  
  rect.SetRectEmpty();
  rect.right = 70;
  rect.bottom = 120;
  m_pCombo->Create( CBS_DROPDOWNLIST | WS_CHILD | CBS_SORT | WS_VSCROLL | CBS_NOINTEGRALHEIGHT, rect, this, SUB_CTRL_COMBO );
	m_pCombo->SetFont( &m_fntDef );
  m_pCombo->SetItemHeight( 0, m_nLineHeight - 1 );  

	GetClientRect( rectClient );
  m_nSplitterPos = m_nLineHeight;
  CDC *pDC = GetDC();
  if ( pDC )
  {
    CSize sz = pDC->GetTextExtent( "Properties", strlen( "Properties" ) );
    m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight );
    ReleaseDC( pDC );
  }
	m_sizeClient = rectClient.Size();

	GetWindowRect( rect );
	rect.bottom = rect.top + m_nLineHeight * GetLineCount() + rect.Height() - rectClient.Height();
	MoveWindow( rect );
	CBitmap bmp;
	imlIcons.Create( 16,	16,	TRUE, 9, 9 );
	bmp.LoadBitmap( IDB_BITMAP1 );
	imlIcons.Add( &bmp, RGB(255,255,255) );
	bmp.DeleteObject();
}


bool CCtrlObjectInspector::IsValidDomen( DomenID idDomen )
{
	if ( DT_DEC <= idDomen && idDomen < DT_CUSTOM )
		return true;
	return false;
}

void CCtrlObjectInspector::SetGroup( GroupID idGroup, const string strName, bool bRadioGroup )
{
	m_mapGroups[m_nCurGroup = idGroup].strGroupName = strName;
  m_mapGroups[idGroup].bRadioGroup = bRadioGroup;
  m_mapGroups[idGroup].iActiveProp = 0;
  CDC *pDC = GetDC();
  if ( pDC )
  {
		pDC->SelectObject( &m_fntDefBold );
		CSize sz = pDC->GetTextExtent( strName.c_str(), strlen( strName.c_str() ) );
    m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight + 2 );
    ReleaseDC( pDC );
  }
}

bool CCtrlObjectInspector::AddPropertiesValue( PropID idProp, DomenID idDomen, const string strName, 
                      const CVariant &var, GroupID idGroup, bool bReadOnly )
{
	if ( IsValidDomen(idDomen) && m_mapProps.find(idProp) == m_mapProps.end() )
	{
		SCOIProperties prop;
    prop.idProp  = idProp;
		prop.idDomen = idDomen;
		prop.idGroup = idGroup;
		prop.strName = strName;
		prop.varValue = var;
    prop.bReadOnly = bReadOnly;
    if ( DT_BOOL == idDomen )
    {
      prop.szStrs.push_back( STR_TRUE );
      prop.szStrs.push_back( STR_FALSE );
      prop.varValue = ( (bool) var != 0 ) ? STR_TRUE : STR_FALSE;
    }
		m_mapProps[idProp] = prop;
		if ( idGroup == GroupDefault )
			idGroup = m_nCurGroup;
		m_mapGroups[idGroup].aPorops.push_back( &m_mapProps[idProp] );
    if ( m_mapGroups[idGroup].bRadioGroup )
      m_mapGroups[idGroup].iActiveProp = idProp;
    CDC *pDC = GetDC();
    if ( pDC )
    {
			pDC->SelectObject( &m_fntDef );
      CSize sz = pDC->GetTextExtent( strName.c_str(), strlen( strName.c_str() ) );
      m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight + 2 );
      ReleaseDC( pDC );
    }
    MakePaintList();
    Invalidate( FALSE );
		return true;
	}
	return false;
}

void CCtrlObjectInspector::ClearAll()
{
	bDraggingSplitter = false;
	if ( m_mapProps.empty() )
		return;
	SelectRow( m_nCurVirtualLine );
	SelectRow( 0 );
	m_nSplitterPos = 0;
	
  m_mapProps.clear();
  m_mapGroups.clear();
  m_nCurVirtualLine = -1;
	m_nFirstElem = 0;
  if ( pActiveWnd )
    pActiveWnd->ShowWindow( SW_HIDE );
  m_pBEdit->SetDirPrefix( "" );
  MakePaintList();
  Invalidate( FALSE );
	UpdateWindow();
}

bool CCtrlObjectInspector::SetPropertiesValue( PropID idProp, const CVariant &var )
{
	if ( m_mapProps.find(idProp) != m_mapProps.end() )
	{
		m_mapProps[idProp].varValue = var;
		SelectRow( m_nCurVirtualLine );
    Invalidate( FALSE );
		return true;
	}
	return false;
}

void CCtrlObjectInspector::MakePaintList()
{
	m_aPaintElems.clear();
	for( CCOIGpoupMap::iterator itGr = m_mapGroups.begin(); itGr != m_mapGroups.end(); ++itGr )
	{
		SCOIGroup &curGroup = itGr->second;
		if ( curGroup.isVisible )
		{
			SCOIPaintElem elem;
			elem.pGroup = &curGroup;
			m_aPaintElems.push_back(elem);
			if ( curGroup.isExpand )
			{
				for( CCOIPropPtrs::const_iterator itPr = curGroup.aPorops.begin(); itPr != curGroup.aPorops.end(); ++itPr )
				{
					elem.pProp = *itPr;
					m_aPaintElems.push_back(elem);
				}
			}
		}
	}
	UpdateScrollers();
}
void CCtrlObjectInspector::UpdateScrollers( int nFirstVirtualLine )
{
	SCROLLINFO yScroll;
	memset( &yScroll, 0, sizeof(yScroll) );

	yScroll.cbSize = sizeof(yScroll);
	yScroll.fMask = SIF_ALL;
	yScroll.nMax = m_aPaintElems.size() - 1;
	yScroll.nMin = 0;
	yScroll.nPage = GetLineCount();
	if ( nFirstVirtualLine != -1 )
		m_nFirstElem = nFirstVirtualLine;

	if ( m_nFirstElem > m_aPaintElems.size() - GetLineCount() )
		m_nFirstElem = m_aPaintElems.size() - GetLineCount();
	if ( m_nFirstElem < 0 )
		m_nFirstElem = 0;

	yScroll.nPos = yScroll.nTrackPos = m_nFirstElem;
	SetScrollInfo( SB_VERT, &yScroll );
}

BOOL CCtrlObjectInspector::PreCreateWindow(CREATESTRUCT& cs) 
{
	CWnd::PreCreateWindow(cs);
	cs.style |= WS_VSCROLL | WS_EX_STATICEDGE;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), 0, NULL);
	
	return TRUE;
}

BOOL CCtrlObjectInspector::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(pt);

	UINT nScrollLines = 3;
	::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &nScrollLines, 0);
	if ( nScrollLines == WHEEL_PAGESCROLL )
	{
		SCROLLINFO yScroll;
		memset( &yScroll, 0, sizeof(yScroll) );
		yScroll.cbSize = sizeof(yScroll);
		yScroll.fMask = SIF_PAGE;
		GetScrollInfo( SB_VERT, &yScroll );
		nScrollLines = yScroll.nPage > 0 ? yScroll.nPage : 1;
	}
	if ( nScrollLines == 0 )
		return TRUE;

	const int nDirection = zDelta > 0 ? -1 : 1;
	const int nSteps = max( 1, abs( zDelta ) / WHEEL_DELTA );
	UpdateScrollers( m_nFirstElem + nDirection * nSteps * int(nScrollLines) );
	SelectRow( m_nCurVirtualLine );
	Invalidate( FALSE );
	return TRUE;
}

void CCtrlObjectInspector::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	SCROLLINFO	yScroll;
	memset( &yScroll, 0, sizeof SCROLLINFO );
	yScroll.cbSize = sizeof SCROLLINFO;
	yScroll.fMask = SIF_ALL;
	GetScrollInfo( SB_VERT, &yScroll );

	switch( nSBCode )
	{
		case SB_LINELEFT:
			UpdateScrollers( m_nFirstElem - 1 );
			break;
		case SB_LINERIGHT:
			UpdateScrollers( m_nFirstElem + 1 );
			break;

		case SB_PAGELEFT:
			UpdateScrollers( m_nFirstElem - int(yScroll.nPage) );
			break;
		case SB_PAGERIGHT:
			UpdateScrollers( m_nFirstElem + yScroll.nPage );
			break;

		case SB_THUMBTRACK:
			yScroll.nPos = yScroll.nTrackPos;

		case SB_THUMBPOSITION:
		case SB_ENDSCROLL:
			UpdateScrollers( yScroll.nPos );
			break;
	}
	SelectRow( m_nCurVirtualLine );
	Invalidate( FALSE );
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

CVariant CCtrlObjectInspector::GetPropertyValue( PropID idProp )
{
  CCOIPropMap::const_iterator it = m_mapProps.find(idProp);

  if ( it != m_mapProps.end() )
  {
    const SCOIProperties &prop = it->second;
		if ( DT_BOOL == prop.idDomen )
		{
			const char* pszValue = prop.varValue.operator const char *();
			return STR_TRUE == pszValue ? true : false;
		}
    return it->second.varValue;
  }
  return CVariant();
}

string CCtrlObjectInspector::GetPropertyName( PropID idProp )
{
  CCOIPropMap::const_iterator it = m_mapProps.find(idProp);
  
  if ( it != m_mapProps.end() )
  {
    return it->second.strName;
  }
  return "";
}

PropID CCtrlObjectInspector::GetActiveProp( int nGroupID )
{
  CCOIGpoupMap::const_iterator it = m_mapGroups.find( nGroupID );

  if ( it == m_mapGroups.end() || !it->second.bRadioGroup )
    return -1;
  return it->second.iActiveProp;
}

void CCtrlObjectInspector::AddPropertyString( PropID idProp, const string &szStr )
{
  CCOIPropMap::iterator it = m_mapProps.find(idProp);
  
  if ( it != m_mapProps.end() )
    it->second.szStrs.push_back( szStr );
}

void CCtrlObjectInspector::SetActiveProp( PropID nID )
{
	CCOIPropMap::const_iterator it = m_mapProps.find( nID );
	if ( m_mapProps.end() == it )
		return;
	GroupID idGroup = it->second.idGroup;
	CCOIGpoupMap::iterator itg = m_mapGroups.find( idGroup );
	if ( m_mapGroups.end() == itg )
		return;
	itg->second.iActiveProp = nID;
	Invalidate( FALSE );
}

PropID CCtrlObjectInspector::GetMyActiveProp()
{
  SCOIPaintElem *pOldElem = GetVirtualElem( m_nCurVirtualLine );
	if ( pOldElem && pOldElem->pProp )
		return pOldElem->pProp->idProp;
	else
		return -1;		//ERROR
}
