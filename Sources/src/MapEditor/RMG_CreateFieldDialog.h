#if !defined(__RMG_Create_Field_Dialog__)
#define __RMG_Create_Field_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "..\RandomMapGen\RMG_Types.h"
#include "Input3DTabWnd.h"

#define IDC_RMG_CF_FIELD_PROPERTIES_TAB 1521

int CALLBACK CF_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CRMGCreateFieldDialog : public CResizeDialog
{
	friend int CALLBACK CF_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend class CRMGFieldTerrainDialog;
	friend class CRMGFieldObjectsDialog;
	friend class CRMGFieldHeightsDialog;

public:
	CRMGCreateFieldDialog( CWnd* pParent = NULL );
	~CRMGCreateFieldDialog();

	enum { IDD = IDD_RMG_CREATE_FIELD };
	CListCtrl	m_FieldsList;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveas();
	afx_msg void OnFileExit();
	afx_msg void OnColumnclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddFieldButton();
	afx_msg void OnDeleteFieldButton();
	afx_msg void OnCheckFieldsButton();
	afx_msg void OnAddFieldMenu();
	afx_msg void OnDeleteFieldMenu();
	afx_msg void OnItemchangedFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSaveButton();
	DECLARE_MESSAGE_MAP()

protected:
	enum FIELD_TABS
	{
		FIELD_TAB_TERRAIN	= 0,
		FIELD_TAB_OBJECTS	= 1,
		FIELD_TAB_HEIGHTS	= 2,
		FIELD_TAB_COUNT		= 3,
	};

	static const char* FIELD_TAB_LABELS[FIELD_TAB_COUNT];
	
	const static int vID[];
	CRMFieldSetsHashMap fields;
	CInput3DTabWindow *pInput3DTabWindow;	

	bool bCreateControls;
	int nSortColumn;
	std::vector<bool> bFieldsSortParam;

	virtual int GetMinimumXDimension() { return	550; }
	virtual int GetMinimumYDimension() { return 450; }
	virtual std::string GetXMLOptionsLabel() { return "CRMGCreateFieldDialog"; }
	virtual bool GetDrawGripper() { return true; }

	bool LoadFieldsList();
	bool SaveFieldsList();

	bool LoadFieldToControls();
	bool UpdateFieldList( const SRMFieldSet *pRMFieldSet );
	void SetFieldItem( int nItem, const SRMFieldSet &rField );

	void CreateControls();
	void ClearControls();
	void UpdateControls();
	
	class CRMGFieldTerrainDialog* GetRMGFieldTerrainDialog();
	class CRMGFieldObjectsDialog* GetRMGFieldObjectsDialog();
	class CRMGFieldHeightsDialog* GetRMGFieldHeightsDialog();
};
#endif // !defined(__RMG_Create_Field_Dialog__)
