#ifndef __IM_NEXT_CHAPTER_H__
#define __IM_NEXT_CHAPTER_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceNextChapter : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceNextChapter );
	NInput::CCommandRegistrator commandMsgs;
	bool bAllowStay;											// allow stay in surrent chapter
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceNextChapter();
	CInterfaceNextChapter();
	
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICNextChapter : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_NEXT_CHAPTER>
{
	OBJECT_NORMAL_METHODS( CICNextChapter );
	
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICNextChapter() {  }
};
#endif		//__IM_NEXT_CHAPTER_H__
