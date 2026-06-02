#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "SquadFrm.h"
#include "SquadTreeItem.h"

void CSquadTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_SQUAD_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_SQUAD_MEMBERS_ITEM;
	child.szDefaultName = "Members";
	child.szDisplayName = "Members";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_SQUAD_FORMATIONS_ITEM;
	child.szDefaultName = "Formations";
	child.szDisplayName = "Formations";
	defaultChilds.push_back( child );
}

void CSquadTreeRootItem::CallMeAfterSerialize()
{
	CTreeItem *pMembers = GetChildItem( E_SQUAD_MEMBERS_ITEM );
	NI_ASSERT( pMembers != 0 );

	CTreeItem *pFormations = GetChildItem( E_SQUAD_FORMATIONS_ITEM );
	for ( CTreeItemList::const_iterator out=pFormations->GetBegin(); out!=pFormations->GetEnd(); ++out )
	{
		CSquadFormationPropsItem *pFormProps = static_cast<CSquadFormationPropsItem *> ( out->GetPtr() );
		CSquadFormationPropsItem::CUnitsList::iterator it=pFormProps->units.begin();
		for ( CTreeItemList::const_iterator ext=pMembers->GetBegin(); ext!=pMembers->GetEnd(); ++ext )
		{
			it->pMemberProps = ext->GetPtr();
			++it;
		}
	}
}

void CSquadCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Squad";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Squad picture";
	prop.szDisplayName = "Squad picture";
	prop.value = "icon.tga";
	prop.szStrings.push_back( "" );			//надо будет скопировать картинку при export проекта
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Squad type";
	prop.szDisplayName = "Squad type";
	prop.value = "riflemans";
	prop.szStrings.push_back( "riflemans" );
	prop.szStrings.push_back( "infantry" );
	prop.szStrings.push_back( "submachine gunners" );
	prop.szStrings.push_back( "machine gunners" );
	prop.szStrings.push_back( "AT team" );
	prop.szStrings.push_back( "mortar team" );
	prop.szStrings.push_back( "snipers" );
	prop.szStrings.push_back( "gunners" );
	prop.szStrings.push_back( "engineers" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	values = defaultValues;
}

int CSquadCommonPropsItem::GetSquadType()
{
	string szName = values[2].value;
	
	if ( szName == "riflemans" || szName == "Riflemans" )
		return SSquadRPGStats::RIFLEMANS;
	if ( szName == "infantry" || szName == "Infantry" )
		return SSquadRPGStats::INFANTRY;
	if ( szName == "submachine gunners" || szName == "SubMachineGunners" )
		return SSquadRPGStats::SUBMACHINEGUNNERS;
	if ( szName == "machine gunners" || szName == "MachineGunners" )
		return SSquadRPGStats::MACHINEGUNNERS;
	if ( szName == "AT team" || szName == "At team" )
		return SSquadRPGStats::AT_TEAM;
	if ( szName == "mortar team" || szName == "Mortar team" )
		return SSquadRPGStats::MORTAR_TEAM;
	if ( szName == "snipers" || szName == "Snipers" )
		return SSquadRPGStats::SNIPERS;
	if ( szName == "gunners" || szName == "Gunners" )
		return SSquadRPGStats::GUNNERS;
	if ( szName == "engineers" || szName == "Engineers" )
		return SSquadRPGStats::ENGINEERS;

	NI_ASSERT_T( 0, "Unknown squad type" );
	return -1;
}

void CSquadMembersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSquadMembersItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CSquadMemberPropsItem;
			pItem->SetItemName( "USSR\\Mosin" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );

			CTreeItem *pFormations = GetSiblingItem( E_SQUAD_FORMATIONS_ITEM );
			NI_ASSERT( pFormations != 0 );
			for ( CTreeItemList::const_iterator it=pFormations->GetBegin(); it!=pFormations->GetEnd(); ++it )
			{
				CSquadFormationPropsItem *pFormationProps = static_cast<CSquadFormationPropsItem *> ( it->GetPtr() );
				pFormationProps->AddUnit( pItem );
			}
			break;
	}
}

void CSquadMembersItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CSquadMemberPropsItem;
		pItem->SetItemName( "USSR\\Mosin" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );

		CTreeItem *pFormations = GetSiblingItem( E_SQUAD_FORMATIONS_ITEM );
		NI_ASSERT( pFormations != 0 );
		for ( CTreeItemList::const_iterator it=pFormations->GetBegin(); it!=pFormations->GetEnd(); ++it )
		{
			CSquadFormationPropsItem *pFormationProps = static_cast<CSquadFormationPropsItem *> ( it->GetPtr() );
			pFormationProps->AddUnit( pItem );
		}
	}
}

void CSquadMemberPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_SOLDIER_REF;
	prop.szDefaultName = "Soldier";
	prop.szDisplayName = "Soldier";
	prop.value = "USSR\\Mosin";
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CSquadMemberPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );

	if ( nItemId == 1 )
	{
		ChangeItemName( value );
		CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
		pFrame->UpdateActiveFormation();
	}
}

void CSquadMemberPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CTreeItem *pParent = GetParentTreeItem();
			CTreeItem *pFormations = pParent->GetSiblingItem( E_SQUAD_FORMATIONS_ITEM );
			NI_ASSERT( pFormations != 0 );
			for ( CTreeItemList::const_iterator it=pFormations->GetBegin(); it!=pFormations->GetEnd(); ++it )
			{
				CSquadFormationPropsItem *pFormationProps = static_cast<CSquadFormationPropsItem *> ( it->GetPtr() );
				pFormationProps->DeleteUnit( this );
			}
			
			g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );
			
			break;
	}
}

void CSquadMemberPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		CTreeItem *pParent = GetParentTreeItem();
		CTreeItem *pFormations = pParent->GetSiblingItem( E_SQUAD_FORMATIONS_ITEM );
		NI_ASSERT( pFormations != 0 );
		for ( CTreeItemList::const_iterator it=pFormations->GetBegin(); it!=pFormations->GetEnd(); ++it )
		{
			CSquadFormationPropsItem *pFormationProps = static_cast<CSquadFormationPropsItem *> ( it->GetPtr() );
			pFormationProps->DeleteUnit( this );
		}
		g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );
	}
}

void CSquadMemberPropsItem::MyLButtonClick()
{
	CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
	pFrame->SelectActiveUnit( this );
}

void CSquadFormationsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSquadFormationsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CSquadFormationPropsItem *pItem = new CSquadFormationPropsItem;
			pItem->SetItemName( "Formation" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );
			
			CTreeItem *pMembers = GetSiblingItem( E_SQUAD_MEMBERS_ITEM );
			NI_ASSERT( pMembers != 0 );

			for ( CTreeItemList::const_iterator it=pMembers->GetBegin(); it!=pMembers->GetEnd(); ++it )
			{
				pItem->AddUnit( it->GetPtr() );
			}

			break;
	}
}

void CSquadFormationsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CSquadFormationPropsItem *pItem = new CSquadFormationPropsItem;
		pItem->SetItemName( "Formation" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->SetChangedFlag( true );
		
		CTreeItem *pMembers = GetSiblingItem( E_SQUAD_MEMBERS_ITEM );
		NI_ASSERT( pMembers != 0 );

		for ( CTreeItemList::const_iterator it=pMembers->GetBegin(); it!=pMembers->GetEnd(); ++it )
		{
			pItem->AddUnit( it->GetPtr() );
		}
	}
}

void CSquadFormationPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Formation type";
	prop.szDisplayName = "Formation type";
	prop.value = "default";
	prop.szStrings.push_back( "default" );
	prop.szStrings.push_back( "movement" );
	prop.szStrings.push_back( "defensive" );
	prop.szStrings.push_back( "offensive" );
	prop.szStrings.push_back( "sneak" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Hit switch formation";
	prop.szDisplayName = "Hit switch formation";
	prop.value = -1;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Lie state";
	prop.szDisplayName = "Lie state";
	prop.szStrings.push_back( "standart" );
	prop.szStrings.push_back( "always stand" );
	prop.szStrings.push_back( "always lie" );
	prop.value = "standart";
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed bonus";
	prop.szDisplayName = "Speed bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Dispersion bonus";
	prop.szDisplayName = "Dispersion bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Fire rate bonus";
	prop.szDisplayName = "Fire rate bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relax time bonus";
	prop.szDisplayName = "Relax time bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Cover bonus";
	prop.szDisplayName = "Cover bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 9;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Visible bonus";
	prop.szDisplayName = "Sight bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

int CSquadFormationPropsItem::GetFormationType()
{
	std::string szVal = values[0].value;
	if ( szVal == "default" || szVal == "Default" )
		return SSquadRPGStats::SFormation::DEFAULT;
	if ( szVal == "movement" || szVal == "Movement" )
		return SSquadRPGStats::SFormation::MOVEMENT;
	if ( szVal == "defensive" || szVal == "Defensive" )
		return SSquadRPGStats::SFormation::DEFENSIVE;
	if ( szVal == "offensive" || szVal == "Offensive" )
		return SSquadRPGStats::SFormation::OFFENSIVE;
	if ( szVal == "sneak" || szVal == "Sneak" )
		return SSquadRPGStats::SFormation::SNEAK;
	NI_ASSERT( 0 );
	return 0;
}

int CSquadFormationPropsItem::GetLieState()
{
	string szVal = values[2].value;
	if ( szVal == "standart" || szVal == "Standart" )
		return 0;
	if ( szVal == "always stand" || szVal == "Always stand" )
		return 1;
	if ( szVal == "always lie" || szVal == "Always lie" )
		return 2;
	NI_ASSERT( 0 );
	return -1;
}

void CSquadFormationPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
		pFrame->SetChangedFlag( true );
		pFrame->UpdateActiveFormation();
	}
}

void CSquadFormationPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
			pFrame->ClearPropView();
			DeleteMeInParentTreeItem();
			pFrame->SetChangedFlag( true );
			break;
	}
}

void CSquadFormationPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
		pFrame->ClearPropView();
		DeleteMeInParentTreeItem();
		pFrame->SetChangedFlag( true );
	}
}

void CSquadFormationPropsItem::AddUnit( CTreeItem *pUnit )
{
	SUnit newUnit;
	newUnit.pMemberProps = pUnit;
	
	int nx, ny;
	nx = units.size() % 8;
	ny = units.size() / 8;
	
	newUnit.vPos.x = vZeroPos.x + nx * fWorldCellSize;
	newUnit.vPos.y = vZeroPos.y + ny * fWorldCellSize;
	newUnit.vPos.z = 0;
	newUnit.fDir = 0;
	
/*
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	string szName = "units\\humans\\";
	szName += pUnit->GetItemName();
	szName += "\\1";
	newUnit.pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( szName.c_str(), 0, SGVOT_SPRITE ) );
	NI_ASSERT( newUnit.pSprite != 0 );

	newUnit.pSprite->SetPosition( newUnit.vPos );
	newUnit.pSprite->SetDirection( 0 );
	newUnit.pSprite->SetAnimation( 0 );
	pSG->AddObject( it->pSprite, SGVOGT_UNIT );
*/

	units.push_back( newUnit );
	CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
	pFrame->UpdateActiveFormation();
}

void CSquadFormationPropsItem::DeleteUnit( CTreeItem *pUnit )
{
	CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
	pFrame->DeleteUnitFromScene( pUnit, this );

	for ( CUnitsList::iterator it=units.begin(); it!=units.end(); ++it )
	{
		if ( it->pMemberProps == pUnit )
		{
			units.erase( it );
			break;
		}
	}
}

void CSquadFormationPropsItem::SetUnitPointer( int nIndex, CTreeItem *pUnit )
{
	int i = 0;
	for ( CUnitsList::iterator it=units.begin(); it!=units.end(); ++it )
	{
		if ( i == nIndex )
		{
			it->pMemberProps = pUnit;
			return;
		}
		i++;
	}
}

void CSquadFormationPropsItem::MyLButtonClick()
{
	CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
	pFrame->SetActiveFormation( this );
}

int CSquadFormationPropsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "units", &units );
	saver.Add( "ZeroPos", &vZeroPos );
	saver.Add( "FormationDir", &fFormationDir );
	
	return 0;
}

int CSquadFormationPropsItem::SUnit::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Pos", &vPos );
	saver.Add( "Dir", &fDir );

	return 0;
}
