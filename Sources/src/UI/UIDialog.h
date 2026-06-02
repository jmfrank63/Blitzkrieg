#ifndef __UI_DIALOG_H__
#define __UI_DIALOG_H__
#include "UIBasic.h"
class CUIDialog : public CMultipleWindow
{
public:
	CUIDialog() {}
};
class CUIDialogBridge : public IUIDialog, public CUIDialog
{
	OBJECT_NORMAL_METHODS( CUIDialogBridge );
	DECLARE_SUPER( CUIDialog );
	DEFINE_UICONTAINER_BRIDGE;

	virtual IUIElement * STDCALL Duplicate()
	{ 
		CUIDialogBridge * pWnd = new CUIDialogBridge;
		CopyInternals( pWnd );
		return pWnd;
	}
};
#endif		//__UI_DIALOG_H__
