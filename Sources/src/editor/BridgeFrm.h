#ifndef __BRIDGEFRM_H__
#define __BRIDGEFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../Main/rpgstats.h"
#include "GridFrm.h"
#include "TreeDockWnd.h"
#include "BridgeTreeItem.h"
#include "DirectionButton.h"
#include "SpriteCompose.h"

interface IObjVisObj;
class CBridgeFirePointPropsItem;
class CBridgeDirExplosionPropsItem;
class CBridgeSmokePropsItem;

class CBridgeFrame : public CGridFrame
{
	DECLARE_DYNCREATE(CBridgeFrame)
public:
	CBridgeFrame();
	virtual ~CBridgeFrame();

public:

	struct SFirePoint
	{
		CBridgeFirePointPropsItem *pFirePoint;
		CPtr<IObjVisObj> pSprite;
		CPtr<IObjVisObj> pHLine;
		
		float fDirection;		//���� ����������� ������ ��������
	};
	
	enum EActiveMode
	{
		E_UNKNOWN_MODE,
		E_FIRE_POINT,
		E_SMOKE_POINT,
	};
	
	enum EActiveSubMode
	{
		E_SUB_MOVE,
		E_SUB_HOR,
		E_SUB_DIR,
	};
	
public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	virtual void Init( IGFX *_pGFX );

	void SaveMyData( CBridgePartsItem *pSpansItem, CTreeAccessor saver );
	void LoadMyData( CBridgePartsItem *pSpansItem, CTreeAccessor saver );

	void SetTranseparenceCombo( CComboBox *pCombo ) { m_pTransparenceCombo = pCombo; }
	void SetActivePartsItem( CBridgePartsItem *pItem, const char *pszProjectFileName );
	void SetActivePartPropsItem( CBridgePartPropsItem *pItem );
	void UpdatePartPropsItem();
	void SetBridgeType( bool bHorizontal );

	void RemoveBridgeIndex( int nActiveDamage, int nIndex );
	int GetFreeBridgeIndex( int nActiveDamage );

protected:

private:
	int m_mode;
	CVec2 objShift, zeroShift;
	CVec3 m_zeroPos;

	CBridgePartsItem *pActiveSpansItem;
	CBridgePartPropsItem *pActiveSpanPropsItem;

	CPtr<IGFXTexture> pKrestTexture;
	bool m_bHorizontal;
	bool bEditSpansEnabled;
	
	enum EDrawMode
	{
		E_DRAW_SPANS = 0,
		E_DRAW_SPAN_PROPS = 1,
	};
	int m_drawMode;
	CVec3 vSpriteCommonPos;					//��� ������� ����� ���� ����������
	
	string szSourceDir;
	string szDestDir;
	CComboBox *m_pTransparenceCombo;
	int m_transValue;
	
	std::list<int> freeSpanIndexes[3];			//��� �������� ������������� �������� span'��
	
	CPtr<IGFXIndices> pLineIndices;
	CPtr<IGFXVertices> pLineVertices;
	float m_fx1, m_fx2, m_fy1, m_fy2;
	CVec3 vBeginPos, vEndPos;				//������� ��������� � �������� ������ �����
	float m_fBack, m_fFront;		//���������� �� ����������� ����� �� ������� ������ �����
	
	EActiveMode eActiveMode;
	EActiveSubMode eActiveSubMode;
	
	typedef list<SFirePoint> CListOfFirePoints;
	CListOfFirePoints firePoints;
	SFirePoint *pActiveFirePoint;				//� ���� ���������� ����� ��������� ��������, ����� ������������ SetActiveFirePoint()
	
	CBridgeSmokePropsItem *pActiveSmokePoint;
	
	CPtr<IGFXVertices> pConeVertices;
	CPtr<IGFXVertices> pHorizontalPointVertices;
	CPtr<IGFXVertices> pFireDirectionVertices;
	
