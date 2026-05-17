#if !defined(AFX_TEMPLATEEDITORFRAME1_H__FA5612CB_1305_4F95_959D_94DB91746A51__INCLUDED_)
#define AFX_TEMPLATEEDITORFRAME1_H__FA5612CB_1305_4F95_959D_94DB91746A51__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TemplateEditorFrame1.h : header file
//

#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"

#include <SECWB.H>
#include "..\GFX\GFX.h"
#include "..\Formats\fmtMap.h"
#include "..\AILogic\AILogic.h"
#include "IUndoRedoCmd.h"
#include "..\Common\WorldBase.h"
#include "InputState.h"
#include "InputMultiState.h"
#include "SEditorMApObject.h" 
#include "EditorObjectItem.h"


/////////////////////////////////////////////////////////////////////////////
// CTemplateEditorFrame window
class CTabTileEditDialog;
class CMapEditorBarWnd;
class CPropertieDialog; 
struct IVisObj; 
interface IImage;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "UnitCreation.h"
#include "AIStartCommand.h"
#include "ReservePosition.h"
#include "MapSoundInfo.h"
#include "MODCollector.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SReinforcementCheckBoxesInfo
{
	std::unordered_map< int, int > groupsCheckBoxes; 
	bool GetCheckState( int nId )
	{
		if( groupsCheckBoxes.find( nId ) != groupsCheckBoxes.end() )
		{
			return groupsCheckBoxes[ nId ] != -1;
		}
		return false;
	}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "ReinforcementGroupCheckBoxes", &groupsCheckBoxes );
		return 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMapEditorOptions
{
public:

	std::string szGameParameters;
	bool bSaveAsBZM;

	CMapEditorOptions()	: bSaveAsBZM( true ) {}
	
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		
		saver.Add( "GameParameters", &szGameParameters );
		saver.Add( "SaveAsBZM", &bSaveAsBZM );

		return 0;
	}

	void Modify();
};

class CTemplateEditorFrame : public   SECWorksheet, public CWorldBase
{ 
	friend class CInputControlBar;
	friend class CTileDrawingState;
	friend class CRoadDrawState;
	friend class CObjectPlacerState;
	friend class CDrawShadeState;
	friend class CMapToolState;
	friend class CRandomMapGeneratorDialog;
	friend class CMainFrame;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ����� ����
public:
	enum INPUT_STATES
	{
		STATE_TERRAIN								= 0,
		STATE_SIMPLE_OBJECTS				= 1,
		STATE_VECTOR_OBJECTS				= 2,
		STATE_TOOLS									= 3,
		STATE_GROUPS								= 4,
		STATE_AI_GENERAL						= 5, 
		STATE_COUNT									= 6,
	};

	enum INPUT_STATES_TERRAIN
	{
		STATE_TERRAIN_TILES					= 0,
		STATE_TERRAIN_ALTITUDES			= 1,
		STATE_TERRAIN_FIELDS				= 2,
		STATE_TERRAIN_COUNT					= 3,
	};

	enum INPUT_STATES_SIMPLE_OBJECTS
	{
		STATE_SO_OBJECTS						= 0,
		STATE_SO_FENCES							= 1,
		STATE_SO_BRIDGES						= 2,
		STATE_SO_COUNT							=	3,
	};

	enum INPUT_STATES_VECTOR_OBJECTS
	{
		STATE_VO_ENTRENCHMENTS			= 0,
		STATE_VO_ROADS3D						= 1,
		STATE_VO_RIVERS							= 2,
		STATE_VO_COUNT							= 3,
	};

	enum USER_MESSAGES
	{
		UM_CHANGE_SHORTCUT_BAR_PAGE	= WM_USER + 1,
	};

	static const char* STATE_TAB_LABELS[STATE_COUNT];
	static const char* STATE_TERRAIN_TAB_LABELS[STATE_TERRAIN_COUNT];
	static const char* STATE_SO_TAB_LABELS[STATE_SO_COUNT];
	static const char* STATE_VO_TAB_LABELS[STATE_VO_COUNT];

