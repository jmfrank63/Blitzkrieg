
#include "stdafx.h"
#include "InterfaceSystemTest.h"
#include "InterfaceSystemTestDlg.h"



BEGIN_MESSAGE_MAP(CInterfaceSystemTestApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CInterfaceSystemTestApp::CInterfaceSystemTestApp()
{
}


CInterfaceSystemTestApp theApp;


BOOL CInterfaceSystemTestApp::InitInstance()
{
	AfxEnableControlContainer();


	CInterfaceSystemTestDlg dlg;
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
