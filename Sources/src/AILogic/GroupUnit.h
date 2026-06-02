#ifndef __GROUP_UNIT_H__
#define __GROUP_UNIT_H__
#pragma ONCE
class CGroupUnit
{
	DECLARE_SERIALIZE;
	
	int nGroup;
	int nPos;
	int nSubGroup;
	CVec2 vShift;
	
	int nSpecialGroup;
	int nSpecialPos;

public:
	CGroupUnit() : nGroup( 0 ), nPos( 0 ), nSubGroup( -1 ), vShift( VNULL2 ), nSpecialGroup( 0 ) { }
	void Init() {}

	const CVec2& GetGroupShift() const { return vShift; }
	const int& GetSubGroup() const { return nSubGroup; }
	const int GetNGroup() const { return nGroup; }
	const int GetSpecialGroup() const { return nSpecialGroup; }

	void SetGroupShift( const CVec2 &_vShift ) { vShift = _vShift; }	
	void SetSubGroup( const int _nSubGroup ) { nSubGroup = _nSubGroup; }

	friend class CGroupLogic;
};
#endif // __GROUP_UNIT_H__
