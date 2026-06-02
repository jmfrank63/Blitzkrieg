
#if !defined(AFX_WINDOWMSBUTTON_H__CBE5FDBF_1AE1_48E8_8464_1E6952CB132E__INCLUDED_)
#define AFX_WINDOWMSBUTTON_H__CBE5FDBF_1AE1_48E8_8464_1E6952CB132E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Interface.h"
#include "WindowMultiBkg.h"

class CWindowMSButton : 
	public CWindowMultiBkg, 
	public IButton  
{
public:
	CWindowMSButton();
	virtual ~CWindowMSButton();

};

#endif // !defined(AFX_WINDOWMSBUTTON_H__CBE5FDBF_1AE1_48E8_8464_1E6952CB132E__INCLUDED_)
