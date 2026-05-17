#if !defined(__CREATE_FILTER_DIALOG__)
#define __CREATE_FILTER_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
#include "..\RandomMapGen\Resource_Types.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::list<std::string> TSimpleFilterItem;
typedef std::list<TSimpleFilterItem> TSimpleFilter;
struct SSimpleFilter
{
	TSimpleFilter filter;
	bool Check( const std::string &rszFolder ) const;

	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};
typedef std::unordered_map<std::string, SSimpleFilter> TFilterHashMap;

int CALLBACK FiltersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCreateFilterDialog : public CResizeDialog
{
	friend int CALLBACK FiltersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
public:
	CCreateFilterDialog( CWnd* pParent = NULL );

	//{{AFX_DATA(CCreateFilterDialog)
	enum { IDD = IDD_CREATE_FILTER };
	CListBox	m_ConditionsList;
	CCheckListBox	m_FoldersList;
	CListCtrl	m_FiltersList;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(CCreateFilterDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CCreateFilterDialog)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnColumnclickFiltersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickFiltersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedFiltersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangeConditionsList();
	afx_msg void OnSelchangeAllFoldersList();
	afx_msg void OnAddConditionButton();
	afx_msg void OnRemoveConditionButton();
	afx_msg void OnAddFilterButton();
	afx_msg void OnAddFilterMenu();
	afx_msg void OnDeleteFilterButton();
	afx_msg void OnDeleteFilterMenu();
	afx_msg void OnDblclkAllFoldersList();
	afx_msg void OnRenameFilterButton();
	afx_msg void OnRenameFilterMenu();
	afx_msg void OnDblclkFiltersList(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 300; }
	virtual int GetMinimumYDimension() { return 230; }
	virtual std::string GetXMLOptionsLabel() { return "CCreateFilterDialog"; }
	virtual bool GetDrawGripper() { return true; }

	int nSortColumn;
	std::vector<bool> bFiltersSortParam;
	bool bCreateControls;
	TSimpleFilterItem lastSimpleFilterItem;

	void CreateControls();
	void ClearControls();
	void UpdateControls();
	
	void FillFilters();
	void FillFolders( const TSimpleFilterItem &rSimpleFilterItem );

	void LoadFilterToControls();
	void GetUniqueFilterName( const std::string &rszNewFilterName, std::string *pszFullNewFilterName );

public:
	TEnumFolders folders;
	TFilterHashMap filters;
	std::string szCurrentFilter;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__CREATE_FILTER_DIALOG__)
