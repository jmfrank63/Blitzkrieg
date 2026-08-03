#ifndef __SQUAD_TREE_ITEM_H__
#define __SQUAD_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Main/rpgstats.h"

interface IObjVisObj;

class CSquadTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadTreeRootItem );
public:
	CSquadTreeRootItem() { bStaticElements = true; nItemType = E_SQUAD_ROOT_ITEM; InitDefaultValues(); }
	~CSquadTreeRootItem() {}
	
	virtual void InitDefaultValues();
	virtual void CallMeAfterSerialize();
};

class CSquadCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadCommonPropsItem );
public:
	CSquadCommonPropsItem() { nItemType = E_SQUAD_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CSquadCommonPropsItem() {};
	
	const char* GetSquadName() { return values[0].value; }
	const char* GetSquadPicture() { return values[1].value; }
	int GetSquadType();
	

	virtual void InitDefaultValues();
};

class CSquadMembersItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadMembersItem );
public:
	CSquadMembersItem() { nItemType = E_SQUAD_MEMBERS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CSquadMembersItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CSquadMemberPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadMemberPropsItem );
public:
	CSquadMemberPropsItem() { bStaticElements = true; nItemType = E_SQUAD_MEMBER_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CSquadMemberPropsItem() {};
	
	
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CSquadFormationsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadFormationsItem );
public:
	CSquadFormationsItem() { nItemType = E_SQUAD_FORMATIONS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CSquadFormationsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CSquadFormationPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CSquadFormationPropsItem );

public:
	struct SUnit
	{
		CVec3 vPos;			// 3d ������� ���������� �����
		float fDir;
		CTreeItem *pMemberProps;
		IObjVisObj *pSprite;

		virtual int operator&( IDataTree &ss );
		SUnit() : pMemberProps( 0 ), fDir( 0 ), vPos( VNULL3 ) {}
	};
	typedef list<SUnit> CUnitsList;
	CUnitsList units;

	CVec3 vZeroPos;					//���������� ������ ��������, ������ ���������� ������ �������� ���� �������� ������, ������
	float fFormationDir;		//����������� ��������

public:
	CSquadFormationPropsItem() : fFormationDir( 0 ), vZeroPos( CVec3(16*fWorldCellSize, 8*fWorldCellSize, 0) ) { bStaticElements = true; nItemType = E_SQUAD_FORMATION_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CSquadFormationPropsItem() {};

	void AddUnit( CTreeItem *pUnit );
	void DeleteUnit( CTreeItem *pUnit );
	void SetUnitPointer( int nIndex, CTreeItem *pUnit );
	
	int GetFormationType();
	int GetHitSwitchFormation() { return values[1].value; }
	int GetLieState();
	float GetSpeedBonus() { return values[3].value; }
	float GetDispersionBonus() { return values[4].value; }
	float GetFireRateBonusBonus() { return values[5].value; }
	float GetRelaxTimeBonus() { return values[6].value; }
	float GetCoverBonus() { return values[7].value; }
	float GetVisibleBonus() { return values[8].value; }
	
	
	virtual int operator&( IDataTree &ss );
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

#endif		//__SQUAD_TREE_ITEM_H__
