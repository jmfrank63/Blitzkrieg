#pragma once

#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include <afxadv.h>
#include <afxtoolbar.h>
#include <afxdockablepane.h>
#include <shlobj.h>
#include <vector>
#include <map>

typedef INT_PTR SEC_INT;

#ifndef SEC_NO_NAMESPACE_USING
#define SEC_NO_NAMESPACE_USING 1
#endif

#ifndef SEC_TBBS_VCENTER
#define SEC_TBBS_VCENTER 0
#endif

#ifndef SEC_OBS_VERT
#define SEC_OBS_VERT 0
#endif

#ifndef SEC_OBS_ANIMATESCROLL
#define SEC_OBS_ANIMATESCROLL 0
#endif

#ifndef seCollapseItem
#define seCollapseItem 0
#endif

#ifndef seCaption
#define seCaption 0
#endif

#ifndef TCM_TABSEL
#define TCM_TABSEL (WM_USER + 0x3F0)
#endif

#ifndef NUMELEMENTS
#define NUMELEMENTS(arrayName) (sizeof(arrayName) / sizeof((arrayName)[0]))
#endif

#ifndef CBRS_EX_COOL
#define CBRS_EX_COOL 0
#endif

#ifndef CBRS_EX_BORDERSPACE
#define CBRS_EX_BORDERSPACE 0
#endif

#ifndef TWS_TABS_ON_BOTTOM
#define TWS_TABS_ON_BOTTOM 0
#endif

#ifndef TWS_DRAW_3D_NORMAL
#define TWS_DRAW_3D_NORMAL 0
#endif

#ifndef TWS_TABS_ON_TOP
#define TWS_TABS_ON_TOP 0
#endif

#ifndef TVXS_COLUMNHEADER
#define TVXS_COLUMNHEADER 0
#endif

#ifndef TVXS_FLYBYTOOLTIPS
#define TVXS_FLYBYTOOLTIPS 0
#endif

#ifndef LVXS_HILIGHTSUBITEMS
#define LVXS_HILIGHTSUBITEMS 0
#endif

#ifndef LVXS_LINESBETWEENITEMS
#define LVXS_LINESBETWEENITEMS 0
#endif

#ifndef LVXS_LINESBETWEENCOLUMNS
#define LVXS_LINESBETWEENCOLUMNS 0
#endif

#ifndef BEGIN_BUTTON_MAP
#define BEGIN_BUTTON_MAP(name) static UINT name[] = {
#endif

#ifndef STD_BUTTON
#define STD_BUTTON(id, style) id,
#endif

#ifndef COMBO_BUTTON
#define COMBO_BUTTON(id, ctrlId, style, comboStyle, width, height, dropWidth) id,
#endif

#ifndef TEXT_BUTTON
#define TEXT_BUTTON(id, textId) id,
#endif

#ifndef TEXT_BUTTON_EX
#define TEXT_BUTTON_EX(id, textId, style) id,
#endif

#ifndef END_BUTTON_MAP
#define END_BUTTON_MAP() 0 };
#endif

class SECWndBtn
{
public:
	enum NotifyCode
	{
		WndInit = 0,
	};
};

class SECControlBar : public CWnd
{
public:
virtual BOOL Create(CWnd* pParentWnd, LPCTSTR lpszWindowName, DWORD dwStyle, DWORD dwStyleEx, UINT nID, CCreateContext* pContext = NULL)
{
UNREFERENCED_PARAMETER(dwStyleEx);
UNREFERENCED_PARAMETER(pContext);
LPCTSTR pszClassName = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1), NULL);
return CWnd::Create(pszClassName, lpszWindowName, dwStyle, CRect(0, 0, 0, 0), pParentWnd, nID);
}

void GetInsideRect(CRect& rect) const
{
GetClientRect(&rect);
}

void EnableDocking(DWORD)
{
}

BOOL IsVisible() const
{
return (GetSafeHwnd() != NULL) && IsWindowVisible();
}

static UINT GetUniqueBarID(CFrameWnd* pFrameWnd, UINT nBase)
{
UNREFERENCED_PARAMETER(pFrameWnd);
return nBase;
}
};

