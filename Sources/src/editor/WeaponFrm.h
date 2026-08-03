
#ifndef __WEAPONFRM_H__
#define __WEAPONFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../Main/rpgstats.h"
#include "ParentFrame.h"

class CWeaponFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CWeaponFrame)
public:
	CWeaponFrame();
	virtual ~CWeaponFrame();

public:

public:
	virtual void GFXDraw();
	
protected:

private:

protected:
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SWeaponRPGStats &rpgStats, CTreeItem *pRootItem );
	void GetRPGStats( const SWeaponRPGStats &rpgStats, CTreeItem *pRootItem );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif		//__WEAPONFRM_H__
