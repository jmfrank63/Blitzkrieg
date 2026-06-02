#if !defined(AFX_AREANAMEDIALOG_H__8AF4FFB0_25B9_4FC4_A933_8CE87B12D6B5__INCLUDED_)
#define AFX_AREANAMEDIALOG_H__8AF4FFB0_25B9_4FC4_A933_8CE87B12D6B5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CAreaNameDialog : public CDialog
{
public:
	CAreaNameDialog(CWnd* pParent = NULL);
	enum { IDD = IDD_TAB_TOOLS_AREAD_NAME };
	CString	m_name;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	void UpdateControls();

	afx_msg void OnChangeAreaNameEdit();
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(AFX_AREANAMEDIALOG_H__8AF4FFB0_25B9_4FC4_A933_8CE87B12D6B5__INCLUDED_)