	static const char RIVERS_3D_MAP_NAME[];
	static const char ROADS_3D_MAP_NAME[];

	CInputMultiState inputStates;

	TMutableMapSoundInfoVector mapSoundInfo;
	TMutableMapSoundInfoVector addedMapSoundInfo[3];
	std::vector<CPtr<IVisObj> > cameraVisObj;
	
	CMapInfo currentMapInfo;
	//std::string szSeasonFolderBackup;
	//std::string szChapterNameBackup;
	//int nMissionIndexBackup;

	CUCHelper ucHelper;
	CAISCHelper aiscHelper;
	CMSHelper msHelper;
	
	
	CArray2D<BYTE> storageCoverageTileArray;

	std::string m_szSaveFilePath;

	bool m_bNeedUpdateUnitHeights;
	bool bShowScene6;
	bool bShowScene7;
	bool bWireframe;
	bool bShowScene11;
	bool bShowScene13;
	bool bShowScene1;
	bool bShowScene2;
	bool bShowScene3;
	bool bShowScene4;
	bool bShowScene8;
	bool bShowScene0;
	bool bShowScene9;
	bool bShowAIPassability;
	bool bShowStorageCoverage;
	bool bNeedDrawUnitsSelection;
	bool bFireRangePressed;

	bool bMapModified;

	void UpdateObjectsZ( const CTRect<int> &rUpdateRect );
	void FillAddedMapSoundInfo();
	void RemoveAddedMapSoundInfo();
	
	std::vector<SEnumFilesInDataStorageParameter> enumFilesInDataStorageParameter;
	SEnumFolderStructureParameter enumFolderStructureParameter;
	void GetFilesInDataStorage();
	bool GetEnumFilesInDataStorage( const std::string &rszFolder, std::list<std::string> *pList );
	bool NeedSaveChanges();
	void CheckMap( bool bProgressIsVisible );
	void CreateMiniMap();
	void SetMapModified( bool bModified = true );
	void ShowStorageCoverage();

	
	CVec3 vScreenCenter;
	CVec3 GetScreenCenter();
	void NormalizeCamera( CVec3 *pCamera );
	
	CMODCollector modCollector;

	void ClearAllBeforeNewMOD();
	void LoadNewMOD( const std::string &rszNewMODFolder );
	void LoadAllAfterNewMOD();
	
	CMapEditorOptions mapEditorOptions;

	std::string SetNameForFlag( SMapObjectInfo *pFlagObjectInfo );
	int SetPlayerForFlag( SMapObjectInfo *pFlagObjectInfo );
	void ResetPlayersForFlags();
	bool NeedUpdateStorages();
// ************************************************************************************************************************ //
private:
	DECLARE_DYNCREATE(CTemplateEditorFrame)

	//-----------------------------------------------------
	std::string  m_scriptName;
	CMutableUnitCreationInfo m_unitCreationInfo;
	//-----------------------------------------------------

	//-----------------------------------------------------
	std::vector<SEntrenchmentInfo> m_entrenchments;	
	std::vector< std::vector< std::vector< IRefCount* > > > m_entrenchmentsAI;
	void CalculateTrenchFromAI();
	void CalculateTrenchToAI();
	//-----------------------------------------------------

	//-----------------------------------------------------
	std::vector<SScriptArea> m_scriptAreas;									// ���������� �������
	void CalculateAreasFromAI();
	void CalculateAreasToAI();
	//-----------------------------------------------------

	//----------------------------------------------------- // ��� ��� ������ 
		std::vector< CPtr<SBridgeSpanObject> >									m_tempSpans;
		std::vector< std::vector< CPtr<SBridgeSpanObject> > >		m_Spans;
	//-----------------------------------------------------

