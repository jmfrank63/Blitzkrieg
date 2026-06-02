#ifndef __INTERFACEAFTERMISSIONPOPUPS_H__
#define __INTERFACEAFTERMISSIONPOPUPS_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CAfterMissionPopups : public IRefCount
{
	OBJECT_NORMAL_METHODS( CAfterMissionPopups );
	DECLARE_SERIALIZE;
	NInput::CCommandRegistrator commandMsgs;

	bool bTutorialWindow;
	bool bCheckReplay;
	int nMedalIterator;			//текущий индекс медали
	bool bMainScreenShown;
	bool bUpgradesShown;
	bool bNewUnitsShown;
	bool bNextChapterShown;
	bool bPlayerRankShown;
	bool bLastFullScreen;		//переменная для работы шторок, если true то последний раз отображался full screen interface
	bool bUnitsPerformanceShown;

	bool bNeedFinish;
	int nCommandID;
	std::string szCommandParams;

	bool bShowBlack;		// blacken bottom screen

	void FinishInterface( const int _nCommandID, const char *pszParam );
public:
	CAfterMissionPopups() : nMedalIterator( 0 ), bMainScreenShown( 0 ),
		bUpgradesShown( 0 ), bNewUnitsShown( 0 ), bNextChapterShown( 0 ), bPlayerRankShown( 0 ),
		bUnitsPerformanceShown( 0 ),
		bCheckReplay( 0 ), bLastFullScreen( 0 ), bNeedFinish( false ), nCommandID ( 0 ), bShowBlack( false ) {  }

	bool IsNeedFinish() const { return bNeedFinish; }
	bool IsNeedBlack() const { return bShowBlack; }
	int GetFinishCommandID() const { return nCommandID; }
	const char * GetFinishCommandParams() const { return (szCommandParams.empty() ? 0 : szCommandParams.c_str() ); }

	virtual void STDCALL OnGetFocus( bool bFocus );
};

#endif // __INTERFACEAFTERMISSIONPOPUPS_H__
