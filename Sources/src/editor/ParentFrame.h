#ifndef __PARENT_FRAME_H__
#define __PARENT_FRAME_H__

#include "..\\Common\\LegacyUiCompat.h"
#include "..\GFX\GFX.h"

using std::string;
using std::vector;
using std::list;

interface IGFX;
class CTreeDockWnd;
class CPropView;
class CTreeItem;
class CETreeCtrl;
class SECCustomToolBar;

struct SObjectInfo
{
	std::string szPath;
	std::string szType;
	std::string szName;
	std::string szVisType;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "path", &szPath );
		saver.Add( "game_type", &szType );
		saver.Add( "name", &szName );
		saver.Add( "type", &szVisType );
		return 0;
	}
};

class CParentFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CParentFrame)
public:
	CParentFrame();
	virtual ~CParentFrame();
	
	virtual void Init( IGFX *_pGFX ) { pGFX = _pGFX; }			//��������������
	virtual void ShowFrameWindows( int nCommand );					//���������� ��� ��������� ������, �������� SW_SHOW ��� SW_HIDE
	virtual void GFXDraw() {}																//���������

	void SetChangedFlag( bool bFlag );											//������������� ���� �����������, ���������� * � ����� �������
	int DisplayInsertMenu();																//���������� ���� � ������� insert
	int DisplayDeleteMenu();
	
	void SaveRegisterData();
	void LoadRegisterData();

	int GetFrameType() { return nFrameType; }
	float GetBrightness() { return m_fBrightness; }
	float GetContrast() { return m_fContrast; }
	float GetGamma() { return m_fGamma; }
	void SetBrightness( float fVal ) { m_fBrightness = fVal; }
	void SetContrast( float fVal ) { m_fContrast = fVal; }
	void SetGamma( float fVal ) { m_fGamma = fVal; }
	int GetCompressedFormat() { NI_ASSERT( m_nCompressedFormat != -1 ); return m_nCompressedFormat; }
	int GetLowFormat() { NI_ASSERT( m_nLowFormat != -1 ); return m_nLowFormat; }
	int GetHighFormat() { NI_ASSERT( m_nHighFormat != -1 ); return m_nHighFormat; }
	int GetCompressedShadowFormat() { NI_ASSERT( m_nCompressedShadowFormat != -1 ); return m_nCompressedShadowFormat; }
	int GetLowShadowFormat() { NI_ASSERT( m_nLowShadowFormat != -1 ); return m_nLowShadowFormat; }
	int GetHighShadowFormat() { NI_ASSERT( m_nHighShadowFormat != -1 ); return m_nHighShadowFormat; }
	
	void SetTreeDockBar( CTreeDockWnd *pWnd ) { pTreeDockBar = pWnd; }
	void SetOIDockBar( CPropView *pWnd );
	void SetToolBar( SECCustomToolBar *pWnd ) { pToolBar = pWnd; }
	void ClearPropView();
	BOOL SaveFrame( bool bUnlock = false );					//��� ���������� ������ frame ����� ���������� ���������
	virtual void UpdatePropView( CTreeItem *pTreeItem );								//��� ����������� � OI ����� ����������
	string GetProjectFileName() { return szProjectFileName; }		//��� ���������� ������������� ���������� ��� ��������� ����� � ������
	const char *GetModuleExtension() { return szExtension.c_str(); }
	void RunBatchExporter( const char *pszSourceDir, const char *pszDestDir, const char *pszMask, bool bForceFlag, bool bOpenSave );
	
