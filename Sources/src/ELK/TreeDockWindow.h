#if !defined(__ELK_TREE_DOCK_WINDOW__)
#define __ELK_TREE_DOCK_WINDOW__

#define IDC_EMBEDDED_CONTROL 200

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTreeDockWindow : public SECControlBar
{
protected:
	//{{AFX_MSG(CTreeDockWindow)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CWnd *pwndMainFrame;
public:
	SECTreeCtrl wndTree;

	CTreeDockWindow();
	virtual ~CTreeDockWindow();

	//{{AFX_VIRTUAL(CTreeDockWindow)
	//}}AFX_VIRTUAL

	void SetMainFrameWindow( CWnd *_pwndMainFrame );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__ELK_TREE_DOCK_WINDOW__)

