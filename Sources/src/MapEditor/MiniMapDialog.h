#if !defined(AFX_MINIMAPDIALOG_H__05E80CC8_2205_405F_95B2_5E364B62161E__INCLUDED_)
#define AFX_MINIMAPDIALOG_H__05E80CC8_2205_405F_95B2_5E364B62161E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "..\AILogic\AITypes.h"
#include "..\Formats\fmtMap.h"
#include "..\GFX\GFX.h"
#include "..\Scene\Terrain.h"
#include "..\Image\Image.h"
#include "..\Scene\Scene.h"

#include "MiniMapTypes.h"
#include "resource.h"

class CMiniMapDialog : public CDialog
{
public:
	CMiniMapDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_MINIMAP };
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
	afx_msg void OnMinimapClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSizing(UINT fwSide, LPRECT pRect);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnMove(int x, int y);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnMinimapGame();
	afx_msg void OnMinimapUpdate();
	afx_msg void OnMinimapGameCreate();
	void UpdateMinimap( bool bUpdateUnits );
	void UpdateMinimapEditor( bool bUpdateUnits );

protected:
	const static int vID[];
	
	int GetMinimumXDimension() { return 0; }
	int GetMinimumYDimension() { return 0; }

	CScreenFrame gameScreenFrame;
	CMiniMapTerrain miniMapTerrain;
	CFireRangeAreas fireRangeAreas;
	CMiniMapTerrainGrid miniMapTerrainGrid;
	CUnitsSelection unitsSelection;

protected:
	void DrawGameScreenFrame( CDC *pDC );
	void DrawMiniMapTerrain( CDC *pDC );
	void DrawFireRangeAreas( CDC *pDC );
	void DrawUnitsSelection( CDC *pDC );
	void DrawMiniMapTerrainGrid( CDC *pDC );

	void GetMiniMapRect( CRect* pRect, bool onlyDimensions = false );
	void InvalidateMiniMapRect();

public:
	class CTemplateEditorFrame* m_frame;
	bool m_isCreating;

	void UpdateScreenFrame();
	void UpdateControls();
};
#endif // !defined(AFX_MINIMAPDIALOG_H__05E80CC8_2205_405F_95B2_5E364B62161E__INCLUDED_)
