#if !defined(__Tabs__Terrain_Altitudes_Tab_Dialog__)
#define __Tabs__Terrain_Altitudes_Tab_Dialog__

#include "..\RandomMapGen\VA_Types.h"
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\RandomMapGen\TerrainGenerator.h"
#include "ResizeDialog.h"

class CShadeEditorWnd : public CResizeDialog
{
public:
	enum LEVEL_TO
	{
		LEVEL_TO_0 = 0, //к нулю 
		LEVEL_TO_1 = 1, //к выделенному тайлу
		LEVEL_TO_2 = 2, //к мгновенной средней высое
		LEVEL_TO_3 = 3, //к выделенной средней высоте
	};

	SVAPattern m_currentPattern;
	SVAPattern m_currentLevelPattern;
	SVAPattern m_currentUndoLevelPattern;
	DWORD m_tickCount;
	DWORD m_refreshRate;
	bool isSetEditCtrlValue;

	void UpdateLevelToButtons();
	void UpdateTerrainGenButtons();
	void UpdateControls();
	CShadeEditorWnd(CWnd* pParent = NULL);

	enum { IDD = IDD_TAB_TERRAIN_ALTITUDES };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);


protected:
	const static int vID[];
	
	virtual std::string GetXMLOptionsLabel() { return "CShadeEditorWnd"; }

	void CreateCurrentPattern();
	virtual BOOL OnInitDialog();
	afx_msg void OnReleasedcaptureShadeBrushSize(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnChangeShadeHeight();
	afx_msg void OnChangeShadeLevelratio();
	afx_msg void OnShadeType0();
	afx_msg void OnShadeType1();
	afx_msg void OnShadeType2();
	afx_msg void OnShadeType3();
	afx_msg void OnShadeType4();
	afx_msg void OnChangeShadeMax();
	afx_msg void OnChangeShadeMin();
	afx_msg void OnShadeGenerateButton();
	afx_msg void OnShadeZeroButton();
	afx_msg void OnChangeShadeGranularity();
	afx_msg void OnShadeLevelTo0();
	afx_msg void OnShadeLevelTo1();
	afx_msg void OnShadeLevelTo2();
	afx_msg void OnShadeLevelTo3();
	afx_msg void OnShadeUpdateButton();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(__Tabs__Terrain_Altitudes_Tab_Dialog__)
