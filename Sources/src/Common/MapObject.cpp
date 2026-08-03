#include "StdAfx.h"

#include "MapObject.h"
#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../Main/TextSystem.h"
void SMapObject::SetDiplomacy( const EDiplomacyInfo eDiplomacy )
{
	NI_ASSERT_T( (eDiplomacy == EDI_NEUTRAL) || (eDiplomacy == EDI_FRIEND) || (eDiplomacy == EDI_ENEMY), NStr::Format("Wrong diplomacy %d", int(eDiplomacy)) );
	diplomacy = BYTE( eDiplomacy );
}
int SMapObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add(  1, &pAIObj );
	saver.Add(  2, &pVisObj );
	saver.Add(  3, &pShadow );
	saver.Add(  4, &pDesc );
	saver.Add(  5, &pRPG );
	saver.Add(  6, &diplomacy );
	saver.Add(  7, &nSelectionGroupID );
	saver.Add(  8, &fHP );
	saver.Add( 10, &bCanSelect );
	return 0;
}
int SBridgeSpanObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pAIObj );
	saver.Add( 2, &pSlab );
	saver.Add( 3, &pBackGirder );
	saver.Add( 4, &pFrontGirder );
	saver.Add( 5, &nIndex );
	return 0;
}
IText* GetLocalName( const SGDBObjectDesc *pDesc )
{
	if ( pDesc == 0 ) 
		return 0;
	IText *pLocalName = GetSingleton<ITextManager>()->GetDialog( (pDesc->szPath + "\\name").c_str() );
	if ( pLocalName == 0 ) 
		pLocalName = GetSingleton<ITextManager>()->GetDialog( "textes\\strings\\widget-object-name"  );
	return pLocalName;
}
