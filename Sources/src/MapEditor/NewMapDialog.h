#if !defined(__NEW_MAP_DIALOG__)
#define __NEW_MAP_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CNewMapDialog : public CResizeDialog
{
public:
	CNewMapDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_NEW_MAP };
	CComboBox	wndSizeXComboBox;
	CComboBox	wndSizeYComboBox;
	CComboBox	wndSeasonComboBox;
	CComboBox	wndMODComboBox;
	CEdit wndFileName;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnChangeNewMapNameEdit();
	afx_msg void OnNewMapNameBrowseButton();
	virtual void OnCancel();
	afx_msg void OnNewMapSquareCheckBox();
	afx_msg void OnSelchangeNewMapSizeXComboBox();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 250; }
	virtual int GetMinimumYDimension() { return 195; }
	virtual std::string GetXMLOptionsLabel() { return "CNewMapDialog"; }
	virtual bool GetDrawGripper() { return true; }
	
	void LoadControls();
	void SaveControls();
	void UpdateControls();

	void GetAllMODs( std::vector<std::string> *pMODsList );

public:
	int GetSizeX()		{ return resizeDialogOptions.nParameters[0]; }
	int GetSizeY()		{ return resizeDialogOptions.nParameters[1]; }
	bool IsSquareMap(){ return ( resizeDialogOptions.nParameters[2] > 0 ); }
	int GetSeason()		{ return resizeDialogOptions.nParameters[3]; }
	std::string GetMapName() { return resizeDialogOptions.szParameters[0]; }
	std::string GetMODKey();
};

#endif // !defined(__NEW_MAP_DIALOG__)
