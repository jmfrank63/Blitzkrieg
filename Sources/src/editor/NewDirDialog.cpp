
#include "stdafx.h"
#include "editor.h"
#include "NewDirDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CNewDirDialog::CNewDirDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CNewDirDialog::IDD, pParent)
{
	m_name = _T("MyMOD");
}


void CNewDirDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_NAME_EDIT, m_name);
}


BEGIN_MESSAGE_MAP(CNewDirDialog, CDialog)
END_MESSAGE_MAP()


void CNewDirDialog::OnOK() 
{
	UpdateData();
	if ( m_name.Find( "\\" ) != -1 || m_name.Find( "/" ) != -1 )
	{
		MessageBox( "Directory name shouldn't contain slash characters!", "Error" );
		return;
	}
	CDialog::OnOK();
}
