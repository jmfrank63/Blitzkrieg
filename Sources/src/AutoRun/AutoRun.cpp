#include "stdafx.h"
#include "AutoRun.h"
#include "AutoRunDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CAutoRunApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

CAutoRunApp::CAutoRunApp()
{
}

CAutoRunApp theApp;

BOOL CAutoRunApp::InitInstance()
{
	::SetErrorMode( SEM_FAILCRITICALERRORS );
	#ifdef _AFXDLL
		Enable3dControls();
	#else
		Enable3dControlsStatic();
	#endif

	m_pMainWnd = 0;
	CAutoRunDialog dlg;
	if ( dlg.Load() )
	{
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}
	return FALSE;
}
