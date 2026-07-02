#ifndef __TILE_SET_TREE_ITEM_H__
#define __TILE_SET_TREE_ITEM_H__

#include "TreeItem.h"
#include "ThumbList.h"


class CTileSetTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetTreeRootItem );
public:
	CTileSetTreeRootItem() { bStaticElements = true; nItemType = E_TILESET_ROOT_ITEM; InitDefaultValues(); }
	~CTileSetTreeRootItem() {}
	
	virtual void InitDefaultValues();
	void ComposeTiles( const char *pszProjectFileName, const char *pszResFileName );
};

class CTileSetCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetCommonPropsItem );
public:
	CTileSetCommonPropsItem() { nItemType = E_TILESET_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CTileSetCommonPropsItem() {};

	const char *GetTileSetName() { return values[0].value; }
	
	void SetTileSetName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
};

class CTileSetTerrainsItem : public CTreeItem
{
	SThumbItems m_thumbItems;			//эти items отображаютс€ в AllDirThumbList
	CImageList imageList;

	OBJECT_NORMAL_METHODS( CTileSetTerrainsItem );
public:
	CTileSetTerrainsItem() { nItemType = E_TILESET_TERRAINS_ITEM; InitDefaultValues(); nImageIndex = 7; imageList.Create(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, ILC_COLOR32, 0, 1); }
	~CTileSetTerrainsItem() {};
	
	SThumbItems* GetThumbItems() { return &m_thumbItems; }
	CImageList*  GetImageList() { return &imageList; }
	
	const char *GetTerrainsDirName() { return values[0].value; }
	void SetTerrainsDirName( const char *pszVal ) { values[0].value = pszVal; }

	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CTileSetTerrainPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetTerrainPropsItem );
public:
	CTileSetTerrainPropsItem() { bStaticElements = true; nItemType = E_TILESET_TERRAIN_PROPS_ITEM; InitDefaultValues(); nImageIndex = 5; }
	~CTileSetTerrainPropsItem() {};

	const char *GetTerrainName() { return values[0].value; }
	int GetCrossetNumber() { return values[1].value; }
	int GetMaskPriority() { return values[2].value; }
	float GetPassability() { return values[3].value; }
	int GetPassForInfantry() { return values[4].value; }
	int GetPassForWheels() { return values[5].value; }
	int GetPassForHalfTracks() { return values[6].value; }
	int GetPassForTracks() { return values[7].value; }
	bool GetMicrotextureFlag() { return values[8].value; }
	float GetSoundVolume() { return values[9].value; }
	const char *GetSoundRef() { return values[10].value; }
	const char *GetLoopedSoundRef() { return values[11].value; }
	bool GetBuildFlag() { return values[12].value; }
	bool GetWaterFlag() { return values[13].value; }
	bool GetTraceFlag() { return values[14].value; }
	bool GetDustFlag() { return values[15].value; }
	
	void SetTerrainName( const char *pszVal ) { values[0].value = pszVal; }
	void SetCrossetNumber( int nVal ) { values[1].value = nVal; }
	void SetMaskPriority( int nVal ) { values[2].value = nVal; }
	void SetPassability( float fVal ) { values[3].value = fVal; }
	void SetPassForInfantry( bool bVal ) { values[4].value = bVal; }
	void SetPassForWheels( bool bVal ) { values[5].value = bVal; }
	void SetPassForHalfTracks( bool bVal ) { values[6].value = bVal; }
	void SetPassForTracks( bool bVal ) { values[7].value = bVal; }
	void SetMicrotextureFlag( bool bVal ) { values[8].value = bVal; }
	void SetSoundVolume( float fVal ) { values[9].value = fVal; }
	void SetSoundRef( const char *pszVal ) { values[10].value = pszVal; }
	void SetLoopedSoundRef( const char *pszVal ) { values[11].value = pszVal; }
	void SetBuildFlag( bool bVal ) { values[12].value = bVal; }
	void SetWaterFlag( bool bVal ) { values[13].value = bVal; }
	void SetTraceFlag( bool bVal ) { values[14].value = bVal; }
	void SetDustFlag( bool bVal ) { values[15].value = bVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CTileSetTilesItem : public CTreeItem
{
private:
	SThumbItems m_thumbItems;			//эти items отображаютс€ в SelectedThumbList
	bool bLoaded;									//этот флаг дл€ подкачки items только в момент когда пользователь выбирает папку c этим terrain или тыкает во frame
	
	OBJECT_NORMAL_METHODS( CTileSetTilesItem );
public:
	CTileSetTilesItem() { bLoaded = false; nItemType = E_TILESET_TILES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CTileSetTilesItem() {};
	
	void SetLoadedFlag( bool bState ) { bLoaded = bState; }
	bool GetLoadedFlag() { return bLoaded; }
	SThumbItems* GetThumbItems() { return &m_thumbItems; }
	
	virtual void InitDefaultValues();
	virtual void InsertChildItems();					//¬ызываетс€ после создани€ всех компонентов дл€ занесени€ их в дерево
	virtual void MyLButtonClick();
};

class CTileSetASoundsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetASoundsItem );
public:
	CTileSetASoundsItem() { nItemType = E_TILESET_ASOUNDS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CTileSetASoundsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CTileSetASoundPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetASoundPropsItem );
public:
	CTileSetASoundPropsItem() { nItemType = E_TILESET_ASOUND_PROPS_ITEM; nImageIndex = 6; InitDefaultValues(); }
	~CTileSetASoundPropsItem() {};
	
	const char *GetSoundName() { return values[0].value; }
	bool GetPeaceFlag() { return values[1].value; }
	float GetProbability() { return values[2].value; }
	
	void SetSoundName( const char *pszVal ) { values[0].value = pszVal; }
	void SetPeaceFlag( bool bVal ) { values[1].value = bVal; }
	void SetProbability( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CTileSetLSoundsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetLSoundsItem );
public:
	CTileSetLSoundsItem() { nItemType = E_TILESET_LSOUNDS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CTileSetLSoundsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CTileSetLSoundPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetLSoundPropsItem );
public:
	CTileSetLSoundPropsItem() { nItemType = E_TILESET_LSOUND_PROPS_ITEM; nImageIndex = 6; InitDefaultValues(); }
	~CTileSetLSoundPropsItem() {};
	
	const char *GetSoundName() { return values[0].value; }
	bool GetPeaceFlag() { return values[1].value; }
	
	void SetSoundName( const char *pszVal ) { values[0].value = pszVal; }
	void SetPeaceFlag( bool bVal ) { values[1].value = bVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CTileSetTilePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTileSetTilePropsItem );
public:
	enum
	{
		E_NORMAL,
		E_FLIPPED,
		E_BOTH,
	};

	int nTileIndex;

	CTileSetTilePropsItem() : nTileIndex( -1 ) { nItemType = E_TILESET_TILE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CTileSetTilePropsItem() {};
	
	float GetProbability() { return values[0].value; }
	int GetFlippedState();

	void SetProbability( float fVal ) { values[0].value = fVal; }
	void SetFippedState( int nVal );

	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual int operator&( IDataTree &ss );
};


class CCrossetsItem : public CTreeItem
{
	SThumbItems m_thumbItems;			//эти items отображаютс€ в AllDirThumbList
	CImageList imageList;
	bool bLoaded;
	
	OBJECT_NORMAL_METHODS( CCrossetsItem );
public:
	CCrossetsItem() { bLoaded = 0; nItemType = E_CROSSETS_ITEM; InitDefaultValues(); nImageIndex = 7; imageList.Create(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, ILC_COLOR32, 0, 1); }
	~CCrossetsItem() {};
	
	SThumbItems* GetThumbItems() { return &m_thumbItems; }
	CImageList*  GetImageList() { return &imageList; }

	void SetLoadedFlag( bool bState ) { bLoaded = bState; }
	bool GetLoadedFlag() { return bLoaded; }
	
	const char *GetCrossetsDirName() { return values[0].value; }
	void SetCrossetsDirName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );		//insert new crosset
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CCrossetPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCrossetPropsItem );
public:
	CCrossetPropsItem() { bStaticElements = true; nItemType = E_CROSSET_PROPS_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CCrossetPropsItem() {};
	
	const char *GetCrossetName() { return values[0].value; }
	
	void SetCrossetName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );			//delete this crosset
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CCrossetTilesItem : public CTreeItem
{
private:
	SThumbItems m_thumbItems;			//эти items отображаютс€ в SelectedThumbList
	bool bLoaded;									//этот флаг дл€ подкачки items только в момент когда пользователь выбирает папку c этим crosset
	
	OBJECT_NORMAL_METHODS( CCrossetTilesItem );
public:
	CCrossetTilesItem() { bLoaded = 0; nItemType = E_CROSSET_TILES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CCrossetTilesItem() {};
	
	void SetLoadedFlag( bool bState ) { bLoaded = bState; }
	bool GetLoadedFlag() { return bLoaded; }
	SThumbItems* GetThumbItems() { return &m_thumbItems; }

	virtual void InsertChildItems();					//¬ызываетс€ после создани€ всех компонентов дл€ занесени€ их в дерево
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CCrossetTilePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCrossetTilePropsItem );
public:
	int nCrossIndex;

	CCrossetTilePropsItem() : nCrossIndex( -1 ) { nItemType = E_CROSSET_TILE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CCrossetTilePropsItem() {};
	
	float GetProbability() { return values[0].value; }
	
	void SetProbability( float fVal ) { values[0].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual int operator&( IDataTree &ss );
};

#endif		//__TILE_SET_TREE_ITEM_H__
