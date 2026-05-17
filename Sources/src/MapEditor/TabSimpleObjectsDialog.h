#if !defined(__Tabs__Simple_Objects_Dialog__)
#define __Tabs__Simple_Objects_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "StdAfx.h"
#include "ResizeDialog.h"
#include "DirectionButton.h"
#include "CreateFilterDialog.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFilterItem
{
	std::string szName;
	std::string szFilter;

	//constructors
	SFilterItem() {}
	SFilterItem( const std::string &rszName, const std::string &rszFilter ) : szName( rszName ), szFilter( rszFilter ) {}
	SFilterItem( const SFilterItem &rFilterItem ) : szName( rFilterItem.szName ), szFilter( rFilterItem.szFilter ) {}

	SFilterItem& operator=( const SFilterItem &rFilterItem )
	{
		if( &rFilterItem != this )
		{
			szName = rFilterItem.szName;
			szFilter = rFilterItem.szFilter;
		}
		return *this;
	}	

	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );
	virtual int STDCALL operator&( IStructureSaver &ss );
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTabSimpleObjectsDialog : public CResizeDialog
{
public:
	CTabSimpleObjectsDialog( CWnd* pParent = NULL );
	  
	//{{AFX_DATA(CTabSimpleObjectsDialog)
	enum { IDD = IDD_TAB_SIMPLE_OBJECTS };
	CComboBox	m_players;
	CButton	m_listCheck;
	CButton	m_check9;
	CButton	m_check8;
	CButton	m_check7;
	CButton	m_check6;
	CButton	m_check5;
	CButton	m_check4;
	CButton	m_check3;
	CButton	m_check2;
	CButton	m_check1;
	CComboBox	m_filtersCtrl;
	CListCtrl	m_imageList;
	CButton	m_flagFlora;
	//}}AFX_DATA
	//CImageList *pIML;
	CDirectionButton m_angelButton;

	//{{AFX_VIRTUAL(CTabSimpleObjectsDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

protected:
	const static int vID[];

	virtual std::string GetXMLOptionsLabel() { return "CTabSimpleObjectsDialog"; }

	std::vector<SFilterItem> m_filters; 
	std::vector<CButton*> m_checkButtons;

	//{{AFX_MSG(CTabSimpleObjectsDialog)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	virtual BOOL OnInitDialog();
	afx_msg void OnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangeCombo1();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCheck0();
	afx_msg void OnCheck1();
	afx_msg void OnCheck2();
	afx_msg void OnCheck3();
	afx_msg void OnCheck4();
	afx_msg void OnCheck5();
	afx_msg void OnCheck6();
	afx_msg void OnCheck7();
	afx_msg void OnCheck8();
	afx_msg void OnCheck9();
	afx_msg void OnButtonDeleteFilter();
	afx_msg void OnSelchangeSoPlayerCombo();
	afx_msg void OnSoListList();
	afx_msg void OnSoListIcons();
	afx_msg void OnDestroy();
	afx_msg void OnDblclkObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnObjectPropertiesMenu();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	TFilterHashMap m_allFilters;
	CImageList objectsImageList;
	std::unordered_map<std::string, int> objectsImageIndices;

	afx_msg void OnButtonNewFilter();
	afx_msg void OnDiplomacyButton();

public:
	void UpdateCheck( int n );
	int GetDefaultDirAngel();
	bool FilterName( const std::string &rszName );
	static bool CommonFilterName( const std::string &rszName );

	bool IsPictures();
	void FillPlayers();
	void UpdateControls();
	void SetObjectsListStyle( bool bPictures );
	void UpdateObjectsListStyle();
	void CreateObjectsImageList();
	int GetPlayer();
	int GetObjectIndex();
	

	//MODs support
	void DeleteImageList();
	void CreateImageList();

	void ShowObjectProperties();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__Tabs__Simple_Objects_Dialog__)
