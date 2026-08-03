#if !defined(AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_)
#define AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "..\\Common\\LegacyUiCompat.h"

const UINT NM_CHANGEPAGE = 1200;     

struct SShortcutBarNotify_NMHDR
{
		NMHDR			hdr;
		int				nIndex;
};

class SECShortcutBarWithNotify : public SECShortcutBar
{
	virtual BOOL OnChangeBar( int iIndex );

public:
	SECShortcutBarWithNotify();

public:

public:


public:
	virtual ~SECShortcutBarWithNotify();

protected:
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_SECSHORTCUTBARWITHNOTIFY_H__5D2065ED_45E5_487B_9FF2_4165020DECF6__INCLUDED_)

