#if !defined(__FMT__SOUND__H__)
#define __FMT__SOUND__H__

#pragma ONCE
struct CMapSoundInfo
{
	std::string szName;										// название звука 
	CVec3 vPos;														// точка приписки

	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
typedef std::vector<CMapSoundInfo> TMapSoundInfoList;
#endif //#if !defined(__FMT__SOUND__H__)

