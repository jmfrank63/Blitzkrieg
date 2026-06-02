#ifndef __MY_OPEN_FILE_DIALOG_H__
#define __MY_OPEN_FILE_DIALOG_H__

class CMyOpenFileDialog : public CFileDialog
{
	DECLARE_DYNAMIC(CMyOpenFileDialog)

public:
	CMyOpenFileDialog(BOOL bOpenFileDialog, // TRUE for FileOpen, FALSE for FileSaveAs
		LPCTSTR lpszDefExt = NULL,
		LPCTSTR lpszFileName = NULL,
		DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		LPCTSTR lpszFilter = NULL,
		CWnd* pParentWnd = NULL) : CFileDialog( bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags, lpszFilter, pParentWnd ) {}
	virtual ~CMyOpenFileDialog() {}

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
};

void SaveFileDialogRegisterData();
void LoadFileDialogRegisterData();
bool GetDirectoryFromExtensionTable( std::string &szRes, const std::string &szExtension );

BOOL ShowFileDialog( std::string &szResult, LPCTSTR lpszInitDir, LPCTSTR lpszTitle, BOOL bOpen, LPCTSTR lpszDefExt = NULL,
										LPCTSTR lpszFileName = NULL, LPCTSTR lpszFilter = NULL );

#endif		//__MY_OPEN_FILE_DIALOG_H__
