#if !defined(__Tabs__VO_Fences_Dialog__)
#define __Tabs__VO_Fences_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFenceSetupWindow : public CResizeDialog
{
public:
	CFenceSetupWindow( CWnd* pParent = NULL );
	std::string GetFenceName();

	//{{AFX_DATA(CFenceSetupWindow)
	enum { IDD = IDD_TAB_VO_FENCES };
	CListCtrl	fencesList;
	//}}AFX_DATA


	//{{AFX_VIRTUAL(CFenceSetupWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

public:
	//MODs support
	void DeleteImageList();
	void CreateImageList();

protected:
	CImageList fencesImageList;
	std::unordered_map<int, std::string> fencesMap;

	void CreateFencesList();

	//{{AFX_MSG(CFenceSetupWindow)
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnDestroy();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__Tabs__VO_Fences_Dialog__)
