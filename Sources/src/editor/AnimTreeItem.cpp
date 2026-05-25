#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "TreeItem.h"
#include "AnimTreeItem.h"
#include "frames.h"
#include "AnimationFrm.h"
#include "MeshFrm.h"
#include "SpriteCompose.h"

/////////////////////////////////////// Animation editor tree //////////////////////////////////////

void CAnimationTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_UNIT_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ACKS_ITEM;
	child.szDefaultName = "Acknowledgments";
	child.szDisplayName = "Acknowledgments";
	defaultChilds.push_back( child );

	child.nChildItemType = E_LOCALIZATION_ITEM;
	child.szDefaultName = "Localization";
	child.szDisplayName = "Localization";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ACTIONS_ITEM;
	child.szDefaultName = "Actions";
	child.szDisplayName = "Actions";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_EXPOSURES_ITEM;
	child.szDefaultName = "Exposures";
	child.szDisplayName = "Exposures";
	defaultChilds.push_back( child );
	
	/*child.nChildItemType = E_UNIT_AI_PROPS_ITEM;
	child.szDefaultName = "AI";
	child.szDisplayName = "AI";
	defaultChilds.push_back( child );*/

	child.nChildItemType = E_UNIT_WEAPON_PROPS_ITEM;
	child.szDefaultName = "Weapon";
	child.szDisplayName = "Weapon";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_GRENADE_PROPS_ITEM;
	child.szDefaultName = "Grenade";
	child.szDisplayName = "Grenade";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORIES_ITEM;
	child.szDefaultName = "Directories";
	child.szDisplayName = "Directories";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATIONS_ITEM;
	child.szDefaultName = "Animations";
	child.szDisplayName = "Animations";
	defaultChilds.push_back( child );
}

void CUnitCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown unit";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Type";
	prop.szDisplayName = "Type";
	prop.value = "soldier";
	prop.szStrings.push_back( "soldier" );
	prop.szStrings.push_back( "engineer" );
	prop.szStrings.push_back( "sniper" );
	prop.szStrings.push_back( "officer" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Image";
	prop.szDisplayName = "Image";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Armor";
	prop.szDisplayName = "Armor";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Camouflage";
	prop.szDisplayName = "Camouflage";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed";
	prop.szDisplayName = "Speed";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability";
	prop.szDisplayName = "Passability";
	prop.value = 100.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can attack up";
	prop.szDisplayName = "Can attack up";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can attack down";
	prop.szDisplayName = "Can attack down";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 11;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "AI price";
	prop.szDisplayName = "AI price";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight";
	prop.szDisplayName = "Sight";
	prop.value = 20.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 13;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight power";
	prop.szDisplayName = "Sight power";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
	defaultChilds.clear();
}

int CUnitCommonPropsItem::GetUnitType()
{
	string szVal = values[1].value;
	if ( szVal == "soldier" || szVal == "Soldier" )
		return RPG_TYPE_SOLDIER;
	if ( szVal == "engineer" || szVal == "Engineer" )
		return RPG_TYPE_ENGINEER;
	if ( szVal == "sniper" || szVal == "Sniper" )
		return RPG_TYPE_SNIPER;
	if ( szVal == "officer" || szVal == "Officer" )
		return RPG_TYPE_OFFICER;
	NI_ASSERT( 0 );
	return RPG_TYPE_SOLDIER;
}

void CUnitCommonPropsItem::SetUnitType( int nVal )
{
	string szVal;
	switch ( nVal )
	{
		case RPG_TYPE_SOLDIER:
			szVal = "soldier";
			break;
		case RPG_TYPE_ENGINEER:
			szVal = "engineer";
			break;
		case RPG_TYPE_SNIPER:
			szVal = "sniper";
			break;
		case RPG_TYPE_OFFICER:
			szVal = "officer";
			break;
		default:
			NI_ASSERT( 0 );
			szVal = "soldier";
	}
	values[1].value = szVal;
}

void CUnitAIPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CUnitWeaponPropsItem::InitDefaultValues()
{
	defaultValues.clear();

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Weapon name";
	prop.szDisplayName = "Weapon name";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo count";
	prop.szDisplayName = "Ammo count";
	prop.value = 100;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reload cost";
	prop.szDisplayName = "Reload cost";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitGrenadePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Grenade name";
	prop.szDisplayName = "Grenade name";
	prop.value = "";
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo count";
	prop.szDisplayName = "Ammo count";
	prop.value = 100;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reload cost";
	prop.szDisplayName = "Reload cost";
	prop.value = 100.0f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CDirectoryPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Directory";
	prop.szDisplayName = "Directory";
	prop.value = "_.";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CDirectoryPropsItem::SetSelectedIcon( bool bFlag )
{
	if ( bFlag )
		ChangeItemImage( nSelImage );
	else
		ChangeItemImage( nImageIndex );
}

void CDirectoryPropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_UNIT_SEASON_PROPS_ITEM );

	pPapa = pPapa->GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_UNIT_DIRECTORIES_ITEM );
	CDirectoriesItem *pDirsItem = (CDirectoriesItem *) pPapa;

	pDirsItem->SetDefaultDirPropsItem( this );
}

void CDirectoryPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	string szOldDirName = GetDirName();
	CTreeItem::UpdateItemValue( nItemId, value );

	if ( nItemId == 1 )
	{
		//���������� �������� ����������, ��������� ��� �������� �� ���� ���� � AllThumbList
		//���������, ��� ���� TreeItem ������� � ������, ����� ����� ��� ����������
		HTREEITEM hSelected = pTreeCtrl->GetSelectedItem();
		if ( hSelected != hItem )
		{
//			SelectMeInTheTree();
			CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
			pFrame->SetActiveDirTreeItem( this );
		}

		if ( !IsRelatedPath( value ) )
		{
			//��� ����������� ������������� ����, ������������ ����� � ��������
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and directories with animations should be on the same drive" );
			}
		}

		if ( strcmp( szOldDirName.c_str(), GetDirName()) )
		{
			CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
			pFrame->ActiveDirNameChanged();
		}
		return;
	}
}

void CUnitSeasonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right Up";
	child.szDisplayName = "Right Up";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Up";
	child.szDisplayName = "Up";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left Up";
	child.szDisplayName = "Left Up";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left Down";
	child.szDisplayName = "Left Down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Down";
	child.szDisplayName = "Down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right Down";
	child.szDisplayName = "Right Down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );
}

void CDirectoriesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Summer";
	child.szDisplayName = "Summer";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Winter";
	child.szDisplayName = "Winter";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Africa";
	child.szDisplayName = "Africa";
	defaultChilds.push_back( child );
}

void CDirectoriesItem::SetDefaultDirPropsItem( CDirectoryPropsItem *pDirProps )
{
	if ( pActiveDirPropsItem )
		pActiveDirPropsItem->SetSelectedIcon( false );

	pActiveDirPropsItem = pDirProps;
	if ( pActiveDirPropsItem )
	{
		pActiveDirPropsItem->SetSelectedIcon( true );
		CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
		pFrame->SetActiveDirTreeItem( pActiveDirPropsItem );
	}
}

const char *CUnitSeasonPropsItem::GetDirName( int nIndex )
{
	NI_ASSERT( nIndex >= 0 && nIndex <= 7 );

	int i = 0;
	for ( CTreeItemList::const_iterator it=GetBegin(); it!=GetEnd(); ++it )
	{
		if ( i == nIndex )
		{
			CDirectoryPropsItem *pDirProps = (CDirectoryPropsItem *) it->GetPtr();
			return pDirProps->GetDirName();
		}
		i++;
	}

	NI_ASSERT( 0 );
	return 0;
}

void CUnitAnimationsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Run";
	child.szDisplayName = "Run";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Crawl";
	child.szDisplayName = "Crawl";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot";
	child.szDisplayName = "Shoot";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot down";
	child.szDisplayName = "Shoot down";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot trench";
	child.szDisplayName = "Shoot trench";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming";
	child.szDisplayName = "Aiming";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming down";
	child.szDisplayName = "Aiming down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming trench";
	child.szDisplayName = "Aiming trench";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw";
	child.szDisplayName = "Throw";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw down";
	child.szDisplayName = "Throw down";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw trench";
	child.szDisplayName = "Throw trench";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Death1";
	child.szDisplayName = "Death";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Death down";
	child.szDisplayName = "Death down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Prisoning";
	child.szDisplayName = "Prisoning";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle";
	child.szDisplayName = "Idle";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle down";
	child.szDisplayName = "Idle down";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle2";
	child.szDisplayName = "Idle2";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Lie to stand cross";
	child.szDisplayName = "Lie to stand cross";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Stand to lie cross";
	child.szDisplayName = "Stand to lie cross";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Use down";
	child.szDisplayName = "Use down";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Use up";
	child.szDisplayName = "Use up";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Pointing";
	child.szDisplayName = "Pointing";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Binoculars";
	child.szDisplayName = "Binoculars";
	defaultChilds.push_back( child );

	child.nChildItemType = E_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Radio";
	child.szDisplayName = "Radio";
	defaultChilds.push_back( child );
}

