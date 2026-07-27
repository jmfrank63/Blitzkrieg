#ifndef __AI_HASH_FUNCS__
#define __AI_HASH_FUNCS__
#pragma ONCE
#include "AIGeometry.h"
interface IUpdatableObj;
class CAIUnit;
struct SUpdatableObjectObjHash
{
	int operator()( const CObj<IUpdatableObj> &a ) const;
};
struct SUnitObjHash
{
	int operator()( const CObj<CAIUnit> &a ) const;
};
struct SUniqueIdHash
{
	template <class T>
		int operator()( const CObj<T> &a ) const { return a.GetPtr()->GetUniqueId(); }
	template <class T>
		int operator()( const T *a ) const { return a->GetUniqueId(); }
};
struct STilesHash
{
	int operator()( const SVector &tile ) const 
	{ 
		NI_ASSERT_T( tile.x >=0 && tile.y >= 0 && tile.x <= 4095 && tile.y <= 4095, NStr::Format( "Can't hash tile ( %d, %d )\n", tile.x, tile.y ) );
		return ( tile.x << 12 ) | tile.y;
	}
};
struct SVec2Hash
{
	int operator()( const CVec2 &pos) const 
	{ 
		return ( (int(pos.x)<<16) | int(pos.y) );
	}
};
struct SVec2Equ
{
	bool operator()( const CVec2 &v1, const CVec2 &v2 ) const
	{
		return fabs2( v1.x - v2.x, v1.y - v2.y ) < 0.0000001f;
	}
};
#endif // __AI_HASH_FUNCS__
