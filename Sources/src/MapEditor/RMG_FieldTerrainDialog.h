#if !defined(__RMG_Field_Terrain_Dialog__)
#define __RMG_Field_Terrain_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

int CALLBACK CFT_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CRMGFieldTerrainDialog : public CResizeDialog
{
	friend int CALLBACK CFT_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend class CRMGCreateFieldDialog;

public:
	static const int DEFAULT_TILE_WEIGHT;
	static const float DEFAULT_SHELL_WIDTH;
	static const char UNKNOWN_TILE[];
	static const char MULTIPLE_SELECTION[];

	CRMGFieldTerrainDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CF_TERRAIN_SHELLS };
	CComboBox	m_SeasonComboBox;
	CListCtrl	m_ShellsList;
	CListCtrl	m_TilesList;
	CListCtrl	m_AvailableTilesList;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnSelchangeSeasonCombo();
	afx_msg void OnItemchangedShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTileProperties();
	afx_msg void OnRemoveTile();
	afx_msg void OnAddTile();
	afx_msg void OnAddShell();
	afx_msg void OnRemoveShell();
	afx_msg void OnShellProperties();
	afx_msg void OnAddShellMenu();
	afx_msg void OnRemoveShellMenu();
	afx_msg void OnShellPropertiesMenu();
	afx_msg void OnDblclkShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddTileMenu();
	afx_msg void OnRemoveTileMenu();
	afx_msg void OnTilePropertiesMenu();
	afx_msg void OnDblclkTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAvailableTilePropertiesMenu();
	afx_msg void OnDblclkAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

	void OnAvailableTileProperties();

protected:
	bool bCreateControls;
	class CRMGCreateFieldDialog *pRMGCreateFieldDialog;
	struct SRMFieldSet *pRMFieldSet;						//главный
	std::vector<struct SRMFieldSet*> fieldSets;	//добавочные ( для изменения сезона и других вещей ( кроме шеллзов ) )
	
	int nSortColumn;
	int nCurrentShell;
	std::vector<bool> bShellsSortParam;
	int nSelectedSeason;
	int nOldSelectedSeason;
	STilesetDesc tilesetDesc;

	virtual std::string GetXMLOptionsLabel() { return "CRMGFieldTerrainDialog"; }

	void CreateControls();
	void UpdateControls();
	void FillAvailableTiles( int nSeason );
	void FillShellTilesList( int nSeason, int nSelectedShell );
	void LoadFieldToControls();
	void SetShellItem( int nItem, const SRMTileSetShell &rTileSetShell );
};
#endif // !defined(__RMG_Field_Terrain_Dialog__)
