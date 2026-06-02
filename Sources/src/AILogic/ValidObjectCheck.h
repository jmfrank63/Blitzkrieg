#ifndef __VALID_OBJECT_CHECK__
#define __VALID_OBJECT_CHECK__
#pragma ONCE
template<class T>
inline bool IsValidObj( const T &pObj )
{
	return pObj && pObj->IsValid() && pObj->IsAlive();
}
#endif //__VALID_OBJECT_CHECK__
