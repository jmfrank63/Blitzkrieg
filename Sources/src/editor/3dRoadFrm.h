#ifndef __3DROAD_FRAME_H__
#define __3DROAD_FRAME_H__

#include "../Formats/fmtmap.h"
#include "ParentFrame.h"
#include "TreeDockWnd.h"

class C3DRoadFrame : public CParentFrame
{
	DECLARE_DYNCREATE(C3DRoadFrame)
public:
	C3DRoadFrame();
	virtual ~C3DRoadFrame();
	
public:
	
public:
	void UpdateRoadView();

	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	
	
private:
	bool bMapLoaded;
	bool bWireFrameMode;

protected:
	virtual void SpecificInit();
	virtual void SpecificClearBeforeBatchMode();
		
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SVectorStripeObjectDesc &desc, CTreeItem *pRootItem );
	void GetRPGStats( const SVectorStripeObjectDesc &desc, CTreeItem *pRootItem );
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );

	void LoadRoadMap();
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSwitchWireframeMode();
	afx_msg void OnUpdateSwitchWireframeMode(CCmdUI* pCmdUI);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	DECLARE_MESSAGE_MAP()
};

#endif		//__3DROAD_FRAME_H__

