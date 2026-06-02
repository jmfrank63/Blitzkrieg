#if !defined(AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_)
#define AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class COICombo : public CComboBox
{
public:
	COICombo();

public:

public:


public:
	virtual ~COICombo();

protected:
  CFont	m_fntDef;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKillFocus(CWnd* pNewWnd);

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_)
