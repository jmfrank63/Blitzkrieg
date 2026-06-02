#if !defined(AFX_BATCHMODEDIALOG_H__8A884632_0001_4426_9D69_70A7A4E2D825__INCLUDED_)
#define AFX_BATCHMODEDIALOG_H__8A884632_0001_4426_9D69_70A7A4E2D825__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CBatchModeDialog : public CDialog
{
public:
	CBatchModeDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_RUN_BATCH_MODE_DIALOG };
	CString	m_szDestDir;
	CString	m_szSourceDir;
	BOOL	m_forceModeFlag;
	CString	m_szSearchMask;
	BOOL	m_openAndSaveFlag;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

public:
	void SetSourceDir( const char *pszDir ) { m_szSourceDir = pszDir; }
	void SetDestDir( const char *pszDir ) { m_szDestDir = pszDir; }
	void SetSearchMask( const char *pszMask ) { m_szSearchMask = pszMask; }
	void SetForceModeFlag( bool bFlag ) { m_forceModeFlag = bFlag; }
	const char *GetSourceDir() { return m_szSourceDir; }
	const char *GetDestDir() { return m_szDestDir; }
	const char *GetSearchMask() { return m_szSearchMask; }
	bool GetForceModeFlag() { return m_forceModeFlag; }
	bool GetOpenAndSaveFlag() { return m_openAndSaveFlag; }

protected:
	afx_msg void OnBrowseDestDir();
	afx_msg void OnBrowseSourceDir();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_BATCHMODEDIALOG_H__8A884632_0001_4426_9D69_70A7A4E2D825__INCLUDED_)
