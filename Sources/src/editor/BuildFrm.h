#ifndef __BUILDFRM_H__
#define __BUILDFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GridFrm.h"

interface IObjVisObj;
class CBuildingSlotPropsItem;
class CBuildingFirePointPropsItem;
class CBuildingDirExplosionPropsItem;
class CBuildingSmokePropsItem;

class CBuildingFrame : public CGridFrame
{
	DECLARE_DYNCREATE(CBuildingFrame)
public:
	CBuildingFrame();
	virtual ~CBuildingFrame();
	
public:
	typedef vector< CPtr<IGFXVertices> > CVectorOfVertices;
	
	struct SShootPoint
	{
		CBuildingSlotPropsItem *pSlot;
		CPtr<IObjVisObj> pSprite;
		CPtr<IObjVisObj> pHLine;
		
		float fDirection;		//угол направления конуса стрельбы
		float fAngle;				//полный угол конуса стрельбы
	};
	
	struct SFirePoint
	{
		CBuildingFirePointPropsItem *pFirePoint;
		CPtr<IObjVisObj> pSprite;
		CPtr<IObjVisObj> pHLine;
		
		float fDirection;		//угол направления конуса стрельбы
	};
	
	enum EActiveMode
	{
		E_UNKNOWN_MODE,
		E_SHOOT_SLOT,
		E_FIRE_POINT,
		E_DIR_EXPLOSION,
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
	
	void SetActiveGraphicPropsItem( CTreeItem *pGraphicProps );
	void UpdateActiveSprite();
	void SetTranseparenceCombo( CComboBox *pCombo ) { m_pTransparenceCombo = pCombo; }
	
	void SetActiveMode( EActiveMode mode );
	void DeleteShootPoint( CTreeItem *pShoot );
	void SelectShootPoint( CTreeItem *pShoot );
	void SetActiveShootPoint( SShootPoint *pShootPoint );
	void SetConeDirection( float fVal );
	void SetConeAngle( float fVal );
	CTreeItem *GetActiveShootPointItem();
	void AddOrSelectShootPoint( const POINT &point );
	void SetHorShootPoint( const POINT &point );
	void SetShootPointAngle( const POINT &point );
	void MoveShootPoint( const POINT &point );
	
	void DeleteFirePoint( CTreeItem *pFire );
	void SelectFirePoint( CTreeItem *pFire );
	void SetActiveFirePoint( SFirePoint *pFirePoint );
	void SetFireDirection( float fVal );
	CTreeItem *GetActiveFirePointItem();
	void AddOrSelectFirePoint( const POINT &point );
	void SetHorFirePoint( const POINT &point );
	void SetFirePointAngle( const POINT &point );
	void MoveFirePoint( const POINT &point );
	
	void AddOrSelectSmokePoint( const POINT &point );
	void DeleteSmokePoint();
	void SelectSmokePoint( CBuildingSmokePropsItem *pSmokePoint );
	void ComputeSmokeLines();
	void MoveSmokePoint( const POINT &point );
	void SetHorSmokePoint( const POINT &point );
	void SetSmokePointAngle( const POINT &point );
	void GenerateSmokePoints();
	void CreateSmokeSprites( CBuildingSmokePropsItem *pSmokePoint );

	void SelectDirExpPoint( CBuildingDirExplosionPropsItem *pDirExpPoint );
	void ComputeDirExpDirectionLines();
	void MoveDirExpPoint( const POINT &point );
	void SetHorDirExpPoint( const POINT &point );
	void SetDirExpPointAngle( const POINT &point );
	void GenerateDirExpPoints();
	
	protected:

private:
	int m_mode;
	CVec2 objShift, zeroShift;
	CVec3 m_zeroPos;
	CVec3 m_SpriteLoadPos;
	
	CPtr<IObjVisObj> pSprite;
	CPtr<IGFXTexture> pKrestTexture;
	
	CListOfTiles lockedTiles;
	CListOfTiles entrances;
	CListOfTiles transeparences;

	EActiveMode eActiveMode;
	EActiveSubMode eActiveSubMode;

	typedef list<SShootPoint> CListOfShootPoints;
	CListOfShootPoints shootPoints;
	SShootPoint *pActiveShootPoint;			//к этой переменной плохо ссылаться напрямую, лучше использовать SetActiveShootPoint()

	typedef list<SFirePoint> CListOfFirePoints;
	CListOfFirePoints firePoints;
	SFirePoint *pActiveFirePoint;				//к этой переменной плохо ссылаться напрямую, лучше использовать SetActiveFirePoint()

	CBuildingSmokePropsItem *pActiveSmokePoint;

	CBuildingDirExplosionPropsItem *pActiveDirExpPoint;

	CPtr<IGFXVertices> pConeVertices;
	CPtr<IGFXVertices> pHorizontalPointVertices;
	CPtr<IGFXVertices> pFireDirectionVertices;

	float m_fMinY;
	float m_fMaxY;
	float m_fX;
	POINT m_beginDrag;
	CComboBox *m_pTransparenceCombo;
	int m_transValue;

	CTreeItem *pActiveGraphicProps;
	
protected:
	void LoadSprite( const char *pszSpriteFullName );

	void CreateKrest();
	virtual void SpecificInit();														//для инициализации внутренних данных после загрузки проекта или создании нового
	virtual void SpecificClearBeforeBatchMode();
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
	
	virtual void SaveFrameOwnData( IDataTree *pDT );				//для сохранения собственных данных проекта
	virtual void LoadFrameOwnData( IDataTree *pDT );				//для загрузки
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );

	virtual bool LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem );
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
	bool ComputeMaxAndMinPositions( const CVec3 &vPos3 );
	bool GetLineIntersection( const CVec2 &vPos2, float fx1, float fy1, float fx2, float fy2, float *y );
	void ComputeAngleLines();

	void ComputeFireDirectionLines();

	bool IsTileLocked( const POINT &pt );

	void DrawLockedTiles( IGFX *pGFX );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDrawGrid();
	afx_msg void OnMoveObject();
	afx_msg void OnUpdateMoveObject(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDrawGrid(CCmdUI* pCmdUI);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSetZeroButton();
	afx_msg void OnUpdateSetZeroButton(CCmdUI* pCmdUI);
	afx_msg void OnSetEntranceButton();
	afx_msg void OnUpdateSetEntranceButton(CCmdUI* pCmdUI);
	afx_msg void OnSetShootPoint();
	afx_msg void OnUpdateSetShootPoint(CCmdUI* pCmdUI);
	afx_msg void OnSetShootAngle();
	afx_msg void OnUpdateSetShootAngle(CCmdUI* pCmdUI);
	afx_msg void OnSetHorizontalShoot();
	afx_msg void OnUpdateSetHorizontalShoot(CCmdUI* pCmdUI);
	afx_msg void OnSetFocusTranseparence();
	afx_msg void OnChangeTranseparence();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnUpdateDrawTransparence(CCmdUI* pCmdUI);
	afx_msg void OnSetFirePoint();
	afx_msg void OnUpdateSetFirePoint(CCmdUI* pCmdUI);
	afx_msg void OnMovePoint();
	afx_msg void OnUpdateMovePoint(CCmdUI* pCmdUI);
	afx_msg void OnSetDirectionExplosion();
	afx_msg void OnUpdateSetDirectionExplosion(CCmdUI* pCmdUI);
	afx_msg void OnSetSmokePoint();
	afx_msg void OnUpdateSetSmokePoint(CCmdUI* pCmdUI);
	afx_msg void OnGeneratePoints();
	afx_msg void OnUpdateGeneratePoints(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};



#endif		//__BUILDFRM_H__
