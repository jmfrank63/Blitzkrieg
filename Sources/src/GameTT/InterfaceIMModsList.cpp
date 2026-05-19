#include "StdAfx.h"

#include "CommonId.h"
#include "..\Main\iMainCommands.h"
#include "InterfaceIMModsList.h"
#include "MainMenu.h"
#include "MultiplayerCommandManager.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceIMModsList::~CInterfaceIMModsList()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceIMModsList::Init()
{
	//инициализируем имена
	fileMasks.clear();
	fileMasks.push_back( "*.pak" );
	szTopDir = GetSingleton<IMainLoop>()->GetBaseDir();
	szTopDir += "Mods\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMModList";
	nSortType = E_SORT_BY_TIME;
	nFirstSortColumn = 0;
	bStorageFiles = false;
	bOnlyDirs = true;
	//
	CInterfaceBaseList::Init();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceIMModsList::PrepareList( std::vector<std::string> *pFiles )
{
	pFiles->push_back( "" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceIMModsList::FillListItem( IUIListRow *pRow, const std::string &szMODPath, bool *pSelectedItem )
{
	IUIElement *pElement = pRow->GetElement( 0 );
	if ( !pElement )
		return false;
	const int nPos = szMODPath.rfind( '\\' );

	if ( nPos == szMODPath.size() - 1 )
	{
		IText *pT = GetSingleton<ITextManager>()->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\remove_all_mods" );
		pElement->SetWindowText( 0, pT->GetString() );
		if ( pSelectedItem && GetGlobalVar( "MOD.Name", "" ) == "" )
			*pSelectedItem = true;
	}
	else
	{
		std::string szMODName = "Unsupported MOD", szMODVersion = "";

		if ( CPtr<IDataStorage> pMOD = OpenStorage((szMODPath + "\\data\\*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON) )
		{
			if ( CPtr<IDataStream> pStream = pMOD->OpenStream("mod.xml", STREAM_ACCESS_READ) )
			{
				{
					CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
					saver.Add( "MODName", &szMODName );
					saver.Add( "MODVersion", &szMODVersion );
					if ( pSelectedItem && GetGlobalVar( "MOD.Name", "" ) == szMODName )
						*pSelectedItem = true;
				}
			}
		}
		const std::string szFullModName = szMODName + " " + szMODVersion;
		const std::wstring wszFullModName = NStr::ToUnicode( szFullModName );
		pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( wszFullModName.c_str() ) );
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceIMModsList::OnOk( const std::string &szFullFileName )
{
	// attach mod
	IMainLoop *pML = GetSingleton<IMainLoop>();

	const int nPos = szFullFileName.rfind( '\\' );
	NI_ASSERT_T( nPos != std::string::npos, NStr::Format( "wrong directory \"%s\"", szFullFileName.c_str() ) );
	{
		const std::string szModName = szFullFileName.substr( nPos + 1 );
		pML->Command( MAIN_COMMAND_CHANGE_MOD, szModName.c_str() );
		pML->Command( MISSION_COMMAND_MAIN_MENU, NStr::Format( "%d", CInterfaceMainMenu::E_OPTIONS ) );
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceIMModsList::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceBaseList::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
		{
			CloseInterface();
			return true;
		}
	}
	//
	return false;
}