	std::map< std::string, int >						m_objectFor;	

		
	//=====================������ Default'��� ��� ���������==============================
	
	///SRiverInfo				 m_defaultRiver;
	//===================================================================================

	std::string szStartDirectory;

	CPtr<IGFX> pGFX; 
	
	CTabTileEditDialog *m_pTabTileEditDialog;
	CInputControlBar *m_mapEditorBarPtr;

	//SRoadsetDesc descrRoads;
	// ��� ����� ���������
	//std::map< int, std::vector< SMapObject * > > m_reinforcementGroups;  
	SReinforcementGroupInfo m_reinforcementGroup;
	SReinforcementCheckBoxesInfo m_reinforcementGroupCheckBoxes;
	
	TMutableAIStartCommandList m_startCommands;
	TMutableAIStartCommandList m_SelectedStartCommands;
	bool isStartCommandPropertyActive;
	std::vector<CVec3> m_PointForAIStartCommand;

	TMutableReservePositionList m_reservePositions;
	CMutableReservePosition m_CurentReservePosition;
	bool isReservePositionActive;
	//0 - pos
	//1 - artillery
	//2 - truck
	std::list<int> m_ReservePositionSequence;
	
	inline void AddArtilleryToCurrentReservePosition()
	{
		m_CurentReservePosition.pArtilleryObject = m_currentMovingObjectPtrAI;
		//0 - pos
		//1 - artillery
		//2 - truck
		if ( ( m_ReservePositionSequence.size() > 2 ) ||
				 ( ( !m_ReservePositionSequence.empty() ) && 
					 ( ( *m_ReservePositionSequence.begin() ) == 1 ) ) )
		{
			m_ReservePositionSequence.pop_front();
		}
		m_ReservePositionSequence.push_front( 1 );
	}
	
	inline void AddTruckToCurrentReservePosition()
	{
		m_CurentReservePosition.pTruckObject = m_currentMovingObjectPtrAI;
		//0 - pos
		//1 - artillery
		//2 - truck
		if ( ( m_ReservePositionSequence.size() > 2 ) ||
				 ( ( !m_ReservePositionSequence.empty() ) && 
					 ( ( *m_ReservePositionSequence.begin() ) == 2 ) ) )
		{
			m_ReservePositionSequence.pop_front();
		}
		m_ReservePositionSequence.push_front( 2 );
	}

	inline void AddPosToCurrentReservePosition( const CVec3 &v )
	{
		m_CurentReservePosition.vPos = CVec2( v.x, v.y );
		//0 - pos
		//1 - artillery
		//2 - truck
		if ( ( m_ReservePositionSequence.size() > 2 ) ||
				 ( ( !m_ReservePositionSequence.empty() ) && 
			     ( ( *m_ReservePositionSequence.begin() ) == 0 ) ) )
		{
			m_ReservePositionSequence.pop_front();
		}
		m_ReservePositionSequence.push_front( 0 );
	}
	
	void SaveReservePosition();
	
	std::map <int ,int> m_imageIndexObject;

	STilesetDesc descrTile;
	
	CPoint	  m_oldMovingPosition;

	int				m_tileX;
	int				m_tileY;

	bool			m_bGrid ;

	//std::vector< SRoadItem > m_roads;
	
	std::unordered_map< IVisObj*, SEditorObjectItem, SDefaultPtrHash > m_objects;  
	//----------- ����� �������������� �������� ����� ����������� -------------------
	CPtr<IVisObj> m_currentMovingObjectForPlacementPtr;	
	
	std:: vector<CPtr<IVisObj> > m_currentMovingObjectsForPlacementPtr;
	std::string						 m_currentMovingPasteGroupName;	// ����� �� Advanced clippboard'a ������ paste 
																												// �� ����� ����������� ����� ����������� 
																												// ��������.
	//-------------------------------------------------------------------------------
	std::vector< std::pair<IVisObj*, CVec2> > m_pickedObjects;

