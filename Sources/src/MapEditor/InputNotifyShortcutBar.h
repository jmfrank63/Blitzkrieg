#if !defined(__INPUT_NOTIFY_SHORTCUT_BAR__)
#define __INPUT_NOTIFY_SHORTCUT_BAR__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
// migrated via StingrayCompat.h



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00 ( 61849 )
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInputNotifyShortcutBar : public SECShortcutBar
{
	friend class CInputControlBar;

	std::vector<CWnd*> inputTabWindows;

protected:
	//{{AFX_MSG(CInputNotifyShortcutBar)
	afx_msg LRESULT OnNotify3DTabChangePage( WPARAM wParam, LPARAM lParam );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	enum NOTIFY_MESSAGES
	{
		NM_CHANGE_PAGE = 1200,
	};

	struct SNotifyStruct : public NMHDR
	{
		int nShortcutIndex;
		int nTabIndex;
	};

	~CInputNotifyShortcutBar()
	{
		for ( int nInputTabWindowIndex = 0; nInputTabWindowIndex < inputTabWindows.size(); ++nInputTabWindowIndex )
		{
			if ( inputTabWindows[nInputTabWindowIndex] )
			{
				delete ( inputTabWindows[nInputTabWindowIndex] );
				inputTabWindows[nInputTabWindowIndex] = 0;
			}
		}
	}

	virtual BOOL OnChangeBar( int nShortcutIndex );

	template<class TINPUTTABWINDOW>
	TINPUTTABWINDOW* AddInputTabWindow( TINPUTTABWINDOW* pDummyInputTabWindow ) 
	{
		TINPUTTABWINDOW *pNewInputTabWindow = pDummyInputTabWindow;
		if ( !pNewInputTabWindow )
		{
			pNewInputTabWindow = new TINPUTTABWINDOW();
		}
		inputTabWindows.push_back( pNewInputTabWindow );
		return pNewInputTabWindow;
	}

	//{{AFX_VIRTUAL(CInputNotifyShortcutBar)
	//}}AFX_VIRTUAL
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__SEC_NOTIFY_SHORTCUT_BAR__)



