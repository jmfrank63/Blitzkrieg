#ifndef __ANIM_TREE_ITEM_H__
#define __ANIM_TREE_ITEM_H__

#include "TreeItem.h"
#include "ThumbList.h"

/////////////////////////////////////// UNIT editor tree //////////////////////////////////////

class CAnimationTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CAnimationTreeRootItem );
public:
	CAnimationTreeRootItem() { bStaticElements = true; nItemType = E_ANIMATION_ROOT_ITEM; InitDefaultValues(); }
	~CAnimationTreeRootItem() {}

	virtual void InitDefaultValues();
	bool ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, bool bSetCycledFlag, bool bShowUserMessages );
	FILETIME FindMaximalSourceTime( const char *pszProjectFileName );
};

class CUnitCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitCommonPropsItem );
public:
	CUnitCommonPropsItem() { nItemType = E_UNIT_COMMON_PROPS_ITEM; InitDefaultValues(); }
	~CUnitCommonPropsItem() {};

	//��������� ���������� ����������
	const char *GetUnitName() { return values[0].value; }
	int GetUnitType();
	const char *GetPictureFileName() { return values[2].value; }
	float GetHealth() { return values[3].value; }
	int GetArmor() { return values[4].value; }
	float GetCamouflage() { return values[5].value; }
	float GetSpeed() { return values[6].value; }
	float GetPassability() { return values[7].value; }
	bool GetAttackUpFlag() { return values[8].value; }
	bool GetAttackDownFlag() { return values[9].value; }
	float GetAIPrice() { return values[10].value; }
	float GetSight() { return values[11].value; }
	float GetSightPower() { return values[12].value; }
	
	//��������� ���������� ����������
	void SetUnitName( const char *pszName ) { values[0].value = pszName; }
	void SetUnitType( int nVal );
	void SetPictureFileName( const char *pszName ) { values[2].value = pszName; }
	void SetHealth( float fVal ) { values[3].value = fVal; }
	void SetArmor( int nVal ) { values[4].value = nVal; }
	void SetCamouflage( float fVal ) { values[5].value = fVal; }
	void SetSpeed( float fVal ) { values[6].value = fVal; }
	void SetPassability( float fVal ) { values[7].value = fVal; }
	void SetAttackUpFlag( bool bVal ) { values[8].value = bVal; }
	void SetAttackDownFlag( bool bVal ) { values[9].value = bVal; }
	void SetAIPrice( float fVal ) { values[10].value = fVal; }
	void SetSight( float fVal ) { values[11].value = fVal; }
	void SetSightPower( float fVal ) { values[12].value = fVal; }
	
	virtual void InitDefaultValues();
};

struct SUnitBaseRPGStats;
class CUnitActionsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitActionsItem );
public:
	CUnitActionsItem() { bStaticElements = true; nItemType = E_UNIT_ACTIONS_ITEM; InitDefaultValues(); nImageIndex = 4; }
	~CUnitActionsItem() {};

	void GetActions( SUnitBaseRPGStats *pRPGStats );
	void SetActions( const SUnitBaseRPGStats *pRPGStats );
	
	virtual void InitDefaultValues();
};

class CUnitExposuresItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitExposuresItem );
public:
	CUnitExposuresItem() { bStaticElements = true; nItemType = E_UNIT_EXPOSURES_ITEM; InitDefaultValues(); nImageIndex = 4; }
	~CUnitExposuresItem() {};
	
	void GetExposures( SUnitBaseRPGStats *pRPGStats );
	void SetExposures( const SUnitBaseRPGStats *pRPGStats );
	
	virtual void InitDefaultValues();
};

class CUnitActionPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitActionPropsItem );
public:
	CUnitActionPropsItem() { nItemType = E_UNIT_ACTION_PROPS_ITEM; InitDefaultValues(); nImageIndex = 1; }
	~CUnitActionPropsItem() {};
/*
	const char *GetActionValue() { return values[0].value; }
*/
	virtual void InitDefaultValues();
