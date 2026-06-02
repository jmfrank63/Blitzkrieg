#if !defined(AFX_MODDIALOG_H__A108E742_4E72_46D3_B97F_F6C20D43E0DA__INCLUDED_)
#define AFX_MODDIALOG_H__A108E742_4E72_46D3_B97F_F6C20D43E0DA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CMODDialog : public CDialog
{
public:
	CMODDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_MOD_SETTINGS_DIALOG };
	CString	mExportDir;
	CString	mName;
	CString	mVersion;
	CString	mDesc;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:

	afx_msg void OnModExportBtn();
	afx_msg void OnModDefaultsBtn();
	afx_msg void OnButtonNewMod();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_MODDIALOG_H__A108E742_4E72_46D3_B97F_F6C20D43E0DA__INCLUDED_)
