
#if !defined(AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_)
#define AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..//Common//LegacyUiCompat.h"
#include "../GFX/GFX.h"
#include "../Input/Input.h"
#include "../Scene/Scene.h"

class CTreeDockWnd;
class CPropView;
class CKeyFrameDockWnd;
class CPropertyDockBar;
class SECCustomToolBar;

#include "ThumbListDockBar.h"
#include "DirectionButtonDock.h"
#include "GameWnd.h"

class CMainFrame : public SECWorkbook
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();
	~CMainFrame();

public:
/*
	CChildView *m_pUIView;		//User interface editor window
	CAnimationView *m_pAnimView;
*/

public:

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

public:
	void UpdateStatusBarIndicators();
	void UpdateStatusBarCoordsIndicator(const POINT &pt);
	void UpdateStatusBarControlIndicator(const RECT &rc);
	void UpdateStatusBarControlIndicator(const CTRect<float> &rc);
	void ClearStatusBarControlIndicator();

	SECToolBarManager* GetControlBarManager() { return (SECToolBarManager *) m_pControlBarManager; }

	void ShowSECControlBar( SECControlBar *pControlBar, int nCommand );
	void ShowSECToolBar( SECControlBar *pToolBar, int nCommand );
	void SetMainWindowTitle( const char *pszTitle ) { SetTitle( pszTitle ); }
	void SetMainWindowText( const char *pszText ) { SetWindowText( pszText ); }

	void DockControlToLeft(SECCustomToolBar *pBar);

protected:
	int InitGameWindow();
	int CreateGUIFrame();
	int CreateAnimationFrame();
	int CreateSpriteFrame();
	int CreateEffectFrame();
	int CreateObjectFrame();
	int CreateMeshFrame();
	int CreateWeaponFrame();
	int CreateBuildingFrame();
	int CreateTileSetFrame();
	int CreateFenceFrame();
	int CreateParticleFrame();
	int CreateTrenchFrame();
	int CreateSquadFrame();
	int CreateMineFrame();
	int CreateBridgeFrame();
	int CreateMissionFrame();
	int CreateChapterFrame();
	int CreateCampaignFrame();
	int Create3DRoadFrame();
	int Create3DRiverFrame();
	int CreateMedalFrame();
	
public:
	SECStatusBar m_wndStatusBar;

	int m_nFireRangeRegisterGroup;
	CComboBox *m_fireRangeFilterComboBox;
	bool m_fireRangePressed;

protected:  // control bar embedded members
	CGameWnd m_gameWnd;						//������� ������, ����� ���� ������ ����
	
	UINT*	m_pDefButtonGroup;			// toolbar default button group
	UINT	m_nDefButtonCount;			// the number of elements in m_pDefaultButtons	
	SECCustomToolBar m_wndToolBar;
	HACCEL m_hMDIAccel;
	HFONT m_hComboFont;
	CComboBox *m_pFenceCombo;
	CComboBox *m_pObjectCombo;
	CComboBox *m_pBuildingCombo;
	CComboBox *m_pBridgeCombo;
	SECCustomToolBar *pCommonToolBar;
	
	CTreeDockWnd *pGUITreeDockWnd;
	CPropView *pGUIPropView;
	CPropertyDockBar *pGUIPropertyDockBar;

	CTreeDockWnd *pAnimTreeDockWnd;
	CPropView *pAnimPropView;
	SECCustomToolBar *pInfantryToolBar;
	
	CTreeDockWnd *pSpriteTreeDockWnd;
	CPropView *pSpritePropView;

	CTreeDockWnd *pEffectTreeDockWnd;
	CPropView *pEffectPropView;
	CDirectionButtonDockBar *pEffectDirectionButtonDockBar;
	SECCustomToolBar *pEffectToolBar;
	
	CTreeDockWnd *pObjectTreeDockWnd;
	CPropView *pObjectPropView;
	SECCustomToolBar *pObjectToolBar;
	
	CTreeDockWnd *pMeshTreeDockWnd;
	CPropView *pMeshPropView;
	CDirectionButtonDockBar *pMeshDirectionButtonDockBar;
	SECCustomToolBar *pMeshToolBar;
	
	CTreeDockWnd *pWeaponTreeDockWnd;
	CPropView *pWeaponPropView;

	CTreeDockWnd *pBuildingTreeDockWnd;
	CPropView *pBuildingPropView;
	SECCustomToolBar *pBuildingToolBar;
	
	CTreeDockWnd *pTileTreeDockWnd;
	CPropView *pTilePropView;
	SECCustomToolBar *pTileToolBar;

	CTreeDockWnd *pFenceTreeDockWnd;
	CPropView *pFencePropView;
	SECCustomToolBar *pFenceToolBar;
	
	CTreeDockWnd *pParticleTreeDockWnd;
	CPropView *pParticlePropView;
	CKeyFrameDockWnd *pParticleKeyFrameDockWnd;
	SECCustomToolBar *pParticleToolBar;
	
	CTreeDockWnd *pTrenchTreeDockWnd;
	CPropView *pTrenchPropView;

	CTreeDockWnd *pSquadTreeDockWnd;
	CPropView *pSquadPropView;
	CDirectionButtonDockBar *pSquadDirectionButtonDockBar;
	SECCustomToolBar *pSquadToolBar;

	CTreeDockWnd *pMineTreeDockWnd;
	CPropView *pMinePropView;

	CTreeDockWnd *pBridgeTreeDockWnd;
	CPropView *pBridgePropView;
	SECCustomToolBar *pBridgeToolBar;
	
	CTreeDockWnd *pMissionTreeDockWnd;
	CPropView *pMissionPropView;
	SECCustomToolBar *pMissionToolBar;

	CTreeDockWnd *pChapterTreeDockWnd;
	CPropView *pChapterPropView;
	SECCustomToolBar *pChapterToolBar;
	
	CTreeDockWnd *pCampaignTreeDockWnd;
	CPropView *pCampaignPropView;

	CTreeDockWnd *p3DRoadTreeDockWnd;
	CPropView *p3DRoadPropView;
	SECCustomToolBar *p3DRoadToolBar;
	
	CTreeDockWnd *p3DRiverTreeDockWnd;
	CPropView *p3DRiverPropView;
	
	CTreeDockWnd *pMedalTreeDockWnd;
	CPropView *pMedalPropView;

	CComboBox m_brushSizeCombo;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnCreateCombo( WPARAM wParam, LPARAM lParam );
	afx_msg void OnClose();
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_)

