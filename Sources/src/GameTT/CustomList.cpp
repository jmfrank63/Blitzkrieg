#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "../Main/GameStats.h"
#include "CommonId.h"
#include "CustomList.h"

namespace
{
	bool StartsWithNoCase( const std::string &szText, const std::string &szPrefix )
	{
		if ( szText.size() < szPrefix.size() )
			return false;
		return NStr::CompareAsciiNoCase( szText.substr( 0, szPrefix.size() ).c_str(), szPrefix.c_str() ) == 0;
	}

	int FindNoCase( const std::string &szText, const std::string &szToken )
	{
		if ( szToken.empty() || szText.size() < szToken.size() )
			return -1;
		for ( int i = 0; i + szToken.size() <= szText.size(); ++i )
		{
			if ( NStr::CompareAsciiNoCase( szText.substr( i, szToken.size() ).c_str(), szToken.c_str() ) == 0 )
				return i;
		}
		return -1;
	}
}

CInterfaceCustomList::~CInterfaceCustomList()
{
}
void CInterfaceCustomList::FillListFromCurrentDir()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( 1000 ) );
	NI_ASSERT( pList != 0 );
	ITextManager *pTextM = GetSingleton<ITextManager>();
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	
	for ( int i=pList->GetNumberOfItems()-1; i>=0; i-- )
	{
		pList->RemoveItem( i );
	}
	
	dirsList.clear();
	filesList.clear();
	std::vector< std::string > dirs;		//��������� ��������� ����������
	std::vector< std::string > files;
	
	IFilesInspectorEntryCollector *pCollector = checked_cast<IFilesInspectorEntryCollector *>( GetSingleton<IFilesInspector>()->GetEntry( szCollectorName.c_str() ) );
	const std::vector<std::string> &tutorialFiles = pCollector->GetCollected();
	std::vector<std::string> normalizedFiles;
	normalizedFiles.reserve( tutorialFiles.size() );
	for ( int i=0; i<tutorialFiles.size(); ++i )
	{
		std::string szNormalized = tutorialFiles[i];
		for ( int k = 0; k < szNormalized.size(); ++k )
		{
			if ( szNormalized[k] == '/' )
				szNormalized[k] = '\\';
		}
		normalizedFiles.push_back( szNormalized );
	}
	if ( getenv( "BK_UI_TRACE" ) )
	{
		fprintf( stderr, "BK_UI_TRACE: custom list collector=%s current=%s collected=%d\n",
			szCollectorName.c_str(), szCurrentDir.c_str(), (int)normalizedFiles.size() );
		if ( szCollectorName == "tutorial" )
		{
			for ( int i = 0; i < normalizedFiles.size() && i < 8; ++i )
				fprintf( stderr, "BK_UI_TRACE: tutorial sample[%d]=%s\n", i, normalizedFiles[i].c_str() );
		}
	}
	std::unordered_set<std::string> setOfDirs;		//����� � �������� ��� ����������
	
	for ( int i=0; i<normalizedFiles.size(); i++ )
	{
		std::string szCurrentFile = normalizedFiles[i];
		if ( !StartsWithNoCase( szCurrentFile, szCurrentDir ) )
		{
			const int nOffset = FindNoCase( szCurrentFile, szCurrentDir );
			if ( getenv( "BK_UI_TRACE" ) && szCollectorName == "tutorial" && i < 3 )
				fprintf( stderr, "BK_UI_TRACE: tutorial match sample[%d] offset=%d file=%s\n", i, nOffset, szCurrentFile.c_str() );
			if ( nOffset < 0 )
				continue;			//�� �����
			szCurrentFile = szCurrentFile.substr( nOffset );
		}

		std::string szCurrentName = szCurrentFile.substr( szCurrentDir.size() );
		int nPos = szCurrentName.rfind( '\\' );
		if ( nPos != std::string::npos )		//������ ���� ��� ����������
		{
			szCurrentName = szCurrentName.substr( 0, szCurrentName.find('\\') );
			setOfDirs.insert( szCurrentName );
			continue;
		}
	}
	if ( getenv( "BK_UI_TRACE" ) && szCollectorName == "tutorial" )
	{
		fprintf( stderr, "BK_UI_TRACE: tutorial top dirs=%d\n", (int)setOfDirs.size() );
		int nPrinted = 0;
		for ( std::unordered_set<std::string>::iterator it = setOfDirs.begin(); it != setOfDirs.end() && nPrinted < 8; ++it, ++nPrinted )
			fprintf( stderr, "BK_UI_TRACE: tutorial top dir[%d]=%s\n", nPrinted, it->c_str() );
	}
	
	// One logical file is collected once per storage that carries it - the game
	// archive and the loose Data tree both do - and each storage's own prefix is
	// stripped below, so they all reduce to the same name. Without this every
	// entry was listed once per storage: five identical tutorials in a row.
	std::unordered_set<std::string> setOfSeenFiles;
	for ( std::unordered_set<std::string>::iterator it = setOfDirs.begin(); it != setOfDirs.end(); ++it )
	{
		std::string szCmpDir = szCurrentDir + *it;
		szCmpDir += '\\';
		std::unordered_set<std::string> setOfSubDirs;
		for ( int i=0; i<normalizedFiles.size(); i++ )
		{
			std::string szCurrentName = normalizedFiles[i];
			if ( !StartsWithNoCase( szCurrentName, szCmpDir ) )
			{
				const int nOffset = FindNoCase( szCurrentName, szCmpDir );
				if ( nOffset < 0 )
					continue;			//�� �����
				szCurrentName = szCurrentName.substr( nOffset );
			}
			szCurrentName = szCurrentName.substr( szCmpDir.size() );
			
			int nPos = szCurrentName.rfind( '\\' );
			if ( nPos != std::string::npos )		//������ ���� ��� ����������
			{
				szCurrentName = szCurrentName.substr( 0, nPos );
				setOfSubDirs.insert( szCurrentName );
				continue;
			}
			
			nPos = szCurrentName.rfind( '.' );
			if ( nPos == std::string::npos )
				continue;

			// Keep the dot: the mask is "*.xml" and the comparison below skips only
			// the star, so it is matched against ".xml". Dropping the dot here made
			// every mask comparison fail, which emptied all four custom lists -
			// tutorials, missions, chapters and campaigns.
			std::string szExtension = szCurrentName.substr( nPos );
			for ( int k=0; k<fileMasks.size(); k++ )
			{
				if ( NStr::CompareAsciiNoCase( szExtension.c_str(), fileMasks[k].c_str() + 1 ) == 0 )
				{
					std::string szName = szCmpDir;
					szName += szCurrentName;
					if ( setOfSeenFiles.insert( szName ).second )
						files.push_back( szName );
				}
			}
		}

		for ( std::unordered_set<std::string>::iterator it=setOfSubDirs.begin(); it!=setOfSubDirs.end(); ++it )
		{
			dirs.push_back( *it );
		}
	}
	if ( getenv( "BK_UI_TRACE" ) )
	{
		fprintf( stderr, "BK_UI_TRACE: custom list collector=%s dirs=%d files=%d\n",
			szCollectorName.c_str(), (int)dirs.size(), (int)files.size() );
	}
	
	
	int nIndex = 0;
	if ( szCurrentDir.size() > szTopDir.size() )
		dirs.push_back( ".." );
	
	for ( int i=0; i<dirs.size(); i++ )
	{
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nIndex );
		pRow->SetUserData( nIndex );
		nIndex++;
		
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		dirsList.push_back( dirs[i] );
		std::wstring wszTemp;
		NStr::ToUnicode( &wszTemp, dirs[i] );
		pContainer->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszTemp.c_str() ) ) );
		
		IUIElement *pElement = pContainer->GetChildByID( 1 );
		NI_ASSERT_T( pElement != 0, "Invalid list control name dialog, it should contain icon" );
		pElement->SetState( 0 );			//����������
		
