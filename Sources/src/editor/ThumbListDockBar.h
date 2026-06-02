#ifndef __THUMB_LIST_DOCK_BAR_H__
#define __THUMB_LIST_DOCK_BAR_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ThumbList.h"
class CTemplateTree;


class CThumbListDockBar : public SECControlBar
{
public:
	CThumbListDockBar();
	
public:
private:
	CThumbList m_wndThumbList;
	
public:
	
	
public:
	virtual ~CThumbListDockBar();
	CThumbList *GetThumbListCtrl() { return &m_wndThumbList; }

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};



#endif		// __THUMB_LIST_DOCK_BAR_H__
