#ifndef __UI_OBJECTIVE_MAP_H__
#define __UI_OBJECTIVE_MAP_H__
#include "UIBasic.h"
class CUIObjMap : public CMultipleWindow
{
	CPtr<IGFXTexture> pMapTexture;
	std::vector<SGFXLVertex> vertices;
	std::vector<WORD> indices;
public:
	CUIObjMap() {}
	virtual ~CUIObjMap() {}
	
	virtual void STDCALL Init();
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual void STDCALL SetMapTexture( IGFXTexture *p ) { pMapTexture = p; }
};
class CUIObjMapBridge : public IUIObjMap, public CUIObjMap
{
	OBJECT_NORMAL_METHODS( CUIObjMapBridge );
public:
	DECLARE_SUPER( CUIObjMap );
	DEFINE_UICONTAINER_BRIDGE;
	virtual void STDCALL Init() { CSuper::Init(); }
	virtual void STDCALL SetMapTexture( IGFXTexture *p ) { CSuper::SetMapTexture( p ); }
};

#endif		//__UI_OBJECTIVE_MAP_H__
