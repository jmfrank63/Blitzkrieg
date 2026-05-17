#include "StdAfx.h"
#include "IniFile.h"
#include "AR_Types.h"
#include "StrProc.h"
#include "Mmsystem.h"
#include "Shellapi.h"
//	int STDCALL GetInt( const char *pszRow, const char *pszEntry, int defval );
//	double STDCALL GetDouble( const char *pszRow, const char *pszEntry, double defval );
//	const char* STDCALL GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize );

#include "FileUtils.h"
#include "AutoRunDialog.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SARButton::Load( const std::string &rszName, CIniFile& rIniFile, std::unordered_map<std::string, SARMenu> *pMenus, const SARMainSection &rMainSection )
{
	char pBuffer[0xFFF];
	dwPresentState = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "PresentState", GAME_INSTALLED | GAME_NOT_INSTALLED ) );	
	dwEnableState = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "EnableState", GAME_INSTALLED | GAME_NOT_INSTALLED ) );
	bSelectable = static_cast<bool>( rIniFile.GetInt( rszName.c_str(), "Selectable", 1 ) );
	//
	szLabelFileName = rIniFile.GetString( rszName.c_str(), "LabelFileName", "", pBuffer, 0xFFF );
	szTooltipFileName = rIniFile.GetString( rszName.c_str(), "TooltipFileName", "", pBuffer, 0xFFF );
	szFocusedSoundFileName = rIniFile.GetString( rszName.c_str(), "FocusedSoundFileName", "", pBuffer, 0xFFF );
	szActionSoundFileName = rIniFile.GetString( rszName.c_str(), "ActionSoundFileName", "", pBuffer, 0xFFF );
	//
	dwColors.resize( 4 );
	dwColors[SARMainSection::STATE_NORMAL] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "NormalColor", rMainSection.dwColors[SARMainSection::STATE_NORMAL] ) );
	dwColors[SARMainSection::STATE_FOCUSED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "FocusedColor", rMainSection.dwColors[SARMainSection::STATE_FOCUSED] ) );
	dwColors[SARMainSection::STATE_SELECTED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "SelectedColor", rMainSection.dwColors[SARMainSection::STATE_SELECTED] ) );
	dwColors[SARMainSection::STATE_DISABLED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "DisabledColor", rMainSection.dwColors[SARMainSection::STATE_DISABLED] ) );
	dwShadowColor = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "ShadowColor", rMainSection.dwShadowColor ) );
	//
	dwAlign = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "LabelAlign", rMainSection.dwAlign ) );
	//
	position.left = rIniFile.GetInt( rszName.c_str(), "PosX", 0 ); 
	position.top = rIniFile.GetInt( rszName.c_str(), "PosY", 0 ); 
	position.right = position.left + rIniFile.GetInt( rszName.c_str(), "Width", 0 ); 
	position.bottom = position.top + rIniFile.GetInt( rszName.c_str(), "Height", 0 ); 
	//
	nFontSize = rIniFile.GetInt( rszName.c_str(), "FontSize", 20 ); 
	//
	action = ACTION_COUNT;
	std::string szActionName = rIniFile.GetString( rszName.c_str(), "Action", "", pBuffer, 0xFFF );
	NStr::ToLower( szActionName ); 
	for ( int nActionIndex = ACTION_OPEN; nActionIndex < ACTION_COUNT; ++nActionIndex )
	{
		if ( szActionName == ACTION_NAMES[nActionIndex] )
		{
			action = static_cast<EAction>( nActionIndex );
		}
	}
	//
	actionBehaviour = AB_COUNT;
	std::string szActionBehaviourName = rIniFile.GetString( rszName.c_str(), "ActionBehaviour", "", pBuffer, 0xFFF );
	NStr::ToLower( szActionBehaviourName ); 
	for ( int nABIndex = AB_LOCK; nABIndex < AB_COUNT; ++nABIndex )
	{
		if ( szActionBehaviourName == ACTION_BEHAVIOUR_NAMES[nABIndex] )
		{
			actionBehaviour = static_cast<EActionBehaviour>( nABIndex );
		}
	}
	//
	szActionTarget = rIniFile.GetString( rszName.c_str(), "ActionTarget", "", pBuffer, 0xFFF );
	szActionParameter = rIniFile.GetString( rszName.c_str(), "ActionParameter", "", pBuffer, 0xFFF );
	szActionFolderName = rIniFile.GetString( rszName.c_str(), "ActionFolder", "", pBuffer, 0xFFF );
	
	if ( ( action == ACTION_SHOW_MENU ) && ( !szActionTarget.empty() ) && ( pMenus != 0 ) )
	{
		( *pMenus )[szActionTarget].Load( szActionTarget, rIniFile, pMenus, rMainSection );
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SARMenu::Load( const std::string &rszName, CIniFile& rIniFile, std::unordered_map<std::string, SARMenu> *pMenus,  const SARMainSection &rMainSection )
{
	buttons.clear();
	char pBuffer[0xFFF];
	int nButtonIndex = 0;
	std::string szButtonName = rIniFile.GetString( rszName.c_str(), NStr::Format( "Button%d", nButtonIndex ), "", pBuffer, 0xFFF );
	while ( !szButtonName.empty() )
	{
		buttons.push_back( SARButton() );	
		buttons.back().Load( szButtonName, rIniFile, pMenus, rMainSection );
		++nButtonIndex;
		szButtonName = rIniFile.GetString( rszName.c_str(), NStr::Format( "Button%d", nButtonIndex ), "", pBuffer, 0xFFF );
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SARMainSection::SRegistryKey::Load( const std::string &rszName, const std::string &rszPrefix, CIniFile& rIniFile )
{
	char pBuffer[0xFFF];

	type = SARMainSection::RT_HKLM;
	std::string szRegistryType = rIniFile.GetString( rszName.c_str(), ( std::string( "RT" ) + rszPrefix ).c_str(), "", pBuffer, 0xFFF );
	NStr::ToLower( szRegistryType ); 
	for ( int nRegistryTypeIndex = SARMainSection::RT_HKLM; nRegistryTypeIndex < SARMainSection::RT_COUNT; ++nRegistryTypeIndex )
	{
		if ( szRegistryType == SARMainSection::RT_NAMES[nRegistryTypeIndex] )
		{
			type = static_cast<SARMainSection::ERegistryType>( nRegistryTypeIndex );
		}
	}
	szKey = rIniFile.GetString( rszName.c_str(), ( std::string( "RK" ) + rszPrefix ).c_str(), "", pBuffer, 0xFFF );
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SARMainSection::Load( const std::string &rszName, CIniFile& rIniFile )
{
	char pBuffer[0xFFF];

	szWrongDiskTitleFileName = rIniFile.GetString( rszName.c_str(), "WrongDiskTitleFileName", "", pBuffer, 0xFFF );;
	szWrongDiskMessageFileName = rIniFile.GetString( rszName.c_str(), "WrongDiskMessageFileName", "", pBuffer, 0xFFF );;
	szRuningGameTitle = rIniFile.GetString( rszName.c_str(), "RunnigGameTitle", "", pBuffer, 0xFFF );
	szRuningInstallTitle = rIniFile.GetString( rszName.c_str(), "RunnigInstallTitle", "", pBuffer, 0xFFF );
	szTitleFileName = rIniFile.GetString( rszName.c_str(), "TitleFileName", "", pBuffer, 0xFFF );
	szSoundFileName = rIniFile.GetString( rszName.c_str(), "SoundFileName", "", pBuffer, 0xFFF );
	szBackgroundImageFileName = rIniFile.GetString( rszName.c_str(), "BackgroundImageFileName", "", pBuffer, 0xFFF );
	
	int nLogoIndex = 0;
	while ( true )
	{
		SLogo logo;
		logo.szImageFileName = rIniFile.GetString( rszName.c_str(), NStr::Format( "LogoImageFileName%d", nLogoIndex ), "", pBuffer, 0xFFF );
		logo.position.x = rIniFile.GetInt( rszName.c_str(), NStr::Format( "LogoImagePosX%d", nLogoIndex ), 0 );
		logo.position.y = rIniFile.GetInt( rszName.c_str(), NStr::Format( "LogoImagePosY%d", nLogoIndex ), 0 );
		if ( !logo.szImageFileName.empty() )
		{
			logos.push_back( logo );
			++nLogoIndex;
		}
		else
		{
			break;
		}
	}
	
	nCodePage = rIniFile.GetInt( rszName.c_str(), "CodePage", 0 );
	bShowToolTips = ( rIniFile.GetInt( rszName.c_str(), "ShowToolTips", 0 ) > 0 );
	szFontName = rIniFile.GetString( rszName.c_str(), "FontName", "", pBuffer, 0xFFF );
	nFontWeight = rIniFile.GetInt( rszName.c_str(), "FontWeight", 0 );

	szMenuName = rIniFile.GetString( rszName.c_str(), "Menu", "", pBuffer, 0xFFF );

	shadowPoint.x = rIniFile.GetInt( rszName.c_str(), "ShadowPointX", 0 );
	shadowPoint.y = rIniFile.GetInt( rszName.c_str(), "ShadowPointY", 0 );

	dwColors.resize( 4 );
	dwColors[STATE_NORMAL] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "NormalColor", 0xFFB1B1B1 ) );
	dwColors[STATE_FOCUSED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "FocusedColor", 0xFFFFFFFF ) );
	dwColors[STATE_SELECTED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "SelectedColor", 0xFF5A5A5A ) );
	dwColors[STATE_DISABLED] = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "DisabledColor", 0xFF323232 ) );
	dwShadowColor = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "ShadowColor", 0xFF000000 ) );

	dwAlign = static_cast<DWORD>( rIniFile.GetInt( rszName.c_str(), "LabelAlign", ALIGN_LEFT | ALIGN_TOP ) );
	
	rkInstallFolder.Load( rszName, std::string( "InstallFolder" ), rIniFile );
	rkUninstallFolder.Load( rszName, std::string( "UninstallFolder" ), rIniFile );
	szGameInstalledFileName = rIniFile.GetString( rszName.c_str(), "GameInstalledFileName", "", pBuffer, 0xFFF );
	menus.clear();
	if ( !szMenuName.empty() )
	{
		menus[szMenuName].Load( szMenuName, rIniFile, &menus, ( *this ) );
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CARMenuSelector::PlayStartSound()
{
	const CARSound* pARSound = dataStorage.GetSound( mainSection.szSoundFileName );
	if ( pARSound )
	{
		const std::vector<BYTE>& rData = pARSound->Get();
		::PlaySound( reinterpret_cast<const char *>( &( rData[0] ) ), 0, SND_MEMORY | SND_ASYNC | SND_NOSTOP );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CARMenuSelector::DrawBackgroundAndLogos( CDC *pDC )
{
	{
		const CARBitmap* pARBitmap = dataStorage.GetBitmap( mainSection.szBackgroundImageFileName );
		if ( pARBitmap )
		{
			CBitmap &rBitmap = const_cast<CBitmap&>( pARBitmap->Get() );
			const CPoint &rSize = pARBitmap->GetSize();
			
			CDC memDC;
			int nRes = memDC.CreateCompatibleDC( pDC );
			CBitmap *pOldBitmap = memDC.SelectObject( &rBitmap );
			pDC->BitBlt( 0, 0, rSize.x, rSize.y, &memDC, 0, 0, SRCCOPY );
			memDC.SelectObject( pOldBitmap );
		}
	}
	for ( int nLogoIndex = 0; nLogoIndex < mainSection.logos.size(); ++nLogoIndex )
	{
		const SARMainSection::SLogo &rLogo = mainSection.logos[nLogoIndex];

		const CARBitmap* pARBitmap = dataStorage.GetBitmap( rLogo.szImageFileName );
		if ( pARBitmap )
		{
			CBitmap &rBitmap = const_cast<CBitmap&>( pARBitmap->Get() );
			const CPoint &rSize = pARBitmap->GetSize();

			CDC memDC;
			int nRes = memDC.CreateCompatibleDC( pDC );
			CBitmap *pOldBitmap = memDC.SelectObject( &rBitmap );
			pDC->BitBlt( rLogo.position.x, rLogo.position.y, rSize.x, rSize.y, &memDC, 0, 0, SRCCOPY );
			memDC.SelectObject( pOldBitmap );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CARMenuSelector::HitTest( const CPoint &rMousePoint )
{
	const SARMenu &rMenu = mainSection.menus[ menuStack.back() ];
	for ( int nButtonIndex = 0; nButtonIndex < rMenu.buttons.size(); ++nButtonIndex )
	{
		const SARButton &rARButton = rMenu.buttons[nButtonIndex];
		if ( ( ( bGameInstalled && ( ( rARButton.dwPresentState & SARButton::GAME_INSTALLED ) == SARButton::GAME_INSTALLED ) ) ||
				   ( ( !bGameInstalled ) && ( ( rARButton.dwPresentState & SARButton::GAME_NOT_INSTALLED ) == SARButton::GAME_NOT_INSTALLED ) ) ) &&
				 ( ( bGameInstalled && ( ( rARButton.dwEnableState & SARButton::GAME_INSTALLED ) == SARButton::GAME_INSTALLED ) ) ||
				   ( ( !bGameInstalled ) && ( ( rARButton.dwEnableState & SARButton::GAME_NOT_INSTALLED ) == SARButton::GAME_NOT_INSTALLED ) ) ) )
		{
			if ( rARButton.position.PtInRect( rMousePoint ) )
			{
				return nButtonIndex;
			}
		}
	}
	return ( -1 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CARMenuSelector::DrawMenu( CDC *pDC, const CPoint &rMousePoint, int nMouseFlags )
{
	int nHitButton = HitTest( rMousePoint );
	const SARMenu &rMenu = mainSection.menus[ menuStack.back() ];
	for ( int nButtonIndex = 0; nButtonIndex < rMenu.buttons.size(); ++nButtonIndex )
	{
		const SARButton &rARButton = rMenu.buttons[nButtonIndex];
		if ( ( bGameInstalled && ( ( rARButton.dwPresentState & SARButton::GAME_INSTALLED ) == SARButton::GAME_INSTALLED ) ) ||
				 ( ( !bGameInstalled ) && ( ( rARButton.dwPresentState & SARButton::GAME_NOT_INSTALLED ) == SARButton::GAME_NOT_INSTALLED ) ) )
		{
			const CARText* pARText = dataStorage.GetText( rARButton.szLabelFileName );
			if ( pARText )
			{
				const CString &rszText = pARText->Get();
				
				UINT format = DT_NOCLIP | DT_SINGLELINE;
				if ( ( rARButton.dwAlign & SARMainSection::ALIGN_HOR_CENTER ) == SARMainSection::ALIGN_HOR_CENTER )
				{
					format |= DT_CENTER;
				}
				else if ( ( rARButton.dwAlign & SARMainSection::ALIGN_RIGHT ) == SARMainSection::ALIGN_RIGHT )
				{
					format |= DT_RIGHT;
				}
				else
				{
					format |= DT_LEFT;
				}
				if ( ( rARButton.dwAlign & SARMainSection::ALIGN_VER_CENTER ) == SARMainSection::ALIGN_VER_CENTER )
				{
					format |= DT_VCENTER;
				}
				else if ( ( rARButton.dwAlign & SARMainSection::ALIGN_BOTTOM ) == SARMainSection::ALIGN_BOTTOM )
				{
					format |= DT_BOTTOM;
				}
				else
				{
					format |= DT_TOP;
				}

				CFont font;
				VERIFY( font.CreateFont( rARButton.nFontSize,		// nHeight
																 0,														// nWidth
																 0,														// nEscapement
																 0,														// nOrientation
																 mainSection.nFontWeight,			// nWeight
																 FALSE,												// bItalic
																 FALSE,												// bUnderline
																 0,														// cStrikeOut
																 DEFAULT_CHARSET,							// nCharSet
																 OUT_DEFAULT_PRECIS,					// nOutPrecision
																 CLIP_DEFAULT_PRECIS,					// nClipPrecision
																 ANTIALIASED_QUALITY,					// nQuality
																 DEFAULT_PITCH,								// nPitchAndFamily
																 mainSection.szFontName.c_str() ) );	// lpszFacename
				
				CFont* pOldFont = pDC->SelectObject( &font );
				int nOldBkMode = pDC->SetBkMode( TRANSPARENT );
				
				if ( GetAlphaFromARGBB( rARButton.dwShadowColor ) > 0 && ( ( mainSection.shadowPoint.x != 0 ) || ( mainSection.shadowPoint.y != 0 ) ) )
				{
					pDC->SetTextColor( GetRGBColorFromARGBB( rARButton.dwShadowColor ) );
					CRect shadowRect = rARButton.position;
					shadowRect.left += mainSection.shadowPoint.x;
					shadowRect.top += mainSection.shadowPoint.y;
					shadowRect.right += mainSection.shadowPoint.x;
					shadowRect.bottom += mainSection.shadowPoint.y;
					pDC->DrawText( rszText, &shadowRect, format );
				}
				DWORD dwTextColor = 0;
				if ( ( bGameInstalled && ( ( rARButton.dwEnableState & SARButton::GAME_INSTALLED ) == SARButton::GAME_INSTALLED ) ) ||
						 ( ( !bGameInstalled ) && ( ( rARButton.dwEnableState & SARButton::GAME_NOT_INSTALLED ) == SARButton::GAME_NOT_INSTALLED ) ) )
				{
					if ( nButtonIndex == nHitButton )
					{
						if ( nMouseFlags & MK_LBUTTON )
						{
							dwTextColor = rARButton.dwColors[SARMainSection::STATE_SELECTED];
						}
						else
						{
							dwTextColor = rARButton.dwColors[SARMainSection::STATE_FOCUSED];

							if ( nCurrentFocusedButton != nButtonIndex )
							{
								const CARSound* pARSound = dataStorage.GetSound( rARButton.szFocusedSoundFileName );
								if ( pARSound )
								{
									const std::vector<BYTE>& rData = pARSound->Get();
									::PlaySound( reinterpret_cast<const char *>( &( rData[0] ) ), 0, SND_MEMORY | SND_ASYNC | SND_NOSTOP );
								}
								nCurrentFocusedButton = nButtonIndex;
							}
						}
					}
					else
					{
						dwTextColor = rARButton.dwColors[SARMainSection::STATE_NORMAL];
						if( nCurrentFocusedButton == nButtonIndex )
						{
							nCurrentFocusedButton = -1;
						}
					}
				}
				else
				{
					dwTextColor = rARButton.dwColors[SARMainSection::STATE_DISABLED];
				}
				if ( GetAlphaFromARGBB( dwTextColor ) > 0 )
				{
					pDC->SetTextColor( GetRGBColorFromARGBB( dwTextColor ) );
					pDC->DrawText( rszText, const_cast<CRect*>( &( rARButton.position ) ), format );
				}

				pDC->SetBkMode( nOldBkMode );
				pDC->SelectObject( pOldFont );

				font.DeleteObject(); 
			}
		}
	}
}
	
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::Load( CWnd *pWnd )
{
	mainSection.Clear();
	dataStorage.Clear();

	ASSERT( pWnd );
	CDC *pDC = pWnd->GetDC();
	{	
		char pBuffer[0xfff];
		GetModuleFileName( 0, pBuffer, 0xfff );
		std::string szFileName( pBuffer );
		szFileName = szFileName.substr( 0, szFileName.rfind( '\\' ) );
		szFileName += std::string( _T( "\\" ) ) + std::string( DATA_FILE_NAME );
		dataStorage.Load( szFileName, pDC );
	}
	pWnd->ReleaseDC( pDC );

	const CARConfiguration *pARConfiguration = dataStorage.GetConfiguration( CONFIGURATION_FILE_NAME );
	if ( pARConfiguration )
	{
		mainSection.Load( "AutoRun", const_cast<CIniFile&>( pARConfiguration->Get() ) );
		
		dataStorage.SetCodePage( mainSection.nCodePage );
		if ( menuStack.empty() )
		{
			menuStack.push_back( mainSection.szMenuName );
		}

		for ( int nFolderIndex = AF_INSTALL; nFolderIndex < AF_COUNT; ++nFolderIndex )
		{
			szActionFolders[nFolderIndex].clear();
		}

    //current folder
		{
			char pBuffer[0xfff];
			GetModuleFileName( 0, pBuffer, 0xfff );
			std::string szExeName( pBuffer );
			szExeName = szExeName.substr( 0, szExeName.rfind( '\\' ) );
			szActionFolders[AF_CURRENT] = szExeName.c_str();
		}
		//install folder
		{
			DWORD dwDisposition;
			//install folder
			{
				HKEY hRegistrySection;
				std::string szRegistryFolder = mainSection.rkInstallFolder.szKey.substr( 0, mainSection.rkInstallFolder.szKey.rfind( '\\' ) );
				std::string szRegistryKey = mainSection.rkInstallFolder.szKey.substr( mainSection.rkInstallFolder.szKey.rfind( '\\' ) + 1 );
				if ( ::RegCreateKeyEx( ( mainSection.rkInstallFolder.type == SARMainSection::RT_HKLM ) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
															 szRegistryFolder.c_str(),
															 0,
															 0,
															 REG_OPTION_NON_VOLATILE,
															 KEY_READ,
															 0,
															 &hRegistrySection,
															 &dwDisposition ) == ERROR_SUCCESS )
				{
					BYTE pBuffer[0xfff];
					DWORD dwLoadValueLength = 0xfff;
					DWORD dwLoadValueType = REG_SZ;
					if ( ( RegQueryValueEx( hRegistrySection,
																	szRegistryKey.c_str(),
																	0,
																	&dwLoadValueType,
																	pBuffer,
																	&dwLoadValueLength ) == ERROR_SUCCESS ) &&
							 ( dwLoadValueType == REG_SZ ) )
					{
						szActionFolders[AF_INSTALL] = reinterpret_cast<char*>(pBuffer);
						if ( szActionFolders[AF_INSTALL][szActionFolders[AF_INSTALL].size() - 1] == '\\' )
						{
							szActionFolders[AF_INSTALL] = szActionFolders[AF_INSTALL].substr( 0, szActionFolders[AF_INSTALL].size() - 1 );
						}
					}
				}
				::RegCloseKey( hRegistrySection );
			}
			
			//uninstall folder
			{
				HKEY hRegistrySection;
				std::string szRegistryFolder = mainSection.rkUninstallFolder.szKey.substr( 0, mainSection.rkUninstallFolder.szKey.rfind( '\\' ) );
				std::string szRegistryKey = mainSection.rkUninstallFolder.szKey.substr( mainSection.rkUninstallFolder.szKey.rfind( '\\' ) + 1 );
				if ( ::RegCreateKeyEx( ( mainSection.rkUninstallFolder.type == SARMainSection::RT_HKLM ) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
															 szRegistryFolder.c_str(),
															 0,
															 0,
															 REG_OPTION_NON_VOLATILE,
															 KEY_READ,
															 0,
															 &hRegistrySection,
															 &dwDisposition ) == ERROR_SUCCESS )
				{
					BYTE pBuffer[0xfff];
					DWORD dwLoadValueLength = 0xfff;
					DWORD dwLoadValueType = REG_SZ;
					if ( ( RegQueryValueEx( hRegistrySection,
																	szRegistryKey.c_str(),
																	0,
																	&dwLoadValueType,
																	pBuffer,
																	&dwLoadValueLength ) == ERROR_SUCCESS ) &&
							 ( dwLoadValueType == REG_SZ ) )
					{
						szActionFolders[AF_UNINSTALL] = reinterpret_cast<char*>(pBuffer);
						if ( szActionFolders[AF_UNINSTALL][szActionFolders[AF_UNINSTALL].size() - 1] == '\\' )
						{
							szActionFolders[AF_UNINSTALL] = szActionFolders[AF_UNINSTALL].substr( 0, szActionFolders[AF_UNINSTALL].size() - 1 );
						}
					}
				}
				::RegCloseKey( hRegistrySection );
			}
		}

		bGameInstalled = ( !szActionFolders[AF_INSTALL].empty() && 
											 NFile::IsFileExist( ( szActionFolders[AF_INSTALL] + 
																						 std::string( "\\" ) + 
																						 mainSection.szGameInstalledFileName ).c_str() ) );
		nCurrentFocusedButton = -1;
		return true;	
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::Action( CAutoRunDialog *pWnd, const CPoint &rMousePoint )
{
	int nHitButton = HitTest( rMousePoint );
	if ( nHitButton >= 0 )
	{
		const SARMenu &rMenu = mainSection.menus[ menuStack.back() ];
		const SARButton &rARButton = rMenu.buttons[nHitButton];
		
		const CARSound* pARSound = dataStorage.GetSound( rARButton.szActionSoundFileName );
		if ( pARSound )
		{
			const std::vector<BYTE>& rData = pARSound->Get();
			::PlaySound( reinterpret_cast<const char *>( &( rData[0] ) ), 0, SND_MEMORY | SND_SYNC | SND_NOSTOP );
		}

		switch( rARButton.action )
		{
			case SARButton::ACTION_OPEN:
			{
				bool bCurrent = false;
				std::string szFolder = ParseFolder( rARButton.szActionFolderName, &bCurrent );
				if ( !ExecuteTarget( pWnd, rARButton.szActionTarget, rARButton.szActionParameter, szFolder, rARButton.actionBehaviour == SARButton::AB_LOCK, bCurrent ) )
				{
					return false;
				}
				return ( rARButton.actionBehaviour == SARButton::AB_CLOSE );
				break;
			}
			case SARButton::ACTION_SHELL_OPEN:
			{
				bool bCurrent = false;
				std::string szFolder = ParseFolder( rARButton.szActionFolderName, &bCurrent );
				if ( !ShellExecuteTarget( pWnd, rARButton.szActionTarget, rARButton.szActionParameter, szFolder, rARButton.actionBehaviour == SARButton::AB_LOCK, bCurrent ) )
				{
					return false;
				}
				return ( rARButton.actionBehaviour == SARButton::AB_CLOSE );
				break;
			}
			case SARButton::ACTION_SHOW_MENU:
			{
				menuStack.push_back( rARButton.szActionTarget );
				break;
			}
			case SARButton::ACTION_RETURN:
			{
				return ReturnMenu();
				break;
			}
			case SARButton::ACTION_CLOSE:
			{
				return true;
				break;
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::ReturnMenu()
{
	menuStack.pop_back();
	return menuStack.empty();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::ActionShortcut( int nShortcut )
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CARMenuSelector::FillToolTips( CToolTipCtrl *pToolTipCtrl, const CPoint &rMousePoint )
{
	if ( pToolTipCtrl )
	{
		int nToolCount = pToolTipCtrl->GetToolCount();
		for ( int nToolID = 0; nToolID < nToolCount; ++nToolID )
		{
			pToolTipCtrl->DelTool( pToolTipCtrl->GetParent(), nToolID + 1 );
		}
		
		const SARMenu &rMenu = mainSection.menus[ menuStack.back() ];
		for ( int nButtonIndex = 0; nButtonIndex < rMenu.buttons.size(); ++nButtonIndex )
		{
			const SARButton &rARButton = rMenu.buttons[nButtonIndex];
			const CARText* pARText = dataStorage.GetText( rARButton.szTooltipFileName );
			if ( pARText )
			{
				const CString &rszText = pARText->Get();
				pToolTipCtrl->AddTool( pToolTipCtrl->GetParent(), static_cast<LPCTSTR>( rszText ), static_cast<LPCRECT>( &( rARButton.position ) ), nButtonIndex + 1 );
			}
		}
	}
	nCurrentFocusedButton = HitTest( rMousePoint );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CString CARMenuSelector::GetTitle()
{
	CString szTitle;
	const CARText* pARText = dataStorage.GetText( mainSection.szTitleFileName );
	if ( pARText )
	{
		szTitle = pARText->Get();
	}
	return CString(" ") + szTitle;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CString CARMenuSelector::GetRunningGameTitle()
{
	return CString(" ") + mainSection.szRuningGameTitle.c_str();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CString CARMenuSelector::GetRunningInstallTitle()
{
	return mainSection.szRuningInstallTitle.c_str();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::ExecuteTarget( CAutoRunDialog *pWnd, const std::string &szTarget, const std::string &szParameters, const std::string &szDirectory, bool bWait, bool bCurrent )
{
	char pszCommandLine[0xFFF];
	if ( !szDirectory.empty() )
	{
		strcpy( pszCommandLine, "\"" );
		strcat( pszCommandLine, szDirectory.c_str() );
		strcat( pszCommandLine, "\\");
		strcat( pszCommandLine, szTarget.c_str() );
		strcat( pszCommandLine, "\"" );
	}
	else
	{
		strcpy( pszCommandLine, "\"" );
		strcat( pszCommandLine, szTarget.c_str() );
		strcat( pszCommandLine, "\"" );
	}
	if ( !szParameters.empty() )
	{
		strcat( pszCommandLine, " " );
		strcat( pszCommandLine, szParameters.c_str() );
	}
	//
	STARTUPINFO startinfo;
	PROCESS_INFORMATION procinfo;
	memset( &startinfo, 0, sizeof( STARTUPINFO ) );
	memset( &procinfo, 0, sizeof( PROCESS_INFORMATION ) );
	startinfo.cb = sizeof( startinfo );
	bool bRetVal = false;
	DWORD dwResult = 0;
	if ( szDirectory.empty() )
	{
		bRetVal = CreateProcess( 0, pszCommandLine, 0, 0, false, 0, 0, 0, &startinfo, &procinfo );
		dwResult = GetLastError();
	}
	else
	{
		bRetVal = CreateProcess( 0, pszCommandLine, 0, 0, false, 0, 0, szDirectory.c_str(), &startinfo, &procinfo );
		dwResult = GetLastError();
	}
	//pWnd->MessageBox( NStr::Format( "pszCommandLine <%s>\nszDirectory <%s>\nbRetVal %d, dwResult %d", pszCommandLine, szDirectory.c_str(), bRetVal, dwResult ), "Debug Output", MB_OK );
	if ( bRetVal != FALSE ) 
	{
		if ( bWait )
		{
			if ( pWnd )
			{
				pWnd->ShowWindow( SW_HIDE );
			}
			DWORD dwWaitObject = WaitForSingleObject( procinfo.hProcess, INFINITE );
			Load( pWnd );
			if ( pWnd )
			{
				if ( !pWnd->CheckGameApp( 0, GetRunningGameTitle() ) )
				{
					pWnd->CDialog::OnOK();
				}
				else if ( !pWnd->CheckGameApp( 0, GetRunningInstallTitle() ) )
				{
					pWnd->CDialog::OnOK();
				}
				else
				{
					pWnd->dwFinishTimerCount = 0;
					pWnd->SetFinishTimer();
					pWnd->ShowWindow( SW_SHOW );
				}
			}
		}
		::CloseHandle( procinfo.hProcess );
		::CloseHandle( procinfo.hThread );
		return true;
	}
	else
	{
		if ( bCurrent && !mainSection.szWrongDiskTitleFileName.empty() && !mainSection.szWrongDiskMessageFileName.empty() )
		{
			CString szTitle;
			CString szMessage;
			{
				const CARText* pARText = dataStorage.GetText( mainSection.szWrongDiskTitleFileName );
				if ( pARText )
				{
					szTitle = pARText->Get();
				}
			}
			{
				const CARText* pARText = dataStorage.GetText( mainSection.szWrongDiskMessageFileName );
				if ( pARText )
				{
					szMessage = pARText->Get();
				}
			}
			if ( !szTitle.IsEmpty() && !szMessage.IsEmpty() )
			{
				pWnd->MessageBox( szMessage, szTitle, MB_ICONEXCLAMATION | MB_OK ); 
				return false;
			}
		}
		pWnd->MessageBox( NStr::Format( "CreateProcess return error: %d.\nCommand: <%s>.\nFolder: <%s>.", GetLastError(), pszCommandLine, szDirectory.c_str() ), "Error!", MB_ICONSTOP | MB_OK ); 
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CARMenuSelector::ShellExecuteTarget( CAutoRunDialog *pWnd, const std::string &szTarget, const std::string &szParameters, const std::string &szDirectory, bool bWait, bool bCurrent )
{
	char pszCommandLine[0xFFF];
	if ( !szDirectory.empty() )
	{
		//strcpy( pszCommandLine, "\"" );
		strcpy( pszCommandLine, szDirectory.c_str() );
		strcat( pszCommandLine, "\\");
		strcat( pszCommandLine, szTarget.c_str() );
		//strcat( pszCommandLine, "\"" );
	}
	else
	{
		//strcpy( pszCommandLine, "\"" );
		strcpy( pszCommandLine, szTarget.c_str() );
		//strcat( pszCommandLine, "\"" );
	}
	int nResult = 33;
	if ( szDirectory.empty() )
	{
		nResult = reinterpret_cast<int>( ShellExecute( pWnd->m_hWnd, "open", pszCommandLine, szParameters.c_str(), 0, SW_SHOWNORMAL ) ); 
	}
	else
	{
		nResult = reinterpret_cast<int>( ShellExecute( pWnd->m_hWnd, "open", pszCommandLine, szParameters.c_str(), szDirectory.c_str(), SW_SHOWNORMAL ) );
	}
	if ( nResult > 32 )
	{
		return true;
	}
	else
	{
		if ( bCurrent && !mainSection.szWrongDiskTitleFileName.empty() && !mainSection.szWrongDiskMessageFileName.empty() )
		{
			CString szTitle;
			CString szMessage;
			{
				const CARText* pARText = dataStorage.GetText( mainSection.szWrongDiskTitleFileName );
				if ( pARText )
				{
					szTitle = pARText->Get();
				}
			}
			{
				const CARText* pARText = dataStorage.GetText( mainSection.szWrongDiskMessageFileName );
				if ( pARText )
				{
					szMessage = pARText->Get();
				}
			}
			if ( !szTitle.IsEmpty() && !szMessage.IsEmpty() )
			{
				pWnd->MessageBox( szMessage, szTitle, MB_ICONEXCLAMATION | MB_OK ); 
				return false;
			}
		}
		pWnd->MessageBox( NStr::Format( "ShellExecute return error: %d.\nCommand: <%s>.\nParametrs: <%s>.\nFolder: <%s>.", nResult, pszCommandLine, szParameters.c_str(), szDirectory.c_str() ), "Error!", MB_ICONSTOP | MB_OK ); 
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CARMenuSelector::ParseFolder( const std::string &rszFolder, bool *pbCurrent )
{
	std::string szFolder = rszFolder;
	if ( szFolder[szFolder.size() - 1] == '\\' )
	{
		szFolder = szFolder.substr( 0, szFolder.size() - 1 );
	}
	//
	if ( pbCurrent )
	{
		( *pbCurrent ) = false;
	}
	//
	if ( szFolder.find( "<" ) == 0 )
	{
		int nArrowchar = szFolder.find( ">" );
		if ( nArrowchar != std::string::npos )
		{
			std::string szLabel = szFolder.substr( 1, nArrowchar - 1 );
			const std::string szAdditionFolder = szFolder.substr( nArrowchar + 1 );
			NStr::ToLower( szLabel );
			int nFolderIndex = AF_INSTALL;
			for ( ; nFolderIndex < AF_COUNT; ++nFolderIndex )
			{
				if ( szLabel == ACTION_FOLDER_NAMES[nFolderIndex] )
				{
					szLabel = szActionFolders[nFolderIndex];
					if ( szLabel[szLabel.size() - 1] == '\\' )
					{
						szLabel = szLabel.substr( 0, szLabel.size() - 1 );
					}
					//
					if ( pbCurrent )
					{
						if ( nFolderIndex == AF_CURRENT )
						{
							( *pbCurrent ) = true;
						}
					}
					break;
				}
			}
			if ( nFolderIndex == AF_COUNT )
			{
				return szAdditionFolder;
			}
			else
			{
				return szLabel + szAdditionFolder;
			}
		}
	}
	return szFolder;
}	
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
