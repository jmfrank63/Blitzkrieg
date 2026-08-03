#ifndef __TREEITEMFACTORY_H__
#define __TREEITEMFACTORY_H__

#include "..\Misc\BasicObjectFactory.h"

class CTreeItemObjectFactory : public CBasicObjectFactory
{
public:
	CTreeItemObjectFactory();
};
IObjectFactory* STDCALL GetTreeItemObjectFactory();

#endif		//__TREEITEMFACTORY_H__
