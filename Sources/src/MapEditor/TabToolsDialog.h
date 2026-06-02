#if !defined(__Tabs__Tools_Dialog__)
#define __Tabs__Tools_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CTabToolsDialog : public CResizeDialog
{
public:
	CTabToolsDialog( CWnd* pParent = NULL );
	void UpdateControls();
	enum { IDD = IDD_TAB_TOOLS };
	CListBox	m_areas;
	CString m_hp; 
	int		m_mode;
	int		m_drawType;

 
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	const static int vID[];
	virtual std::string GetXMLOptionsLabel() { return "CTabToolsDialog"; }
	bool bCreateControls;

	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEdit1();
	afx_msg void OnRadioChanged0();
	afx_msg void OnRadioChanged1();
	afx_msg void OnRadio1Changed0();
	afx_msg void OnRadio1Changed1();
	afx_msg void OnButtonDelArea();
	afx_msg void OnSelchangeToolsSaList();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(__Tabs__Tools_Dialog__)
