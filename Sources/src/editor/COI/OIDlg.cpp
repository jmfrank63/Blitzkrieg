
#include "stdafx.h"
#include "mapedit.h"
#include "OIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



COIDlg::COIDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COIDlg::IDD, pParent)
{
}


void COIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(COIDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()


BOOL COIDlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  
  RECT rect;

  GetClientRect( &rect );
  m_wndOI.Create( 0, "ObjectInspector", WS_CHILD | WS_VISIBLE, rect, this, ID_OI );
  m_wndOI.SetWindowPos( 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
  
  return TRUE;  // return TRUE  unless you set the focus to a control
}

void COIDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	
}

void COIDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
  RECT r;
  GetClientRect( &r );
  if ( ::IsWindow( m_wndOI.m_hWnd ) )
    m_wndOI.SetWindowPos( 0, 0, 0, r.right, r.bottom, SWP_NOZORDER );
}

void COIDlg::OnCancel()
{
  return;
}