	std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash > m_objectsAI;  
	SMapObject *																	m_currentMovingObjectPtrAI;
	std::vector<SMapObject *>											m_currentMovingObjectsAI;
	std::map<SMapObject*, CVec2>									m_shiftsForMovingObjectsAI;
	std::map<IRefCount*, CVec2>									m_squadsShiftsForMovingObjectsAI;
	std::vector<SMapObject*>											m_currentForPasteObjectsAI;
	std::map<SMapObject*, CVec3>									m_shiftsForPasteObjectsAI;
	//SMapObject*																		m_currentObjectForPastePtrAI;	
	// ��� ���������������� ��������� �������
	std::vector<IVisObj*>	m_currentFences;
	
	bool				m_ifCanMultiSelect;
	bool				m_ifCanMovingMultiGroup;
 
	CVec2			m_currentObjectShift;
	int m_curPickNum;

	//void DrawLine( int x1, int y1, int x2,int y2);

	//std::stack< IUndoRedoCmd* > m_undoStack;

	CTPoint<int>		m_firstSelectPoint;
	CTPoint<int>		m_lastSelectPoint;

	bool ifFitToAI;
	
	LPTSTR m_cursorName;

// Construction 
public: 

	CTemplateEditorFrame();
	 
	CInputControlBar* GetMapEditorBar() { return m_mapEditorBarPtr; } 
//	CAdvancedClipboardBar* GetAdvancedClipboardBar() { return m_advancedClipBoardBar; } 

	int				m_brushDX; 
	int				m_brushDY;
 
	int				m_lastMouseTileX;
	int				m_lastMouseTileY;

	int				m_lastMouseX;
	int				m_lastMouseY;

	GPoint		m_firstPoint;
	GPoint		m_lastPoint;
 
	std::string m_currentMapName;
	CPropertieDialog *dlg;

	//---------------------------------------------------------------------------------------
	// ��� ���������� �������� ���������� ��������
	//CMiniMapDialog, CCreateForestDilaog
	CTRect<int> m_minimapDialogRect;
	//---------------------------------------------------------------------------------------
	
	SEditorObjectItem* GetEditorObjectItem( SMapObject* ptr )
	{
		if( m_objectsAI.find( ptr ) != m_objectsAI.end() )
		{
		  NI_ASSERT_T( ptr != 0,"No such SEditorObjectItem.");
			return m_objectsAI[ ptr ];  
		}
		else
		{
		  NI_ASSERT_T( false,"No such SEditorObjectItem.");
			return 0;
		}
	}


	//===================================================================================
  /**
	void UpdateRiver()
	{
			if( inputStates.GetActiveState() == STATE_VECTOR_OBJECTS )
			{
				inputStates.OnKeyDown( VK_RETURN, 1, 0, this );		
			}
			RedrawWindow();
	}
	SRiverInfo::SLayer GetDefaultRiverLayer();
	/**/
	//===================================================================================

// Attributes
public: 

// Operations 
public:
		
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTemplateEditorFrame)
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL


