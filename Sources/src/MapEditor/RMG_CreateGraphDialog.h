#if !defined(__RMG_Create_Graph_Dialog__)
#define __RMG_Create_Graph_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"
#include "../RandomMapGen/RMG_Types.h"

int CALLBACK CG_GraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

class CRMGCreateGraphDialog : public CResizeDialog
{
	friend int CALLBACK CG_GraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );

public:
	CRMGCreateGraphDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CREATE_GRAPH };
	CStatic	m_NodesMessageTop;
	CStatic	m_NodesMessageBottom;
	CListCtrl	m_GraphsList;
	CSliderCtrl	m_NodesSlider;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnAddGraphButton();
	afx_msg void OnDeleteGraphButton();
	afx_msg void OnGraphPropertiesButton();
	afx_msg void OnItemchangedGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddGraphMenu();
	afx_msg void OnDeleteGraphMenu();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnColumnclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void OnSaveButton();
	afx_msg void OnPropertiesMenu();
	afx_msg void OnDeleteMenu();
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveas();
	afx_msg void OnFileExit();
	afx_msg void OnCheckGraphsButton();
	DECLARE_MESSAGE_MAP()

protected:
	enum EInputState
	{
		STATE_NONE			= 0,
		STATE_ADD				= 1,
		STATE_MOVE			= 2,	
		STATE_RESIZE		= 3,
		STATE_ADD_LINK	= 4,
	};

	struct SGraphCheckInfo
	{
		static const DWORD SIDE_MINX;
		static const DWORD SIDE_MINY;
		static const DWORD SIDE_MAXX;
		static const DWORD SIDE_MAXY;
		static const DWORD SIDE_CENTER;

		bool bSomeChecked;
		std::vector<int> linksIndices;
		int nNodeIndex;
		DWORD dwSide;

		SGraphCheckInfo() : bSomeChecked( false ), nNodeIndex( -1 ), dwSide( 0 ) {}
	};

	const static int vID[];
	CRMGraphsHashMap graphs;
	bool isChanged;
	bool bCreateControls;
	int nSortColumn;
	std::vector<bool> bGraphSortParam;
	CRect oldClipRect;
	CTRect<int> mousePoints;
	EInputState inputState;
	int nPatchesCount;
	HCURSOR defaultCursor;
	SGraphCheckInfo lastGraphCheckInfo;
	CTRect<int> lastNodePlace;

	virtual int GetMinimumXDimension() { return	500; }
	virtual int GetMinimumYDimension() { return 500; }
	virtual std::string GetXMLOptionsLabel() { return "CRMGCreateGraphDialog"; }
	virtual bool GetDrawGripper() { return true; }

	bool LoadGraphsList();
	bool SaveGraphsList();
	
	bool LoadGraphToControls();
	bool SaveGraphFromControls();
	void  SetGraphItem( int nItem, SRMGraph &rGraph );
	
	bool IsValidGraphEntered();

	void UpdateControls();
	
	void CreateControls();
	void ClearControls();
	
	void GetNodesPlaceRect( CRect* pRect, bool onlyDimensions = false );
	CTPoint<int> GetTilePoint( int x, int y );
	CTRect<int> GetTileRect( int minx, int miny, int maxx, int maxy );
	CTRect<int> GetScreenRect( int minx, int miny, int maxx, int maxy );
	inline CTPoint<int> GetTilePoint( const CTPoint<int> &rPoint )
	{
		return GetTilePoint( rPoint.x, rPoint.y );
	}
	inline CTRect<int> GetTileRect( const CTRect<int> &rRect )
	{
		return GetTileRect( rRect.minx, rRect.miny, rRect.maxx, rRect.maxy );
	}
	inline CTRect<int> GetScreenRect( const CTRect<int> &rRect )
	{
		return GetScreenRect( rRect.minx, rRect.miny, rRect.maxx, rRect.maxy );
	}
	bool CheckForGraphElement( const SRMGraph &rGraph, const CTPoint<int> rMousePoint, SGraphCheckInfo *pGraphCheckInfo );

public:
};
#endif // !defined(__RMG_Create_Graph_Dialog__)
