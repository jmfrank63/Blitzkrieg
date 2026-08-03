#include "StdAfx.h"

#include "../Scene/Track.h"
inline const float GetRandomValue( const float fValue, const float fTime, const CTrack &rnd )
{
	return fValue * ( 1.0f + GetRandomFromTrack(fTime, rnd) );
}
inline const float Integrate( const float fTime1, const float fVal1, const float fTime2, const float fVal2 )
{
	return 0.5f * ( fVal2 + fVal1 ) * ( fTime2 - fTime1 );
}
CTrack::CTrack() : fScale( 1000 )
{
}
void CTrack::AddKey( const float fTime, const float fValue )
{
	const SKey key( fTime, fValue );
	CKeysList::iterator pos = std::upper_bound( keys.begin(), keys.end(), key );
	keys.insert( pos, SKey(fTime, fValue) );
}
void CTrack::RemoveKey( const float fTime )
{
	CKeysList::iterator pos = FindKey( fTime );
	if ( (pos != keys.end()) && (pos->fTime == fTime) ) 
		keys.erase( pos );
}
const float CTrack::GetValue( const float fTime ) const
{
	NI_ASSERT_SLOW_T( !keys.empty(), "Can't get value from empty track!" );
	CKeysList::const_iterator posEnd = keys.end();
	--posEnd;
	CKeysList::const_iterator posLow, posHigh = keys.begin();
	while ( (posHigh != posEnd) && (fTime >= posHigh->fTime) ) 
		posLow = posHigh++;

	return posLow->fValue + ( fTime - posLow->fTime ) / ( posHigh->fTime - posLow->fTime ) * ( posHigh->fValue - posLow->fValue );
/*
	CKeysList::const_iterator posHigh = FindKey( fTime );
	if ( posHigh == keys.begin() ) 
		return posHigh->fValue;
	else
	{
		CKeysList::const_iterator posLow = posHigh - 1;
		if ( posHigh == keys.end() )
			return posLow->fValue;
		else
		{
			const float fCoeff = ( fTime - posLow->fTime ) / ( posHigh->fTime - posLow->fTime );
			return posLow->fValue + fCoeff*( posHigh->fValue - posLow->fValue );
		}
	}
	*/
} 
const float CTrack::GetValue( const float fTime, const CTrack &rnd ) const
{
	return GetRandomValue( GetValue(fTime), fTime, rnd );
}
const float CTrack::Integrate( STrackContext *pContext, const float _fTimeEnd, const CTrack &rnd ) const
{
	const float fTimeEnd = Clamp( _fTimeEnd, 0.0f, 1.0f );
	NI_ASSERT_SLOW_TF( pContext->fTime <= fTimeEnd, "Can't integrate track - start time are greater then finish one", return 0 );
	if ( pContext->nUpperKeyIndex == keys.size() ) 
		return 0;
	if ( fTimeEnd <= keys[pContext->nUpperKeyIndex].fTime ) 
	{
		const float fCoeff = ( fTimeEnd - keys[pContext->nUpperKeyIndex - 1].fTime ) / 
			                   ( keys[pContext->nUpperKeyIndex].fTime - keys[pContext->nUpperKeyIndex - 1].fTime );
		const float fValueEnd = pContext->fLowerKeyValue + fCoeff * ( pContext->fUpperKeyValue - pContext->fLowerKeyValue );
		const float fTotalValue = ::Integrate( pContext->fTime, pContext->fValue, fTimeEnd, fValueEnd );
		pContext->fTime = fTimeEnd;
		pContext->fValue = fValueEnd;
		return fTotalValue;
	}
	float fTotalValue = ::Integrate( pContext->fTime, pContext->fValue, keys[pContext->nUpperKeyIndex].fTime, pContext->fUpperKeyValue );
	CKeysList::const_iterator pos = std::upper_bound( keys.begin(), keys.end(), SKey(fTimeEnd, 0) );
	const int nUpperKeyIndex = std::distance( keys.begin(), pos );
	if ( nUpperKeyIndex > pContext->nUpperKeyIndex + 1 ) 
	{
		float fLowerKeyValue, fUpperKeyValue = pContext->fUpperKeyValue;
		float fLowerKeyTime, fUpperKeyTime = keys[pContext->nUpperKeyIndex].fTime;
		for ( CKeysList::const_iterator it = keys.begin() + pContext->nUpperKeyIndex + 1; it != pos; ++it )
		{
			fLowerKeyTime = fUpperKeyTime;
			fLowerKeyValue = fUpperKeyValue;

			fUpperKeyValue = it->fValue * ( 1.0f + GetRandomFromTrack(it->fTime, rnd) );
			fUpperKeyTime = it->fTime;
			fTotalValue += ::Integrate( fLowerKeyTime, fLowerKeyValue, fUpperKeyTime, fUpperKeyValue );
		}
		pContext->fLowerKeyValue = fLowerKeyValue;
		pContext->fUpperKeyValue = fUpperKeyValue;
		pContext->nUpperKeyIndex = nUpperKeyIndex - 1;
		pContext->fTime = fUpperKeyTime;
		pContext->fValue = fUpperKeyValue;
	}
	if ( pContext->nUpperKeyIndex + 1 < keys.size() ) 
	{
		const SKey &keyLast = keys[pContext->nUpperKeyIndex + 1];
		const float fCoeff = ( keyLast.fTime - fTimeEnd ) / ( keyLast.fTime - keys[pContext->nUpperKeyIndex].fTime );

		pContext->fLowerKeyValue = pContext->fUpperKeyValue;
		pContext->fUpperKeyValue = keyLast.fValue * ( 1.0f + GetRandomFromTrack(keyLast.fTime, rnd) );
		pContext->fValue = pContext->fLowerKeyValue + fCoeff * ( pContext->fUpperKeyValue - pContext->fLowerKeyValue );
		fTotalValue += ::Integrate( pContext->fTime, pContext->fLowerKeyValue, fTimeEnd, pContext->fValue );
		pContext->fTime = fTimeEnd;
		++pContext->nUpperKeyIndex;
	}
	return fTotalValue;
}
const float CTrack::Integrate( STrackContext *pContext, const float _fTimeEnd ) const
{
	const float fTimeEnd = Clamp( _fTimeEnd, 0.0f, 1.0f );
	NI_ASSERT_SLOW_TF( pContext->fTime <= fTimeEnd, "Can't integrate track - start time are greater then finish one", return 0 );
	if ( pContext->nUpperKeyIndex == keys.size() || pContext->nUpperKeyIndex < 0 )
		return 0;
	if ( fTimeEnd <= keys[pContext->nUpperKeyIndex].fTime ) 
	{
		const float fCoeff = ( fTimeEnd - keys[pContext->nUpperKeyIndex - 1].fTime ) / 
			                   ( keys[pContext->nUpperKeyIndex].fTime - keys[pContext->nUpperKeyIndex - 1].fTime );
		const float fValueEnd = pContext->fLowerKeyValue + fCoeff * ( pContext->fUpperKeyValue - pContext->fLowerKeyValue );
		const float fTotalValue = ::Integrate( pContext->fTime, pContext->fValue, fTimeEnd, fValueEnd );
		pContext->fTime = fTimeEnd;
		pContext->fValue = fValueEnd;
		return fTotalValue;
	}
	float fTotalValue = ::Integrate( pContext->fTime, pContext->fValue, keys[pContext->nUpperKeyIndex].fTime, pContext->fUpperKeyValue );
	CKeysList::const_iterator pos = std::upper_bound( keys.begin(), keys.end(), SKey(fTimeEnd, 0) );
	const int nUpperKeyIndex = std::distance( keys.begin(), pos );
	if ( nUpperKeyIndex > pContext->nUpperKeyIndex + 1 ) 
	{
		float fLowerKeyValue, fUpperKeyValue = pContext->fUpperKeyValue;
		float fLowerKeyTime, fUpperKeyTime = keys[pContext->nUpperKeyIndex].fTime;
		for ( CKeysList::const_iterator it = keys.begin() + pContext->nUpperKeyIndex + 1; it != pos; ++it )
		{
			fLowerKeyTime = fUpperKeyTime;
			fLowerKeyValue = fUpperKeyValue;

			fUpperKeyValue = it->fValue;
			fUpperKeyTime = it->fTime;
			fTotalValue += ::Integrate( fLowerKeyTime, fLowerKeyValue, fUpperKeyTime, fUpperKeyValue );
		}
		pContext->fLowerKeyValue = fLowerKeyValue;
		pContext->fUpperKeyValue = fUpperKeyValue;
		pContext->nUpperKeyIndex = nUpperKeyIndex - 1;
		pContext->fTime = fUpperKeyTime;
		pContext->fValue = fUpperKeyValue;
	}
	if ( pContext->nUpperKeyIndex + 1 < keys.size() ) 
	{
		const SKey &keyLast = keys[pContext->nUpperKeyIndex + 1];
		const float fCoeff = ( keyLast.fTime - fTimeEnd ) / ( keyLast.fTime - keys[pContext->nUpperKeyIndex].fTime );

		pContext->fLowerKeyValue = pContext->fUpperKeyValue;
		pContext->fUpperKeyValue = keyLast.fValue;
		pContext->fValue = pContext->fLowerKeyValue + fCoeff * ( pContext->fUpperKeyValue - pContext->fLowerKeyValue );
		fTotalValue += ::Integrate( pContext->fTime, pContext->fLowerKeyValue, fTimeEnd, pContext->fValue );
		pContext->fTime = fTimeEnd;
		++pContext->nUpperKeyIndex;
	}
	return fTotalValue;
}
void CTrack::CreateStartContext( STrackContext *pContext, const CTrack &rnd ) const
{	
	pContext->fLowerKeyValue = keys[0].fValue * ( 1.0f + GetRandomFromTrack(keys[0].fTime, rnd) );
	pContext->fUpperKeyValue = keys[1].fValue * ( 1.0f + GetRandomFromTrack(keys[1].fTime, rnd) );
	pContext->nUpperKeyIndex = 1;
	pContext->fTime = keys[0].fTime;
	pContext->fValue = pContext->fLowerKeyValue;
}
void CTrack::CreateStartContext( STrackContext *pContext ) const
{	
	pContext->fLowerKeyValue = keys[0].fValue;
	pContext->fUpperKeyValue = keys[1].fValue;
	pContext->nUpperKeyIndex = 1;
	pContext->fTime = keys[0].fTime;
	pContext->fValue = pContext->fLowerKeyValue;
}
void CTrack::Normalize( float _fScale )
{
	NI_ASSERT_SLOW_T( !keys.empty(), "Normalizing empty track!" );
	if ( keys.empty() )
		return;

	if ( _fScale != fScale )
	{
		const float fMultiplier = _fScale / fScale;
		for ( CKeysList::iterator it = keys.begin(); it != keys.end(); ++it )
			it->fTime *= fMultiplier;
		if ( keys[0].fTime > 0 )
		{
			AddKey( 0, keys[0].fValue );
		}
		if ( keys[keys.size() - 1].fTime < _fScale )
		{
			AddKey( _fScale, keys[keys.size() - 1].fValue );
		}
		fScale = _fScale;
	}
}
int CTrack::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 
	if ( saver.IsReading() )
	{
		fScale = 1000.0f;
		Clear();
	}
	saver.Add( "keys", &keys );
	if ( saver.IsReading() )
	{
		/*if ( fScale == 0 )
		{
			fScale = keys[keys.size() - 1].fTime;
		}*/
		Normalize( 1 );
	}
	return 0;
}
float CTrack::GetTimeByIndex( int index ) const
{
	return keys[index].fTime;
}
int CTrack::GetNumKeys() const
{
	return keys.size();
}