/*
		std::string szFullName = szCurrentDir;
		szFullName += dirs[i];
		if ( !FillListItem( pRow, szFullName ) )
			return;
*/
	}

	for ( int k=0; k<files.size(); k++ )
	{
		std::string szName = files[k];
		if ( pStorage->IsStreamExist(szName.c_str()) == false )
			continue;
		{
			const int nPos = szName.rfind( '.' );
			if ( nPos != std::string::npos ) 
				szName.resize( nPos );
		}
		const SBasicGameStats *pStats = NGDB::GetGameStats<SBasicGameStats>( szName.c_str(), IObjectsDB::BASIC );
		if ( (pStats == 0) || pStats->szHeaderText.empty() )
			continue;
		
		CPtr<IText> p = pTextM->GetDialog( pStats->szHeaderText.c_str() );
		NI_ASSERT_T( p != 0, (std::string("Can not get text by key") + pStats->szHeaderText).c_str() );
		if ( !p )
			continue;
		
		filesList.push_back( szName.substr( szCurrentDir.size() ) );
		
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nIndex );
		pRow->SetUserData( nIndex );
		nIndex++;
		
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		pContainer->SetWindowText( 0, p->GetString() );
		
		IUIElement *pElement = pContainer->GetChildByID( 1 );
		NI_ASSERT_T( pElement != 0, "Invalid list control name dialog, it should contain icon" );
		pElement->SetState( 1 );			//����
		
		FillListItem( pRow, szName );
	}

	pList->InitialUpdate();
	
	if ( pList->GetNumberOfItems() < nBeginSelItem )
		nBeginSelItem = pList->GetNumberOfItems();
	pList->SetSelectionItem( nBeginSelItem );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
}
bool CInterfaceCustomList::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceBaseList::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			{
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				return true;
			}
/*
		case IMC_OK:
			{
				IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			//�� ������� list control
				int nSelItem = pList->GetSelectionItem();			//������ � ������
				if ( nSelItem == -1 )
					return true;
				
				IUIListRow *pSelRow = pList->GetItem( nSelItem );
				int nSel = pSelRow->GetUserData();						//������ � �������
				if ( nSel < dirsList.size() )
				{
					if ( dirsList[ nSel ] == ".." )
					{
						NI_ASSERT_T( !stack.empty(), "Popup stack is empty" );
						nBeginSelItem = stack.back();
						stack.pop_back();
						szCurrentDir.pop_back();
						szCurrentDir = szCurrentDir.substr( 0, szCurrentDir.rfind('\\')+1 );
					}
					else
					{
						szCurrentDir += dirsList[ nSel ];
						szCurrentDir += "\\";
						stack.push_back( nSelItem );
						nBeginSelItem = 0;
					}
					FillListFromCurrentDir();
				}
				else
				{
					std::string szFullName;
					if ( bStorageFiles )
						szFullName = GetSingleton<IDataStorage>()->GetName();
					szFullName += szCurrentDir;
					szFullName += filesList[nSel - dirsList.size() ];
					OnOk( szFullName.c_str() );
				}
				return true;
			}
*/
	}

	return false;
}
