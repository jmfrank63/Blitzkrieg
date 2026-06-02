#if !defined(AFX_PICTUREOPTIONS_H__4B3C7B0B_071A_4C25_B8E6_77939D0D5800__INCLUDED_)
#define AFX_PICTUREOPTIONS_H__4B3C7B0B_071A_4C25_B8E6_77939D0D5800__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SingleIcon.h"


class CPictureOptions : public CDialog
{
public:
	CPictureOptions(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_SET_PICTURE_OPTIONS };
	CSingleIcon	m_rightImage;
	CSingleIcon	m_leftImage;
	CSliderCtrl	m_contrastSlider;
	CSliderCtrl	m_gammaSlider;
	CSliderCtrl	m_brightnessSlider;
	CEdit	m_editGamma;
	CEdit	m_editContrast;
	CEdit	m_editBrightness;
	BOOL	m_CurrentProjectCheck;

	float fGamma, fContrast, fBrightness;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:

public:
	void SetBrightness( float fVal ) { fBrightness = fVal; }
	void SetContrast( float fVal ) { fContrast = fVal; }
	void SetGamma( float fVal ) { fGamma = fVal; }

	float GetBrightness() { return fBrightness; }
	float GetContrast() { return fContrast; }
	float GetGamma() { return fGamma; }

	void SetCurrentProjectOnly( bool bFlag ) { m_CurrentProjectCheck = bFlag; }
	bool GetCurrentProjectOnly() { return m_CurrentProjectCheck; }

	afx_msg void OnApply();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditContrast();
	afx_msg void OnChangeEditBrightness();
	afx_msg void OnChangeEditGamma();
	afx_msg void OnReleasedcaptureContrast(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnReleasedcaptureBrightness(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnReleasedcaptureGamma(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_PICTUREOPTIONS_H__4B3C7B0B_071A_4C25_B8E6_77939D0D5800__INCLUDED_)
