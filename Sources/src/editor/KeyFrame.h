#if !defined(AFX_KEYFRAME_H__B132E21D_2C65_44F9_A0C5_8F120B411033__INCLUDED_)
#define AFX_KEYFRAME_H__B132E21D_2C65_44F9_A0C5_8F120B411033__INCLUDED_

#include "TreeItem.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// KeyFrame.h : header file
//

const UINT WM_KEY_FRAME_RCLICK	= WM_USER + 35;		// ���� ������ ������� � ������� ��������
const UINT WM_KEY_FRAME_UPDATE	= WM_USER + 36;		// ����� �������� ������ framesList

using std::list;
using std::pair;

/////////////////////////////////////////////////////////////////////////////
// CKeyFrameEditor window

class CKeyFrameEditor : public CWnd
{
// Construction
public:
	enum EActiveMode
	{
		E_FREE_MODE,
		E_DRAG_MODE,
	};

	CKeyFrameEditor();
	virtual ~CKeyFrameEditor();
	
// Attributes
public:

// Operations
public:
	void SetDimentions( float fMinX, float fMaxX, float fStepX, float fMinY, float fMaxY, float fStepY );
	void SetFramesList( CFramesList frames ) { framesList = frames; }
	CFramesList GetFramesList() { return framesList; }
	void ClearAll() { m_fMinValX = m_fMaxValX = m_fMinValY = m_fMaxValY = 0; framesList.clear(); m_BottomScroll.ShowScrollBar( 0 ); m_LeftScroll.ShowScrollBar( 0 ); Invalidate(); }
	void ResetNodes();
	void SetXResizeMode( bool bResize );
	void DeleteActiveNode();
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKeyFrameEditor)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	//}}AFX_VIRTUAL

// Implementation
protected:
//	void GetScreenCoord( float x, float y, float *screenX, float *screenY );
	void SetHDimention( float fMin, float fMax );
	void SetVDimention( float fMin, float fMax );
	void GetVisibleX( int *nMin, int *nMax );			//�������� ����� ��������� ������� �� ������, � �������� ScrollBar
	void GetVisibleY( int *nMin, int *nMax );
	void GetScreenByValue( float fValX, float fValY, float *pScreenX, float *pScreenY );
	void GetValueByScreen( int x, int y, float *pValX, float *pValY );
//	CFramesList::iterator GetNearNodeIndex( int x, int y, int *pIndex );
		
private:
	CScrollBar m_BottomScroll, m_LeftScroll;
	float m_fStepX, m_fStepY;
	float m_fMinValX, m_fMaxValX;
	float m_fMinValY, m_fMaxValY;
	int m_nDragIndex;
	int m_mode;
	int m_nHighNodeIndex;
	POINT m_beginDrag;						//��� ���������� ����� ��������������
	CFramesList framesList;
	bool m_bResizeMode;
	float m_XS;
	int m_YS;

	// Generated message map functions
protected:
	//{{AFX_MSG(CKeyFrameEditor)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnPaint();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyframeZoominx();
	afx_msg void OnKeyframeZoominy();
	afx_msg void OnKeyframeZoomoutx();
	afx_msg void OnKeyframeZoomouty();
	afx_msg void OnUpdateKeyframeZoominx(CCmdUI* pCmdUI);
	afx_msg void OnUpdateKeyframeZoominy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateKeyframeZoomoutx(CCmdUI* pCmdUI);
	afx_msg void OnUpdateKeyframeZoomouty(CCmdUI* pCmdUI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KEYFRAME_H__B132E21D_2C65_44F9_A0C5_8F120B411033__INCLUDED_)