inline void SECPositionControlBar(CWnd* pParentWnd, CWnd* pControlBar, UINT nDockBarID, int nWidth)
{
if (pParentWnd == NULL || pControlBar == NULL || pControlBar->GetSafeHwnd() == NULL)
{
return;
}

CRect rect;
pParentWnd->GetClientRect(&rect);

int nBarWidth = nWidth > 0 ? nWidth : 240;
int nBarHeight = rect.Height() > 0 ? rect.Height() : 240;

switch (nDockBarID)
{
case AFX_IDW_DOCKBAR_RIGHT:
	pControlBar->MoveWindow(max(0, rect.right - nBarWidth), rect.top, nBarWidth, nBarHeight);
	break;
case AFX_IDW_DOCKBAR_BOTTOM:
	pControlBar->MoveWindow(rect.left, max(0, rect.bottom - nBarWidth), rect.Width(), nBarWidth);
	break;
case AFX_IDW_DOCKBAR_TOP:
	pControlBar->MoveWindow(rect.left, rect.top, rect.Width(), nBarWidth);
	break;
case AFX_IDW_DOCKBAR_LEFT:
default:
	pControlBar->MoveWindow(rect.left, rect.top, nBarWidth, nBarHeight);
	break;
}
}

class SECCustomToolBar : public SECControlBar
{
public:
	SECCustomToolBar()
	{
	}

	void SetButtons(const UINT* pButtons, int nButtonCount)
	{
		m_buttonIds.assign(pButtons, pButtons + nButtonCount);
	}

