
#include "StdAfx.h"
#include "editor.h"
#include "AreaNameDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CAreaNameDialog::CAreaNameDialog( CWnd* pParent )
	: CDialog( CAreaNameDialog::IDD, pParent )
{
	m_name = _T("");
}

void CAreaNameDialog::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_AREA_NAME_EDIT, m_name);
}

BEGIN_MESSAGE_MAP(CAreaNameDialog, CDialog)
	ON_EN_CHANGE(IDC_AREA_NAME_EDIT, OnChangeAreaNameEdit)
END_MESSAGE_MAP()

void CAreaNameDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDC_AREA_NAME_EDIT ) )
	{
		CString strText;
		pWnd->GetWindowText( strText );
		if ( pWnd = GetDlgItem( IDOK ) )
		{
			pWnd->EnableWindow( !strText.IsEmpty() );
		}
	}
}

void CAreaNameDialog::OnChangeAreaNameEdit() 
{
	UpdateControls();
}
