
#include "StdAfx.h"

#include "editor.h"
#include "BatchModeDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CBatchModeDialog::CBatchModeDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CBatchModeDialog::IDD, pParent)
{
	m_szDestDir = _T("");
	m_szSourceDir = _T("");
	m_forceModeFlag = FALSE;
	m_szSearchMask = _T("");
	m_openAndSaveFlag = FALSE;
}


void CBatchModeDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DEST_DIR, m_szDestDir);
	DDX_Text(pDX, IDC_EDIT_SOURCE_DIR, m_szSourceDir);
	DDX_Check(pDX, IDC_FORCE_MODE_CHECK, m_forceModeFlag);
	DDX_Text(pDX, IDC_EDIT_SEARCH_MASK, m_szSearchMask);
	DDX_Check(pDX, IDC_OPEN_AND_SAVE, m_openAndSaveFlag);
}


BEGIN_MESSAGE_MAP(CBatchModeDialog, CDialog)
	ON_BN_CLICKED(IDC_BROWSE_DEST_DIR, OnBrowseDestDir)
	ON_BN_CLICKED(IDC_BROWSE_SOURCE_DIR, OnBrowseSourceDir)
END_MESSAGE_MAP()


void CBatchModeDialog::OnBrowseDestDir() 
{
	CFolderPickerDialog dlg( m_szDestDir, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, this, 0 );
	dlg.m_ofn.lpstrTitle = _T( "Select Destination Directory" );
	if ( dlg.DoModal() == IDOK )
	{
		UpdateData( TRUE );
		m_szDestDir = dlg.GetPathName();
		m_szDestDir.MakeLower();
		UpdateData( FALSE );
	}
}

void CBatchModeDialog::OnBrowseSourceDir() 
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
