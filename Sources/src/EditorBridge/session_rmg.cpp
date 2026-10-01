// The RMG storage-folder scan (M3, D-08/D-21): the lists the Fields tool's
// field-set combo, Create Random Map (05-08) and the composers (05-09/10)
// share. The MFC editor read Editor\Default*.xml list files instead - they
// are not shipped, so its composers opened empty (D-08) - this walks the
// mounted storages' own folders.
//
// Names are storage-relative, lowercased like the MFC combo's own entries
// (TabTerrainFieldsDialog.cpp:138), the .xml stripped, sorted, deduped. A
// folder no storage carries is an empty list, not an error (D-08).
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include <algorithm>
#include <set>

namespace
{

// The folder each kind walks, storage-relative with the trailing separator
// the storage masks want.
const char *RmgFolder( int nKind )
{
	switch ( nKind )
	{
		case 0: return "scenarios\\fieldsets\\";
		case 1: return "scenarios\\templates\\";
		case 2: return "graphs\\";
		case 3: return "scenarios\\containers\\";
		case 4: return "scenarios\\settings\\";
		case 5: return "scenarios\\chapters\\";
		default: return 0;
	}
}

}

bool ListRmgFolder( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames )
{
	pNames->clear();
	if ( pSession == 0 )
		return false;
	const char *pszFolder = RmgFolder( nKind );
	if ( pszFolder == 0 )
	{
		pSession->szMessage = "no such RMG folder kind";
		return false;
	}
	IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
	if ( pDataStorage == 0 )
	{
		pSession->szMessage = "the data storage is not there";
		return false;
	}
	CPtr<IStorageEnumerator> pEnum = pDataStorage->CreateEnumerator();
	if ( pEnum == 0 )
	{
		pSession->szMessage = "the data storage does not enumerate";
		return false;
	}
	std::set<std::string> names;
	// The enumerator walks whole storages: a folder-scoped mask is not a
	// filter, it answers every element (full mount-relative paths) - the
	// folder is this function's own prefix filter, exactly the VSO
	// descriptors' scan (session_vso.cpp VsoDescriptors).
	const std::string szFolder = pszFolder;
	for ( pEnum->Reset( "*.*" ); pEnum->Next(); )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		if ( pStats == 0 || pStats->pszName == 0 )
			continue;
		std::string szName = pStats->pszName;
		NStr::ToLower( szName );
		std::replace( szName.begin(), szName.end(), '/', '\\' );
		// Under the folder, ending .xml, with something between: the whole
		// storage-relative path less the extension - "scenarios\fieldsets\
		// summer\field00", the shape the engine's own templates carry in
		// their Fields lists and LoadDataResource opens back.
		if ( szName.size() <= szFolder.size() + 4 || szName.compare( 0, szFolder.size(), szFolder ) != 0 )
			continue;
		if ( szName.compare( szName.size() - 4, 4, ".xml" ) != 0 )
			continue;
		names.insert( szName.substr( 0, szName.size() - 4 ) );
	}
	pNames->assign( names.begin(), names.end() );
	return true;
}
