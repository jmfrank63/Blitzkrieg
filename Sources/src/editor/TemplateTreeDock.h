#ifndef __TEMPLATE_TREE_DOCK_H__
#define __TEMPLATE_TREE_DOCK_H__

using namespace std;
#include "TemplateTree.h"

class CTemplateTreeCtrl;


class CTemplateTreeDockBar : public SECControlBar
{
public:
	CTemplateTreeDockBar();
	virtual ~CTemplateTreeDockBar();
	
public:
	
public:
	void SaveTemplateTree( IDataTree *pDT );
	void LoadTemplateTree( IDataTree *pDT );
	
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
public:
	
protected:
	CTemplateTreeCtrl *pTemplateTree;
	
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif // __TEMPLATE_TREE_DOCK_H__
