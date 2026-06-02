
#include "stdafx.h"
#include "InterfaceSystemTest.h"
#include "InterfaceSystemTestDlg.h"



CInterfaceSystemTestDlg::CInterfaceSystemTestDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CInterfaceSystemTestDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CInterfaceSystemTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CInterfaceSystemTestDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


BOOL CInterfaceSystemTestDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	pScreen = new CScreen;
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}


void CInterfaceSystemTestDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		
		lines.clear();
		pScreen->Visit( this );
	
		CPaintDC dc(this); // device context for painting
		CPen pen( PS_SOLID, 2, 0x00ff8888 );
		dc.SelectObject( &pen );
		for ( std::list<SColorLine>::iterator it = lines.begin(); it != lines.end(); ++it )
		{
			dc.MoveTo( it->v.first.x, it->v.first.y );
			dc.LineTo( it->v.second.x, it->v.second.y );
		}
		CDialog::OnPaint();
	}
}

HCURSOR CInterfaceSystemTestDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CInterfaceSystemTestDlg::VisitBoldLine( CVec3 *corners, float fWidth, DWORD color )
{
	lines.push_back( SColorLine( corners, color, fWidth ) );
}

