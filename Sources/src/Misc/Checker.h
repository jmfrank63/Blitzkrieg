#ifndef __CHECKER_H__
#define __CHECKER_H__
#if defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
inline bool CheckFixedRange( const int nIndex, const int nSize, const char *pszName )
{
	NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < nSize, NStr::Format("Index (%d) must be in the range [0..%d) for \"%s\"", nIndex, nSize, pszName) );
	return nIndex >= 0 && nIndex < nSize;
}
template <class TContainer>
inline bool CheckRange( const TContainer &container, const int nIndex )
{
	NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < container.size(), NStr::Format("Index (%d) must be in the range [0..%d)", nIndex, container.size()) );
	return nIndex >= 0 && nIndex < container.size();
}
inline bool CheckRPGStats( const std::string &szRPGStats )
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( szRPGStats.c_str() );
	NI_ASSERT_SLOW_TF( pDesc != 0, NStr::Format("Can't find object \"%s\" descriptor", szRPGStats.c_str()), return false );
	const IGDBObject *pStats = pGDB->GetRPGStats( pDesc );
	NI_ASSERT_SLOW_TF( pStats != 0, NStr::Format("Can't find object \"%s\" RPG stats", szRPGStats.c_str()), return false );
	return pStats != 0;
}
#else
#define CheckRange( x, y ) true
#define CheckFixedRange( x, y, z ) true
#define CheckRPGStats( x ) true
#endif // defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
#endif // __CHECKER_H__