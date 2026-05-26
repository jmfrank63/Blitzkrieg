// MODDialog.cpp : implementation file
//

#include "stdafx.h"
#include "editor.h"
#include "..\Common\StingrayCompat.h"
#include "MODDialog.h"
#include "NewDirDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMODDialog dialog


CMODDialog::CMODDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CMODDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMODDialog)
	mExportDir = _T("");
	mName = _T("");
	mVersion = _T("");
	mDesc = _T("");
	//}}AFX_DATA_INIT
}


void CMODDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMODDialog)
	DDX_Text(pDX, IDC_MOD_EXPORT_TEXT, mExportDir);
	DDX_Text(pDX, IDC_MOD_NAME_TEXT, mName);
	DDX_Text(pDX, IDC_MOD_VERSION_TEXT, mVersion);
	DDX_Text(pDX, IDC_MOD_DESC_TEXT, mDesc);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMODDialog, CDialog)
	//{{AFX_MSG_MAP(CMODDialog)
	ON_BN_CLICKED(IDC_MOD_EXPORT_BTN, OnModExportBtn)
	ON_BN_CLICKED(IDC_MOD_DEFAULTS_BTN, OnModDefaultsBtn)
	ON_BN_CLICKED(IDC_BUTTON_NEW_MOD, OnButtonNewMod)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMODDialog message handlers

void CMODDialog::OnModExportBtn() 
{
	SECDirSelectDlg dlg( "Select MOD Destination Directory" );
	dlg.SetInitialDir( mExportDir );
	if ( dlg.DoModal() == IDOK )
	{
		dlg.GetPath( mExportDir );
		mExportDir.MakeLower();
		std::string szPath = mExportDir + "data\\";
		std::string szName = "";
		std::string szVersion = "";
		std::string szDesc = "";
		theApp.ReadMODFile( szPath, szName, szVersion, szDesc );
		mName = szName.c_str();
		mVersion = szVersion.c_str();
		mDesc = szDesc.c_str();
		UpdateData( FALSE );
	}
}

void CMODDialog::OnModDefaultsBtn() 
{
	std::string szPath = theApp.GetEditorDir();
	szPath = szPath + "mods\\mymod\\";
	mExportDir = szPath.c_str();
	mName = "";
	mVersion = "";
	mDesc = "";
	UpdateData( FALSE );
}

void CMODDialog::OnButtonNewMod() 
{
	CNewDirDialog dlg;
	UpdateData();
	if ( dlg.DoModal() == IDOK )
	{
		std::string szPath = theApp.GetEditorDir();
		szPath = szPath + "mods\\";
		szPath = szPath.c_str() + dlg.m_name + "\\";
		mExportDir = szPath.c_str();
		UpdateData( FALSE );
	}
}
