#include "StdAfx.h"

#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"

#include "editor.h"
#include "TreeDockWnd.h"
#include "SpriteCompose.h"
#include "MainFrm.h"
#include "PropView.h"
#include "TreeItem.h"
#include "BuildTreeItem.h"
#include "BuildFrm.h"
#include "BuildView.h"
#include "GameWnd.h"
#include "frames.h"

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;

static const int LINE_LENGTH = 100;			//длина линии, используемой для задания конуса стрельбы
static const int EDGE_LENGTH = 200;			//длина ребра конуса
static const int SHOOT_PICTURE_SIZE = 8;


void CBuildingFrame::SetActiveFirePoint( SFirePoint *pFirePoint )
{
	if ( pActiveFirePoint )
	{
		pActiveFirePoint->pSprite->SetOpacity( MIN_OPACITY );
		if ( pActiveFirePoint->pHLine )
			pActiveFirePoint->pHLine->SetOpacity( 0 );
	}
	pActiveFirePoint = pFirePoint;
	if ( !pFirePoint )
		return;
	
	pActiveFirePoint->pSprite->SetOpacity( MAX_OPACITY );
	if ( pActiveFirePoint->pHLine )
		pActiveFirePoint->pHLine->SetOpacity( MAX_OPACITY );
	
	if ( eActiveSubMode == E_SUB_MOVE || eActiveSubMode == E_SUB_HOR )
		ComputeMaxAndMinPositions( pActiveFirePoint->pSprite->GetPosition() );
	else if ( eActiveSubMode == E_SUB_DIR )
		ComputeFireDirectionLines();
}

void CBuildingFrame::SetFireDirection( float fVal )
{
	NI_ASSERT( pActiveFirePoint != 0 );
	
	pActiveFirePoint->fDirection = fVal;
	ComputeFireDirectionLines();
}

void CBuildingFrame::DeleteFirePoint( CTreeItem *pFire )
{
	for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
	{
		if ( it->pFirePoint == pFire )
		{
			IScene *pSG = GetSingleton<IScene>();
			pSG->RemoveObject( it->pSprite );
			if ( it->pHLine )
				pSG->RemoveObject( it->pHLine );
			firePoints.erase( it );
			pActiveFirePoint = 0;
			SetChangedFlag( true );
			GFXDraw();
			break;
		}
	}
}

void CBuildingFrame::SelectFirePoint( CTreeItem *pFire )
{
	for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
	{
		if ( it->pFirePoint == pFire )
		{
			SetActiveFirePoint( &(*it) );
			GFXDraw();
			break;
		}
	}
}

