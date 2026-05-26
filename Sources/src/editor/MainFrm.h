// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_)
#define AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
#include "..\GFX\GFX.h"
#include "..\Input\Input.h"
#include "..\Scene\Scene.h"

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

// Attributes
public:
/*
	CChildView *m_pUIView;		//User interface editor window
	CAnimationView *m_pAnimView;
*/

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
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
	//�����
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
	
	//GUI editor frame
	CTreeDockWnd *pGUITreeDockWnd;
	CPropView *pGUIPropView;
	CPropertyDockBar *pGUIPropertyDockBar;

	//���� ��� Animations Frame
	CTreeDockWnd *pAnimTreeDockWnd;
	CPropView *pAnimPropView;
	SECCustomToolBar *pInfantryToolBar;
	
	//��� Sprite Composer Frame
	CTreeDockWnd *pSpriteTreeDockWnd;
	CPropView *pSpritePropView;

	//��� Effect Composer Frame
	CTreeDockWnd *pEffectTreeDockWnd;
	CPropView *pEffectPropView;
	CDirectionButtonDockBar *pEffectDirectionButtonDockBar;
	SECCustomToolBar *pEffectToolBar;
	
	//��� Object Composer Frame
	CTreeDockWnd *pObjectTreeDockWnd;
	CPropView *pObjectPropView;
	SECCustomToolBar *pObjectToolBar;
	
	//��� Mesh Composer Frame
	CTreeDockWnd *pMeshTreeDockWnd;
	CPropView *pMeshPropView;
	CDirectionButtonDockBar *pMeshDirectionButtonDockBar;
	SECCustomToolBar *pMeshToolBar;
	
	//��� Weapon Composer Frame
	CTreeDockWnd *pWeaponTreeDockWnd;
	CPropView *pWeaponPropView;

	//��� Building Composer Frame
	CTreeDockWnd *pBuildingTreeDockWnd;
	CPropView *pBuildingPropView;
	SECCustomToolBar *pBuildingToolBar;
	
	//��� TileSet Composer Frame
	CTreeDockWnd *pTileTreeDockWnd;
	CPropView *pTilePropView;
	SECCustomToolBar *pTileToolBar;

	//��� Fence Composer Frame
	CTreeDockWnd *pFenceTreeDockWnd;
	CPropView *pFencePropView;
	SECCustomToolBar *pFenceToolBar;
	
	//��� Particle Frame
	CTreeDockWnd *pParticleTreeDockWnd;
	CPropView *pParticlePropView;
	CKeyFrameDockWnd *pParticleKeyFrameDockWnd;
	SECCustomToolBar *pParticleToolBar;
	
	//��� Trench Frame
	CTreeDockWnd *pTrenchTreeDockWnd;
	CPropView *pTrenchPropView;

	//��� Squad Frame
	CTreeDockWnd *pSquadTreeDockWnd;
	CPropView *pSquadPropView;
	CDirectionButtonDockBar *pSquadDirectionButtonDockBar;
	SECCustomToolBar *pSquadToolBar;

	//��� Mine Composer Frame
	CTreeDockWnd *pMineTreeDockWnd;
	CPropView *pMinePropView;

	//��� Bridge Composer Frame
	CTreeDockWnd *pBridgeTreeDockWnd;
	CPropView *pBridgePropView;
	SECCustomToolBar *pBridgeToolBar;
	
	//��� Mission Composer Frame
	CTreeDockWnd *pMissionTreeDockWnd;
	CPropView *pMissionPropView;
	SECCustomToolBar *pMissionToolBar;

	//��� Chapter Composer Frame
	CTreeDockWnd *pChapterTreeDockWnd;
	CPropView *pChapterPropView;
	SECCustomToolBar *pChapterToolBar;
	
	//��� Campaign Composer Frame
	CTreeDockWnd *pCampaignTreeDockWnd;
	CPropView *pCampaignPropView;

	//��� 3DRoad Frame
	CTreeDockWnd *p3DRoadTreeDockWnd;
	CPropView *p3DRoadPropView;
	SECCustomToolBar *p3DRoadToolBar;
	
	//��� 3DRiver Frame
	CTreeDockWnd *p3DRiverTreeDockWnd;
	CPropView *p3DRiverPropView;
	
	//��� Medal Frame
	CTreeDockWnd *pMedalTreeDockWnd;
	CPropView *pMedalPropView;

	CComboBox m_brushSizeCombo;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnCreateCombo( WPARAM wParam, LPARAM lParam );
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__24238F56_2C9E_4211_B736_2B74E0980EF1__INCLUDED_)