protected:
	CWnd *pWndView;											//view ������ ��� �����������
	CPtr<IGFX> pGFX;										//��� ��������� ������� � GFX, ������ �������
	CTreeDockWnd *pTreeDockBar;				//������ ��� ������ �������������� item
	CPropView *pOIDockBar;							//object inspector, ���� ��������������
	SECCustomToolBar *pToolBar;					//toolbar ��� ������� ������
	void HookViewMouseMessages();
	bool HitDockSplitter( CPoint point ) const;
	void RepositionFrameWindows();

	string szProjectFileName;						//��� ��������� �������
	string szPrevExportFileName;				//����� ���������������� ��� ��� �������, ������������ szSourceDir
	bool bChanged;											//��������� �� ������ ����� �������� / ��������
	bool bNewProjectJustCreated;				//���� true, �� ������ ��� ������ ����� ������, ���� ���������� ������ �� RPG stats

	int m_nCompressedFormat;
	int m_nLowFormat;
	int m_nHighFormat;

	int m_nCompressedShadowFormat;
	int m_nLowShadowFormat;
	int m_nHighShadowFormat;

	struct STransaction
	{
		string szSourceName;
		string szSourceDate;
		string szSourceTime;
		string szSourceOwner;
		string szAction;
		string szDestName;
		string szDestDate;
		string szDestTime;
		string szDestOwner;

		int operator&( IDataTree &ss );
	};
	string m_szOldProjectName;					//��� ����������� ����� �������
	vector<STransaction> transactions;

	string szComposerName;							//��� composer, ������������ � title
	string szExtension;									//���������� ��� �������� ���������
	string szExportExtension;						//���������� ��� �������� ������ �������
	string szComposerSaveName;					//������������ ��� ���������� ������ � XML
	string szAddDir;										//���������� ����������, ����������� � szSourceDir
	int nTreeRootItemID;								//ID root item ��� ������� ���������
	bool bDefaultExportName;						//���� ���������� ����, �� ��� �������� ����� ������������ ��� ����� 1.xml �� ���������
	int nFrameType;

/*
	string szLocalSourceDir;						//���������� ����������
	string szLocalDestDir;							//���������� �����������
*/
	DWORD m_backgroundColor;						//���� background ��� GFXDraw()
	float m_fBrightness;
	float m_fContrast;
	float m_fGamma;
	bool bTreeExpand;
	bool m_bDraggingDockSplitter;
	int m_nDockSplitterY;

protected:
	void ComputeCaption();							//��������� title ������
	void GenerateProjectName();					//������� ��� ��� ������ �������, ������������ ��� �������� ������ �������

	bool ReadConfigFile( const char *pszDirectory, bool bBatchMode );
	bool WriteConfigFile( bool bAsk, bool bCurrentProjectOnly );	//���������� ���� � ������ ����

	void LoadTransactions( IDataTree *pDT );		//��������� ���� � ���������� ���������� � �������
	void SaveTransactions( FILETIME *pFT, IDataTree *pDT, const char *pszDest, int nAction );	//���������� ���� � ������������ ����������
	bool ConvertAndSaveImage( const char *pszSrc, const char *pszDest );								//��������� �������������� � ��������� ��������
	CETreeCtrl *CreateTrees();
	virtual string GetExportFileName();

	virtual BOOL SpecificTranslateMessage( MSG *pMsg ) { return FALSE; }
	virtual void SpecificInit() {}													//��� ������������� ���������� ������ ����� �������� ������� ��� �������� ������
	virtual void SpecificSave( IDataTree *pDT ) {}					//���������� ��� ������ �������, ����� ������ � GUI composer
	virtual void SpecificClearBeforeBatchMode() {};					//��� ������� ����� �������� batch mode
	
	virtual void SaveFrameOwnData( IDataTree *pDT );				//��� ���������� ����������� ������ �������
	virtual void LoadFrameOwnData( IDataTree *pDT );				//��� ��������

	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName ) {}
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem ) {}

/*
	void FillRPGStats( SCommonRPGStats *pCommonRpgStats, CTreeItem *pRootItem );
	void GetRPGStats( const SCommonRPGStats *pCommonRpgStats, CTreeItem *pRootItem );
*/

	int ExportSingleFile( const char *pszFileName, const char *pszDestDir, bool bForceFlag, bool bOpenSave );
	virtual bool LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem ) { return true; }
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem ) { FILETIME time; time.dwHighDateTime = 0xffffffff; time.dwLowDateTime = 0xffffffff; return time; }
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem ) { FILETIME time; time.dwHighDateTime = 0; time.dwLowDateTime = 0; return time; }
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem ) { return true; }
	
	bool LockFile();				//���������� false ���� �� ����� �������� ���� (��� �������)
	bool UnLockFile();			//���������� false ���� �� ����� ��������� (������ �������� ����� ��� ��� ����� ���)
	void SwitchDockerVisible( SECControlBar *pBar );
	void UpdateShowMenu( CCmdUI* pCmdUI, SECControlBar *pBar );

	void SwitchActiveFrame( int id );
	void UpdateFrameMenu( CCmdUI* pCmdUI, int id );

