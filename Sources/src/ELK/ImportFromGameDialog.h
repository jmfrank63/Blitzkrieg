#if !defined(__ELK_IMPORT_FROM_GAME_DIALOG__)
#define __ELK_IMPORT_FROM_GAME_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CImportFromGameDialog : public CResizeDialog
{
public:
	CImportFromGameDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_IMPORT_FROM_GAME };
	CEdit	m_FolderEdit;
	CEdit	m_FileEdit;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnFileBrowseButton();
	afx_msg void OnFolderBrowseButton();
	afx_msg void OnChangeFileBrowseEdit();
	afx_msg void OnChangeFolderBrowseEdit();
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
	void GetGamePath( std::string *pszGamePath );
	void GetFilePath( std::string *pszFilePath );
};
#endif // !defined(__ELK_IMPORT_FROM_GAME_DIALOG__)
