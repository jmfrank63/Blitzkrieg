#include "StdAfx.h"
#include "resource.h"
#include "ELK.h"

#include "MainFrm.h"
#include "AboutDialog.h"
#include "..\Misc\FileUtils.h"
#include "..\RandomMapGen\Registry_Types.h"
#include "..\Image\Image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CELKApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_HELP_CONTENTS, OnHelpContents)
END_MESSAGE_MAP()

CELKApp::CELKApp()
{
}

CELKApp theApp;

BOOL CELKApp::InitInstance()
{
	{
		CString strProgramTitle;
		strProgramTitle.LoadString( AFX_IDS_APP_TITLE );

		std::string szMessage;
		char pBuffer[0xFFF + 1];
		::GetCurrentDirectory( 0xFFF, pBuffer );
		std::string szZIPToolPath = std::string( pBuffer ) + std::string( "\\" ) + std::string( CELK::ZIP_EXE );
		if ( !NFile::IsFileExist( szZIPToolPath.c_str() ) )
		{
			szMessage = NStr::Format( _T( "Can't find file \"%s\" in ELK work directory: %s\\. PAK export will be unavailable." ), CELK::ZIP_EXE, pBuffer );
		}
		if ( !szMessage.empty() )
		{
			::MessageBox( ::GetDesktopWindow(), szMessage.c_str(), strProgramTitle, MB_OK | MB_ICONWARNING );
		}

		HMODULE hImage = LoadLibrary( ( std::string( pBuffer ) + _T( "\\image.dll" ) ).c_str() );
		if ( hImage )
		{
			GETMODULEDESCRIPTOR pfnGetModuleDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( GetProcAddress( hImage, _T( "GetModuleDescriptor" ) ) );
			if ( pfnGetModuleDescriptor )
			{
				const SModuleDescriptor *pDesc = ( *pfnGetModuleDescriptor )();
				if ( pDesc && pDesc->pFactory )
				{
					IImageProcessor *pIP = CreateObject<IImageProcessor>( pDesc->pFactory, IImageProcessor::tidTypeID );
					if ( pIP )
					{
						RegisterSingleton( IImageProcessor::tidTypeID, pIP );
					}
				}
			}
		}
	}

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

/**
#if defined( _DO_SEH ) && !defined( _DEBUG )
	SetCrashHandlerFilter( CrashHandlerFilter );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
/**/

	CString strRegistryPathName;
	strRegistryPathName.LoadString( IDS_REGISTRY_PATH );
	SetRegistryKey( strRegistryPathName );

	CMainFrame* pFrame = new CMainFrame;
	if ( !pFrame )
	{
		return false;
	}
	{
		std::string szCommandLine( m_lpCmdLine );
		NStr::ToLower( szCommandLine );
		pFrame->bShortApperence = ( szCommandLine == std::string( _T( "-short" ) ) );
	}
	{
		std::string szGameFolder;
		CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_READ, CELK::GAME_REGISTRY_FOLDER );
		registrySection.LoadString( CELK::GAME_REGISTRY_KEY, &szGameFolder, "" );
		pFrame->bGameExists = ( !szGameFolder.empty() );
	}	
	
	m_pMainWnd = pFrame;
	if ( pFrame->bShortApperence )
	{
		if ( !pFrame->LoadFrame( IDR_SHORT_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, 0, 0 ) )
		{
			NI_ASSERT_T( 0, NStr::Format( _T( "CELKApp::InitInstance, can't create main frame" ) ) ); // Unable to load frame
			return false;
		}
	}
	else
	{
		if ( !pFrame->LoadFrame( IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, 0, 0 ) )
		{
			NI_ASSERT_T( 0, NStr::Format( _T( "CELKApp::InitInstance, can't create main frame" ) ) ); // Unable to load frame
			return false;
		}
	}
	pFrame->UpdateWindow();
	if ( pFrame->params.bFullScreen )
	{
		pFrame->ShowWindow( SW_MAXIMIZE );
	}
	else
	{
		pFrame->ShowWindow( SW_SHOW );
	}
	pFrame->UpdateWindow();

	return true;
}

void CELKApp::OnAppAbout()
{
	CAboutDialog wndAboutDialog;
	wndAboutDialog.DoModal();
}

int CELKApp::ExitInstance() 
{
	return CWinApp::ExitInstance();
}

void CELKApp::OnHelpContents() 
{
	if ( m_pMainWnd != 0 )
  {
    CMainFrame *pFrame = static_cast<CMainFrame*>( m_pMainWnd );
		if ( NFile::IsFileExist( pFrame->params.szHelpFilePath.c_str() ) )
		{
			pFrame->RunExternalHelpFile( pFrame->params.szHelpFilePath );
		}
  }
}
