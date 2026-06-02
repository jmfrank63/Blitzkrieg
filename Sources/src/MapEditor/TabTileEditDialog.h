#if !defined(__Tabs__Tile_Edit_Dialog__)
#define __Tabs__Tile_Edit_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"

class CTabTileEditDialog : public CResizeDialog
{
public:
	CTabTileEditDialog( CWnd* pParent = NULL );

	CImageList tilesImageList;

	enum { IDD = IDD_TAB_TILE_EDIT };
	CListCtrl	m_TilesList;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);


protected:
	const static int vID[];
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnRclickilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTilePropertiesMenu();
	DECLARE_MESSAGE_MAP()

public:
	
	std::unordered_map<int, int> seasonTilesIndices;
	void CreateTilesList( const std::string &rszSeasonFolder, int nSelectedTileIndex );

	void DeleteImageList();
	void CreateImageList();
	void ShowTileProperties();
};
#endif // !defined(__Tabs__Tile_Edit_Dialog__)
