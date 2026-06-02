#if !defined(__ELK_IMPORT_FROM_XLS_DIALOG__)
#define __ELK_IMPORT_FROM_XLS_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CImportFromXLSDialog : public CResizeDialog
{
public:
	CImportFromXLSDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_IMPORT_FROM_XLS };
	CEdit	m_XLSEdit;
	CEdit	m_FileEdit;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnFileBrowseButton();
	afx_msg void OnXLSBrowseButton();
	afx_msg void OnChangeFileBrowseEdit();
	afx_msg void OnChangeXLSBrowseEdit();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 350; }
	virtual int GetMinimumYDimension() { return 125; }
	virtual bool SerializeToRegistry() { return true; }
	virtual std::string GetRegistryKey();
	virtual bool GetDrawGripper() { return true; }

	void UpdateControls();

public:
	void GetXLSPath( std::string *pszXLSPath );
	void GetFilePath( std::string *pszFilePath );
};
#endif // !defined(__ELK_IMPORT_FROM_XLS_DIALOG__)
