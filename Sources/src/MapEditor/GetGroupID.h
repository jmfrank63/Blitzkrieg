#if !defined(AFX_GETGROUPID_H__F1E869B2_25A8_40CB_BB77_CFE65772EA33__INCLUDED_)
#define AFX_GETGROUPID_H__F1E869B2_25A8_40CB_BB77_CFE65772EA33__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CGetGroupID : public CDialog
{
public:
	CGetGroupID(CWnd* pParent = NULL);

	enum { IDD = IDD_TAB_GROUPS_GET_GROUP_ID };
	UINT	m_id;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	void UpdateControls();

	afx_msg void OnChangeGroupIdEdit();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(AFX_GETGROUPID_H__F1E869B2_25A8_40CB_BB77_CFE65772EA33__INCLUDED_)
