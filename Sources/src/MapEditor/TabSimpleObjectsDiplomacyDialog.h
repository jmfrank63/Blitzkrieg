#if !defined(__Tabs__Simple_Objects_Diplomacy_Dialog__)
#define __Tabs__Simple_Objects_Diplomacy_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "../RandomMapGen/RMG_Types.h"

int CALLBACK PlayersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CTabSimpleObjectsDiplomacyDialog : public CResizeDialog
{
	friend int CALLBACK PlayersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
public:
	CTabSimpleObjectsDiplomacyDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_TAB_SIMPLE_OBJECTS_DIPLOMACY };
	CComboBox	m_Types;
	CComboBox	m_Sides;
	CListCtrl	m_PlayersList;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnColumnclickPlayersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddButton();
	afx_msg void OnDeleteButton();
	afx_msg void OnSide0Button();
	afx_msg void OnSide1Button();
	afx_msg void OnItemchangedPlayersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSimpleObjectsDiplomacyAddPlayerMenu();
	afx_msg void OnSimpleObjectsDiplomacyDeletePlayerMenu();
	afx_msg void OnSimpleObjectsDiplomacySide0Menu();
	afx_msg void OnSimpleObjectsDiplomacySide1Menu();
	afx_msg void OnRclickPlayersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownPlayersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangeSoDiplomacyTypeComboBox();
	afx_msg void OnSelchangeSoDiplomacyAttackSideComboBox();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];
	static const char SIDE0_LABEL[];
	static const char SIDE1_LABEL[];

	bool bCreateControls;
	int nSortColumn;
	int nType;
	int nAttackingSide;
	std::vector<bool> bPlayersSortParam;

	virtual int GetMinimumXDimension() { return 250; }
	virtual int GetMinimumYDimension() { return 200; }
	virtual std::string GetXMLOptionsLabel() { return "CTabSimpleObjectsDiplomacyDialog"; }

	void FillPlayers();
	void CreateControls();
	void UpdateControls();
public:
	std::vector<BYTE> diplomacies;

	void SetType( int _nType ) { nType = _nType; }
	void SetAttackingSide( int _nAttackingSide ) { nAttackingSide = _nAttackingSide; }
	int GetType() { return nType; }
	int GetAttackingSide() { return nAttackingSide; }
};

#endif // !defined(__Tabs__Simple_Objects_Diplomacy_Dialog__)
