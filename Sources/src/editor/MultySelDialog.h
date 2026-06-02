#if !defined(AFX_MULTYSELDIALOG_H__72F92FDC_23D0_4F60_804D_5E21317C1884__INCLUDED_)
#define AFX_MULTYSELDIALOG_H__72F92FDC_23D0_4F60_804D_5E21317C1884__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RefDlg.h"


class CMultySelDialog : public CDialog
{
public:
	CMultySelDialog(CWnd* pParent = NULL);   // standard constructor
	
	enum { IDD = IDD_MULTY_SEL_DIALOG };
	CCheckListBox	m_multyList;
	
	
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	
protected:
	EReferenceType nReferenceType;
	std::string szResult;
	int64 nValue;
	
	void LoadActions();
	
public:
	void Init( int nRefId, int64 nVal );
	std::string GetValue();

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_MULTYSELDIALOG_H__72F92FDC_23D0_4F60_804D_5E21317C1884__INCLUDED_)
