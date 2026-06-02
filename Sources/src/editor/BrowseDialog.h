#if !defined(AFX_BROWSEDIALOG_H__840A0428_5CE0_474A_A7D0_E4D42738147D__INCLUDED_)
#define AFX_BROWSEDIALOG_H__840A0428_5CE0_474A_A7D0_E4D42738147D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CBrowseDialog : public CDialog
{
public:
	CBrowseDialog(CWnd* pParent = NULL);   // standard constructor

	void SetFileName( const char *pszName ) { m_szFileName = pszName; }
	const char *GetFileName() { return (const char *) m_szFileName; }
	void SetExtension( const char *pszExt ) { m_szExtension = pszExt; }
	void SetFilter( const char *pszFilter ) { m_szFilter = pszFilter; }
	void SetTitle( const char *pszTitle ) { m_szTitle = pszTitle; }
	
	enum { IDD = IDD_BROWSE_DIALOG };
	CString	m_szFileName;

	CString	m_szExtension;
	CString	m_szFilter;
	CString	m_szTitle;
	
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:

	afx_msg void OnBrowse();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_BROWSEDIALOG_H__840A0428_5CE0_474A_A7D0_E4D42738147D__INCLUDED_)
