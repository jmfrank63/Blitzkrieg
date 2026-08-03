#include "StdAfx.h"
#include "editor.h"
#include "EnterScriptIDDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CEnterScriptIDDialog::CEnterScriptIDDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CEnterScriptIDDialog::IDD, pParent)
{
	m_id = 0;
}


void CEnterScriptIDDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_SCRIPT_ID_EDIT, m_id);
}


BEGIN_MESSAGE_MAP(CEnterScriptIDDialog, CDialog)
	ON_EN_CHANGE(IDC_SCRIPT_ID_EDIT, OnChangeScriptIdEdit)
END_MESSAGE_MAP()

void CEnterScriptIDDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDC_SCRIPT_ID_EDIT ) )
	{
		CString strText;
		pWnd->GetWindowText( strText );
		if ( pWnd = GetDlgItem( IDOK ) )
		{
			pWnd->EnableWindow( !strText.IsEmpty() );
		}
	}
}

void CEnterScriptIDDialog::OnChangeScriptIdEdit() 
{
}

BOOL CEnterScriptIDDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	UpdateControls();
	return TRUE;
}
