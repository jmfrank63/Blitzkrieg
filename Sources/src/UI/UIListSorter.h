#ifndef __UI_LIST_SORTER_H__
#define __UI_LIST_SORTER_H__
#include "UI.h"
enum
{
	UI_SORT_BASE_VALUE		= 0x10001150,
	UI_TEXT_SORTER				= UI_SORT_BASE_VALUE + 1,
	UI_NUMBER_SORTER			= UI_SORT_BASE_VALUE + 2,
	UI_USER_DATA_SORTER		= UI_SORT_BASE_VALUE + 3,

	UI_LIST_ROW						= UI_SORT_BASE_VALUE + 4,
	UI_LIST_HEADER				= UI_SORT_BASE_VALUE + 5,
};
class CUIListTextSorter : public IUIListSorter
{
	OBJECT_NORMAL_METHODS( CUIListTextSorter );
public:
	virtual bool STDCALL operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const;
};
class CUIListNumberSorter : public IUIListSorter
{
	OBJECT_NORMAL_METHODS( CUIListNumberSorter );
public:
	virtual bool STDCALL operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const;
};
class CUIListUserDataSorter : public IUIListSorter
{
	OBJECT_NORMAL_METHODS( CUIListUserDataSorter );
public:
	virtual bool STDCALL operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const;
};
#endif		//__UI_LIST_SORTER_H__