	int CommandToIndex(UINT nID) const
	{
		for (size_t i = 0; i < m_buttonIds.size(); ++i)
		{
			if (m_buttonIds[i] == nID)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void SetButtonStyle(int nIndex, UINT nStyle)
	{
		m_buttonStyles[nIndex] = nStyle;
	}

	DWORD GetBarStyle() const
	{
		return GetStyle();
	}

private:
	std::vector<UINT> m_buttonIds;
	std::map<int, UINT> m_buttonStyles;
};

class SECMenuBar : public SECCustomToolBar
{
};

class SECMDIMenuBar : public SECCustomToolBar
{
};

class SECToolBar : public SECCustomToolBar
{
};

class SECWorksheet;

class SECFrameWnd : public CFrameWnd
{
public:
	SECFrameWnd()
		: m_pControlBarManager(NULL)
		, m_pMenuBar(NULL)
	{
	}

	virtual ~SECFrameWnd()
	{
	}

void EnableBmpMenus() {}
void SetTitle(LPCTSTR pszTitle) { CFrameWnd::SetWindowText(pszTitle); }
	void ShowControlBar(SECControlBar* pControlBar, BOOL bShow, BOOL bDelay)
	{
		UNREFERENCED_PARAMETER(bDelay);
		if (pControlBar != NULL)
		{
			pControlBar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
		}
	}
	void ShowControlBar(CControlBar* pControlBar, BOOL bShow, BOOL bDelay)
	{
		UNREFERENCED_PARAMETER(bDelay);
		CFrameWnd::ShowControlBar(pControlBar, bShow, FALSE);
	}
	void DockControlBarEx(SECControlBar* pControlBar, UINT nDockBarID, int, int, float, int nWidth)
	{
		if (pControlBar != NULL)
		{
			SECPositionControlBar(this, pControlBar, nDockBarID, nWidth);
			pControlBar->ShowWindow(SW_SHOW);
		}
	}
	void DockControlBar(SECCustomToolBar* pBar, UINT nDockBarID, LPCRECT)
	{
		if (pBar != NULL)
		{
			SECPositionControlBar(this, pBar, nDockBarID, 0);
			pBar->ShowWindow(SW_SHOW);
		}
	}

	CObject* m_pControlBarManager;
	SECMenuBar* m_pMenuBar;
};

class SECWorkbook : public CMDIFrameWnd
{
public:
	SECWorkbook()
		: m_pControlBarManager(NULL)
		, m_pMenuBar(NULL)
	{
	}

	virtual ~SECWorkbook()
	{
	}

void EnableBmpMenus() {}
void SetTitle(LPCTSTR pszTitle) { CMDIFrameWnd::SetWindowText(pszTitle); }
	void ShowControlBar(SECControlBar* pControlBar, BOOL bShow, BOOL bDelay)
	{
		UNREFERENCED_PARAMETER(bDelay);
		if (pControlBar != NULL)
		{
			pControlBar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
		}
	}
	void ShowControlBar(CControlBar* pControlBar, BOOL bShow, BOOL bDelay)
	{
		UNREFERENCED_PARAMETER(bDelay);
		CMDIFrameWnd::ShowControlBar(pControlBar, bShow, FALSE);
	}
	void DockControlBarEx(SECControlBar* pControlBar, UINT nDockBarID, int, int, float, int nWidth)
	{
		if (pControlBar != NULL)
		{
			SECPositionControlBar(this, pControlBar, nDockBarID, nWidth);
			pControlBar->ShowWindow(SW_SHOW);
		}
	}
	void DockControlBar(SECCustomToolBar* pBar, UINT nDockBarID, LPCRECT)
	{
		if (pBar != NULL)
		{
			SECPositionControlBar(this, pBar, nDockBarID, 0);
			pBar->ShowWindow(SW_SHOW);
		}
	}
	CMDIChildWnd* CreateNewChild(CRuntimeClass* pClass, UINT nResourceID, HMENU hMenu, HACCEL hAccel);
	SECWorksheet* GetWorksheet(int nIndex);

	CObject* m_pControlBarManager;
	SECMDIMenuBar* m_pMenuBar;

protected:
	std::vector<SECWorksheet*> m_worksheets;
};

class SECWorksheet : public CMDIChildWnd
{
public:
void SetTitle(LPCTSTR pszTitle) { CMDIChildWnd::SetWindowText(pszTitle); }
};

inline CMDIChildWnd* SECWorkbook::CreateNewChild(CRuntimeClass* pClass, UINT nResourceID, HMENU hMenu, HACCEL hAccel)
{
	UNREFERENCED_PARAMETER(hMenu);
	UNREFERENCED_PARAMETER(hAccel);

	CMDIChildWnd* pChildWnd = DYNAMIC_DOWNCAST(CMDIChildWnd, pClass->CreateObject());
	if (pChildWnd == NULL)
	{
		return NULL;
	}

	if (!pChildWnd->LoadFrame(nResourceID, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, this, NULL))
	{
		delete pChildWnd;
		return NULL;
	}

	SECWorksheet* pWorksheet = DYNAMIC_DOWNCAST(SECWorksheet, pChildWnd);
	if (pWorksheet != NULL)
	{
		m_worksheets.push_back(pWorksheet);
	}

	return pChildWnd;
}

inline SECWorksheet* SECWorkbook::GetWorksheet(int nIndex)
{
	if (nIndex < 0 || nIndex >= static_cast<int>(m_worksheets.size()))
	{
		return NULL;
	}

	return m_worksheets[nIndex];
}

class SECStatusBar : public CStatusBar
{
};

class SECTreeCtrl : public CTreeCtrl
{
public:
BOOL Create(DWORD dwStyle, DWORD dwStyleEx, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
UNREFERENCED_PARAMETER(dwStyleEx);
return CTreeCtrl::Create(dwStyle, rect, pParentWnd, nID);
}

	HTREEITEM InsertItem(LPCTSTR lpszItem, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST)
	{
		return CTreeCtrl::InsertItem(lpszItem, NormalizeParent(hParent), NormalizeInsertAfter(hInsertAfter));
	}

	HTREEITEM InsertItem(LPCTSTR lpszItem, int nImage, int nSelectedImage, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST)
	{
		return CTreeCtrl::InsertItem(lpszItem, nImage, nSelectedImage, NormalizeParent(hParent), NormalizeInsertAfter(hInsertAfter));
	}

	HTREEITEM InsertItem(UINT nMask, LPCTSTR lpszItem, int nImage, int nSelectedImage, UINT nState, UINT nStateMask, LPARAM lParam, HTREEITEM hParent, HTREEITEM hInsertAfter)
	{
		return CTreeCtrl::InsertItem(nMask, lpszItem, nImage, nSelectedImage, nState, nStateMask, lParam, NormalizeParent(hParent), NormalizeInsertAfter(hInsertAfter));
	}

	HTREEITEM InsertItem(LPTVINSERTSTRUCT lpInsertStruct)
	{
		if ( lpInsertStruct != NULL )
		{
			lpInsertStruct->hParent = NormalizeParent( lpInsertStruct->hParent );
			lpInsertStruct->hInsertAfter = NormalizeInsertAfter( lpInsertStruct->hInsertAfter );
		}
		return CTreeCtrl::InsertItem( lpInsertStruct );
	}

	HTREEITEM InsertItem(TVINSERTSTRUCT *lpInsertStruct)
	{
		return InsertItem( reinterpret_cast<LPTVINSERTSTRUCT>( lpInsertStruct ) );
	}

	BOOL SubclassTreeCtrlId(UINT nID, CWnd* pParentWnd)
	{
		CWnd* pWnd = pParentWnd != NULL ? pParentWnd->GetDlgItem(nID) : NULL;
		return pWnd != NULL ? SubclassWindow(pWnd->GetSafeHwnd()) : FALSE;
	}

	void ModifyTreeCtrlStyles(DWORD dwRemove, DWORD dwAdd, DWORD, DWORD)
	{
		ModifyStyle(dwRemove, dwAdd);
	}

	void ModifyListCtrlStyles(DWORD, DWORD, DWORD, DWORD)
	{
	}

	void StoreSubItemText(BOOL)
	{
	}

	void SetColumnHeading(int nColumn, LPCTSTR pszText)
	{
		m_columnHeadings[nColumn] = pszText;
	}

	void SetColumnWidth(int nColumn, int nWidth)
	{
		m_columnWidths[nColumn] = nWidth;
	}

	int GetColumnWidth(int nColumn) const
	{
		std::map<int, int>::const_iterator it = m_columnWidths.find(nColumn);
		return it != m_columnWidths.end() ? it->second : 0;
	}

	void SetColumnFormat(int nColumn, int nFormat)
	{
		m_columnFormats[nColumn] = nFormat;
	}

	void InsertColumn(int nColumn, LPCTSTR pszText, int nFormat, int nWidth)
	{
		SetColumnHeading(nColumn, pszText);
		SetColumnFormat(nColumn, nFormat);
		SetColumnWidth(nColumn, nWidth);
	}

	void SetItemText(HTREEITEM hItem, int nColumn, LPCTSTR pszText)
	{
		if (nColumn == 0)
		{
			CTreeCtrl::SetItemText(hItem, pszText);
			return;
		}
		m_subItemText[std::make_pair(hItem, nColumn)] = pszText;
	}

	void SetItemText(HTREEITEM hItem, LPCTSTR pszText)
	{
		CTreeCtrl::SetItemText(hItem, pszText);
	}

	int GetChildCount(HTREEITEM hItem, bool = true, bool = true) const
	{
		int nCount = 0;
		for (HTREEITEM hChild = GetChildItem(hItem); hChild != NULL; hChild = GetNextSiblingItem(hChild))
		{
			++nCount;
		}
		return nCount;
	}

	HTREEITEM GetFirstSelectedItem() const
	{
		return GetSelectedItem();
	}

	void CollapseCompletely(HTREEITEM hItem, bool)
	{
		for (HTREEITEM hChild = GetChildItem(hItem); hChild != NULL; hChild = GetNextSiblingItem(hChild))
		{
			CollapseCompletely(hChild, false);
		}
		Expand(hItem, TVE_COLLAPSE);
	}

	HTREEITEM GetRootItem(HTREEITEM hItem) const
	{
		HTREEITEM hCurrent = hItem;
		while (hCurrent != NULL)
		{
			HTREEITEM hParent = GetParentItem(hCurrent);
			if (hParent == NULL)
			{
				return hCurrent;
			}
			hCurrent = hParent;
		}
		return NULL;
	}

	HTREEITEM GetRootItem() const
	{
		return CTreeCtrl::GetRootItem();
	}

	void DeselectAllItems()
	{
		SelectItem(NULL);
	}

	void ReMeasureAllItems()
	{
	}

	BOOL EnsureVisible(HTREEITEM hItem, BOOL)
	{
		return CTreeCtrl::EnsureVisible(hItem);
	}

	void SetNotifyWnd(CWnd* pWnd)
	{
		m_pNotifyWnd = pWnd;
	}

private:
	static HTREEITEM NormalizeParent(HTREEITEM hParent)
	{
		return hParent != NULL ? hParent : TVI_ROOT;
	}

	static HTREEITEM NormalizeInsertAfter(HTREEITEM hInsertAfter)
	{
		return hInsertAfter != NULL ? hInsertAfter : TVI_LAST;
	}

	std::map<int, CString> m_columnHeadings;
	std::map<int, int> m_columnWidths;
	std::map<int, int> m_columnFormats;
	std::map<std::pair<HTREEITEM, int>, CString> m_subItemText;
	CWnd* m_pNotifyWnd = NULL;
};

class SECBar
{
public:
SECBar() : m_pWnd(NULL) {}
SECBar(CWnd* pWnd, const CString& title) : m_pWnd(pWnd), m_title(title) {}

CWnd* GetWnd() const { return m_pWnd; }
const CString& GetTitle() const { return m_title; }

private:
CWnd* m_pWnd;
CString m_title;
};

class SECShortcutBar : public CWnd
{
public:
	SECShortcutBar()
		: m_activeIndex(0)
	{
	}

BOOL Create(CWnd* pParentWnd, DWORD dwStyle, UINT nID)
{
return CWnd::Create(NULL, _T("SECShortcutBar"), dwStyle, CRect(0, 0, 0, 0), pParentWnd, nID);
}

virtual BOOL OnChangeBar(int) { return TRUE; }

void AddBar(CWnd* pWnd, LPCTSTR pszLabel, BOOL)
{
m_bars.push_back(SECBar(pWnd, pszLabel));
}

int GetBarCount() const { return static_cast<int>(m_bars.size()); }
SECBar& GetBar(int index) { return m_bars.at(index); }
const SECBar& GetBar(int index) const { return m_bars.at(index); }
void SelectPane(int index) { m_activeIndex = index; OnChangeBar(index); }
int GetActiveIndex() const { return m_activeIndex; }

private:
std::vector<SECBar> m_bars;
int m_activeIndex;
};

class SEC3DTabWnd : public CWnd
{
public:
SEC3DTabWnd() : m_activeTab(0) {}

BOOL Create(CWnd* pParentWnd, DWORD dwStyle, UINT nID)
{
return CWnd::Create(NULL, _T("SEC3DTabWnd"), dwStyle, CRect(0, 0, 0, 0), pParentWnd, nID);
}
	BOOL Create(CWnd* pParentWnd, DWORD dwStyle)
	{
		return Create(pParentWnd, dwStyle, AFX_IDW_PANE_FIRST);
	}

void AddTab(CWnd* pWnd, LPCTSTR pszLabel)
{
UNREFERENCED_PARAMETER(pszLabel);
m_tabs.push_back(pWnd);
}

int GetTabCount() const { return static_cast<int>(m_tabs.size()); }
BOOL GetActiveTab(int& nActiveTab) const
{
if (m_tabs.empty())
{
return FALSE;
}
nActiveTab = m_activeTab;
return TRUE;
}
void SelectTab(int index)
{
m_activeTab = index;
if (GetSafeHwnd() != NULL && GetParent() != NULL)
{
GetParent()->SendMessage(TCM_TABSEL, index, 0);
}
}
	void SetTabIcon(int, UINT)
	{
	}
	void ActivateTab(int index)
	{
		SelectTab(index);
	}

private:
std::vector<CWnd*> m_tabs;
int m_activeTab;
};

class SECToolBarManager : public CObject
{
public:
SECToolBarManager(CFrameWnd* pFrameWnd) : m_pFrameWnd(pFrameWnd) {}
~SECToolBarManager()
{
for (std::map<UINT, SECCustomToolBar*>::iterator it = m_toolbars.begin(); it != m_toolbars.end(); ++it)
{
delete it->second;
}
}

	BOOL LoadToolBarResource(LPCTSTR, LPCTSTR) { return TRUE; }
	BOOL AddToolBarResource(LPCTSTR, LPCTSTR) { return TRUE; }
	BOOL DefineDefaultToolBar(UINT nID, const CString&, int nButtonCount, UINT* pButtons, DWORD, UINT, UINT = 0, BOOL = TRUE, BOOL = TRUE)
{
if (m_toolbars.find(nID) == m_toolbars.end())
{
m_toolbars[nID] = new SECCustomToolBar();
}
		m_toolbars[nID]->SetButtons(pButtons, nButtonCount);
return TRUE;
}
	void SetMenuInfo(int, UINT) {}
	void SetMenuInfo(int, UINT, UINT) {}
	void SetButtonMap(UINT*) {}
	void EnableLargeBtns(BOOL) {}
	void EnableCoolLook(BOOL) {}
	void SetDefaultDockState() {}
	void LoadState(LPCTSTR) {}
	void SaveState(LPCTSTR) {}
SECCustomToolBar* ToolBarFromID(UINT nID)
{
std::map<UINT, SECCustomToolBar*>::iterator it = m_toolbars.find(nID);
return it != m_toolbars.end() ? it->second : NULL;
}

private:
CFrameWnd* m_pFrameWnd;
std::map<UINT, SECCustomToolBar*> m_toolbars;
};

class SECToolBarsPage : public CPropertyPage
{
public:
	enum { IDD = 0 };
SECToolBarsPage() : CPropertyPage() {}
void SetManager(SECToolBarManager*) {}
};

class SECToolBarCmdPage : public CPropertyPage
{
public:
	enum { IDD = 0 };
	SECToolBarCmdPage() : CPropertyPage() {}
	SECToolBarCmdPage(UINT, UINT) : CPropertyPage() {}
void SetManager(SECToolBarManager*) {}
	void DefineBtnGroup(const CString&, int, UINT*) {}
	void DefineMenuGroup(const CString&) {}
};

class SECToolBarSheet : public CPropertySheet
{
public:
	SECToolBarSheet() : CPropertySheet(_T("Toolbar Customization")) {}
SECToolBarSheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0)
: CPropertySheet(nIDCaption, pParentWnd, iSelectPage) {}
};

class SECDirSelectDlg : public CFileDialog
{
public:
	enum DialogStyles
	{
		win16Style = 0x01,
		win32Style = 0x02,
	};

