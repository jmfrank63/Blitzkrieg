#if !defined(__RMG_Create_Container_Dialog__)
#define __RMG_Create_Container_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "..\RandomMapGen\RMG_Types.h"

int CALLBACK CC_PatchesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
int CALLBACK CC_ContainersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CRMGCreateContainerDialog : public CResizeDialog
{
	friend int CALLBACK CC_PatchesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend int CALLBACK CC_ContainersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
public:
	CRMGCreateContainerDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CREATE_CONTAINER };
	CListCtrl	m_ContainersList;
	CListCtrl	m_PatchesList;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnAddButton();
	afx_msg void OnDeleteButton();
	afx_msg void OnPropertiesButton();
	afx_msg void OnAddContainerButton();
	afx_msg void OnDeleteContainerButton();
	afx_msg void OnContainerPropertiesButton();
	afx_msg void OnItemchangedPatchesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkPatchesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickPatchesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownPatchesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickPatchesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedContainersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkContainersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickContainersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownContainersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickContainersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddMenu();
	afx_msg void OnDeleteMenu();
	afx_msg void OnPropertiesMenu();
	afx_msg void OnAddContainerMenu();
	afx_msg void OnDeleteContainerMenu();
	afx_msg void OnSaveButton();
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveas();
	afx_msg void OnFileExit();
	afx_msg void OnCheckContainersButton();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];
	CRMContainersHashMap containers;
	bool isChanged;
	bool bSomeDeleted;
	int nSortColumn;
	std::vector<bool> bContainersSortParam;
	std::vector<bool> bPatchesSortParam;

	virtual int GetMinimumXDimension() { return 550; }
	virtual int GetMinimumYDimension() { return 400; }
	virtual std::string GetXMLOptionsLabel() { return "CRMGCreateContainerDialog"; }
	virtual bool GetDrawGripper() { return true; }

	bool LoadContainersList();
	bool SaveContainersList();
	
	bool LoadContainerToControls();
	bool SaveContainerFromControls();
	void SetContainerItem( int nItem, const SRMContainer &rContainer );
	bool IsValidContainerEntered();

	void UpdateControls();
	
	void CreateControls();
	void ClearControls();
public:
};

#endif // !defined(__RMG_Create_Container_Dialog__)