void CBuildingFrame::ComputeFireDirectionLines()
{
	NI_ASSERT( pActiveFirePoint != 0 );
	if ( !pActiveFirePoint->pHLine )
		return;

	IScene *pSG = GetSingleton<IScene>();
	CVec3 vCenter3 = pActiveFirePoint->pHLine->GetPosition();		//положение центра линии
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	
	float fA;
	fA = ToRadian( pActiveFirePoint->fDirection );

	CVec3 vPos3;
	vPos3.x = vCenter3.x - (float) EDGE_LENGTH * sin( fA );
	vPos3.y = vCenter3.y + (float) EDGE_LENGTH * cos( fA );
	vPos3.z = 0;
	CVec2 vPos2;
	pSG->GetPos2( &vPos2, vPos3 );

	CVec3 vLine1, vLine2;			//линии, отображающие красную стрелочку
	vLine1.z = vLine2.z = 0;
	float fTemp = ToRadian( 5.0f );
	vLine1.x = vCenter3.x - (float) (EDGE_LENGTH - 20) * sin( fA - fTemp );
	vLine1.y = vCenter3.y + (float) (EDGE_LENGTH - 20) * cos( fA - fTemp );
	vLine2.x = vCenter3.x - (float) (EDGE_LENGTH - 20) * sin( fA + fTemp );
	vLine2.y = vCenter3.y + (float) (EDGE_LENGTH - 20) * cos( fA + fTemp );
	
	{
		CVerticesLock<SGFXTLVertex> vertices( pFireDirectionVertices );
		
		CVec2 v;
		
		DWORD dwColor = 0xffffff00;
		vertices[0].Setup( vCenter2.x, vCenter2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( vPos2.x, vPos2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine1 );
		vertices[2].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[3].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine2 );
		vertices[4].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[5].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pActiveFirePoint->pFirePoint->SetDirection( pActiveFirePoint->fDirection );
	pOIDockBar->SetItemProperty( pActiveFirePoint->pFirePoint->GetItemName(), pActiveFirePoint->pFirePoint );
	
	GFXDraw();
}

void CBuildingFrame::AddOrSelectFirePoint( const POINT &point )
{
	CVec2 firePos2;
	firePos2.x = point.x;
	firePos2.y = point.y;
	CVec3 firePos3;
	IScene *pSG = GetSingleton<IScene>();
	pSG->GetPos3( &firePos3, firePos2 );

	objShift = VNULL2;
	zeroShift = VNULL2;

	CListOfFirePoints::iterator it=firePoints.begin();
	for ( ; it!=firePoints.end(); ++it )
	{
		CVec3 vPos3 = it->pSprite->GetPosition();
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, vPos3 );

		if ( point.x >= vPos2.x - SHOOT_PICTURE_SIZE && point.x <= vPos2.x + SHOOT_PICTURE_SIZE &&
			point.y >= vPos2.y - SHOOT_PICTURE_SIZE && point.y <= vPos2.y + SHOOT_PICTURE_SIZE )
		{
			SetActiveFirePoint( &(*it) );
			it->pFirePoint->SelectMeInTheTree();

			SetChangedFlag( true );
			objShift.x = vPos2.x - point.x;
			objShift.y = vPos2.y - point.y;

			vPos3 = it->pHLine->GetPosition();
			pSG->GetPos2( &vPos2, vPos3 );
			zeroShift.x = vPos2.x - point.x;
			zeroShift.y = vPos2.y - point.y;

			m_mode = E_SET_FIRE_POINT;
			g_frameManager.GetGameWnd()->SetCapture();
			return;
		}
	}
	
	if ( !ComputeMaxAndMinPositions( firePos3 ) )
	{
		return;
	}
	
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	CPtr<IObjVisObj> pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pObject != 0 );
	if ( !pObject )
	{
		return;
	}
	
	pObject->SetPosition( firePos3 );
	pObject->SetDirection( 0 );
	pObject->SetAnimation( 0 );
	pSG->AddObject( pObject, SGVOGT_OBJECT );
	pObject->SetOpacity( MAX_OPACITY );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	CTreeItem *pFiresItem = pRoot->GetChildItem( E_BUILDING_FIRE_POINTS_ITEM );
	CBuildingFirePointPropsItem *pNewPoint = new CBuildingFirePointPropsItem;
	pNewPoint->SetItemName( "Fire point" );
	pFiresItem->AddChild( pNewPoint );
	pNewPoint->SelectMeInTheTree();
	
	SFirePoint fire;
	fire.pFirePoint = pNewPoint;
	fire.pSprite = pObject;
	
	if ( pActiveFirePoint )
	{
		fire.fDirection = pActiveFirePoint->fDirection;
		fire.pFirePoint->SetDirection( fire.fDirection );
		fire.pFirePoint->SetEffectName( pActiveFirePoint->pFirePoint->GetEffectName() );
	}
	else
	{
		fire.fDirection = 0;
		fire.pFirePoint->SetDirection( fire.fDirection );
	}
	
	CVec3 vHPos3 = firePos3;
	if ( pActiveFirePoint )
	{
		CVec2 vHPos2, vSprite2;
		pSG->GetPos2( &vHPos2, pActiveFirePoint->pHLine->GetPosition() );
		pSG->GetPos2( &vSprite2, pActiveFirePoint->pSprite->GetPosition() );
		
		CVec2 vNewSpritePos2;
		pSG->GetPos2( &vNewSpritePos2, firePos3 );
		vNewSpritePos2.y += vHPos2.y - vSprite2.y;
		if ( vNewSpritePos2.y < m_fMinY )
			vNewSpritePos2.y = m_fMinY;
		if ( vNewSpritePos2.y > m_fMaxY )
			vNewSpritePos2.y = m_fMaxY;
		pSG->GetPos3( &vHPos3, vNewSpritePos2 );
	}
	else
	{
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, firePos3 );
		vPos2.y = m_fMaxY;
		pSG->GetPos3( &vHPos3, vPos2 );
	}

	pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pObject != 0 );
	
	pObject->SetDirection( 0 );
	pObject->SetAnimation( 0 );
	pObject->SetPosition( vHPos3 );
	pSG->AddObject( pObject, SGVOGT_OBJECT );
	pObject->Update( GetSingleton<IGameTimer>()->GetGameTime() );
	pObject->SetOpacity( MAX_OPACITY );
	fire.pHLine = pObject;

	firePoints.push_back( fire );
	SetActiveFirePoint( &(firePoints.back()) );
	pOIDockBar->SetItemProperty( pActiveFirePoint->pFirePoint->GetItemName(), pActiveFirePoint->pFirePoint );
	SetChangedFlag( true );
	GFXDraw();
}