int CUnitAnimationPropsItem::GetAnimationType()
{
	std::string szName = szDisplayName;
	if ( szName == "Run" )
		return ANIMATION_MOVE;
	if ( szName == "Crawl" )
		return ANIMATION_CRAWL;
	if ( szName == "Shoot" )
		return ANIMATION_SHOOT;
	if ( szName == "Shoot down" )
		return ANIMATION_SHOOT_DOWN;
	if ( szName == "Shoot trench" )
		return ANIMATION_SHOOT_TRENCH;
	if ( szName == "Aiming" )
		return ANIMATION_AIMING;
	if ( szName == "Aiming down" )
		return ANIMATION_AIMING_DOWN;
	if ( szName == "Aiming trench" )
		return ANIMATION_AIMING_TRENCH;
	if ( szName == "Throw" )
		return ANIMATION_THROW;
	if ( szName == "Throw trench" )
		return ANIMATION_THROW_TRENCH;
	if ( szName == "Death" )
		return ANIMATION_DEATH;
	if ( szName == "Death down" )
		return ANIMATION_DEATH_DOWN;
	if ( szName == "Idle" )
		return ANIMATION_IDLE;
	if ( szName == "Idle down" )
		return ANIMATION_IDLE_DOWN;
	if ( szName == "Use down" )
		return ANIMATION_USE_DOWN;
	if ( szName == "Use up" )
		return ANIMATION_USE;
	if ( szName == "Pointing" )
		return ANIMATION_POINTING;
	if ( szName == "Binoculars" )
		return ANIMATION_BINOCULARS;
	if ( szName == "Radio" )
		return ANIMATION_RADIO;
	
	if ( szName == "Lie to stand cross" )
		return ANIMATION_LIE;
	if ( szName == "Stand to lie cross" )
		return ANIMATION_STAND;
	if ( szName == "Throw down" )
		return ANIMATION_THROW_DOWN;
	if ( szName == "Idle2" )
		return ANIMATION_IDLE2;
	if ( szName == "Prisoning" )
		return ANIMATION_PRISONING;
	
	NI_ASSERT_T( 0, "Unknown animation" );
	return -1;
}

