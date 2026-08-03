#ifndef __TRACK_H__
#define __TRACK_H__
#pragma ONCE
#include "..\Misc\Win32Random.h"
struct STrackContext
{
	float fTime;													// time of the last calculated value
	float fValue;													// value, calculated at the 'fTime'
	int nUpperKeyIndex;										// last upper bound key
	float fLowerKeyValue;									// lower bound key value (randomized)
	float fUpperKeyValue;									// upper bound key value (randomized)
};
class CTrack
{
	struct SKey
	{
		float fTime;												// time of this key
		float fValue;												// value
		SKey() {  }
		SKey( const float _fTime, const float _fValue ) : fTime( _fTime ), fValue( _fValue ) {  }
		const bool operator<( const SKey &key ) const { return fTime < key.fTime; }
		int operator&( IDataTree &ss ) { CTreeAccessor saver = &ss; saver.Add( "value", &fValue ); saver.Add( "time", &fTime );	return 0; }
	};
	typedef std::vector<SKey> CKeysList;
	CKeysList keys;												// keys
	float fScale;                         // масштаб времени
	CKeysList::iterator FindKey( const float fTime ) { return std::upper_bound( keys.begin(), keys.end(), SKey(fTime, 0) ); }
	CKeysList::const_iterator FindKey( const float fTime ) const { return std::upper_bound( keys.begin(), keys.end(), SKey(fTime, 0) ); }
public:
	CTrack();
	CTrack& operator=( const CTrack &trc ) { keys = trc.keys; fScale = trc.fScale; return *this; }
	void AddKey( const float fTime, const float fValue );
	void RemoveKey( const float fTime );
	void Normalize( const float _fScale );
	const int FindLowerBound( const float fTime ) const { return std::distance( keys.begin(), std::lower_bound(keys.begin(), keys.end(), SKey(fTime, 0)) ); }
	const int FindUpperBound( const float fTime ) const { return std::distance( keys.begin(), std::upper_bound(keys.begin(), keys.end(), SKey(fTime, 0)) ); }
	const float GetValue( const float fTime ) const;
	const float GetValue( const float fTime, const CTrack &rnd ) const;
	void CreateStartContext( STrackContext *pContext ) const;
	const float Integrate( STrackContext *pContext, const float fTimeEnd ) const;
	void CreateStartContext( STrackContext *pContext, const CTrack &rnd ) const;
	const float Integrate( STrackContext *pContext, const float fTimeEnd, const CTrack &rnd ) const;
	int operator&( IDataTree &ss );
	bool IsEmpty() const { return keys.empty(); }
	float GetTimeByIndex( int index ) const;
	int GetNumKeys() const;
	void Clear() { keys.clear(); fScale = 1000.0f; }
};
inline const float GetRandomFromTrack( const float fTime, const CTrack &rnd )
{
	const float fRnd = rnd.GetValue( fTime );
	return NWin32Random::RandomCheck( -fRnd, fRnd );
}
#endif // __TRACK_H__
