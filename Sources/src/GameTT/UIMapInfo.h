#ifndef __UIMAPINFO_H__
#define __UIMAPINFO_H__
#pragma ONCE
#include "..\RandomMapGen\MapInfo_Types.h"
struct SUIMapInfo : public IRefCount
{
	OBJECT_NORMAL_METHODS( SUIMapInfo );
public:
	SQuickLoadMapInfo mapInfo;
	std::string szPath;
	std::string GetID() const { return szPath; }
	
	const wchar_t * GetVisualName();
	bool LoadMapInfo( const char *szMapName );
	bool LoadMapByPath( const char *_szPath );

	static const wchar_t * GetVisualName( const std::string szPath );
};
#endif // __UIMAPINFO_H__
