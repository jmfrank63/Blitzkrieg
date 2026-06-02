
#if !defined(AFX_PLANEFLIGHTEXECUTOR_H__C3415880_828B_4197_B70E_467D6502407D__INCLUDED_)
#define AFX_PLANEFLIGHTEXECUTOR_H__C3415880_828B_4197_B70E_467D6502407D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IPlane.h"

class CPlaneFlightExecutor : public IPlane
{
public:
	CPlaneFlightExecutor();
	virtual ~CPlaneFlightExecutor();

	void Segment();
};

#endif // !defined(AFX_PLANEFLIGHTEXECUTOR_H__C3415880_828B_4197_B70E_467D6502407D__INCLUDED_)
