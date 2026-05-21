#ifndef __KEY_FRAME_DOCK_H__
#define __KEY_FRAME_DOCK_H__

#include "KeyFrame.h"

/////////////////////////////////////////////////////////////////////////////
// CKeyFrameDockWnd window

class CKeyFrameDockWnd : public SECControlBar
{
	// Construction
public:
	CKeyFrameDockWnd();
	virtual ~CKeyFrameDockWnd();
	
	// Attributes
public:
	
	// Operations
public:
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKeyFrameDockWnd)
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL
	
	// Implementation
public:
	void ClearControl();
	void SetDimentions( float fMinX, float fMaxX, float fStepX, float fMinY, float fMaxY, float fStepY );
	void SetFramesList( CFramesList frames );
	void ResetNodes();
	void SetXResizeMode( bool bResizeMode );
	void SetActiveKeyFrameTreeItem( CKeyFrameTreeItem *pItem );
	
private:
	CKeyFrameEditor *m_pKeyFramer;
	CPtr<CKeyFrameTreeItem> pActiveKeyItem;

	// Generated message map functions
protected:
	
	//{{AFX_MSG(CKeyFrameDockWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyframeDeleteNode();
	afx_msg void OnKeyframeResetAll();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif	//__KEY_FRAME_DOCK_H__