bool CAnimationTreeRootItem::ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, bool bSetCycledFlag, bool bShowUserMessages )
{
	if ( GetChildsCount() == 0 )
		return false;			//�����
	
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME );
	CDirectoriesItem *pDirsItem = static_cast<CDirectoriesItem *> ( GetChildItem( E_UNIT_DIRECTORIES_ITEM ) );
	CUnitAnimationsItem *pAnimsItem = static_cast<CUnitAnimationsItem *> ( GetChildItem( E_UNIT_ANIMATIONS_ITEM ) );

	//������, ���� ���������� blood � ���������� �������, ��� ���
	bool bBloodDirExist = false;
	{
		std::string szBloodDir = GetDirectory( pszProjectFileName );
		szBloodDir += "blood\\";
		if ( !_access( szBloodDir.c_str(), 00 ) )
			bBloodDirExist = true;
	}

	for ( int nBlood=0; nBlood<2; nBlood++ )
	{
		CVec2 vCommonFrameSize( 32, 32 );
		bool bCommonSizeComputed = false;
		int nSeason = 0;
		for ( CTreeItemList::const_iterator seasonIt=pDirsItem->GetBegin(); seasonIt!=pDirsItem->GetEnd(); ++seasonIt )
		{
			CUnitSeasonPropsItem *pSeasonProps = static_cast<CUnitSeasonPropsItem*> ( seasonIt->GetPtr() );

			int nCurrentFrame = 0;
			int nCurrentAnim = 0;
			int nCurrentSpriteNumber = 0;
			int nError = 0;

			CVectorOfStrings fileNameVector;
			CVectorOfStrings invalidNameVector;
			vector<SAnimationDesc> animDescVector( pAnimsItem->GetChildsCount() );

			CTreeItemList::const_iterator animIt;
			for ( animIt=pAnimsItem->GetBegin(); animIt!=pAnimsItem->GetEnd(); ++animIt )
			{
				CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) animIt->GetPtr();
				int nDirsCount = pAnimProps->GetNumberOfDirections();
				
				if ( pAnimProps->GetChildsCount() == 0 )
					continue;

				SAnimationDesc &animDesc = animDescVector[nCurrentAnim];
				if ( bSetCycledFlag )
					animDesc.bCycled = bSetCycledFlag;
				else
					animDesc.bCycled = pAnimProps->GetCycledFlag();

				animDesc.fSpeed = pAnimProps->GetAnimationSpeed();
				const int nLastSprite = nCurrentSpriteNumber + nDirsCount * pAnimProps->GetChildsCount();
				animDesc.nFrameTime = pAnimProps->GetFrameTime();
				animDesc.ptFrameShift = CVec2( 0, 0 );
				animDesc.szName = pAnimProps->GetItemName();

				//��������� ������ directions
				fileNameVector.resize( nLastSprite );
				animDesc.dirs.resize( nDirsCount );
				animDesc.frames.reserve( nDirsCount * pAnimProps->GetChildsCount() );
				for ( int i=0; i<nDirsCount; i++ )
				{
					string szDirName;
					if ( nDirsCount == 4 )
						szDirName = pSeasonProps->GetDirName( i*2 );
					else
						szDirName = pSeasonProps->GetDirName( i );
					if ( IsRelatedPath( szDirName.c_str() ) )
					{
						string szFull, szProjectDir;
						szProjectDir = GetDirectory( pszProjectFileName );
						MakeFullPath( szProjectDir.c_str(), szDirName.c_str(), szFull );
						szDirName = szFull;
					}
					
					SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[ i ];
					dirDesc.ptFrameShift = CVec2( 0, 0 );
					dirDesc.frames.resize( pAnimProps->GetChildsCount() );
					
					CTreeItemList::const_iterator frameIt;
					int k = 0;
					if ( !bCommonSizeComputed && (*animIt)->GetChildsCount() > 0 )
					{
						//���������� ������ ��������
						CUnitFramePropsItem *pProps = static_cast<CUnitFramePropsItem *> ( (*animIt)->GetBegin()->GetPtr() );
						string szTempFileName = szDirName + pProps->GetItemName() + ".tga";
						if ( _access( szTempFileName.c_str(), 04 ) == 0 )
						{
							IImageProcessor *pIP = GetSingleton<IImageProcessor>();
							CPtr<IDataStream> pStream = OpenFileStream( szTempFileName.c_str(), STREAM_ACCESS_READ );
							CPtr<IImage> pImage = pIP->LoadImage( pStream );
							vCommonFrameSize.x = pImage->GetSizeX() / 2;
							vCommonFrameSize.y = pImage->GetSizeY() / 2;
							bCommonSizeComputed = true;
						}
					}
					
					for ( frameIt=(*animIt)->GetBegin(); frameIt!=(*animIt)->GetEnd(); ++frameIt )
					{
						dirDesc.frames[ k ] = nCurrentFrame;
//					animDesc.frames[nCurrentFrame] = CVec2( 32, 32 );
						animDesc.frames[nCurrentFrame] = vCommonFrameSize;
						
						std::string szTempFileName = szDirName;
						std::string szName = (*animIt)->GetItemName();
						if ( bBloodDirExist && nBlood == 1 && ( szName == "Death" || szName == "Death down" ) )
						{
							//������� ���������� blood � �����
							std::string szShortDirName;
							if ( nDirsCount == 4 )
								szShortDirName = pSeasonProps->GetDirName( i*2 );
							else
								szShortDirName = pSeasonProps->GetDirName( i );
							szTempFileName = szTempFileName.substr( 0, szTempFileName.size() - szShortDirName.size() );
							szTempFileName += "blood\\";
							szTempFileName += szShortDirName;
						}
						szTempFileName += (*frameIt)->GetItemName();
						szTempFileName += ".tga";
						
						if ( _access( szTempFileName.c_str(), 04 ) == 0 )
						{
							fileNameVector[nCurrentFrame] = szTempFileName;
							nCurrentFrame++;
						}
						else
						{
							invalidNameVector.push_back( szTempFileName );
							szTempFileName = "editor\\invalid.tga";
							fileNameVector[nCurrentFrame] = szTempFileName;
							nError++;
						}

						k++;
					}
				}

				nCurrentSpriteNumber = nLastSprite;
				nCurrentAnim++;
			}
			animDescVector.resize( nCurrentAnim );

			if ( bShowUserMessages && nError > 0 )
			{
				CString szErrorStr;
				szErrorStr.Format( "Can not find files total count %d\n", nError );
				for ( int i=0; i<invalidNameVector.size(); i++ )
				{
					szErrorStr += invalidNameVector[i].c_str();
					szErrorStr += '\n';
				}

				AfxMessageBox( szErrorStr );
			}

			//���� ������ ������ ����, �� �������
			if ( nCurrentFrame == 0 )
			{
				if ( bShowUserMessages )
					AfxMessageBox( "Error: no valid animations" );
				return false;
			}

			SSpriteAnimationFormat spriteAnimFmt;
			CPtr<IImage> pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector );

			if ( !pImage )
			{
				if ( bShowUserMessages )
					AfxMessageBox( "Composing images failed!" );
				return false;
			}
			
			{
				std::string szSaveImage = pszResultingDir;
				szSaveImage += "1";
				if ( nBlood )
					szSaveImage += "b";
				std::string szSaveSan = "1";
				if ( nBlood )
					szSaveSan += "b";

				if ( nSeason == 1 )
				{
					szSaveImage += "w";
					szSaveSan += "w";
				}
				else if ( nSeason == 2 )
				{
					szSaveImage += "a";
					szSaveSan += "a";
				}
				else if ( nSeason > 3 )
					NI_ASSERT_T( 0, "The number of seasons is above 3" );
				szSaveSan += ".san";

				//��������� .SAN
				CPtr<IDataStorage> pSaveStorage = CreateStorage( pszResultingDir, STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
				CPtr<IDataStream> pSaveSAFStream = pSaveStorage->CreateStream( szSaveSan.c_str(), STREAM_ACCESS_WRITE );
				if ( !pSaveSAFStream )
					return false;
				CPtr<IStructureSaver> pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
				CSaverAccessor saver = pSS;
				saver.Add( 1, &spriteAnimFmt );

				//��������� ��������
				SaveCompressedTexture( pImage, szSaveImage.c_str() );
			}

			nSeason++;
		}
	}			//end of blood cycle
	
	return true;
}

