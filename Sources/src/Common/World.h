#ifndef __WORLD_H__
#define __WORLD_H__
#pragma ONCE
enum
{
	WCB_YOU_WIN		= 0x00300001,
	WCB_YOU_LOOSE	= 0x00300002,
	WCB_DRAW			= 0x00300003,

	WCB_FORCE_DWORD = 0x7fffffff
};
enum ESeason
{
	SEASON_SUMMER = 0,
	SEASON_WINTER = 1,
	SEASON_AFRIKA = 2
};
interface IWorld : public IRefCount
{
	virtual void STDCALL SetSeason( int nSeason ) = 0;
	virtual void STDCALL Init( interface ISingleton *pSingleton ) = 0;
	virtual void STDCALL Clear() = 0;
	virtual void STDCALL Update( const NTimer::STime &currTime ) = 0;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg ) = 0;
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) = 0;
	
	virtual void STDCALL GetAviationCircles( interface IUIMiniMap * pMinimap, const NTimer::STime curTime ) = 0;
};
interface IWorldClient : public IWorld
{
	virtual void STDCALL Start() = 0;
	virtual void STDCALL Select( const CVec2 &vPos ) = 0;
	virtual int STDCALL Select( interface IVisObj **objects, int nNumObjects ) = 0;
	virtual void STDCALL ResetSelection( interface IVisObj *pObj = 0 ) = 0;
	virtual void STDCALL PreSelect( interface IVisObj **objects, int nNumObjects ) = 0;
	virtual void STDCALL ResetPreSelection( interface IVisObj *pObj = 0 ) = 0;
	virtual void STDCALL MoveObject( interface IVisObj *pObj, const CVec3 &vPos ) = 0;
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, interface IUIElement *pElement ) = 0;
	virtual void STDCALL BeginAction( const SGameMessage &msg ) {  }
	virtual void STDCALL DoAction( const SGameMessage &msg ) = 0;
};
#endif // __WORLD_H__
