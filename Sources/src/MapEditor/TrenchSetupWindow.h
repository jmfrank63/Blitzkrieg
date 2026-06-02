#if !defined(__Tabs__VO_Entrenchments_Dialog__)
#define __Tabs__VO_Entrenchments_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"

class CTrenchSetupWindow : public CResizeDialog
{
public:
	CTrenchSetupWindow(CWnd* pParent = NULL);

	enum { IDD = IDD_TAB_VO_ENTRENCHMENTS };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(__Tabs__VO_Entrenchments_Dialog__)