	SECDirSelectDlg(LPCTSTR lpcszCaption = NULL, CWnd* pWndParent = NULL, LPCTSTR lpcszInitialDir = NULL, DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT)
		: CFileDialog(TRUE, NULL, NULL, dwFlags, NULL, pWndParent)
		, m_strCaption(lpcszCaption != NULL ? lpcszCaption : _T("Select Directory"))
		, m_strInitialDir(lpcszInitialDir != NULL ? lpcszInitialDir : _T(""))
	{
		m_ofn.lpstrInitialDir = m_strInitialDir;
		m_ofn.lpstrTitle = m_strCaption;
	}

	SECDirSelectDlg(WORD, LPCTSTR lpszCaption = NULL, LPCTSTR lpszInitialDir = NULL, CWnd* pParentWnd = NULL, DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT)
		: CFileDialog(TRUE, NULL, NULL, dwFlags, NULL, pParentWnd)
		, m_strCaption(lpszCaption != NULL ? lpszCaption : _T("Select Directory"))
		, m_strInitialDir(lpszInitialDir != NULL ? lpszInitialDir : _T(""))
	{
		m_ofn.lpstrInitialDir = m_strInitialDir;
		m_ofn.lpstrTitle = m_strCaption;
	}

	void GetPath(CString& strPath) const
	{
		strPath = m_strSelectedPath;
	}

	void SetInitialDir(LPCTSTR lpszCaption)
	{
		m_strInitialDir = lpszCaption != NULL ? lpszCaption : _T("");
		m_ofn.lpstrInitialDir = m_strInitialDir;
	}

	void SetBrowseCaption(LPCTSTR lpszCaption)
	{
		m_strCaption = lpszCaption != NULL ? lpszCaption : _T("Select Directory");
		m_ofn.lpstrTitle = m_strCaption;
	}

	virtual SEC_INT DoModal()
	{
		BROWSEINFO browseInfo = { 0 };
		browseInfo.hwndOwner = m_pParentWnd != NULL ? m_pParentWnd->GetSafeHwnd() : NULL;
		browseInfo.lpszTitle = m_strCaption;
		browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

		LPITEMIDLIST pItemIdList = SHBrowseForFolder(&browseInfo);
		if (pItemIdList == NULL)
		{
			return IDCANCEL;
		}

		TCHAR szPath[MAX_PATH] = { 0 };
