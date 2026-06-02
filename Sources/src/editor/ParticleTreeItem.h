#ifndef __PARTICLE_TREE_ITEM_H__
#define __PARTICLE_TREE_ITEM_H__

#include "TreeItem.h"

class CParticleTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleTreeRootItem );
public:
	CParticleTreeRootItem() { bStaticElements = true; nItemType = E_PARTICLE_ROOT_ITEM; InitDefaultValues(); }
	~CParticleTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CParticleCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleCommonPropsItem );
public:
	CParticleCommonPropsItem() { nItemType = E_PARTICLE_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CParticleCommonPropsItem() {};
	
	const char* GetParticleName() { return values[0].value; }
	int GetLifeTime() { return values[1].value; }
	float GetScaleFactor() { return values[2].value; }
	float GetGravity() { return values[3].value; }
	CVec3 GetPositionVector();
	CVec3 GetDirectionVector();
	CVec3 GetWindVector();
	float GetWindPower() { return values[13].value; }
	int GetAreaType();
	
	void SetParticle( const char *pszName ) { values[0].value = pszName; }
	void SetLifeTime( int nVal ) { values[1].value = nVal; }
	void SetScaleFactor( float fVal ) { values[2].value = fVal; }
	void SetGravity( float fVal ) { values[3].value = fVal; }
	void SetPositionVector( const CVec3 &vec );
	void SetDirectionVector( const CVec3 &vec );
	void SetWindVector( const CVec3 &vec );
	void SetWindPower( float fVal ) { values[13].value = fVal; }
	void SetAreaType( int nVal );
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CParticleSourcePropItems : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleSourcePropItems );
public:
	CParticleSourcePropItems() { bStaticElements = true; nItemType = E_PARTICLE_SOURCE_PROP_ITEMS; InitDefaultValues(); nImageIndex = 2; }
	~CParticleSourcePropItems() {};
	
	const char *GetTextureFileName() { return values[0].value; }
	int GetTextureXSize() { return values[1].value; }
	int GetTextureYSize() { return values[2].value; }
	
	void SetTextureFileName( const char *pszName ) { values[0].value = pszName; }
	void SetTextureXSize( int nVal ) { values[1].value = nVal; }
	void SetTextureYSize( int nVal ) { values[2].value = nVal; }
	
	virtual void InitDefaultValues();
};

class CParticleGenerateLifeItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateLifeItem );
public:
	CParticleGenerateLifeItem() { nItemType = E_PARTICLE_GENERATE_LIFE_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateLifeItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleRandLifeItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleRandLifeItem );
public:
	CParticleRandLifeItem() { nItemType = E_PARTICLE_RAND_LIFE_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleRandLifeItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateSpeedItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateSpeedItem );
public:
	CParticleGenerateSpeedItem() { nItemType = E_PARTICLE_GENERATE_SPEED_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateSpeedItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleRandSpeedItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleRandSpeedItem );
public:
	CParticleRandSpeedItem() { nItemType = E_PARTICLE_RAND_SPEED_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleRandSpeedItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateSpinItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateSpinItem );
public:
	CParticleGenerateSpinItem() { nItemType = E_PARTICLE_GENERATE_SPIN_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateSpinItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateRandomSpinItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateRandomSpinItem );
public:
	CParticleGenerateRandomSpinItem() { nItemType = E_PARTICLE_GENERATE_RANDOM_SPIN_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateRandomSpinItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateAreaItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateAreaItem );
public:
	CParticleGenerateAreaItem() { nItemType = E_PARTICLE_GENERATE_AREA_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateAreaItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateAngleItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateAngleItem );
public:
	CParticleGenerateAngleItem() { nItemType = E_PARTICLE_GENERATE_ANGLE_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateAngleItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateDensityItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateDensityItem );
public:
	CParticleGenerateDensityItem() { nItemType = E_PARTICLE_GENERATE_DENSITY_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateDensityItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleGenerateOpacityItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleGenerateOpacityItem );
public:
	CParticleGenerateOpacityItem() { nItemType = E_PARTICLE_GENERATE_OPACITY_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleGenerateOpacityItem() {};
	
	virtual void InitDefaultValues();
};


class CParticleComplexSourceItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleComplexSourceItem );
public:
	CParticleComplexSourceItem() { bStaticElements = true; nItemType = E_PARTICLE_COMPLEX_SOURCE_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CParticleComplexSourceItem() {};
	
	const char *GetParticleName() { return values[0].value; }
	
	void SetParticleName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CParticlePropItems : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticlePropItems );
public:
	CParticlePropItems() { bStaticElements = true; nItemType = E_PARTICLE_PROP_ITEMS; InitDefaultValues(); nImageIndex = 2; }
	~CParticlePropItems() {};
	
	virtual void InitDefaultValues();
};

class CParticleComplexItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleComplexItem );
public:
	CParticleComplexItem() { bStaticElements = true; nItemType = E_PARTICLE_COMPLEX_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CParticleComplexItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleSpinItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleSpinItem );
public:
	CParticleSpinItem() { nItemType = E_PARTICLE_SPIN_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleSpinItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleWeightItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleWeightItem );
public:
	CParticleWeightItem() { nItemType = E_PARTICLE_WEIGHT_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleWeightItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleSpeedItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleSpeedItem );
public:
	CParticleSpeedItem() { nItemType = E_PARTICLE_SPEED_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleSpeedItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleCRandomSpeedItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleCRandomSpeedItem );
public:
	CParticleCRandomSpeedItem() { nItemType = E_PARTICLE_C_RANDOM_SPEED_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleCRandomSpeedItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleSizeItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleSizeItem );
public:
	CParticleSizeItem() { nItemType = E_PARTICLE_SIZE_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleSizeItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleOpacityItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleOpacityItem );
public:
	CParticleOpacityItem() { nItemType = E_PARTICLE_OPACITY_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleOpacityItem() {};
	
	virtual void InitDefaultValues();
};

class CParticleTextureFrameItem : public CKeyFrameTreeItem
{
	OBJECT_NORMAL_METHODS( CParticleTextureFrameItem );
public:
	CParticleTextureFrameItem() { nItemType = E_PARTICLE_TEXTURE_FRAME_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CParticleTextureFrameItem() {};
	
	virtual void InitDefaultValues();
};

#endif		//__PARTICLE_TREE_ITEM_H__