public:
	void LoadComposerFile( const char *pszFileName );

protected:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual void OnUpdateFrameTitle( BOOL bAddToTitle ) { /* ��� ����� ������ ���� ������ */ }
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
protected:
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnFileCreateNewProject();
	afx_msg void OnFileOpen();
	afx_msg void OnFileClose();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveProjectAs();
	afx_msg void OnExportPak();
	afx_msg void OnRunGame();
	afx_msg void OnHelp();
	afx_msg void OnFileExportFiles();
	afx_msg void OnUpdateFileExportFiles(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSaveProjectAs(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCloseFile(CCmdUI* pCmdUI);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnFileSetdirectories();
	afx_msg void OnSetPictureOptions();
	afx_msg void OnFileBatchMode();
	afx_msg void OnEditSetbackgroundcolor();
	afx_msg void OnUpdateInsertTreeItem(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDeleteTreeItem(CCmdUI* pCmdUI);
	afx_msg void OnViewAdvancedToolbar();
	afx_msg void OnUpdateViewAdvancedToolbar(CCmdUI* pCmdUI);
	afx_msg void OnShowTree();	
	afx_msg void OnUpdateShowTree(CCmdUI* pCmdUI);
	afx_msg void OnShowOI();	
	afx_msg void OnUpdateShowOI(CCmdUI* pCmdUI);
	afx_msg void OnMODSettings();	
	afx_msg void OnExpandTree();
	
	afx_msg void OnUnitEditor();	
	afx_msg void OnUpdateUnitEditor(CCmdUI* pCmdUI);
	afx_msg void OnInfantryEditor();	
	afx_msg void OnUpdateInfantryEditor(CCmdUI* pCmdUI);
	afx_msg void OnSquadEditor();	
	afx_msg void OnUpdateSquadEditor(CCmdUI* pCmdUI);
	afx_msg void OnWeaponEditor();	
	afx_msg void OnUpdateWeaponEditor(CCmdUI* pCmdUI);
	afx_msg void OnMineEditor();	
	afx_msg void OnUpdateMineEditor(CCmdUI* pCmdUI);

	afx_msg void OnParticleEditor();	
	afx_msg void OnUpdateParticleEditor(CCmdUI* pCmdUI);
	afx_msg void OnSpriteEditor();	
	afx_msg void OnUpdateSpriteEditor(CCmdUI* pCmdUI);
	afx_msg void OnEffectEditor();	
	afx_msg void OnUpdateEffectEditor(CCmdUI* pCmdUI);

	afx_msg void OnBuildingEditor();	
	afx_msg void OnUpdateBuildingEditor(CCmdUI* pCmdUI);
	afx_msg void OnObjectEditor();	
	afx_msg void OnUpdateObjectEditor(CCmdUI* pCmdUI);
	afx_msg void OnFenceEditor();	
	afx_msg void OnUpdateFenceEditor(CCmdUI* pCmdUI);
	afx_msg void OnBridgeEditor();	
	afx_msg void OnUpdateBridgeEditor(CCmdUI* pCmdUI);
	afx_msg void OnTrenchEditor();	
	afx_msg void OnUpdateTrenchEditor(CCmdUI* pCmdUI);

	afx_msg void OnMissionEditor();	
	afx_msg void OnUpdateMissionEditor(CCmdUI* pCmdUI);
	afx_msg void OnChapterEditor();	
	afx_msg void OnUpdateChapterEditor(CCmdUI* pCmdUI);
	afx_msg void OnCampaignEditor();	
	afx_msg void OnUpdateCampaignEditor(CCmdUI* pCmdUI);
	afx_msg void OnMedalEditor();	
	afx_msg void OnUpdateMedalEditor(CCmdUI* pCmdUI);

	afx_msg void OnTerrainEditor();	
	afx_msg void OnUpdateTerrainEditor(CCmdUI* pCmdUI);
	afx_msg void OnRoadEditor();	
	afx_msg void OnUpdateRoadEditor(CCmdUI* pCmdUI);
	afx_msg void OnRiverEditor();	
	afx_msg void OnUpdateRiverEditor(CCmdUI* pCmdUI);
	afx_msg void OnSaveObjects();
	DECLARE_MESSAGE_MAP()
};

#endif		//__PARENT_FRAME_H__

