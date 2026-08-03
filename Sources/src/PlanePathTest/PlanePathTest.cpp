
#include "StdAfx.h"
#include "PlanePathTest.h"
#include "PlanePathTestDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CPlanePathTestApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CPlanePathTestApp::CPlanePathTestApp()
{
}


CPlanePathTestApp theApp;


BOOL CPlanePathTestApp::InitInstance()
{
	AfxEnableControlContainer();


	CPlanePathTestDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}

	return FALSE;
}
