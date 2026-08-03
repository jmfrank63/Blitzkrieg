
#include "StdAfx.h"
#include "editor.h"
#include "IUndoRedoCmd.h"
#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"

#include <Mmsystem.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


void CTileRedoCmd::Undo()
{
	m_ptr->AddTileCmd( m_data, false );
}
void CDellObjRedoCmd::Undo()
{
	CVec3 v( position.x, position.y, 0 );
	SMapObjectInfo info;
	info.szName = desc.szKey;
	info.vPos = v;
	Vis2AI( &info.vPos );
	info.nDir = m_dir ;
	info.nPlayer = player;
	info.nScriptID = scriptId;
	info.fHP = 1.0f;
	info.nFrameIndex = frameIndex;
	m_ptr->AddObjectByAI( info, player, true );
}
void CAddObjRedoCmd::Undo()
{
	m_ptr->RemoveObject( m_obj );
}
void CAddMultiObjRedoCmd::Undo()
{
	for( std::vector<SMapObject*>::iterator it = m_objs.begin(); it != m_objs.end(); ++it )
	{
		m_ptr->RemoveObject( *it );
	}
}
void CMoveObjRedoCmd::Undo()
{
	CVec3 vAI;
	Vis2AI( &vAI, CVec3( m_oldPos.x, m_oldPos.y, 0 ) );	
	if( m_ptr->ifObjectExist(m_obj) )
	{
		m_ptr->MoveObject( m_obj->pAIObj, vAI.x, vAI.y );
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		pTimer->Update( timeGetTime() );
		m_ptr->Update( pTimer->GetGameTime() );
	}
}
void CPutRoadRedoCmd::Undo()
{
}
void CDellRoadRedoCmd::Undo()
{
}
void CDellMultiObjRedoCmd::Undo()
{
	for ( std::vector<SObjectDellDisciption>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		CVec3 v( it->m_position );
		SMapObjectInfo info;
		info.szName = it->m_desc.szKey;
		info.vPos = v;
		Vis2AI( &info.vPos );
		info.nDir = it->m_dir ;
		info.nPlayer = it->m_player;
		info.nScriptID = it->m_scriptID;
		info.fHP = 1.0f;
		info.nFrameIndex = it->m_frameIndex;
		SMapObject *pObj = m_ptr->AddObjectByAI( info, it->m_player, true, it->m_bScenarioUnit );	
	}
}