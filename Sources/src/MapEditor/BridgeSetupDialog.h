#if !defined(__Tabs__VO_Bridges_Dialog__)
#define __Tabs__VO_Bridges_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CBridgeSetupDialog : public CResizeDialog
{
public:
	CBridgeSetupDialog( CWnd* pParent = NULL );
	std::string GetBridgeName();

	enum { IDD = IDD_BRIDGESETUP };
	CListCtrl	bridgesList;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

public:
	void DeleteImageList();
	void CreateImageList();

protected:
	CImageList bridgesImageList;
	std::unordered_map<int, std::string> bridgesMap;

	void CreateBridgesList();

	afx_msg void OnSize( UINT nType, int cx, int cy );
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(__Tabs__VO_Bridges_Dialog__)
