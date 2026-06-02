#ifndef __SAVE_DBID_H__
#define __SAVE_DBID_H__
#pragma ONCE
inline void SaveDBID( CSaverAccessor *pSaver, const int nChunk, const int dbID )
{
	if ( dbID >= 0 )
	{
		CGDBPtr<SGDBObjectDesc> pDesc = GetSingleton<IObjectsDB>()->GetDesc( dbID );
		NI_ASSERT_SLOW_TF( pDesc != 0, NStr::Format( "Can't find DB description with index %d", dbID ), return );
		
		pSaver->Add( nChunk, &pDesc );
	}
}
inline void LoadDBID( CSaverAccessor *pSaver, const int nChunk, int *pDBID )
{
	CGDBPtr<SGDBObjectDesc> pDesc;
	pSaver->Add( nChunk, &pDesc );

	*pDBID = GetSingleton<IObjectsDB>()->GetIndex( pDesc );
}
#endif // __SAVE_DBID_H__
