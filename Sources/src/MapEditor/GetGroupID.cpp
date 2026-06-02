#include "stdafx.h"
#include "editor.h"
#include "GetGroupID.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CGetGroupID::CGetGroupID(CWnd* pParent /*=NULL*/)
	: CDialog(CGetGroupID::IDD, pParent)
{
	m_id = 0;
}


void CGetGroupID::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_GROUP_ID_EDIT, m_id);
	DDV_MinMaxUInt(pDX, m_id, 0, 32000);
}


BEGIN_MESSAGE_MAP(CGetGroupID, CDialog)
	ON_EN_CHANGE(IDC_GROUP_ID_EDIT, OnChangeGroupIdEdit)
END_MESSAGE_MAP()

void CGetGroupID::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUP_ID_EDIT ) )
	{
		CString strText;
		pWnd->GetWindowText( strText );
		if ( pWnd = GetDlgItem( IDOK ) )
		{
			pWnd->EnableWindow( !strText.IsEmpty() );
		}
	}
}

void CGetGroupID::OnChangeGroupIdEdit() 
{
	UpdateControls();
}

BOOL CGetGroupID::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	UpdateControls();
	return TRUE;
}
