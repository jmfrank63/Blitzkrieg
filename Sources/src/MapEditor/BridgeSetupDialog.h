#if !defined(__Tabs__VO_Bridges_Dialog__)
#define __Tabs__VO_Bridges_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ResizeDialog.h"
class CBridgeSetupDialog : public CResizeDialog
{
public:
	CBridgeSetupDialog( CWnd* pParent = NULL );
	std::string GetBridgeName();

	//{{AFX_DATA(CBridgeSetupDialog)
	enum { IDD = IDD_BRIDGESETUP };
	CListCtrl	bridgesList;
	//}}AFX_DATA


	//{{AFX_VIRTUAL(CBridgeSetupDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

public:
	//MODs support
	void DeleteImageList();
	void CreateImageList();

protected:
	CImageList bridgesImageList;
	std::unordered_map<int, std::string> bridgesMap;

	void CreateBridgesList();

	//{{AFX_MSG(CBridgeSetupDialog)
	afx_msg void OnSize( UINT nType, int cx, int cy );
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
#endif // !defined(__Tabs__VO_Bridges_Dialog__)
