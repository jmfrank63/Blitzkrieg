
#include "stdafx.h"
#include "editor.h"
#include "ProgressDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CProgressDialog::CProgressDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CProgressDialog::IDD, pParent), m_nSize( 0 )
{
	m_projectName = _T("");
}


void CProgressDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BATCH_MODE_PROGRESS, m_batchProgress);
	DDX_Text(pDX, IDC_PROGRESS_DIALOG_STATIC, m_projectName);
}


BEGIN_MESSAGE_MAP(CProgressDialog, CDialog)
END_MESSAGE_MAP()



BOOL CProgressDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_batchProgress.SetRange( 0, m_nSize );
	m_batchProgress.SetPos( 0 );
	m_batchProgress.SetStep( 1 );
	
	return TRUE;  // return TRUE unless you set the focus to a control
}
