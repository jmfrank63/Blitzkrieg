#ifndef __GLOBALVARS2_H__
#define __GLOBALVARS2_H__
#include "..\Misc\VarSystem.h"
interface IGlobalVarsIterator : public IVarIterator
{
};
interface IGlobalVars2 : public IVarSystem
{
	virtual IGlobalVarsIterator* STDCALL CreateIterator() const = 0;
	virtual int STDCALL operator&( IDataTree &ss ) = 0;
	virtual int STDCALL operator&( IStructureSaver &ss ) = 0;
};
#endif // __GLOBALVARS2_H__