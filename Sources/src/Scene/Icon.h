#ifndef __ICON_H__
#define __ICON_H__
#pragma ONCE
struct SIconDesc
{
	CPtr<ISceneIcon> pIcon;								// icon itself
	int nID;															// icons ID
	int nPriority;												// priority (for place selection)
	DWORD placement;											// palcement flags
	CVec3 vAddValue;											// additional value for placement
	CVec3 vAddStep;
	int operator&( IStructureSaver &ss );
};
typedef std::list<SIconDesc> CIconsList;
struct SIconDescPriorityLessFunctional
{
	bool operator()( const SIconDesc &id1, const SIconDesc &id2 ) const { return id1.nPriority < id2.nPriority; }
};
void RepositionIconsLocal( CIconsList &icons, DWORD placement, const CTRect<float> &rcRect, const float _fAddZ = 20 );
#endif // __ICON_H__
