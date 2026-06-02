#ifndef __MINEFRM_H__
#define __MINEFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Main\rpgstats.h"
#include "ParentFrame.h"
#include "MineTreeItem.h"


class CMineFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CMineFrame)
public:
	CMineFrame();
	virtual ~CMineFrame();

public:

public:

protected:

private:

protected:
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SMineRPGStats &rpgStats, CTreeItem *pRootItem );
	void GetRPGStats( const SMineRPGStats &rpgStats, CTreeItem *pRootItem );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif		//__MINEFRM_H__
