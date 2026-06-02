#if !defined(__RMG_Field_Objects_Dialog__)
#define __RMG_Field_Objects_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "CreateFilterDialog.h"

int CALLBACK CFO_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CRMGFieldObjectsDialog : public CResizeDialog
{
	friend int CALLBACK CFO_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend class CRMGCreateFieldDialog;

public:
	static const int DEFAULT_OBJECT_WEIGHT;
	static const float DEFAULT_SHELL_WIDTH;
	static const int DEFAULT_SHELL_STEP;
	static const float DEFAULT_SHELL_RATIO;
	static const char UNKNOWN_OBJECT[];
	static const char MULTIPLE_SELECTION[];
	CRMGFieldObjectsDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CF_OBJECTS_SHELLS };
	CComboBox	m_FilterComboBox;
	CListCtrl	m_ShellsList;
	CListCtrl	m_ObjectsList;
	CListCtrl	m_AvailableObjectsList;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnListRadio();
	afx_msg void OnThumbnailsRadio();
	afx_msg void OnSelchangeObjectsFilterCombo();
	afx_msg void OnItemchangedShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnObjectProperties();
	afx_msg void OnAddObject();
	afx_msg void OnRemoveObject();
	afx_msg void OnShellProperties();
	afx_msg void OnRemoveShell();
	afx_msg void OnAddShell();
	afx_msg void OnAddShellMenu();
	afx_msg void OnRemoveShellMenu();
	afx_msg void OnShellPropertiesMenu();
	afx_msg void OnDblclkShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownShellsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddObjectMenu();
	afx_msg void OnRemoveObjectMenu();
	afx_msg void OnObjectPropertiesMenu();
	afx_msg void OnAvailableObjectPropertiesMenu();
	afx_msg void OnDblclkObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
	
	void OnAvailableObjectProperties();

protected:
	bool bCreateControls;
	class CRMGCreateFieldDialog *pRMGCreateFieldDialog;
	struct SRMFieldSet *pRMFieldSet;						//главный
	std::vector<struct SRMFieldSet*> fieldSets;	//довавочные ( для изменения сезона и других вещей ( кроме шеллзов ) )
	
	int nSortColumn;
	int nCurrentShell;
	std::vector<bool> bShellsSortParam;
	TFilterHashMap filters;

	bool bInitialPictures;
	std::string szCurrentFilter;

	virtual std::string GetXMLOptionsLabel() { return "CRMGFieldObjectsDialog"; }

	void CreateControls();
	void UpdateControls();
	void FillAvailableObjects( const std::string &rszFilter );
	void FillShellObjectsList( int nSelectedShell );
	void LoadFieldToControls();
	void SetShellItem( int nItem, const SRMObjectSetShell &rObjectSetShell );
	void SetObjectsListsStyle( bool bPictures );
	void UpdateObjectsListsStyle();
};
#endif // !defined(__RMG_Field_Objects_Dialog__)
