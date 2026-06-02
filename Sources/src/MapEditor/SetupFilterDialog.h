#if !defined(__Tabs__Simple_Objects_Set_Filter_Dialog__)
#define __Tabs__Simple_Objects_Set_Filter_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "CreateFilterDialog.h"

#include "Resource.h"
#include "ResizeDialog.h"

class CSetupFilterDialog : public CResizeDialog
{
public:
	static const char SELECTED_UNITS[];
	CSetupFilterDialog( CWnd* pParent = 0 );

	enum { IDD = IDD_TAB_SIMPLE_OBJECTS_SET_FILTER };
	CListBox	m_filtersNames;
	TFilterHashMap m_allFilters;
	CString	m_filterName;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	protected:
	const static int vID[];
	
	virtual int GetMinimumXDimension() { return 150; }
	virtual int GetMinimumYDimension() { return 150; }
	virtual std::string GetXMLOptionsLabel() { return "CSetupFilterDialog"; }
	virtual bool GetDrawGripper() { return true; }

	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeList1();
	virtual void OnOK();
	afx_msg void OnDblclkSoSetFilterList();
	DECLARE_MESSAGE_MAP()

	void UpdateControls();
};
#endif // !defined(__Tabs__Simple_Objects_Set_Filter_Dialog__)
