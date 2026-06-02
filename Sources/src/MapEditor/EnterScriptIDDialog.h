#if !defined(AFX_ENTERSCRIPTIDDIALOG_H__A20FCCF4_2814_43E9_9491_FBB61E8F622E__INCLUDED_)
#define AFX_ENTERSCRIPTIDDIALOG_H__A20FCCF4_2814_43E9_9491_FBB61E8F622E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CEnterScriptIDDialog : public CDialog
{
public:
	CEnterScriptIDDialog(CWnd* pParent = NULL);

	enum { IDD = IDD_TAB_GROUPS_GET_SCRIPT_ID };
	int		m_id;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
protected:
	void UpdateControls();

	afx_msg void OnChangeScriptIdEdit();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(AFX_ENTERSCRIPTIDDIALOG_H__A20FCCF4_2814_43E9_9491_FBB61E8F622E__INCLUDED_)