// Implementation
public: 
	//=============================== ������� ��� ������� =========================================
	void PopFromBuilding( SEditorObjectItem *obj ); // ������� ���� ���������� ���� ��� ���� 
	int  GetNumSoldiersInBuilding( SEditorObjectItem *obj ); // ������� ���������� � ���� ������� �����  
	int  GetSoldiersInBuilding( SEditorObjectItem *obj, std::vector<SEditorObjectItem*> &units );// ���������� ���� ���������� � obj

	void MoveObject( IRefCount *pAiObject, short x, short y, bool isFormation = false ); // ������� � ������ ������� unit' ��   
	//=============================================================================================
	
	void  FillMapInfoParamForObject( SMapObjectInfo &info, SMapObject* obj  );

	void ClearAllDataBeforeNewMap();

	void SaveMapEditorOptions();
	void LoadMapEditorOptions();
	
	SReinforcementGroupInfo& GetGroupInfo() { return m_reinforcementGroup; }
	void SetGroupInfo( SReinforcementGroupInfo &info ) { m_reinforcementGroup = info; }
	void CalculateReinforcementGroups( bool update = true ); //true  - m_reinforcementGroupCheckBoxes->list
																													 //false - list->m_reinforcementGroupCheckBoxes

	void ShowAIInfo()												{	ToggleAIInfo(); RedrawWindow(); }
	bool ifObjectExist( SMapObject * ptr )	{ return m_objectsAI.find(ptr) !=  m_objectsAI.end() ;}
	bool IfCashedFile( std::string name );
	//void AddRoad( SRoadItem &s );
	//void DeleteRoad( SRoadItem &road);
	void MoveObject( IVisObj *obj, CVec3 &pos, bool isFormation = false );
	void AddTileCmd(  std::vector<STileRedoCmdInfo> &inf, bool cmd = true );
	void RemoveObject( IVisObj* object);
	void RemoveObject(SMapObject *object);
	
	//������ �� ���������� ���������
	void RemoveObjectFromAIStartCommand( SMapObject *object );
	void AddObjectToAIStartCommand( SMapObject *object, bool isRemove = false );
	
	void ClearAIStartCommandFlags();
	void CreateSelectedAIStartCommandList( bool isRemove = false );

	void AddStartCommandRedLines( TMutableAIStartCommandList::iterator startCommandIterator );
	void RecalculateStartCommandRedLines( const CVec3& rPos );
	void DrawAIStartCommandRedLines();
	void DrawUnitsSelection();
	//

	//������ � reserve positions
	void RemoveObjectFromReservePositions( SMapObject *object );
	void DrawReservePositionRedLines();
	
	//temp == true ���� ���� ��������� � ������ undo / redo
	IVisObj* AddObject( const SGDBObjectDesc &desc, int p = 0 ,bool temp = false);
	SMapObject*	AddObjectByAI( SMapObjectInfo &info , int p = 0 ,bool temp = false, bool bScenarioUnit = false );
	//void CalculateRoads();
	void CalculateAreas();

	void ShowFrameWindows( int nCommand);
	virtual ~CTemplateEditorFrame();
	void Init( IGFX *_pGFX)  { pGFX = _pGFX; }
	void SetTabTileEditDialog( CTabTileEditDialog *_pTabTileEditDialog ) { m_pTabTileEditDialog = _pTabTileEditDialog;}
	void SetMapEditorBar( CInputControlBar *ptr ) { m_mapEditorBarPtr = ptr;}
	
	virtual void NewObjectAdded( SMapObject *pMO );
	
	void SetGrid( bool i) { m_bGrid  = i; }
	int  GetAnimationFrameIndex( struct IVisObj *pVisObj);
	
	void ShowFireRange( bool isShow );
	// Generated message map functions
protected:
	virtual void ResetSelection( SMapObject *pMO ) {};
