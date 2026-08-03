#ifndef __TRENCHFRM_H__
#define __TRENCHFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../Main/rpgstats.h"

#include "ParentFrame.h"
#include "TreeDockWnd.h"

interface IObjVisObj;
struct SProp;

class CTrenchFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CTrenchFrame)
public:
	CTrenchFrame();
	virtual ~CTrenchFrame();

public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );

	void RemoveTrenchIndex( int nIndex );
	int GetFreeTrenchIndex();
	
protected:

private:
	std::list<int> freeIndexes;			//��� �������� ������������� ��������

protected:
	virtual void SpecificInit();														//��� ������������� ���������� ������ ����� �������� ������� ��� �������� ������
	virtual void SpecificClearBeforeBatchMode();
	void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif		//__TRENCHFRM_H__