void CBuildingFrame::SetHorFirePoint( const POINT &point )
{
	if ( pActiveFirePoint == 0 )
		return;
	
	const int DELTA_X = 10;
	if ( point.x >= m_fX - DELTA_X && point.x <= m_fX + DELTA_X && point.y >= m_fMinY && point.y <= m_fMaxY )
	{
		CVec2 vPos2;
		vPos2.x = m_fX;
		vPos2.y = point.y;
		CVec3 vPos3;
		IScene *pSG = GetSingleton<IScene>();
		pSG->GetPos3( &vPos3, vPos2 );
		
		NI_ASSERT( pActiveFirePoint->pHLine != 0 );
		if ( pActiveFirePoint->pHLine )
		{
			pActiveFirePoint->pHLine->SetPosition( vPos3 );
			pSG->MoveObject( pActiveFirePoint->pHLine, vPos3 );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pActiveFirePoint->pHLine->Update( pTimer->GetGameTime() );
		}
		SetChangedFlag( true );
		GFXDraw();
	}
}

void CBuildingFrame::SetFirePointAngle( const POINT &point )
{
	if ( pActiveFirePoint == 0 || pActiveFirePoint->pHLine == 0 )
		return;
	IScene *pSG = GetSingleton<IScene>();
	
	CVec3 vCenter3 = pActiveFirePoint->pHLine->GetPosition();		//положение центра конуса
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	float temp = (vCenter2.x - point.x)*(vCenter2.x - point.x) + (vCenter2.y - point.y)*(vCenter2.y - point.y);
	if ( sqrt( temp ) < 5 )
		return;				//если очень маленькие расстояния, то будет сильно скакать, избегаем скачков
	
	CVec2 vPos2;
	vPos2.x = point.x;
	vPos2.y = point.y;
	CVec3 vPos3;
	pSG->GetPos3( &vPos3, vPos2 );
	
	CVec3 vCone;
	vCone.x = vPos3.x - vCenter3.x;
	vCone.y = vPos3.y - vCenter3.y;
	float fA = -atan2( vCone.x, vCone.y );
	fA = ToDegree( fA );
	if ( fA < 0 )
		fA = 360 + fA;
	pActiveFirePoint->fDirection = fA;
	
	SetChangedFlag( true );
	SetCapture();
	ComputeFireDirectionLines();
}

void CBuildingFrame::MoveFirePoint( const POINT &point )
{
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	
	CVec2 pos2;
	CVec3 pos3;
	pos2.x = point.x + objShift.x;
	pos2.y = point.y + objShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveFirePoint->pSprite, pos3 );
	pActiveFirePoint->pSprite->Update( pTimer->GetGameTime() );
	
	pos2.x = point.x + zeroShift.x;
	pos2.y = point.y + zeroShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveFirePoint->pHLine, pos3 );
	pActiveFirePoint->pHLine->Update( pTimer->GetGameTime() );
	GFXDraw();
}
