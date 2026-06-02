#if !defined(__RMG_Create_Template_Dialog__)
#define __RMG_Create_Template_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "..\RandomMapGen\RMG_Types.h"
#include "PropertieDialog.h"

int CALLBACK CT_TemplatesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
int CALLBACK CT_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
int CALLBACK CT_VSOCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
int CALLBACK CT_TemplateGraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CTemplateUnitsDialog : public CPropertieDialog
{
	public:
	CMutableUnitCreationInfo unitCreation;
	CTemplateUnitsDialog( CWnd* pWnd ) : CPropertieDialog( pWnd ) {}
	
	protected:
	virtual BOOL OnInitDialog()
	{
		CPropertieDialog::OnInitDialog();

		SetWindowText( "Unit Creation Info" );
		ClearVariables();
		AddObjectWithProp( unitCreation.GetManipulator() );
		return true;
	}
	virtual void OnOK()
	{
		ClearVariables();
		CDialog::OnOK();
	}
	virtual void OnCancel()
	{
		ClearVariables();
		CDialog::OnCancel();
	}
};

class CRMGCreateTemplateDialog : public CResizeDialog
{
	friend int CALLBACK CT_TemplatesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend int CALLBACK CT_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend int CALLBACK CT_VSOCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend int CALLBACK CT_TemplateGraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

public:
	CRMGCreateTemplateDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CREATE_TEMPLATE };
	CEdit	m_ScriptFileNameEdit;
	CListCtrl	m_VSOList;
	CListCtrl	m_FieldsList;
	CListCtrl	m_GraphsList;
	CListCtrl	m_TemplatesList;
	CComboBox	m_MODComboBox;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnColumnclickVsoList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickTemplatesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveas();
	afx_msg void OnFileExit();
	afx_msg void OnAddTemplateButton();
	afx_msg void OnDeleteTemplateButton();
	afx_msg void OnTemplateUnitsButton();
	afx_msg void OnCheckTemplatesButton();
	afx_msg void OnTemplateUnitsMenu();
	afx_msg void OnAddTemplateMenu();
	afx_msg void OnDeleteTemplateMenu();
	afx_msg void OnItemchangedTemplatesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkTemplatesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickTemplatesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownTemplatesList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnChangeRmgFileNameEdit();
	afx_msg void OnScriptFileNameBrowseButton();
	afx_msg void OnAddGraphButton();
	afx_msg void OnAddGraphMenu();
	afx_msg void OnDeleteGraphButton();
	afx_msg void OnDeleteGraphMenu();
	afx_msg void OnGraphPropertiesButton();
	afx_msg void OnGraphPropertiesMenu();
	afx_msg void OnDblclkGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSaveButton();
	afx_msg void OnAddVsoButton();
	afx_msg void OnDeleteVsoButton();
	afx_msg void OnVsoPropertiesButton();
	afx_msg void OnAddVsoMenu();
	afx_msg void OnDeleteVsoMenu();
	afx_msg void OnVsoPropertiesMenu();
	afx_msg void OnDblclkVsoList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickVsoList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedVsoList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownVsoList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddFieldButton();
	afx_msg void OnDeleteFieldButton();
	afx_msg void OnFieldPropertiesButton();
	afx_msg void OnAddFieldMenu();
	afx_msg void OnDeleteFieldMenu();
	afx_msg void OnFieldPropertiesMenu();
	afx_msg void OnDblclkFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownFieldsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTemplateDiplomacyButton();
	afx_msg void OnTemplateDiplomacyMenu();
	afx_msg void OnSelchangeModComboBox();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];
	CRMTemplatesHashMap templates;
	
	std::string szMODNameBackup;
	std::string szMODVersionBackup;

	bool bCreateControls;
	int nSortColumn;
	std::vector<bool> bTemplatesSortParam;
	std::vector<bool> bFieldsSortParam;
	std::vector<bool> bVSOSortParam;
	std::vector<bool> bGraphsSortParam;

	virtual int GetMinimumXDimension() { return	550; }
	virtual int GetMinimumYDimension() { return 450; }
	virtual std::string GetXMLOptionsLabel() { return "CRMGCreateTemplateDialog"; }
	virtual bool GetDrawGripper() { return true; }

	bool LoadTemplatesList();
	bool SaveTemplatesList();

	bool LoadTemplateToControls();
	bool SaveTemplateFromControls();
	void SetTemplateItem( int nItem, const SRMTemplate &rTemplate );
	void SetGraphItem( int nItem, int nWeight, const SRMGraph &rGraph );
	void SetVSOItem( int nItem, int nWeight, const SRMVSODesc &rVSODesc );
	void SetFieldItem( int nItem, int nWeight, bool bDefault, const SRMFieldSet &rFieldSet );

	void GetAllMODs( std::vector<std::string> *pMODsList );

	void CreateControls();
	void ClearControls();
	void UpdateControls();
public:
};
#endif // !defined(__RMG_Create_Template_Dialog__)
