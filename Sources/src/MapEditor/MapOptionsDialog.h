#if !defined(AFX_MAPOPTIONSDIALOG_H__9C3B4443_6465_427B_97E9_CDF3F6A28762__INCLUDED_)
#define AFX_MAPOPTIONSDIALOG_H__9C3B4443_6465_427B_97E9_CDF3F6A28762__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CMapOptionsDialog : public CResizeDialog
{

public:
	CMapOptionsDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_SCRIPT_NAME };
	CString	m_name;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 300; }
	virtual int GetMinimumYDimension() { return 80; }
	virtual std::string GetXMLOptionsLabel() { return "CMapOptionsDialog"; }
	virtual bool GetDrawGripper() { return true; }

	afx_msg void OnGetFile();
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(AFX_MAPOPTIONSDIALOG_H__9C3B4443_6465_427B_97E9_CDF3F6A28762__INCLUDED_)