FILETIME CAnimationTreeRootItem::FindMaximalSourceTime( const char *pszProjectFileName )
{
	FILETIME zero;
	zero.dwHighDateTime = 0;
	zero.dwLowDateTime = 0;
	if ( GetChildsCount() == 0 )
		return zero;			//�����
	
	CVectorOfStrings fileNameVector;
	CDirectoriesItem *pDirsItem = static_cast<CDirectoriesItem *> ( GetChildItem( E_UNIT_DIRECTORIES_ITEM ) );
	CUnitAnimationsItem *pAnimsItem = static_cast<CUnitAnimationsItem *> ( GetChildItem( E_UNIT_ANIMATIONS_ITEM ) );
	
	for ( CTreeItemList::const_iterator seasonIt=pDirsItem->GetBegin(); seasonIt!=pDirsItem->GetEnd(); ++seasonIt )
	{
		CUnitSeasonPropsItem *pSeasonProps = static_cast<CUnitSeasonPropsItem*> ( seasonIt->GetPtr() );

		CTreeItemList::const_iterator animIt;
		for ( animIt=pAnimsItem->GetBegin(); animIt!=pAnimsItem->GetEnd(); ++animIt )
		{
			CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) animIt->GetPtr();
			int nDirsCount = pAnimProps->GetNumberOfDirections();
			
			if ( pAnimProps->GetChildsCount() == 0 )
				continue;

			//����� ���� ����� ������� �������������� ����������� �� �������...

			//��������� ������ directions
			for ( int i=0; i<nDirsCount; i++ )
			{
				string szDirName;
				if ( nDirsCount == 4 )
					szDirName = pSeasonProps->GetDirName( i*2 );
				else
					szDirName = pSeasonProps->GetDirName( i );
				if ( IsRelatedPath( szDirName.c_str() ) )
				{
					string szFull, szProjectDir;
					szProjectDir = GetDirectory( pszProjectFileName );
					MakeFullPath( szProjectDir.c_str(), szDirName.c_str(), szFull );
					szDirName = szFull;
				}

				CTreeItemList::const_iterator frameIt;
				for ( frameIt=(*animIt)->GetBegin(); frameIt!=(*animIt)->GetEnd(); ++frameIt )
				{
					string szTempFileName = szDirName + (*frameIt)->GetItemName() + ".tga";
					if ( _access( szTempFileName.c_str(), 04 ) == 0 )
						fileNameVector.push_back( szTempFileName );
				}
			}
		}
	}

	//���� ������ ������ ����, �� �������
	if ( fileNameVector.empty() )
		return zero;

	//�������� �� ���� ������ � ������� ������������ ����� ���������
	FILETIME nMaxTime;
	nMaxTime.dwHighDateTime = 0;
	nMaxTime.dwLowDateTime = 0;
	for ( int i=0; i<fileNameVector.size(); i++ )
	{
		FILETIME current = GetFileChangeTime( fileNameVector[i].c_str() );
		if ( current > nMaxTime )
			nMaxTime = current;
	}
	return nMaxTime;
}

void CUnitAnimationPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Frame time";
	prop.szDisplayName = "Frame time";
	prop.value = 125;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Action frame";
	prop.szDisplayName = "Action frame";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Animation speed";
	prop.szDisplayName = "Animation speed";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is cycled?";
	prop.szDisplayName = "Is cycled?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Number of directions";
	prop.szDisplayName = "Number of directions";
	prop.value = "8";
	prop.szStrings.push_back( "8" );
	prop.szStrings.push_back( "4" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 5;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound file name";
	prop.szDisplayName = "Sound file name";
	prop.value = "";
	defaultValues.push_back( prop );

	//next will be 7 see up

	values = defaultValues;
}

void CUnitAnimationPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 4 )
	{
		CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
		pFrame->ClearComposedFlag();
	}
}

bool CUnitAnimationPropsItem::CopyItemTo( CTreeItem *pToItem )
{
	NI_ASSERT( pToItem->GetItemType() == E_UNIT_ANIMATION_PROPS_ITEM );
	CUnitAnimationPropsItem *pTo = static_cast<CUnitAnimationPropsItem *> (pToItem);
	if ( !CTreeItem::CopyItemTo( pTo ) )
		return false;

	//������ �������� ������ CUnitFramePropsItem
	pTo->RemoveAllChilds();
	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		CUnitFramePropsItem *pFrame = new CUnitFramePropsItem();
		(*it)->CopyItemTo( pFrame );
		pFrame->SetItemName( (*it)->GetItemName() );
		pTo->AddChild( pFrame );
	}
	return true;
}

void CUnitAnimationPropsItem::MyLButtonClick()
{
	CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
	pFrame->SetActiveAnimItem( this );
}

int CUnitAnimationPropsItem::GetFrameTime()
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 1 )
			return (*it).value;
	}
	NI_ASSERT( 0 );
	return 0;
}

float CUnitAnimationPropsItem::GetAnimationSpeed()
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 2 )
			return (*it).value;
	}
	NI_ASSERT( 0 );
	return 0;
}

void CUnitAnimationPropsItem::SetAnimationSpeed( float fVal )
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 2 )
		{
			it->value = fVal;
			return;
		}
	}
	NI_ASSERT( 0 );
}

bool CUnitAnimationPropsItem::GetCycledFlag()
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 3 )
			return (*it).value;
	}
	NI_ASSERT( 0 );
	return false;
}

int CUnitAnimationPropsItem::GetNumberOfDirections()
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 4 )
		{
			if ( !strcmp( it->value, "8" ) )
				return 8;
			else
				return 4;
		}
	}
	NI_ASSERT( 0 );
	return false;
}

