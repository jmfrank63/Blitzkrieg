#if !defined(AFX_MiniMapBAR_H__C74720A7_7908_45A3_8223_D32542D40385__INCLUDED_)
#define AFX_MiniMapBAR_H__C74720A7_7908_45A3_8223_D32542D40385__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TemplateEditorFrame1.h"
#include "MiniMapDialog.h"
class CMiniMapBar : public SECControlBar
{
public:
	CMiniMapBar();
	virtual ~CMiniMapBar();

	class CTemplateEditorFrame *pwndMainFrame;
	CMiniMapDialog wndMiniMapDialog;
	void SetMainFrameWindow( class CTemplateEditorFrame *_pwndMainFrame );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(AFX_MiniMapBAR_H__C74720A7_7908_45A3_8223_D32542D40385__INCLUDED_)