	float m_fMinY;
	float m_fMaxY;
	float m_fX;
	POINT m_beginDrag;
	
public:
	void SetActiveMode( EActiveMode mode );
	void DeleteFirePoint( CTreeItem *pFire );
	void SelectFirePoint( CTreeItem *pFire );
	void SetActiveFirePoint( SFirePoint *pFirePoint );
	void SetFireDirection( float fVal );
	CTreeItem *GetActiveFirePointItem();
	void AddOrSelectFirePoint( const POINT &point );
	void SetHorFirePoint( const POINT &point );
	void SetFirePointAngle( const POINT &point );
	void MoveFirePoint( const POINT &point );
	void GenerateFirePoints();
	
	void AddOrSelectSmokePoint( const POINT &point );
	void DeleteSmokePoint();
	void SelectSmokePoint( CBridgeSmokePropsItem *pSmokePoint );
	void ComputeSmokeLines();
	void MoveSmokePoint( const POINT &point );
	void SetHorSmokePoint( const POINT &point );
	void SetSmokePointAngle( const POINT &point );
	void GenerateSmokePoints();
	void CreateSmokeSprites( CBridgeSmokePropsItem *pSmokePoint );
	
protected:
	void AddSpriteAndShadow( const char *pszProjectFileName, CSpritesPackBuilder::CPackParameters *pPacks, CSpritesPackBuilder::CPackParameters *pShadowPacks, CBridgePartPropsItem *pProps, const CVec3 &vTemp );
	float GetPointOnLine( float fx );
	void LoadSpriteItem( CBridgePartPropsItem *pItem, const char *pszName, const char *pszProjectFileName );
	void SetZeroCoordinate( POINT point );
	void CreateKrest();
	virtual void SpecificInit();														//��� ������������� ���������� ������ ����� �������� ������� ��� �������� ������
	virtual void SpecificClearBeforeBatchMode();
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
	
	virtual void SaveFrameOwnData( IDataTree *pDT );				//��� ���������� ����������� ������ �������
	virtual void LoadFrameOwnData( IDataTree *pDT );				//��� ��������
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );

	void SavePointsInformation( SBridgeRPGStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName );
	void FillRPGStats( SBridgeRPGStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName );
	void GetRPGStats( const SBridgeRPGStats &rpgStats, CTreeItem *pRootItem );
	
	virtual bool LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem );
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
	bool ComputeMaxAndMinPositions( const CVec3 &vPos3 );
	bool GetLineIntersection( const CVec2 &vPos2, float fx1, float fy1, float fx2, float fy2, float *y );
	void ComputeAngleLines();
	
	void ComputeFireDirectionLines();
	bool IsTileLocked( const POINT &pt );

	void SaveSegmentInformation( SBridgeRPGStats::SSegmentRPGStats &segment, CBridgePartsItem *pBridgeSpansItem, CSpritesPackBuilder::CPackParameters *pPacks, const CVec3 &vPapa, int &nUMinX, int &nUMaxX, int &nUMinY, int &nUMaxY );
	void DrawLockedTiles( IGFX *pGFX );
		
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDrawGrid();
	afx_msg void OnUpdateDrawGrid(CCmdUI* pCmdUI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnSetFocusTranseparence();
	afx_msg void OnChangeTranseparence();
	afx_msg void OnSetZeroButton();
	afx_msg void OnUpdateSetZeroButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDrawTransparence(CCmdUI* pCmdUI);
	afx_msg void OnDrawPass();
	afx_msg void OnUpdateDrawPass(CCmdUI* pCmdUI);

	afx_msg void OnSetHorizontalShoot();
	afx_msg void OnUpdateSetHorizontalShoot(CCmdUI* pCmdUI);
	afx_msg void OnSetShootAngle();
	afx_msg void OnUpdateSetShootAngle(CCmdUI* pCmdUI);
	afx_msg void OnSetFirePoint();
	afx_msg void OnUpdateSetFirePoint(CCmdUI* pCmdUI);
	afx_msg void OnMovePoint();
	afx_msg void OnUpdateMovePoint(CCmdUI* pCmdUI);
	afx_msg void OnSetSmokePoint();
	afx_msg void OnUpdateSetSmokePoint(CCmdUI* pCmdUI);
	afx_msg void OnGeneratePoints();
	afx_msg void OnUpdateGeneratePoints(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};



#endif		//__BRIDGEFRM_H__
