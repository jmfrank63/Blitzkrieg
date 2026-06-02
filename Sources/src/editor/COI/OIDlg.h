#if !defined(AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_)
#define AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "CtrlObjectInspector.h"


class COIDlg : public CDialog
{
public:
	COIDlg(CWnd* pParent = NULL);   // standard constructor

  CCtrlObjectInspector m_wndOI;
  
	enum { IDD = IDD_OBJINSPECTOR };


	protected:
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
  
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_)
