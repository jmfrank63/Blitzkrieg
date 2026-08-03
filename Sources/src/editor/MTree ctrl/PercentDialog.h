#if !defined(AFX_PERCENTDIALOG_H__3809B33A_7D0D_4885_A6E4_D06D7CB356FC__INCLUDED_)
#define AFX_PERCENTDIALOG_H__3809B33A_7D0D_4885_A6E4_D06D7CB356FC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "..//Resource.h"

class CPercentDialog : public CDialog
{
public:
	CPercentDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_DIALOG1 };
	CSliderCtrl	m_slider;
	int		m_value;
	int		m_variable;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:

	virtual void OnOK();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnReleasedcaptureSlider1(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_PERCENTDIALOG_H__3809B33A_7D0D_4885_A6E4_D06D7CB356FC__INCLUDED_)
