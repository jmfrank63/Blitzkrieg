#ifndef __GFXOBJECTFACTORY_H__
#define __GFXOBJECTFACTORY_H__
#pragma ONCE
#include "..\Misc\BasicObjectFactory.h"
class CGFXObjectFactory : public CBasicObjectFactory
{
public:
	CGFXObjectFactory();
};
extern CGFXObjectFactory theGFXObjectFactory;
class CGFXModuleChecker : public IModuleChecker
{
public:
	int STDCALL CheckFunctionality() const;
	void STDCALL SetModuleFunctionalityLimits() const;
};
#endif // __GFXOBJECTFACTORY_H__
