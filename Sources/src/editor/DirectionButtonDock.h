#ifndef __DIRECTION_BUTTON_DOCK_H__
#define __DIRECTION_BUTTON_DOCK_H__

#include "DirectionButton.h"

class CDirectionButtonDockBar : public SECControlBar
{
public:
	CDirectionButtonDockBar();
	virtual ~CDirectionButtonDockBar();
	
public:
private:
	CDirectionButton m_DirectionButton;
	
public:
	
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
public:
	float GetAngle();
	void SetAngle( float fVal );
	int GetIntAngle();
	void SetIntAngle( int nVal );

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};



#endif		// __DIRECTION_BUTTON_DOCK_H__
