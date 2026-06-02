#include "StdAfx.h"

#include "fmtMesh.h"

#include "..\Anim\Animation.h"
int SAnimationFormat::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &nodes );
	saver.Add( 3, &nAction );
	saver.Add( 4, &nAABB_AIndex );
	saver.Add( 5, &nAABB_DIndex );
	if ( saver.IsReading() )
	{
		NStr::ToLower( szName );
		if ( szName.compare(0, 5, "death") == 0 )
			nType = ANIMATION_DEATH;
		else if ( szName.compare(0, 5, "shoot") == 0 )
			nType = ANIMATION_SHOOT;
		else if ( szName.compare(0, 4, "move") == 0 )
			nType = ANIMATION_MOVE;
		else if ( szName.compare(0, 14, "install_attack") == 0 )
			nType = ANIMATION_INSTALL;
		else if ( szName.compare(0, 16, "uninstall_attack") == 0 )
			nType = ANIMATION_UNINSTALL;
		else if ( szName.compare(0, 14, "install_rotate") == 0 )
			nType = ANIMATION_INSTALL_ROT;
		else if ( szName.compare(0, 16, "uninstall_rotate") == 0 )
			nType = ANIMATION_UNINSTALL_ROT;
		else if ( szName.compare(0, 14, "install_push") == 0 )
			nType = ANIMATION_INSTALL_PUSH;
		else if ( szName.compare(0, 16, "uninstall_push") == 0 )
			nType = ANIMATION_UNINSTALL_PUSH;
		else if ( szName.compare(0, 8, "fatality") == 0 ) 
			nType = ANIMATION_DEATH_FATALITY;
	}
	return 0;
}
int SMeshFormat::SVertexComponent::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &geom );
	saver.Add( 2, &norm );
	saver.Add( 3, &tex  );
	return 0;
}
int SMeshFormat::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &nIndex );
	
	saver.Add( 10, &geoms );
	saver.Add( 11, &norms );
	saver.Add( 12, &texes );
	
	saver.Add( 20, &components );
	saver.Add( 21, &indices );
	saver.Add( 30, &aabb );
	saver.Add( 31, &bsphere );
	return 0;
}
int SSkeletonFormat::SNodeFormat::SConstraint::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fMin );
	saver.Add( 2, &fMax );
	saver.Add( 3, &type );
	saver.Add( 4, &axis );
	return 0;
}
int SSkeletonFormat::SNodeFormat::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &nIndex );
	saver.Add( 10, &bone );
	saver.Add( 11, &quat );
	saver.Add( 12, &constraint );
	saver.Add( 20, &children );
	return 0;
}
int SSkeletonFormat::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nodes );
	saver.Add( 2, &nTopNode );
	saver.Add( 3, &locators );
	return 0;
}
