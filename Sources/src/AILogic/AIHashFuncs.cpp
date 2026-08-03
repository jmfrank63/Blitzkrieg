#include "StdAfx.h"

#include "UpdatableObject.h"
#include "AIHashFuncs.h"
#include "AIUnit.h"
int SUpdatableObjectObjHash::operator()( const CObj<IUpdatableObj> &a ) const
{ 
	return a.GetPtr()->GetUniqueId(); 
}
int SUnitObjHash::operator()( const CObj<CAIUnit> &a ) const
{
	return a.GetPtr()->GetUniqueId(); 
}
