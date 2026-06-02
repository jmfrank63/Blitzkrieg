#ifndef __KEY_FRAME_DOCK_H__
#define __KEY_FRAME_DOCK_H__

#include "KeyFrame.h"


class CKeyFrameDockWnd : public SECControlBar
{
public:
	CKeyFrameDockWnd();
	virtual ~CKeyFrameDockWnd();
	
public:
	
public:
	
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
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

protected:
	
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyframeDeleteNode();
	afx_msg void OnKeyframeResetAll();
	DECLARE_MESSAGE_MAP()
};



#endif	//__KEY_FRAME_DOCK_H__

