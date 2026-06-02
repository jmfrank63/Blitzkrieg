#if !defined(__MAP_EDITOR_OPTIONS_DIALOG__)
#define __MAP_EDITOR_OPTIONS_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CMapEditorOptionsDialog : public CResizeDialog
{
public:
	CMapEditorOptionsDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_MAP_EDITOR_OPTIONS };
	CEdit	m_Parameters;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 250; }
	virtual int GetMinimumYDimension() { return 155; }
	virtual std::string GetXMLOptionsLabel() { return "CMapEditorOptionsDialog"; }
	virtual bool GetDrawGripper() { return true; }

	void LoadControls();
	void SaveControls();

	bool bIsBZM;
	std::string szParameter;

public:
	void SetIsBZM( bool _bIsBZM ) { bIsBZM = _bIsBZM; }
	void SetParameter( const std::string &rszParameter ) { szParameter = rszParameter; }

	bool IsBZM(){ return ( resizeDialogOptions.nParameters[0] == 0 ); }
	std::string GetParameter() { return resizeDialogOptions.szParameters[0]; }
};

#endif // !defined(__MAP_EDITOR_OPTIONS_DIALOG__)