protected:
	//{{AFX_MSG(CTemplateEditorFrame)
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnRButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDblClk( UINT nFlags, CPoint point );
	afx_msg void OnMButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnMButtonDblClk( UINT nFlags, CPoint point );
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnSetFocus(CWnd* pOldWnd); 
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnUpdateTileCoord(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveMap();
	afx_msg void OnFillArea();
	afx_msg void OnEditUnitCreationInfo();
	afx_msg void OnAddStartCommand();
	afx_msg void OnShowFireRange();
	afx_msg void OnShowScene0();
	afx_msg void OnShowScene1();
	afx_msg void OnShowScene2();
	afx_msg void OnShowScene3();
	afx_msg void OnShowScene4();
	afx_msg void OnShowScene6();
	afx_msg void OnShowScene7();
	afx_msg void OnShowScene8();
	afx_msg void OnShowScene9();
	afx_msg void OnShowScene10();
	afx_msg void OnShowScene11();
	afx_msg void OnShowScene13();
	afx_msg void OnShowScene12();
	afx_msg void OnShowScene14();
	afx_msg void OnReservePositions();
	afx_msg void OnUpdateReservePositions(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnObjectPaste();
	afx_msg void OnEditCopy();
	afx_msg void OnButtonFog();
	afx_msg void OnFileSave();
	afx_msg void OnEditUndo();
	afx_msg void OnToolsClearCash();
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnButtonAI();
	afx_msg void OnButtonfit();
	afx_msg void OnButtonShowLayers();
	afx_msg void OnCheckMap();
	afx_msg void OnFileOpen();
	afx_msg void OnFileCreateNewProject();
	afx_msg void OnButtonshowBoxes();
	afx_msg void OnOptions();
	afx_msg void OnButtonSetCameraPos();
	afx_msg void OnButtonDeleteArea();
	afx_msg void OnShowStorageCoverage();
	afx_msg void OnNewTemplate();
	afx_msg void OnNewContainer();
	afx_msg void OnNewGraph();
	afx_msg void OnNewField();
	afx_msg void OnUpdateEditUnitCreationInfo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateButtonupdate(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAddStartCommand(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStartCommandList(CCmdUI* pCmdUI);
	afx_msg void OnFileCreateRandomMission();
	afx_msg void OnUpdateShowScene14(CCmdUI* pCmdUI);
	afx_msg void OnUpdateButtonFit(CCmdUI* pCmdUI);
	afx_msg void OnUpdateButtonfillarea(CCmdUI* pCmdUI);
	afx_msg void OnUpdateButtonsetcamera(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptions(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene6(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene7(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene10(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene11(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene13(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene2(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene3(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene4(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene8(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene0(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene9(CCmdUI* pCmdUI);
	afx_msg void OnUpdateButtonai(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowStorageCoverage(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowScene12(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowFireRange(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnDiplomacy();
	afx_msg void OnUpdateDiplomacy(CCmdUI* pCmdUI);
	afx_msg void OnFilterComposer();
	afx_msg void OnUpdateFilterComposer(CCmdUI* pCmdUI);
	afx_msg void OnUpdateToolsDelr(CCmdUI* pCmdUI);
	afx_msg void OnStartCommandsList();
	afx_msg void OnToolsOptions();
	afx_msg void OnUpdateToolsOptions(CCmdUI* pCmdUI);
	afx_msg void OnUpdateToolsRunGame(CCmdUI* pCmdUI);
	afx_msg void OnToolsRunGame();
	afx_msg void OnFileSaveBzm();
	afx_msg void OnUpdateFileSaveBzm(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveXml();
	afx_msg void OnUpdateFileSaveXml(CCmdUI* pCmdUI);
	//}}AFX_MSG
public:
	afx_msg void OnButtonUpdate();
	DECLARE_MESSAGE_MAP() 
	
public:
	void OnFileLoadMap( const std::string &rszFileName );
	void OnFileNewMap();

public:
	void MakeCamera();
private:
	void Pick( CVec2 &point, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible = true,  bool IsMoving = false );
	void Pick( const CTRect<float> &rcRect, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible = true,  bool IsMoving = false );

	void SaveMap( std::string &name, bool isBinary = false );
	void FillGRect( GRect &r, std::vector< CTPoint<int> > &points  );
	void GetTileIndexBy2DPoint( int x, int y , int &xtile, int &ytile );
	SRoadItem ifRoadItemIntersect( int x, int y );
	//ERoadItemTypes GetTileType( SRoadItem &item, int x, int y );
	int GetCurrentDirection();
	//int GetCurrentRoadType();
};
/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEMPLATEEDITORFRAME1_H__FA5612CB_1305_4F95_959D_94DB91746A51__INCLUDED_)
