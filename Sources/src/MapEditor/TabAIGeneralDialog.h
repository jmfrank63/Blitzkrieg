#if !defined(__Tabs__AIGeneral_Dialog__)
#define __Tabs__AIGeneral_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"

int CALLBACK ReinforcementsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
int CALLBACK PositionsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CTabAIGeneralDialog : public CResizeDialog
{
	friend int CALLBACK ReinforcementsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
	friend int CALLBACK PositionsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

	afx_msg void OnColumnclickPositionsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAigDeleteMobileReinforcementScriptIdButton();
	afx_msg void OnAigAddMobileReinforcementScriptIdButton();
	afx_msg void OnItemchangedAigMobileReinforcementScriptIdList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSide1RadioButton();
	afx_msg void OnSide0RadioButton();
	afx_msg void OnRclickReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddReinforcementMenu();
	afx_msg void OnDeleteReinforcementMenu();
	afx_msg void OnItemchangedPositionsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeletePositionMenu();
	afx_msg void OnPositionTypeMenu();
	afx_msg void OnPositionTypeButton();
	afx_msg void OnDeletePositionButton();
	afx_msg void OnRclickPositionsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownPositionsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkPositionsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

public:
	enum { IDD = IDD_TAB_AI_GENERAL };
	CListCtrl	m_PositionsList;
	CListCtrl	m_ReinforcementsList;
	CStatic	m_MessageStatic;
	int		m_nSide;

	CTabAIGeneralDialog( CWnd* pParent = NULL );

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
protected:
	const static int vID[];
	
	virtual std::string GetXMLOptionsLabel() { return "CTabAIGeneralDialog"; }

	bool bCreateControls;
	int nSortColumn;
	std::vector<bool> bReinforcementsSortParam;
	std::vector<bool> bPositionsSortParam;
	
	void CreateControls();
	void UpdateControls();

	std::string AngleToText( WORD angle );
	void GetUnitsCountByScriptID( int nScriptID, int nSide, class CTemplateEditorFrame *pFrame, int *pUnits, int *pInvalidUnits, int *pSquads );
public:
	bool LoadAIGReinforcementsInfo();
	bool LoadAIGPositionsInfo();

	void SetReinforcementItem( int nNewItem, int nScriptID );
	void SetPositionItem( int nNewItem, const SAIGeneralParcelInfo &rAIGeneralParcelInfo );
	
	void UpdatePosition( int nPositionIndex );
	void DeletePosition( int nPositionIndex );
	void AddPosition( int nPositionIndex );
};

#endif // !defined(__Tabs__AIGeneral_Dialog__)