//	virtual void MyKeyDown( int nChar );
//	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CUnitAIPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAIPropsItem );
public:
	CUnitAIPropsItem() { nItemType = E_UNIT_AI_PROPS_ITEM; InitDefaultValues(); }
	~CUnitAIPropsItem() {};
	
	virtual void InitDefaultValues();
};

class CUnitWeaponPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitWeaponPropsItem );
public:
	CUnitWeaponPropsItem() { nItemType = E_UNIT_WEAPON_PROPS_ITEM; InitDefaultValues(); }
	~CUnitWeaponPropsItem() {};
	
	const char *GetWeaponName() { return values[0].value; }
	int GetAmmoCount() { return values[1].value; }
	float GetReloadCost() { return values[2].value; }
	
	void SetWeaponName( const char *pszName ) { values[0].value = pszName; }
	void SetAmmoCount( int nVal ) { values[1].value = nVal; }
	void SetReloadCost( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CUnitGrenadePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitGrenadePropsItem );
public:
	CUnitGrenadePropsItem() { nItemType = E_UNIT_GRENADE_PROPS_ITEM; InitDefaultValues(); }
	~CUnitGrenadePropsItem() {};

	const char *GetGrenadeName() { return values[0].value; }
	int GetAmmoCount() { return values[1].value; }
	float GetReloadCost() { return values[2].value; }
	
	void SetGrenadeName( const char *pszName ) { values[0].value = pszName; }
	void SetAmmoCount( int nVal ) { values[1].value = nVal; }
	void SetReloadCost( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CDirectoryPropsItem : public CTreeItem
{
private:
	int nSelImage;
	SThumbItems m_thumbItems;			//��� items ������������ � AllDirThumbList
	CImageList imageList;
	bool bLoaded;									//���� ���� ��� �������� items ������ � ������ ����� ������������ �������� ����� � ���������� ��� ������ �� frame
	
	OBJECT_NORMAL_METHODS( CDirectoryPropsItem );
public:
	CDirectoryPropsItem() {
		nItemType = E_UNIT_DIRECTORY_PROPS_ITEM;
		nImageIndex = 3;
		InitDefaultValues();
		bComplexItem = true;
		nSelImage = 8;
		bLoaded = false;
		imageList.Create(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, ILC_COLOR24, 0, 1);
	}
	~CDirectoryPropsItem() {};

	const char*  GetDirName() { return values[0].value; }
	SThumbItems* GetThumbItems() { return &m_thumbItems; }
	CImageList*  GetImageList() { return &imageList; }
	void SetLoadedFlag( bool bState ) { bLoaded = bState; }
	bool GetLoadedFlag() { return bLoaded; }
	
	void SetSelectedIcon( bool bFlag );
	void SetDirName( const char *pszVal ) { values[0].value = pszVal; }

	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CUnitSeasonPropsItem : public CTreeItem
{
private:
	OBJECT_NORMAL_METHODS( CUnitSeasonPropsItem );
public:
	CUnitSeasonPropsItem() { bStaticElements = true; nItemType = E_UNIT_SEASON_PROPS_ITEM; InitDefaultValues(); bComplexItem = true; nImageIndex = 2; }
	~CUnitSeasonPropsItem() {};

	const char *GetDirName( int nIndex );
	virtual void InitDefaultValues();
};

class CDirectoriesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CDirectoriesItem );
	CDirectoryPropsItem *pActiveDirPropsItem;
public:
	CDirectoriesItem() : pActiveDirPropsItem(0) {	bStaticElements = true; nItemType = E_UNIT_DIRECTORIES_ITEM; InitDefaultValues(); bComplexItem = true; nImageIndex = 2; }
	~CDirectoriesItem() {};

	void SetDefaultDirPropsItem( CDirectoryPropsItem *pDirProps );

	virtual void InitDefaultValues();
};

class CUnitAnimationsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAnimationsItem );
public:
	CUnitAnimationsItem() { bStaticElements = true; nItemType = E_UNIT_ANIMATIONS_ITEM; InitDefaultValues(); bComplexItem = true; nImageIndex = 2; }
	~CUnitAnimationsItem() {};
	
	virtual void InitDefaultValues();
