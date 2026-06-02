#ifndef __IM_CUSTOM_CHAPTER_H__
#define __IM_CUSTOM_CHAPTER_H__
#pragma ONCE
#include "CustomList.h"
class CInterfaceCustomChapter : public CInterfaceCustomList
{
	OBJECT_NORMAL_METHODS( CInterfaceCustomChapter );
	virtual ~CInterfaceCustomChapter();
protected:
	CInterfaceCustomChapter() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:
	virtual bool STDCALL Init();
};
class CICCustomChapter : public CInterfaceCommandBase<CInterfaceCustomChapter, MISSION_INTERFACE_CUSTOM_CHAPTER>
{
	OBJECT_NORMAL_METHODS( CICCustomChapter );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceCustomChapter *pInterface ) { pML->PushInterface( pInterface ); }
	CICCustomChapter() {  }
};
#endif		//__IM_CUSTOM_CHAPTER_H__
