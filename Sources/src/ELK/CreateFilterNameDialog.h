#if !defined(__CREATE_FILTER_NAME_DIALOG__)
#define __CREATE_FILTER_NAME_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CCreateFilterNameDialog : public CResizeDialog
{
public:
	CCreateFilterNameDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_CREATE_FILTER_NAME };
	CString	m_Name;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	afx_msg void OnChangeFilterNameEdit();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 200; }
	virtual int GetMinimumYDimension() { return 60; }
	virtual bool SerializeToRegistry() { return true; }
	virtual std::string GetRegistryKey();
	virtual bool GetDrawGripper() { return true; }
	
	void UpdateControls();
public:
};

#endif // !defined(__CREATE_FILTER_NAME_DIALOG__)
