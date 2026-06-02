
#include "stdafx.h"
#include "editor.h"
#include "frames.h"
#include "BrowseDialog.h"
#include "MyOpenFileDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CBrowseDialog::CBrowseDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CBrowseDialog::IDD, pParent)
{
	m_szFileName = _T("");
}


void CBrowseDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_FILE, m_szFileName);
}


BEGIN_MESSAGE_MAP(CBrowseDialog, CDialog)
	ON_BN_CLICKED(ID_BROWSE, OnBrowse)
END_MESSAGE_MAP()


void CBrowseDialog::OnBrowse() 
{
	std::string szRes;
	if ( !ShowFileDialog( szRes, GetDirectory(m_szFileName).c_str(), m_szTitle, FALSE, m_szExtension, m_szFileName, m_szFilter ) )
		return;

	UpdateData( TRUE );
	m_szFileName = szRes.c_str();
	UpdateData( FALSE );
}
