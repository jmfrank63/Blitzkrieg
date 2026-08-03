#if !defined(__CREATE_RANDOM_MAP_DIALOG__)
#define __CREATE_RANDOM_MAP_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ProgressDialog.h"
#include "ResizeDialog.h"
#include "../StreamIO/ProgressHook.h"

class CCreateRandomMapProgress : public IProgressHook
{
	OBJECT_NORMAL_METHODS( CCreateRandomMapProgress );
	
	CProgressDialog progressDialog;
	
public:
	CCreateRandomMapProgress();
	CCreateRandomMapProgress( CWnd *pWnd, int nNumSteps, const std::string &rszTitle, const std::string &rszMessage );
	~CCreateRandomMapProgress();
	
	virtual void STDCALL SetNumSteps( const int nRange, const float fPercentage = 1.0f );
	virtual void STDCALL Step();
	virtual void STDCALL Recover() {}
	virtual void STDCALL SetCurrPos( const int nPos ) {}
	virtual int STDCALL GetCurrPos() const { return 0; }
	virtual void Init( const std::string &szMovieName ) {}
	virtual void Stop() {}
};
class CCreateRandomMapDialog : public CResizeDialog
{
public:
	CCreateRandomMapDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_CREATE_RANDOM_MAP };
	CComboBox	m_Setting;
	CComboBox	m_Template;
	CEdit	m_Map;
	CEdit	m_Graph;
	CComboBox	m_Context;
	BOOL	m_SaveAsBZM;
	BOOL	m_SaveAsDDS;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnChangeCrmGraphEdit();
	afx_msg void OnChangeCrmMapEdit();
	afx_msg void OnSelchangeCrmSettingCombobox();
	afx_msg void OnCrmMapBrowseButton();
	afx_msg void OnCrmTemplateBrowseButton();
	afx_msg void OnCrmContextBrowseButton();
	virtual void OnCancel();
	afx_msg void OnEditchangeCrmContextEdit();
	afx_msg void OnSelchangeCrmContextEdit();
	afx_msg void OnEditchangeCrmTemplateEdit();
	afx_msg void OnSelchangeCrmTemplateEdit();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];
	bool bCreateControls;
	virtual int GetMinimumXDimension() { return 350; }
	virtual int GetMinimumYDimension() { return 320; }
	virtual std::string GetXMLOptionsLabel() { return "CCreateRandomMapDialog"; }
	virtual bool GetDrawGripper() { return true; }

	void UpdateControls();

public:
	static void ValidatePath( std::string *pszPath );

	std::vector<std::string> szSettings;
	std::vector<std::string> szTemplates;
	std::vector<std::string> szContexts;

	void GetTemplate( std::string *pszTemplate )
	{
		if ( pszTemplate )
		{
			( *pszTemplate ) = resizeDialogOptions.szParameters[0];
		}
	}
	void GetContext( std::string *pszContext )
	{
		if ( pszContext )
		{
			( *pszContext ) = resizeDialogOptions.szParameters[1];
		}
	}

	void GetSetting( std::string *pszSetting )
	{
		if ( pszSetting )
		{
			( *pszSetting ) = resizeDialogOptions.szParameters[2];
		}
	}
	void GetMap( std::string *pszMap )
	{
		if ( pszMap )
		{
			( *pszMap ) = resizeDialogOptions.szParameters[3];
		}
	}
	int GetGraph() { return resizeDialogOptions.nParameters[0]; }
	int GetAngle() { return resizeDialogOptions.nParameters[1]; }
	int GetLevel() { return resizeDialogOptions.nParameters[2]; }
	bool SaveAsBZM() { return ( resizeDialogOptions.nParameters[3] > 0 ); }
	bool SaveAsDDS() { return ( resizeDialogOptions.nParameters[4] > 0 ); }
};
#endif // !defined(__CREATE_RANDOM_MAP_DIALOG__)
