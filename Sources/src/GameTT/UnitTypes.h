#ifndef __UNIT_TYPES_H__
#define	__UNIT_TYPES_H__
#pragma ONCE
struct SUnitClass
{
	int nClass;
	const char *pszName;
};
extern SUnitClass unitClasses[];
extern int nUnitClassesSize;

extern SUnitClass unitTypes[];

extern int nUnitTypesSize;

extern CTRect<float> rcTechnicsInfoPanelMap;
const char *GetUnitClassName( int nUnitClass );

void FillUnitInfoItemNoIDs( const struct SUnitBaseRPGStats *pRPG, interface IUIDialog *pItem, int nScenarioIndex, bool bFillCommanderName, const char *pszCommanderName );
void FillUnitInfoItem( const struct SUnitBaseRPGStats *pRPG, interface IUIDialog *pItem, int nIndex, bool bFillCommanderName, const char *pszCommanderName = 0 );
#endif		//__UNIT_TYPES_H__
