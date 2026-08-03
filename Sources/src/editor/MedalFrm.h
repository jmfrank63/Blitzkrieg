#ifndef __MEDALFRM_H__
#define __MEDALFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ParentFrame.h"
#include "..\Main\GameStats.h"
#include "ImageFrm.h"

class CMedalFrame : public CImageFrame
{
	DECLARE_DYNCREATE(CMedalFrame)
public:
	CMedalFrame();
	virtual ~CMedalFrame();
	virtual void UpdatePropView( CTreeItem *pTreeItem );				
	
public:
	
public:
	
protected:
	
private:
	std::string szPrefix;		//эта переменная используется для передачи параметра в функцию FillRpgStats, чтобы не было необходимости изменять интерфейс
	
protected:
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SMedalStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName );
	void GetRPGStats( const SMedalStats &rpgStats, CTreeItem *pRootItem );
	
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif		//__MEDALFRM_H__
