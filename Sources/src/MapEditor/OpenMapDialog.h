#if !defined(__OPEN_MAP_DIALOG__)
#define __OPEN_MAP_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class COpenMapDialog : public CResizeDialog
{
public:
	std::list<std::string> mapNames;
	COpenMapDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_OPEN_MAP };
	CComboBox wndFileName;
	CComboBox	wndMODComboBox;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnOpenMapNameBrowseButton();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnEditChangeOpenMapNameComboBox();
	afx_msg void OnSelChangeOpenMapNameComboBox();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 250; }
	virtual int GetMinimumYDimension() { return 115; }
	virtual std::string GetXMLOptionsLabel() { return "COpenMapDialog"; }
	virtual bool GetDrawGripper() { return true; }
	
	void LoadControls();
	void SaveControls();
	void UpdateControls();

	void GetAllMODs( std::vector<std::string> *pMODsList );

public:
	std::string GetMapName() { return resizeDialogOptions.szParameters[0]; }
	std::string GetMODKey();
};

#endif // !defined(__OPEN_MAP_DIALOG__)
