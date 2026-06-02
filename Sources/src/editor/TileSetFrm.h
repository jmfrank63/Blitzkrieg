
#ifndef __TILESETFRM_H__
#define __TILESETFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ParentFrame.h"
#include "TreeDockWnd.h"
#include "TileTreeItem.h"
#include "ThumbList.h"

class CTileSetTreeRootItem;
class CTileSetFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CTileSetFrame)
public:
	friend class CTileSetTreeRootItem;
	CTileSetFrame();
	virtual ~CTileSetFrame();

public:

public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	
	void ViewSizeChanged();
	void UpdateThumbWindows() { m_wndAllDirThumbItems.Invalidate(); m_wndSelectedThumbItems.Invalidate(); }

	void SelectItemInSelectedThumbList( DWORD dwData );
	void DeleteFrameInSelectedList( DWORD dwData );

	void TerrainsDirChanged();
	void SetActiveTerrainItem( CTileSetTerrainPropsItem *pActiveTerrainItem );

	void CrossetsDirChanged();
	void SetActiveCrossetItem( CCrossetTilesItem *pActiveCrossetItem );

	void SwitchToEditCrossetsMode( bool bMode );

	void RemoveTerrainIndex( int nIndex );
	int GetFreeTerrainIndex();
	void RemoveCrossetIndex( int nIndex );
	int GetFreeCrossetIndex();

protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

private:
	CTileSetTerrainsItem *m_pTerrainsItem;
	CTileSetTerrainPropsItem *m_pActiveTerrainItem;
	CCrossetsItem *m_pCrossetsItem;
	CCrossetTilesItem *m_pActiveCrossetItem;

	CThumbList m_wndAllDirThumbItems;
	CThumbList m_wndSelectedThumbItems;

	bool bEditCrossets;

	std::list<int> freeTerrainIndexes;			//для хранения незаполненных индексов terrain
	std::list<int> freeCrossetIndexes;			//для хранения незаполненных индексов crosset

protected:
	void ClickOnThumbList( int nID );
	void DoubleClickOnThumbList( int nID );
	void DeleteFrameInTree( int nID );

	virtual void SpecificInit();														//для инициализации внутренних данных после загрузки проекта или создании нового
	virtual void SpecificClearBeforeBatchMode();
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );

	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );

	void InitFreeTerrainIndexes( CTreeItem *pRootItem );
	void InitFreeCrossetIndexes( CTreeItem *pRootItem );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnImportTerrains();
	afx_msg void OnUpdateImportTerrains(CCmdUI* pCmdUI);
	afx_msg void OnImportCrossets();
	afx_msg void OnUpdateImportCrossets(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};



#endif		//__TILESETFRM_H__
