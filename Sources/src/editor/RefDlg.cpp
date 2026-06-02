
#include "stdafx.h"

#include "editor.h"
#include "RefDlg.h"
#include "CtrlObjectInspector.h"
#include "ParentFrame.h"
#include "..\Misc\fileutils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif




CReferenceDialog::CReferenceDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CReferenceDialog::IDD, pParent)
{
	nReferenceType = E_CRATER_REF;

	m_refVal = _T("");
}
void CReferenceDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_REFERENCE_LIST, m_refList);
	DDX_LBString(pDX, IDC_REFERENCE_LIST, m_refVal);
}
BEGIN_MESSAGE_MAP(CReferenceDialog, CDialog)
	ON_LBN_DBLCLK(IDC_REFERENCE_LIST, OnDblclkReferenceList)
END_MESSAGE_MAP()
void CReferenceDialog::Init( int nRefId )
{
	nReferenceType = (EReferenceType) nRefId;
}
string CReferenceDialog::GetValue()
{
	string szResult = (const char *) m_refVal;
	return szResult;
}


BOOL CReferenceDialog::PreTranslateMessage(MSG* pMsg) 
{
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE )
		CDialog::OnCancel();
	
	return CDialog::PreTranslateMessage(pMsg);
}
BOOL CReferenceDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	for ( int i=0; i<m_refList.GetCount(); i++)
		m_refList.DeleteString( i );

	LoadItems( nReferenceType );

	m_refList.SetCurSel( 0 );

	return TRUE;  // return TRUE unless you set the focus to a control
}
void CReferenceDialog::OnDblclkReferenceList() 
{
	CDialog::OnOK();
}
void CReferenceDialog::InitLists()
{
	spritesList.clear();
	particlesList.clear();
	effectsList.clear();
	weaponsList.clear();
	soldiersList.clear();
	scenariomissionsList.clear();
	templatemissionsList.clear();
	chaptersList.clear();
	soundsList.clear();
	settingList.clear();
	asksList.clear();
	craterList.clear();
	deathholeList.clear();
	mapList.clear();
	musicList.clear();
	movieList.clear();
	particleTextureList.clear();
	roadTextureList.clear();
	waterTextureList.clear();
	CPtr<IDataStorage> pDS = GetSingleton<IDataStorage>();
	CPtr<IStorageEnumerator> pEnum = pDS->CreateEnumerator();
	pEnum->Reset( "*.*" );
	CPtr<IObjectsDB> pDB = GetSingleton<IObjectsDB>();
	int nNumDesc = pDB->GetNumDescs();
	while ( pEnum->Next() )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		std::string szName = pStats->pszName;
		if ( pStats->type == SET_STREAM && CheckParentDir( szSpritesDir, szName ) && CheckExtention( "1.xml", szName ) )
			spritesList.push_back( StripFilename( szSpritesDir, "\\1.xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szParticlesDir, szName ) && CheckExtention( ".xml", szName ) )				
			particlesList.push_back( StripFilename( szParticlesDir, ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szEffectsDir, szName ) && CheckExtention( ".xml", szName ) )
			effectsList.push_back( StripFilename( szEffectsDir, ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szWeaponsDir, szName ) && CheckExtention( ".xml", szName ) )
			weaponsList.push_back( StripFilename( szWeaponsDir, ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && ( CheckParentDir( szTemplateDir, szName ) || CheckParentDir( szMissionDir1, szName ) ) && CheckExtention( ".xml", szName ) )				
			templatemissionsList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szSettingDir, szName ) && CheckExtention( ".xml", szName ) )				
			settingList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szCraterDir, szName ) && CheckExtention( ".xml", szName ) )				
			craterList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szDeathDir, szName ) && CheckExtention( ".xml", szName ) )				
			deathholeList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szMapDir, szName ) )				
		{
			if ( CheckExtention( ".xml", szName ) )
				mapList.push_back( StripFilename( szMapDir, ".xml", szName ).c_str() );
			else if ( CheckExtention( ".bzm", szName ) )
				mapList.push_back( StripFilename( szMapDir, ".bzm", szName ).c_str() );
		}
		if ( pStats->type == SET_STREAM && CheckParentDir( szMusicDir, szName ) && CheckExtention( ".ogg", szName ) )				
			musicList.push_back( StripFilename( "", ".ogg", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szMovieDir, szName ) && CheckExtention( ".xml", szName ) )				
			movieList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && ( CheckParentDir( szMissionDir, szName ) || CheckParentDir( szMissionDir1, szName ) || CheckParentDir( szMissionDir2, szName ) ) && CheckExtention( ".xml", szName ) )				
			scenariomissionsList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && ( CheckParentDir( szChapterDir, szName ) || CheckParentDir( szChapterDir1, szName ) ) && CheckExtention( ".xml", szName ) )				
			chaptersList.push_back( StripFilename( "", ".xml", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szAskDir, szName ) )
		{
			std::string szFileName = StripFilename( szAskDir, ".xml", szName );
			int nPos = szFileName.find( "\\" );
			if ( nPos != std::string::npos )
				if ( szFileName.substr( nPos + 1 ).find( "\\" ) == std::string::npos )
					asksList.push_back( (szAskDir + szFileName).c_str() );
		}
		if ( pStats->type == SET_STREAM && CheckParentDir( szParticleTextureDir, szName ) && CheckExtention( "_h.dds", szName ) )				
			particleTextureList.push_back( StripFilename( "", "_h.dds", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szWaterTextureDir, szName ) && CheckExtention( "_h.dds", szName ) )				
			waterTextureList.push_back( StripFilename( "", "_h.dds", szName ).c_str() );
		if ( pStats->type == SET_STREAM && CheckParentDir( szRoadTextureDir, szName ) && CheckExtention( "_h.dds", szName ) )				
		{
			std::string szFilename = StripFilename( szRoadTextureDir, "_h.dds", szName );
			int nPos = szFilename.find( "\\" );
			if ( nPos != std::string::npos )
			{
				szFilename = szFilename.substr( nPos + 1 );
				if ( CheckParentDir( "roads3d\\", szFilename ) )
					roadTextureList.push_back( StripFilename( "", "_h.dds", szName ).c_str() );
			}
		}
	}
	for ( int i = 0; i < nNumDesc; ++i )
	{
		const SGDBObjectDesc *pDesc = pDB->GetDesc( i );
		if ( pDesc->eGameType == SGVOGT_SOUND )
			soundsList.push_back( pDesc->szKey.c_str() );
		if ( pDesc->eGameType == SGVOGT_UNIT && pDesc->eVisType == SGVOT_SPRITE )
			soldiersList.push_back( pDesc->szKey.c_str() );
	}
}
bool CReferenceDialog::CheckParentDir( const std::string &szDir, const std::string &szPath )
{
	return szPath.substr( 0, szDir.length() ) == szDir;
}
bool CReferenceDialog::CheckExtention( const std::string &szExt, const std::string &szPath )
{
	int nExtLen = szExt.length();
	return szPath.substr( szPath.length() - nExtLen, nExtLen ) == szExt;
}
std::string CReferenceDialog::StripFilename( const std::string &szDir, const std::string &szExt, const std::string &szPath )
{
	return szPath.substr( 0, szPath.length() - szExt.length() ).substr( szDir.length() );
}
void CReferenceDialog::FillFromList( const std::list<std::string> &entries )
{
	for ( std::list<std::string>::const_iterator it = entries.begin(); it != entries.end(); ++it )
		m_refList.AddString( it->c_str() );
}
void CReferenceDialog::CheckedAdd( const std::list<std::string> &entries, const std::string &szRef )
{
	for ( std::list<std::string>::const_iterator it = entries.begin(); it != entries.end(); ++it )
		if ( *it == szRef ) return;
	m_refList.AddString( szRef.c_str() );
}
void CReferenceDialog::LoadItems( EReferenceType eType )
{
	std::vector<std::string> files;
	switch ( eType )
	{
		case E_ANIMATIONS_REF:
			SetWindowText( "Select Sprite Effect Reference" );
			FillFromList( spritesList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szSpritesDir).c_str(), "1.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( spritesList, StripFilename( szSpritesDir, "\\1.xml", *it ).c_str() );
			break;
		case E_FUNC_PARTICLES_REF:
			SetWindowText( "Select Particle Effect Reference" );
			FillFromList( particlesList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szParticlesDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( particlesList, StripFilename( szParticlesDir, ".xml", *it ).c_str() );
			break;
		case E_EFFECTS_REF:
			SetWindowText( "Select Effect Reference" );
			FillFromList( effectsList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szEffectsDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( effectsList, StripFilename( szEffectsDir, ".xml", *it ).c_str() );
			break;
		case E_WEAPONS_REF:
			SetWindowText( "Select Weapon Reference" );
			FillFromList( weaponsList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szWeaponsDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( weaponsList, StripFilename( szWeaponsDir, ".xml", *it ).c_str() );
			break;
		case E_TEMPLATE_MISSIONS_REF:
			SetWindowText( "Select Mission Template Reference" );
			FillFromList( templatemissionsList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szTemplateDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMissionDir1).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( templatemissionsList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_SETTING_REF:
			SetWindowText( "Select Setting Reference" );
			FillFromList( settingList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szSettingDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( settingList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_CRATER_REF:
			SetWindowText( "Select Crater Reference" );
			FillFromList( craterList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szCraterDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( craterList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_DEATHHOLE_REF:
			SetWindowText( "Select Death Hole Reference" );
			FillFromList( deathholeList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szDeathDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( deathholeList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_MAP_REF:
			SetWindowText( "Select Map Reference" );
			FillFromList( mapList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMapDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMapDir).c_str(), "*.bzm", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
					if ( CheckExtention( ".xml", *it ) )
						CheckedAdd( mapList, StripFilename( szMapDir, ".xml", *it ).c_str() );
					else if ( CheckExtention( ".bzm", *it ) )
						CheckedAdd( mapList, StripFilename( szMapDir, ".bzm", *it ).c_str() );
			break;
		case E_MUSIC_REF:
			SetWindowText( "Select Music Reference" );
			FillFromList( musicList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMusicDir).c_str(), "*.ogg", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( musicList, StripFilename( "", ".ogg", *it ).c_str() );
			break;
		case E_MOVIE_REF:
			SetWindowText( "Select Movie Reference" );
			FillFromList( movieList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMusicDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( movieList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_SCENARIO_MISSIONS_REF:
			SetWindowText( "Select Mission Reference" );
			FillFromList( scenariomissionsList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMissionDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMissionDir1).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szMissionDir2).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( scenariomissionsList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_CHAPTERS_REF:
			SetWindowText( "Select Chapter Reference" );
			FillFromList( chaptersList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szChapterDir).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szChapterDir1).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( chaptersList, StripFilename( "", ".xml", *it ).c_str() );
			break;
		case E_ASKS_REF:
			SetWindowText( "Select Ask Set Reference" );			
			FillFromList( asksList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szAskDir).c_str(), "", NFile::CGetAllDirectoriesRelative( theApp.GetDestDir().c_str(), &files ), false );
			{
				std::vector<std::string> askFiles;
				for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
					NFile::EnumerateFiles( (theApp.GetDestDir() + (*it)).c_str(), "*.xml", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &askFiles ), false );
				for ( std::vector<std::string>::const_iterator it = askFiles.begin(); it != askFiles.end(); ++it )
					CheckedAdd( asksList, StripFilename( "", ".xml", *it ).c_str() );
			}
			break;
		case E_SOUNDS_REF:
			SetWindowText( "Select Sound Reference" );
			FillFromList( soundsList );
			break;
		case E_SOLDIER_REF:
			SetWindowText( "Select Infantry Reference" );
			FillFromList( soldiersList );
			break;
		case E_PARTICLE_TEXTURE_REF:
			SetWindowText( "Select Particle Texture Reference" );
			FillFromList( particleTextureList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szParticleTextureDir).c_str(), "*_h.dds", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( particleTextureList, StripFilename( "", "_h.dds", *it ).c_str() );
			break;
		case E_WATER_TEXTURE_REF:
			SetWindowText( "Select Water Texture Reference" );
			FillFromList( waterTextureList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szWaterTextureDir).c_str(), "*_h.dds", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &files ), true );
			for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
				CheckedAdd( waterTextureList, StripFilename( "", "_h.dds", *it ).c_str() );
			break;
		case E_ROAD_TEXTURE_REF:
			SetWindowText( "Select Road Texture Reference" );
			FillFromList( roadTextureList );
			NFile::EnumerateFiles( (theApp.GetDestDir() + szRoadTextureDir).c_str(), "", NFile::CGetAllDirectoriesRelative( theApp.GetDestDir().c_str(), &files ), false );
			{
				std::vector<std::string> textureFiles;
				for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
					NFile::EnumerateFiles( (theApp.GetDestDir() + (*it) + "roads3d\\").c_str(), "*_h.dds", NFile::CGetAllFilesRelative( theApp.GetDestDir().c_str(), &textureFiles ), true );
				for ( std::vector<std::string>::const_iterator it = textureFiles.begin(); it != textureFiles.end(); ++it )
					CheckedAdd( roadTextureList, StripFilename( "", "_h.dds", *it ).c_str() );
			}
			break;
		default:
			break;
	}
}
const std::string CReferenceDialog::szSpritesDir = "effects\\sprites\\";
const std::string CReferenceDialog::szParticlesDir = "effects\\particles\\";
const std::string CReferenceDialog::szEffectsDir = "effects\\effects\\";
const std::string CReferenceDialog::szWeaponsDir = "weapons\\";
const std::string CReferenceDialog::szInfantryDir = "units\\humans\\";
const std::string CReferenceDialog::szMissionDir = "scenarios\\scenariomissions\\";
const std::string CReferenceDialog::szMissionDir1 = "scenarios\\custom\\missions\\";	
const std::string CReferenceDialog::szMissionDir2 = "scenarios\\tutorials\\";
const std::string CReferenceDialog::szTemplateDir = "scenarios\\templatemissions\\";
const std::string CReferenceDialog::szChapterDir = "scenarios\\chapters\\";
const std::string CReferenceDialog::szChapterDir1 = "scenarios\\custom\\chapters\\";
const std::string CReferenceDialog::szSettingDir = "scenarios\\settings\\";
const std::string CReferenceDialog::szAskDir = "sounds\\ack\\";
const std::string CReferenceDialog::szDeathDir = "objects\\terraobjects\\death_hole\\";
const std::string CReferenceDialog::szCraterDir = "objects\\terraobjects\\shell_hole\\";
const std::string CReferenceDialog::szMapDir = "maps\\";
const std::string CReferenceDialog::szMusicDir = "music\\";
const std::string CReferenceDialog::szMovieDir = "movies\\";
const std::string CReferenceDialog::szParticleTextureDir = "effects\\particles\\";
const std::string CReferenceDialog::szRoadTextureDir = "terrain\\sets\\";
const std::string CReferenceDialog::szWaterTextureDir = "water\\";
std::list<std::string> CReferenceDialog::spritesList;
std::list<std::string> CReferenceDialog::particlesList;
std::list<std::string> CReferenceDialog::effectsList;
std::list<std::string> CReferenceDialog::weaponsList;
std::list<std::string> CReferenceDialog::soldiersList;
std::list<std::string> CReferenceDialog::scenariomissionsList;
std::list<std::string> CReferenceDialog::templatemissionsList;
std::list<std::string> CReferenceDialog::chaptersList;
std::list<std::string> CReferenceDialog::soundsList;
std::list<std::string> CReferenceDialog::settingList;
std::list<std::string> CReferenceDialog::asksList;
std::list<std::string> CReferenceDialog::craterList;
std::list<std::string> CReferenceDialog::deathholeList;
std::list<std::string> CReferenceDialog::mapList;
std::list<std::string> CReferenceDialog::musicList;
std::list<std::string> CReferenceDialog::movieList;
std::list<std::string> CReferenceDialog::particleTextureList;
std::list<std::string> CReferenceDialog::roadTextureList;
std::list<std::string> CReferenceDialog::waterTextureList;
