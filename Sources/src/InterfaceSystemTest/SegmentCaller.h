
#if !defined(AFX_SEGMENTCALLER_H__8EE96FEB_D4DB_4EAC_9B05_73713A683C65__INCLUDED_)
#define AFX_SEGMENTCALLER_H__8EE96FEB_D4DB_4EAC_9B05_73713A683C65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Interface.h"

class CSegmentCaller  
{
	std::list< CPtr<CWindow> > segmentObjs;

public:
	CSegmentCaller();
	virtual ~CSegmentCaller();

	
	void RegisterToSegment( CWindow * pObj )
	{
		UnregisterToSegment( pObj );
		segmentObjs.push_back( pObj );
	}

	void UnregisterToSegment( CWindow * pObj )
	{
		segmentObjs.remove( pObj );
	}
};

#endif // !defined(AFX_SEGMENTCALLER_H__8EE96FEB_D4DB_4EAC_9B05_73713A683C65__INCLUDED_)
