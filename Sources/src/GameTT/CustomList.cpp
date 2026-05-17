#include "StdAfx.h"

#include "..\Main\GameStats.h"
#include "CommonId.h"
#include "CustomList.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceCustomList::~CInterfaceCustomList()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceCustomList::FillListFromCurrentDir()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( 1000 ) );
	NI_ASSERT( pList != 0 );
	ITextManager *pTextM = GetSingleton<ITextManager>();
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	
	//������� ��� items �� ListControl
	for ( int i=pList->GetNumberOfItems()-1; i>=0; i-- )
	{
		pList->RemoveItem( i );
	}
	
	// enumerate all available files and dirs
	dirsList.clear();
	filesList.clear();
	std::vector< std::string > dirs;		//��������� ��������� ����������
	std::vector< std::string > files;
	
	IFilesInspectorEntryCollector *pCollector = checked_cast<IFilesInspectorEntryCollector *>( GetSingleton<IFilesInspector>()->GetEntry( szCollectorName.c_str() ) );
	const std::vector<std::string> &tutorialFiles = pCollector->GetCollected();
	std::unordered_set<std::string> setOfDirs;		//����� � �������� ��� ����������
	
	for ( int i=0; i<tutorialFiles.size(); i++ )
	{
		if ( strncmp( tutorialFiles[i].c_str(), szCurrentDir.c_str(), szCurrentDir.size() ) )
			continue;			//�� �����
		
		//��������, ��� ������ ���� ��������� ����������
		std::string szCurrentName = tutorialFiles[i].c_str() + szCurrentDir.size();
		int nPos = szCurrentName.rfind( '\\' );
		if ( nPos != std::string::npos )		//������ ���� ��� ����������
		{
			//������� ��� ����������
			szCurrentName = szCurrentName.substr( 0, szCurrentName.find('\\') );
			setOfDirs.insert( szCurrentName );
			continue;
		}
	}
	
	for ( std::unordered_set<std::string>::iterator it = setOfDirs.begin(); it != setOfDirs.end(); ++it )
	{
		//������ ��� �������� ������������� ������ ������ ����������
		std::string szCmpDir = szCurrentDir + *it;
		szCmpDir += '\\';
		std::unordered_set<std::string> setOfSubDirs;
		for ( int i=0; i<tutorialFiles.size(); i++ )
		{
			std::string szCurrentName = tutorialFiles[i];
			if ( strncmp( szCurrentName.c_str(), szCmpDir.c_str(), szCmpDir.size() ) )
				continue;			//�� �����
			szCurrentName = szCurrentName.substr( szCmpDir.size() );
			
			//��������, ��� ��� �������������
			int nPos = szCurrentName.rfind( '\\' );
			if ( nPos != std::string::npos )		//������ ���� ��� ����������
			{
				szCurrentName = szCurrentName.substr( 0, nPos );
				setOfSubDirs.insert( szCurrentName );
				continue;
			}
			
			//��������, ��� ��� �� ����
			nPos = szCurrentName.rfind( '.' );
			if ( nPos == std::string::npos )
				continue;

			std::string szExtension = szCurrentName.substr( nPos );
			for ( int k=0; k<fileMasks.size(); k++ )
			{
				if ( szExtension == fileMasks[k].c_str() + 1 )
				{
					//��������� ����� �����, ��������� � ������ ������
					std::string szName = szCmpDir;
					szName += szCurrentName;
					files.push_back( szName );
				}
			}
		}

		for ( std::unordered_set<std::string>::iterator it=setOfSubDirs.begin(); it!=setOfSubDirs.end(); ++it )
		{
			dirs.push_back( *it );
		}
	}
	
	
	int nIndex = 0;
	if ( szCurrentDir.size() > szTopDir.size() )
		dirs.push_back( ".." );
	
	// add strings to list control
	for ( int i=0; i<dirs.size(); i++ )
	{
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nIndex );
		pRow->SetUserData( nIndex );
		nIndex++;
		
		//��������� ��� ����������
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		dirsList.push_back( dirs[i] );
		std::wstring wszTemp;
		NStr::ToUnicode( &wszTemp, dirs[i] );
		pContainer->SetWindowText( 0, wszTemp.c_str() );
		
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
		// ������� ���� �� �����, ��� ����� �� �������� SBasicGameStats
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
		
		//�������� ���������� ������ ���� ������
		filesList.push_back( szName.substr( szCurrentDir.size() ) );
		
		//������� �������������� ��� � ������ �� ������
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nIndex );
		pRow->SetUserData( nIndex );
		nIndex++;
		
		//��������� ��� �����
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		pContainer->SetWindowText( 0, p->GetString() );
		
		IUIElement *pElement = pContainer->GetChildByID( 1 );
		NI_ASSERT_T( pElement != 0, "Invalid list control name dialog, it should contain icon" );
		pElement->SetState( 1 );			//����
		//TODO ������ 1 ������� ��������������� �����
		
		FillListItem( pRow, szName );
	}

	pList->InitialUpdate();
	
	//��������� ������� ���������� �������
	if ( pList->GetNumberOfItems() < nBeginSelItem )
		nBeginSelItem = pList->GetNumberOfItems();
	pList->SetSelectionItem( nBeginSelItem );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
				//��������� ����� ������� selection �� list control
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
					//������ ������� ����������
					if ( dirsList[ nSel ] == ".." )
					{
						//���������� ������
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

	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
