#if !defined(AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)
#define AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_

using namespace std;
#include "..\\Common\\StingrayCompat.h"
#include "ETreeCtrl.h"

class CPropView;
class CKeyFrameDockWnd;

/////////////////////////////////////////////////////////////////////////////
// CTreeDockWnd window

class CTreeDockWnd : public SECControlBar
{
// Construction
public:
	CTreeDockWnd();
	virtual ~CTreeDockWnd();
	
// Attributes
public:

// Operations
public:
  SECTreeCtrl* GetActiveTree();

	void SaveTrees( IStructureSaver *pSS );
	void LoadTrees( IStructureSaver *pSS );
	void SaveTrees( IDataTree *pDT );
	void LoadTrees( IDataTree *pDT );
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeDockWnd)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	CETreeCtrl* AddTree( const char *szName, int nId, bool bViz );
	CETreeCtrl* GetTreeWithIndex( int nIndex );
	void DeleteTree( int nIndex );
	void SetPropView( CPropView *pView ) { pPropView = pView; }
	void SetKeyFrameDockWnd( CKeyFrameDockWnd *pWnd ) { pKeyFrameDockWnd = pWnd; }
//	void SetTemplateDialog( CKeyFrameDockWnd *pWnd ) { pKeyFrameDockWnd = pWnd; }
	
	// Generated message map functions
protected:
	CETreeCtrl *pTree;
	CPropView *pPropView;
	CKeyFrameDockWnd *pKeyFrameDockWnd;

	//{{AFX_MSG(CTreeDockWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline SECTreeCtrl* CTreeDockWnd::GetActiveTree()
{
  return dynamic_cast<SECTreeCtrl*>( pTree );
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)


