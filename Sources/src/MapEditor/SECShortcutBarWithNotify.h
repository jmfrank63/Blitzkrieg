#if !defined(AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_)
#define AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SECShortcutBarWithNotify.h : header file
//
#include "..\\Common\\StingrayCompat.h"

/////////////////////////////////////////////////////////////////////////////
// SECShortcutBarWithNotify window
const UINT NM_CHANGEPAGE = 1200;     

struct SShortcutBarNotify_NMHDR
{
		NMHDR			hdr;
		int				nIndex;
};

class SECShortcutBarWithNotify : public SECShortcutBar
{
	virtual BOOL OnChangeBar( int iIndex );

// Construction
public:
	SECShortcutBarWithNotify();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(SECShortcutBarWithNotify)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~SECShortcutBarWithNotify();

	// Generated message map functions
protected:
	//{{AFX_MSG(SECShortcutBarWithNotify)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_)

