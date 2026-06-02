#ifndef __ENEMY_REMEMBERER__
#define __ENEMY_REMEMBERER__

class CCommonUnit;
class CEnemyRememberer : public IRefCount
{
	OBJECT_COMPLETE_METHODS( CEnemyRememberer );
	DECLARE_SERIALIZE;

	CVec2 vPosition;
	NTimer::STime timeLastSeen;
	int timeBeforeForget;
public:
	CEnemyRememberer() {  }
	CEnemyRememberer( const int timeBeforeForget );
	void SetVisible( const class CCommonUnit *pUnit, const bool bVisible );
	const CVec2 &GetPos( const class CCommonUnit *pUnit ) const;
	const NTimer::STime GetLastSeen() const { return timeLastSeen; }
	const bool IsTimeToForget() const;
};
#endif // #define __ENEMY_REMEMBERER__

