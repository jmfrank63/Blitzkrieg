#ifndef __MISSIONFRM_H__
#define __MISSIONFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Main\gamestats.h"
#include "ImageFrm.h"
class CMissionObjectivePropsItem;

class CMissionFrame : public CImageFrame
{
	DECLARE_DYNCREATE(CMissionFrame)
public:
	CMissionFrame();
	virtual ~CMissionFrame();
	
	void SetActiveObjective( CMissionObjectivePropsItem *pObjective );

public:
	
public:
	
protected:
	
private:
	std::string szPrefix;		//эта переменная используется для передачи параметра в функцию FillRpgStats, чтобы не было необходимости изменять интерфейс
	CMissionObjectivePropsItem *pActiveObjective;
	
protected:
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SMissionStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName );
	void GetRPGStats( const SMissionStats &rpgStats, CTreeItem *pRootItem );
	
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	virtual void SpecificInit();
	virtual void SpecificClearBeforeBatchMode();
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnGenerateImage();
	afx_msg void OnUpdateGenerateImage(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};



#endif		//__MISSIONFRM_H__
