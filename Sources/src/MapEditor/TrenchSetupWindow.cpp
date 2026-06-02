#include "stdafx.h"
#include "editor.h"
#include "TrenchSetupWindow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTrenchSetupWindow::CTrenchSetupWindow( CWnd* pParent )
	: CResizeDialog( CTrenchSetupWindow::IDD, pParent )
{

	SetControlStyle( IDC_E_IMAGE, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_E_FRAME, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
}

void CTrenchSetupWindow::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTrenchSetupWindow, CResizeDialog)
END_MESSAGE_MAP()

BOOL CTrenchSetupWindow::OnInitDialog() 
{
	const int nSizeX = 90;
	const int nSizeY = 89;
	
	CResizeDialog::OnInitDialog();
	
	CRect rect;
	GetClientRect( &rect );
	int nPosX = 0;
	int nPosY = 0;
	if ( CWnd* pWnd = GetDlgItem( IDC_E_IMAGE ) )
	{
		nPosX = ( rect.Width() - nSizeX ) / 2;
		nPosY = ( rect.Height() - nSizeY ) / 2;
		pWnd->MoveWindow( nPosX, nPosY, nSizeX, nSizeY );
	}
	if ( CWnd* pWnd = GetDlgItem( IDC_E_FRAME ) )
	{
		pWnd->MoveWindow( nPosX - 2, nPosY - 2, nSizeX + 5, nSizeY + 5 );
	}
	UpdateControlPositions();
	return TRUE;
}