int CUnitAnimationPropsItem::GetActionFrame()
{
	for ( CPropVector::iterator it=values.begin(); it!=values.end(); ++it )
	{
		if ( (*it).nId == 6 )
			return (*it).value;
	}
	NI_ASSERT( 0 );
	return 0;
}

int CUnitAnimationPropsItem::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTreeItem*>(this) );
	return 0;
/*
	CSaverAccessor saver = pSS;
	//��������� ����� ������ m_selectedItems
	saver.AddContainer( 10, &m_selectedItems.thumbDataList );
*/
}

void CUnitAnimationPropsItem::InsertChildItems()
{
	CTreeItem::InsertChildItems();
	SThumbData thumbData;

	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		thumbData.szThumbName = (*it)->GetItemName();
		m_thumbItems.thumbDataList.push_back( thumbData );
	}
}

void CUnitFramePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CUnitFramePropsItem::MyLButtonClick()
{
	//� ThumbList ��������� Animations ��������������� ���� �����
	CTreeItem *pPapa = GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_UNIT_ANIMATION_PROPS_ITEM );

	CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) pPapa;
	CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
	pFrame->SetActiveAnimItem( pAnimProps );

	//� ���������� ThumbList items ������� item ��������������� this
	pFrame->SelectItemInSelectedThumbList( (long) this );
}

void CUnitFramePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
		/*
		//������� ����� frame ����� ��������� ���������� � ������ � �������� ��� � SelectedThumbList
		HTREEITEM hNextSibling = pTreeCtrl->GetNextItem( hItem, TVGN_NEXT );
		if ( hNextSibling )
		{
				CTreeItem *pNextSelItem = (CTreeItem *) pTreeCtrl->GetItemData( hNextSibling );
				if ( pNextSelItem->GetItemType() == E_UNIT_FRAME_PROPS_ITEM )
				g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME )()->SelectItemInSelectedThumbList( (DWORD) pNextSelItem );
				}
			*/
			
			//������� ���� frame
			CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
			pFrame->DeleteFrameInSelectedList( (DWORD) this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CUnitActionsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Available actions";
	prop.szDisplayName = "Available actions";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitActionsItem::GetActions( SUnitBaseRPGStats *pRPGStats )
{
	int64 nVal = values[0].value;
	pRPGStats->availCommands.Clear();
	for ( int i=0; i<64; i++ )
	{
		if ( nVal & ((int64) 1 << i) )
			pRPGStats->AddCommand( i );
	}
}

void CUnitActionsItem::SetActions( const SUnitBaseRPGStats *pRPGStats )
{
	int64 nVal = 0;
	for ( int i=0; i<64; i++ )
	{
		if ( pRPGStats->HasCommand( i ) )
			nVal |= (int64) 1 << i;
	}
	values[0].value = nVal;
}

void CUnitExposuresItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Available exposures";
	prop.szDisplayName = "Available exposures";
	prop.value = "";
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CUnitExposuresItem::GetExposures( SUnitBaseRPGStats *pRPGStats )
{
	int64 nVal = values[0].value;
	pRPGStats->availExposures.Clear();
	for ( int i=0; i<64; i++ )
	{
		if ( nVal & ((int64) 1 << i) )
		{
			if ( pRPGStats->availExposures.GetSize() <= i )
				pRPGStats->availExposures.SetSize( i + 1 );
			pRPGStats->availExposures.SetData( i );
		}
	}
}

void CUnitExposuresItem::SetExposures( const SUnitBaseRPGStats *pRPGStats )
{
	int64 nVal = 0;
	for ( int i=0; i<64; i++ )
	{
		if ( i < pRPGStats->availExposures.GetSize() )
		{
			if ( pRPGStats->availExposures.GetData( i ) )
				nVal |= (int64) 1 << i;
		}
	}
	values[0].value = nVal;
}

void CUnitActionPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Unknown action";
	prop.szDisplayName = "Unknown action";
	prop.value = (int64) 0;
	defaultValues.push_back( prop );
}

void CUnitAcksItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_ASK_REF;
	prop.szDefaultName = "Acks file";
	prop.szDisplayName = "Acks set";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_ASK_REF;
	prop.szDefaultName = "Acks file 2";
	prop.szDisplayName = "Acks set 2";
	prop.value = "";
	defaultValues.push_back( prop );
}


