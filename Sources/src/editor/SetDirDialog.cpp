
#include "StdAfx.h"

#include "editor.h"
#include "SetDirDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CSetDirDialog::CSetDirDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSetDirDialog::IDD, pParent)
{
	m_szSourceDir = _T("");
	m_szExecDir = _T("");
	m_szExecArgs = _T("");
}


void CSetDirDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SOURCE_DIR, m_szSourceDir);
	DDX_Text(pDX, IDC_EDIT_EXEC_DIR, m_szExecDir);
	DDX_Text(pDX, IDC_EDIT_COMMAND, m_szExecArgs);
}


BEGIN_MESSAGE_MAP(CSetDirDialog, CDialog)
	ON_BN_CLICKED(IDC_BROWSE_SOURCE_DIR, OnBrowseSourceDir)
	ON_BN_CLICKED(IDC_BROWSE_EXEC_DIR, OnBrowseExecDir)
END_MESSAGE_MAP()


void CSetDirDialog::OnBrowseSourceDir() 
{
	CFolderPickerDialog dlg( m_szSourceDir, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, this, 0 );
	dlg.m_ofn.lpstrTitle = _T( "Select Source Directory" );
	if ( dlg.DoModal() == IDOK )
	{
		UpdateData( TRUE );
		m_szSourceDir = dlg.GetPathName();
		m_szSourceDir.MakeLower();
		UpdateData( FALSE );
	}
}
void CSetDirDialog::OnBrowseExecDir() 
{
	CFolderPickerDialog dlg( m_szExecDir, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, this, 0 );
	dlg.m_ofn.lpstrTitle = _T( "Select Executable Directory" );
	if ( dlg.DoModal() == IDOK )
	{
		UpdateData( TRUE );
		m_szExecDir = dlg.GetPathName();
		m_szExecDir.MakeLower();
		UpdateData( FALSE );
	}
}
