#if !defined(AFX_NEWDIRDIALOG_H__D8FA628C_1C81_46CA_8150_377DFFEAC767__INCLUDED_)
#define AFX_NEWDIRDIALOG_H__D8FA628C_1C81_46CA_8150_377DFFEAC767__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CNewDirDialog : public CDialog
{
public:
	CNewDirDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_NEW_DIR_DIALOG };
	CString	m_name;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:

	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_NEWDIRDIALOG_H__D8FA628C_1C81_46CA_8150_377DFFEAC767__INCLUDED_)
