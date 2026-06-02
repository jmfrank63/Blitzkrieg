#ifndef __CHAPTER_H__
#define __CHAPTER_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "InterMission.h"
#include "iMission.h"
struct SChapterStats;
class CAfterMissionPopups;
class CInterfaceChapter : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceChapter );
	DECLARE_SERIALIZE;
	NInput::CCommandRegistrator commandMsgs;
	CPtr<CAfterMissionPopups> pPopups;
	int nSelected;												// current selected mission
	
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceChapter();
	CInterfaceChapter() : CInterfaceInterMission( "InterMission" ), nSelected( -1 ) {  }
	void IncrementChapterVisited();
	
	std::vector<int> missionIndeces;
	void SetMissionDescription( const int nSelected );
	void InitWindow();

public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();

	static const SChapterStats *ReadChapterStats();
};
class CICChapter : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_CHAPTER>
{
	OBJECT_NORMAL_METHODS( CICChapter );

	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICChapter() {  }
};
#endif	//__CHAPTER_H__
