
#if !defined(AFX_MULTIPLEWINDOW_H__E9951EA6_F10E_49BA_91A3_61408A6EC2D7__INCLUDED_)
#define AFX_MULTIPLEWINDOW_H__E9951EA6_F10E_49BA_91A3_61408A6EC2D7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SimpleWindow.h"

class CMultipleWindow : public CSimpleWindow  
{

	
	void ApplyEffect( IUIEffector *pEffect, IUIElement *pElement )
	{
		pEffect->SetElement( pElement );
		theSegmentCaller.RegisterToSegment( pEffect );
	}

public:
	CMultipleWindow();
	virtual ~CMultipleWindow();

};

#endif // !defined(AFX_MULTIPLEWINDOW_H__E9951EA6_F10E_49BA_91A3_61408A6EC2D7__INCLUDED_)
