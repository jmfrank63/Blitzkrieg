#ifndef __SELECTORVISITORS_H__
#define __SELECTORVISITORS_H__
#pragma ONCE
#include "../Common/MapObject.h"
class CGetActionsSelectiorVisitor : public ISelectorVisitor
{
	const IMapObj::EActionsType eActions;
	CUserActions *pActions;
public:
	CGetActionsSelectiorVisitor( const IMapObj::EActionsType _eActions, CUserActions *_pActions )
		: eActions( _eActions ), pActions( _pActions ) {  }
	virtual void STDCALL VisitMapObject( struct SMapObject *pMO ) const
	{
		CUserActions actions;
		pMO->GetActions( &actions, eActions );
		*pActions |= actions;
	}
};
class CGetActionsExceptSelectiorVisitor : public ISelectorVisitor
{
	const IMapObj::EActionsType eActions;
	CUserActions *pActions;
	const IMapObj *pException;
public:
	CGetActionsExceptSelectiorVisitor( const IMapObj::EActionsType _eActions, CUserActions *_pActions, const IMapObj *_pException )
		: eActions( _eActions ), pActions( _pActions ), pException( _pException ) {  }
	virtual void STDCALL VisitMapObject( struct SMapObject *pMO ) const
	{
		if ( pMO != pException ) 
		{
			CUserActions actions;
			pMO->GetActions( pActions, eActions );
			*pActions |= actions;
		}
	}
};
class CCollectObjectsSelectiorVisitor : public ISelectorVisitor
{
	mutable CMapObjectsList objects;
public:
	virtual void STDCALL VisitMapObject( struct SMapObject *pMO ) const
	{
		objects.push_back( pMO );
	}
	CMapObjectsList& GetObjects() { return objects; }
};
#endif // __SELECTORVISITORS_H__
