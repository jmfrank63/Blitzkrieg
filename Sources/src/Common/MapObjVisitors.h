#ifndef __MAPOBJVISITORS_H__
#define __MAPOBJVISITORS_H__
#pragma ONCE
#include "MapObject.h"
struct SGetVisObjesVisitor : public IMapObjVisitor
{
	struct SVisObjDesc
	{
		CPtr<IVisObj> pVisObj;
		EObjGameType eGameType;
		EObjVisType eVisType;
		bool bOutbound;
	};
	std::list<SVisObjDesc> objects;
	void Clear() { objects.clear(); }
	virtual void STDCALL VisitSprite( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false )
	{
		SVisObjDesc desc;
		desc.pVisObj = pVO;
		desc.eGameType = eGameType;
		desc.eVisType = eVisType;
		desc.bOutbound = bOutbound;
		objects.push_back( desc );
	}
	virtual void STDCALL VisitMesh( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false )
	{
		SVisObjDesc desc;
		desc.pVisObj = pVO;
		desc.eGameType = eGameType;
		desc.eVisType = eVisType;
		desc.bOutbound = bOutbound;
		objects.push_back( desc );
	}
	virtual void STDCALL VisitEffect( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false )
	{
		SVisObjDesc desc;
		desc.pVisObj = pVO;
		desc.eGameType = eGameType;
		desc.eVisType = eVisType;
		desc.bOutbound = bOutbound;
		objects.push_back( desc );
	}
};
#endif // __MAPOBJVISITORS_H__
