#if !defined(__Property_Editor_PointsListDialog__)
#define __Property_Editor_PointsListDialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ResizeDialog.h"

int CALLBACK PointsListCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
class CPEPointsListDialog : public CResizeDialog
{
friend int CALLBACK PointsListCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

public:
	std::string szDialogName;
	std::vector<CVec3> points;

public:
	CPEPointsListDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_PE_POINTS_LIST };
	CListCtrl	m_PointsList;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
protected:
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnAddPointButton();
	afx_msg void OnDeletePointButton();
	afx_msg void OnPointPropertiesButton();
	afx_msg void OnAddMenu();
	afx_msg void OnDeleteMenu();
	afx_msg void OnPropertiesMenu();
	afx_msg void OnDblclkPointsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickPointsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickPointsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedPointsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownPointsList(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

	const static int vID[];
	bool bCreateControls;
	int nSortColumn;
	std::vector<bool> bPointsSortParam;

	virtual int GetMinimumXDimension() { return 200; }
	virtual int GetMinimumYDimension() { return 200; }
	virtual std::string GetXMLOptionsLabel() { return "CPEPointsListDialog"; }
	virtual bool GetDrawGripper() { return true; }

	bool LoadPointsList();
	void SetPointItem( int nItem, int nPointIndex );
	
	void CreateControls();
	void UpdateControls();
};
#endif // !defined(#define __Property_Editor_PointsListDialog__)
