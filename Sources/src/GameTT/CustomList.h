#ifndef __CUSTOM_LIST_H__
#define __CUSTOM_LIST_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "BaseList.h"
class CInterfaceCustomList : public CInterfaceBaseList
{
	OBJECT_NORMAL_METHODS( CInterfaceCustomList );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual void FillListFromCurrentDir();
protected:
	std::string szCollectorName;

	CInterfaceCustomList() {}
	virtual ~CInterfaceCustomList();
	
public:
};
#endif // __CUSTOM_LIST_H__
