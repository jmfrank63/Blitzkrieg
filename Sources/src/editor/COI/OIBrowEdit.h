#ifndef __OIBROWEDIT_H__
#define __OIBROWEDIT_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIEdit.h"

class COIBrowseEdit;


class COIBrowseButton : public CButton
{
public:
  COIBrowseButton( COIBrowseEdit *pPrnt, CEdit* pEdtBrowse );
  
  ~COIBrowseButton();
  
  BOOL Create();
  
protected:
  DECLARE_MESSAGE_MAP()
    
protected:
  virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pLResult);
  
  enum { 
    BTN_WIDTH = 20		// emem Button width.
  } ;
  
  CEdit* m_pEdtBrowse;
  COIBrowseEdit *m_pParentWnd;
  
  UINT m_uiID;  
};


class COIBrowseEdit : public CWnd
{
public:
  
  COIBrowseEdit();  
  virtual ~COIBrowseEdit();
  
protected:
	enum { 
		BTN_WIDTH = 17
	};
	CFont	m_fntDef;
	COIEdit m_Edit;
	COIBrowseButton m_BrowseBtn;
	bool bDirectory;

	CString m_strCaption;
	CString m_szDirPrefix;
	CString szFilter;
	CString szDir;

public:
	void SetBrowseCaption(LPCTSTR lpcszCaption)	
		{ m_strCaption = lpcszCaption; }

	LPCTSTR GetBrowseCaption() const
		{ return m_strCaption; }

	COIBrowseButton* GetBrowseButton()
		{ return &m_BrowseBtn; }
	void SetWindowText( LPCTSTR lpszString );
	void GetWindowText( CString &rString ) const;
	void SetDirPrefix( LPCTSTR lpszDirPref );
	void SetDirectoryFlag( bool bDir ) { bDirectory = bDir; }
	void SetBrowseFilter( const char *pszBrowseFilter ) { szFilter = pszBrowseFilter; }
	void SetSourceDir( const char *pszDir ) { szDir = pszDir; }

public:
	virtual void OnBrowse();
  
protected:

	virtual BOOL PreTranslateMessage( MSG* pMsg );

	afx_msg void OnEnable(BOOL bEnable);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

	DECLARE_MESSAGE_MAP()

};



#endif // __OIBROWEDIT_H__
