
#if !defined(AFX_SINTERFACECONSTS_H__87DEE1F5_7500_4647_9FA9_5216B18EE536__INCLUDED_)
#define AFX_SINTERFACECONSTS_H__87DEE1F5_7500_4647_9FA9_5216B18EE536__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class SInterfaceConsts  
{
public:
	SInterfaceConsts();
	virtual ~SInterfaceConsts();

	static const NTimer::STime SCROLLER_ANIM_TIME()
	{
		return 100;
	}
	static const NTimer::STime SCROLLER_ANIM_TIME_1()
	{
		return 1000;
	}
};

#endif // !defined(AFX_SINTERFACECONSTS_H__87DEE1F5_7500_4647_9FA9_5216B18EE536__INCLUDED_)
