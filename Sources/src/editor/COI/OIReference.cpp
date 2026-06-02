#include "StdAfx.h"
#include "OIReference.h"
#include "..\RefDlg.h"
#include "..\MultySelDialog.h"

BEGIN_MESSAGE_MAP(COIReferenceButton, CButton)
END_MESSAGE_MAP()

static std::string MakeOneSlash( std::string str )
{
  for ( std::string::iterator it = str.begin(); it != str.end(); ++it )
    if ( *it == '\\' )
    {
      if ( ++it == str.end() )
        return str;
      if ( *it == '\\' )
      {
        str.erase( it );
        return MakeOneSlash( str );
      }
    }
    return str;
}

COIReferenceButton::COIReferenceButton( COIReferenceEdit *pPrnt, CEdit* pEdtBrowse )
{
  ASSERT( pPrnt );
  ASSERT( pEdtBrowse );
  
  m_pEdtBrowse = pEdtBrowse;
  m_pParentWnd = pPrnt;
  m_uiID = 0;
} 

COIReferenceButton::~COIReferenceButton()
{
} 

BOOL COIReferenceButton::Create()
{
  ASSERT(m_pEdtBrowse != NULL);
  
  CRect rc;

  m_pEdtBrowse->GetWindowRect(&rc);
  m_pEdtBrowse->SetWindowPos(NULL, 0, 0, rc.Width() - (BTN_WIDTH + 1),
    rc.Height(), SWP_NOZORDER | SWP_NOMOVE);
  
  
  m_pParentWnd->ScreenToClient(&rc);
  rc.left = rc.right - BTN_WIDTH;

  const UINT MAX_DLGID = 32767;
  const UINT MIN_DLGID = 1;
  for (m_uiID = MAX_DLGID; m_uiID != MIN_DLGID; m_uiID--)
    if (::GetDlgItem(m_pParentWnd->GetSafeHwnd(), m_uiID) == NULL)
      break;
    ASSERT(m_uiID != MIN_DLGID);
    
  return CButton::Create(_T("..."), WS_VISIBLE | WS_CHILD, rc, m_pParentWnd, m_uiID);  
}

BOOL COIReferenceButton::OnChildNotify( UINT uiMsg, WPARAM wParam, LPARAM lParam,
                                    LRESULT* pLResult )
{
  if ((WM_COMMAND == uiMsg) && (m_uiID == LOWORD(wParam)) && (BN_CLICKED == HIWORD(wParam)))
  {
    m_pParentWnd->OnBrowse();
    return TRUE;
  }
  
  return FALSE;
}

BEGIN_MESSAGE_MAP(COIReferenceEdit, CWnd)
  ON_WM_ENABLE()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

COIReferenceEdit::COIReferenceEdit() : m_BrowseBtn( this, &m_Edit )
{
	nRefType = 0;
	nValue = 0;
}

COIReferenceEdit::~COIReferenceEdit()
{
}

int COIReferenceEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  LOGFONT lf;
  memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
  lf.lfHeight = 15;							// request a ?-pixel-height font
  strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
  m_fntDef.CreateFontIndirect(&lf);			// create the fonts
  lf.lfWeight = FW_BOLD;

  CRect rect;
  m_Edit.Create( WS_CHILD | ES_LEFT | ES_AUTOHSCROLL, rect, this, 1 );
  m_Edit.ModifyStyleEx( 0, WS_EX_STATICEDGE );
  m_Edit.SetFont( &m_fntDef );

  m_BrowseBtn.Create();
  m_BrowseBtn.SetFont( &m_fntDef );
  return 0;
}

void COIReferenceEdit::OnEnable(BOOL bEnable)
{
  m_Edit.EnableWindow(bEnable);
	m_BrowseBtn.EnableWindow(bEnable);
	ASSERT(m_BrowseBtn.IsWindowEnabled() == bEnable);
}

void COIReferenceEdit::OnShowWindow( BOOL bShow, UINT nState )
{
	if ( bShow )
  {
    m_Edit.ShowWindow( SW_SHOW );
		m_BrowseBtn.ShowWindow( SW_SHOW );
  }
	else
  {
    m_Edit.ShowWindow( SW_HIDE );    
		m_BrowseBtn.ShowWindow( SW_HIDE );
  }
  CWnd::OnShowWindow( bShow, nState );
}

void COIReferenceEdit::OnBrowse()
{
	if ( nRefType == E_ACTIONS_REF )
	{
		CMultySelDialog msDlg;
		msDlg.Init( nRefType, nValue );
		if ( msDlg.DoModal() == IDOK )
		{
			string str = msDlg.GetValue();
			SetWindowText( str.c_str() );
			GetParent()->PostMessage( WM_USER_LOST_FOCUS );
		}
	}
	else
	{
		CReferenceDialog refDlg;
		refDlg.Init( nRefType );
		if ( refDlg.DoModal() == IDOK )
		{
			string str = refDlg.GetValue();
			SetWindowText( str.c_str() );
			GetParent()->PostMessage( WM_USER_LOST_FOCUS );
		}
	}
}

void COIReferenceEdit::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);	
  m_Edit.MoveWindow( 0, 0, cx, cy );

  CRect rc;
  m_Edit.GetWindowRect(&rc);
  m_Edit.SetWindowPos(NULL, 0, 0, rc.Width() - (BTN_WIDTH + 1),
    rc.Height(), SWP_NOZORDER | SWP_NOMOVE);
  
  ScreenToClient(&rc);
  rc.left = rc.right - BTN_WIDTH;
  
  m_BrowseBtn.MoveWindow( &rc );
  m_BrowseBtn.EnableWindow( IsWindowEnabled() );
}

void COIReferenceEdit::SetWindowText( LPCTSTR lpszString )
{
  m_Edit.SetWindowText( lpszString );
  CWnd::SetWindowText( lpszString );
}

void COIReferenceEdit::GetWindowText( CString &rString ) const
{
  m_Edit.GetWindowText( rString );
}

BOOL COIReferenceEdit::PreTranslateMessage( MSG* pMsg )
{
  if ( pMsg->message > WM_USER )
  {
    if ( WM_USER + 2 == pMsg->message && pMsg->wParam == (WPARAM)m_BrowseBtn.m_hWnd )
      return true;
    GetParent()->PostMessage( pMsg->message, pMsg->wParam, pMsg->lParam );
    return true;
  }
  return CWnd::PreTranslateMessage( pMsg );
}
