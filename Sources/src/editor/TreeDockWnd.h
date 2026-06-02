#if !defined(AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)
#define AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_

using namespace std;
#include "..\\Common\\StingrayCompat.h"
#include "ETreeCtrl.h"

class CPropView;
class CKeyFrameDockWnd;


class CTreeDockWnd : public SECControlBar
{
public:
	CTreeDockWnd();
	virtual ~CTreeDockWnd();
	
public:

public:
  SECTreeCtrl* GetActiveTree();

	void SaveTrees( IStructureSaver *pSS );
	void LoadTrees( IStructureSaver *pSS );
	void SaveTrees( IDataTree *pDT );
	void LoadTrees( IDataTree *pDT );
	
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	CETreeCtrl* AddTree( const char *szName, int nId, bool bViz );
	CETreeCtrl* GetTreeWithIndex( int nIndex );
	void DeleteTree( int nIndex );
	void SetPropView( CPropView *pView ) { pPropView = pView; }
	void SetKeyFrameDockWnd( CKeyFrameDockWnd *pWnd ) { pKeyFrameDockWnd = pWnd; }
	
protected:
	CETreeCtrl *pTree;
	CPropView *pPropView;
	CKeyFrameDockWnd *pKeyFrameDockWnd;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	DECLARE_MESSAGE_MAP()
};

inline SECTreeCtrl* CTreeDockWnd::GetActiveTree()
{
  return dynamic_cast<SECTreeCtrl*>( pTree );
}



#endif // !defined(AFX_TREEDOCKWND_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)


