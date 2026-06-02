
#if !defined(AFX_EDITOR_H__EF9ACF1F_0933_498C_B092_A3DED0F07F99__INCLUDED_)
#define AFX_EDITOR_H__EF9ACF1F_0933_498C_B092_A3DED0F07F99__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

using std::string;

#include "resource.h"       // main symbols
class CSimpleWindow;
class CMainFrame;


class CEditorApp : public CWinApp
{
public:
	CEditorApp();
	~CEditorApp() {}

	void SaveRegisterData();

	void ShowSECControlBar( SECControlBar *pControlBar, int nCommand );
	
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	virtual BOOL SaveAllModified();

	CMainFrame *GetMainFrame() { return m_pMainFrame; }
/*	string GetEditorDir() const { return szEditorDir; }
	string GetEditorTempResourceDir() const;
	string GetEditorTempDir() const;
	string GetEditorDataDir() const;*/


protected:
	HMENU m_hMDIMenu;
	HACCEL hMDIAccel;
	CMainFrame *m_pMainFrame;

public:
	afx_msg void OnAppAbout();
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};

extern CEditorApp theApp;



#endif // !defined(AFX_EDITOR_H__EF9ACF1F_0933_498C_B092_A3DED0F07F99__INCLUDED_)