//	virtual void MyKeyDown( int nChar );
};

class CUnitFramePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitFramePropsItem );
public:
	CUnitFramePropsItem() { nItemType = E_UNIT_FRAME_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CUnitFramePropsItem() {};
	
	virtual void InitDefaultValues();

	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CUnitAnimationPropsItem : public CTreeItem
{
public:
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS( CUnitAnimationPropsItem );

private:
	SThumbItems m_thumbItems;			//��� items ������������ � SelectedThumbList
	bool bLoaded;									//���� ���� ��� �������� items ������ � ������ ����� ������������ �������� ����� � ���������� ��� ������ �� frame

public:
	CUnitAnimationPropsItem() { nItemType = E_UNIT_ANIMATION_PROPS_ITEM; InitDefaultValues(); bComplexItem = true; nImageIndex = 7; bLoaded = false; }
	~CUnitAnimationPropsItem() {};

	SThumbItems* GetThumbItems() { return &m_thumbItems; }

	virtual bool CopyItemTo( CTreeItem *pTo );

	//��������� ���������� ����������
	int GetFrameTime();
	float GetAnimationSpeed();
	bool GetCycledFlag();
	int GetNumberOfDirections();
	int GetActionFrame();
	int GetAnimationType();
	
	void SetLoadedFlag( bool bState ) { bLoaded = bState; }
	bool GetLoadedFlag() { return bLoaded; }
	void SetAnimationSpeed( float fVal );
	virtual void InsertChildItems();					//���������� ����� �������� ���� ����������� ��� ��������� �� � ������
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CUnitAcksItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAcksItem );

public:
	CUnitAcksItem() { bStaticElements = true; nItemType = E_UNIT_ACKS_ITEM; nImageIndex = 1; InitDefaultValues(); }
	~CUnitAcksItem() {};
	
	//��������� ���������� ����������
	const char *GetAckName() { return values[0].value; }
	const char *GetAckName2() { return values[1].value; }
	
	//���������
	void SetAckName( const char *pszVal ) { values[0].value = pszVal; }
	void SetAckName2( const char *pszVal ) { values[1].value = pszVal; }
	
	virtual void InitDefaultValues();
};

class CUnitAckTypesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAckTypesItem );
public:
	CUnitAckTypesItem() { nItemType = E_UNIT_ACK_TYPES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CUnitAckTypesItem() {};
};

class CUnitAckTypePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAckTypePropsItem );
public:
	CUnitAckTypePropsItem() { nItemType = E_UNIT_ACK_TYPE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CUnitAckTypePropsItem() {};
};

/*
class CUnitAcksItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAcksItem );
	const char* GetAckName( int nID );

public:
	CUnitAcksItem() { bStaticElements = true; nItemType = E_UNIT_ACKS_ITEM; nImageIndex = 1; InitDefaultValues(); }
	~CUnitAcksItem() {};

	typedef std::vector< std::vector< std::pair< float,std::string > > > CAcks;
	void FillAcks( CAcks &acks );		//acks will be changed here
	void GetAcks( const CAcks &acks );

	virtual void InitDefaultValues();
	virtual void MyRButtonClick();
};

class CUnitAckTypesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAckTypesItem );
public:
	CUnitAckTypesItem() { nItemType = E_UNIT_ACK_TYPES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CUnitAckTypesItem() {};
	
	int GetAckType();

	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
};

class CUnitAckTypePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CUnitAckTypePropsItem );
public:
	CUnitAckTypePropsItem() { nItemType = E_UNIT_ACK_TYPE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CUnitAckTypePropsItem() {};
	
	//��������� ���������� ����������
	const char* GetSoundName() { return values[0].value; }
	float GetProbability() { return values[1].value; }
	
	//��������� ���������� ����������
	void SetSoundName( const char *pszVal ) { values[0].value = pszVal; }
	void SetProbability( float fVal ) { values[1].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
};
*/

#endif		//__ANIM_TREE_ITEM_H__

