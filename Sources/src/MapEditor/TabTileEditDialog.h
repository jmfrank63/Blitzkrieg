#if !defined(__Tabs__Tile_Edit_Dialog__)
#define __Tabs__Tile_Edit_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTabTileEditDialog : public CResizeDialog
{
public:
	CTabTileEditDialog( CWnd* pParent = NULL );

	CImageList tilesImageList;

	//{{AFX_DATA(CTabTileEditDialog)
	enum { IDD = IDD_TAB_TILE_EDIT };
	CListCtrl	m_TilesList;
	//}}AFX_DATA


	//{{AFX_VIRTUAL(CTabTileEditDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL


protected:
	const static int vID[];
	//{{AFX_MSG(CTabTileEditDialog)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnRclickilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTilePropertiesMenu();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	
	//std::unordered_map<MAKELPARAM(����� ������, �����_�����), �����_�������� � �����>
	std::unordered_map<int, int> seasonTilesIndices;
	void CreateTilesList( const std::string &rszSeasonFolder, int nSelectedTileIndex );

	//MODs support
	void DeleteImageList();
	void CreateImageList();
	void ShowTileProperties();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__Tabs__Tile_Edit_Dialog__)
