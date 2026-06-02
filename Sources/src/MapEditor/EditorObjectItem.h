#if !defined(AFX_EDITOROBJECTITEM_H__F7764657_2FB9_46D6_BAA2_A6FDCAE7EB50__INCLUDED_)
#define AFX_EDITOROBJECTITEM_H__F7764657_2FB9_46D6_BAA2_A6FDCAE7EB50__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "..\Formats\fmtMap.h"
#include "..\Common\MapObject.h"


struct SEditorObjectItem
{
	SEditorObjectItem() : bScenarioUnit( false ), frameIndex( -1 ), pLink( 0 ), nLogicGroupId( 0 ), notActivatedLogicGroupId( 0 ) {}
	virtual ~SEditorObjectItem() {}
	SGDBObjectDesc	sDesc;
	int							nPlayer;
	int							nScriptID;
	bool						bScenarioUnit;
	int							frameIndex;	
	SMapObject			*pObj;
	CPtr<SMapObject> pLink;

	int							nLogicGroupId;
	int							notActivatedLogicGroupId;
	virtual  IManipulator* GetManipulator() { return 0; }
};
#endif // !defined(AFX_EDITOROBJECTITEM_H__F7764657_2FB9_46D6_BAA2_A6FDCAE7EB50__INCLUDED_)
