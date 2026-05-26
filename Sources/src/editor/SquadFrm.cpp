#include "stdafx.h"
#include <io.h>
#include "..\Common\StingrayCompat.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"

#include "common.h"
#include "editor.h"
#include "PropView.h"
#include "DirectionButtonDock.h"
#include "TreeItem.h"
#include "SquadFrm.h"
#include "SquadView.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "frames.h"
#include "SetDirDialog.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int zeroSizeX = 32;
static const int zeroSizeY = 32;
static const float zeroShiftX = 15.4f;
static const float zeroShiftY = 15.4f;
static const int LINE_LENGTH = 200;

int GetIntAngle( float fAngle )
{
	return fAngle * 65535 / ( PI * 2 );
}

float SumAngle( float f1, float f2 )
{
	float fTemp = f1 + f2;
	if ( fTemp > 2*PI )
		fTemp = fTemp - 2*PI;
	else if ( fTemp < 0 )
		fTemp = 2*PI + fTemp;

	return fTemp;
}

/////////////////////////////////////////////////////////////////////////////
// CSquadFrame

IMPLEMENT_DYNCREATE(CSquadFrame, CGridFrame)

BEGIN_MESSAGE_MAP(CSquadFrame, CGridFrame)
	//{{AFX_MSG_MAP(CSquadFrame)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_SET_ZERO_BUTTON, OnSetZeroButton)
	ON_UPDATE_COMMAND_UI(ID_SET_ZERO_BUTTON, OnUpdateSetZeroButton)
	ON_COMMAND(ID_SHOW_DIRECTION_BUTTON, OnShowDirectionButton)
	ON_UPDATE_COMMAND_UI(ID_SHOW_DIRECTION_BUTTON, OnUpdateShowDirectionButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSquadFrame construction/destruction

CSquadFrame::CSquadFrame()
{
	szComposerName = "Squad Editor";
	szExtension = "*.scp";
	szComposerSaveName = "Squad_Composer_Project";
	nTreeRootItemID = E_SQUAD_ROOT_ITEM;
	nFrameType = CFrameManager::E_SQUAD_FRAME;
	pWndView = new CSquadView;
	szAddDir = "squads\\";
	
	pDirectionButtonDockBar = 0;
	m_mode = E_FREE;
	pActiveFormation = 0;
	pDraggingUnit = 0;
}

CSquadFrame::~CSquadFrame()
{
}

void CSquadFrame::Init( IGFX *_pGFX )
{
	CGridFrame::Init( _pGFX );
	
	CreateKrest();
	pFormationDirVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pFormationDirVertices );
		vertices[0].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( 1, 1, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pLineIndices = pGFX->CreateIndices( 2, GFXIF_INDEX16, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CIndicesLock<WORD> indices( pLineIndices );
		indices[0] = 0;
		indices[1] = 1;
	}
}

int CSquadFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CGridFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );
	
	// create a view to occupy the client area of the frame
	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CSquadFrame message handlers

void CSquadFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	if ( pDirectionButtonDockBar )
		theApp.ShowSECControlBar( pDirectionButtonDockBar, nCommand );
	
	if ( nCommand == SW_SHOW )
	{
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(16*fWorldCellSize, 8*fWorldCellSize, 0) );
		pCamera->Update();
		
		IGFX *pGFX = GetSingleton<IGFX>();
		pGFX->SetViewTransform( pCamera->GetPlacement() );
	}
}

void CSquadFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	
	CGridFrame::GFXDraw();
	
	if ( pActiveFormation )
	{
		pGFX->Draw( pFormationDirVertices, pLineIndices );
	}

	ICamera*	pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
  pSG->Draw( pCamera );

	pGFX->SetShadingEffect( 3 );
	if ( pActiveFormation )
	{
		SGFXRect2 rc;
		CVec2 zeroPos2;
		pGFX->SetTexture( 0, pKrestTexture );
		pSG->GetPos2( &zeroPos2, pActiveFormation->vZeroPos );
		rc.rect = CTRect<float> ( zeroPos2.x, zeroPos2.y, zeroPos2.x+zeroSizeX, zeroPos2.y+zeroSizeY );
		rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
		pGFX->SetupDirectTransform();
		pGFX->DrawRects( &rc, 1 );
		pGFX->RestoreTransform();
	}
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CSquadFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CSquadFrame::SpecificClearBeforeBatchMode()
{
	pActiveFormation = 0;
	pDraggingUnit = 0;
	GetSingleton<IScene>()->Clear();
}

struct SUnitsGDBStats
{
	std::string szKey;										// object key name
	std::string szPath;										// path to game resources for this object
};

std::string MakeName( const std::string &szName, const std::vector<SUnitsGDBStats> &descs )
{
	if ( szName.find('\\') != std::string::npos )
	{
		// добавим к имени объекта units\\humans
		std::string szNewName = "units\\humans\\" + szName;
		NStr::ToLower( szNewName );
		for ( std::vector<SUnitsGDBStats>::const_iterator it = descs.begin(); it != descs.end(); ++it )
		{
			if ( szNewName == it->szPath )
				return it->szKey;
		}
		NI_ASSERT_T( false, NStr::Format("Can't find stats for \"%s\"", szNewName.c_str()) );
		return "";
	}
	else
		return szName;			
}

void CSquadFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	SSquadRPGStats rpgStats;
	CSquadCommonPropsItem *pCommonPropsItem = static_cast<CSquadCommonPropsItem *> ( pRootItem->GetChildItem( E_SQUAD_COMMON_PROPS_ITEM ) );
	NI_ASSERT( pCommonPropsItem != 0 );
	rpgStats.szIcon = pCommonPropsItem->GetSquadPicture();
	rpgStats.type = ( SSquadRPGStats::ESquadType ) pCommonPropsItem->GetSquadType();

	IObjectsDB *pObjDB = GetSingleton<IObjectsDB>();
	int nNumDescs = pObjDB->GetNumDescs();
	const SGDBObjectDesc *pObjDescs = pObjDB->GetAllDescs();

	vector<SUnitsGDBStats> unitsStats;

	for ( int i=0; i<nNumDescs; i++ )
	{
		if ( pObjDescs[i].eVisType == SGVOT_SPRITE && pObjDescs[i].eGameType == SGVOGT_UNIT )
		{
			SUnitsGDBStats unit;
			unit.szPath = pObjDescs[i].szPath;
			unit.szKey = pObjDescs[i].szKey;

			unitsStats.push_back( unit );
		}
	}

	CSquadMembersItem *pMembersItem = static_cast<CSquadMembersItem *> ( pRootItem->GetChildItem( E_SQUAD_MEMBERS_ITEM ) );
	for ( CTreeItem::CTreeItemList::const_iterator it=pMembersItem->GetBegin(); it!=pMembersItem->GetEnd(); ++it )
	{
		const std::string szName = MakeName( (*it)->GetItemName(), unitsStats );
		rpgStats.memberNames.push_back( szName );			
	}

	IScene *pSG = GetSingleton<IScene>();
	//заполним формации
	CTreeItem *pFormations = pRootItem->GetChildItem( E_SQUAD_FORMATIONS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator ext=pFormations->GetBegin(); ext!=pFormations->GetEnd(); ++ext )
	{
		SSquadRPGStats::SFormation form;
		CSquadFormationPropsItem *pFormProps = static_cast<CSquadFormationPropsItem *> ( ext->GetPtr() );
		form.type = ( SSquadRPGStats::SFormation::EType ) pFormProps->GetFormationType();
		form.changesByEvent.resize( 1 );
		form.changesByEvent[0] = pFormProps->GetHitSwitchFormation();
		form.cLieFlag = pFormProps->GetLieState();
		form.fSpeedBonus = pFormProps->GetSpeedBonus();
		form.fDispersionBonus = pFormProps->GetDispersionBonus();
		form.fFireRateBonus = pFormProps->GetFireRateBonusBonus();
		form.fRelaxTimeBonus = pFormProps->GetRelaxTimeBonus();
		form.fCoverBonus = pFormProps->GetCoverBonus();

		//3D точная координата нуля формации
		CVec3 vRealZero3;
		CVec2 vRealZero2;
		pSG->GetPos2( &vRealZero2, pFormProps->vZeroPos );
		vRealZero2.x += zeroShiftX;
		vRealZero2.y += zeroShiftY;
		pSG->GetPos3( &vRealZero3, vRealZero2 );
		
		for ( CSquadFormationPropsItem::CUnitsList::iterator it=pFormProps->units.begin(); it!=pFormProps->units.end(); ++it )
		{
			SSquadRPGStats::SFormation::SEntry entry;
			entry.szSoldier = MakeName( it->pMemberProps->GetItemName(), unitsStats );
			entry.vPos.x = it->vPos.x - vRealZero3.x;
			entry.vPos.y = it->vPos.y - vRealZero3.y;
			entry.fDir = ToDegree( it->fDir );
			form.order.push_back( entry );
		}
		rpgStats.formations.push_back( form );
	}

	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CSquadFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	/*	
	SSquadRPGStats rpgStats;
	FillRPGStats( rpgStats, pRootItem );			//перед загрузкой инициализирую значениями по умолчанию
	
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
		GetRPGStats( rpgStats, pRootItem );
	*/
}

bool CSquadFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	//Сохраняем RPG stats
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	CSquadCommonPropsItem *pCommonPropsItem = static_cast<CSquadCommonPropsItem *>( pRootItem->GetChildItem( E_SQUAD_COMMON_PROPS_ITEM ) );
	std::string szTemp = pCommonPropsItem->GetSquadPicture();
	if ( szTemp.length() > 0 )
	{
		int nPos = szTemp.rfind( "\\" );
		std::string szSource;
		if ( nPos != std::string::npos )
		{
			szSource = szTemp;
			szTemp = szTemp.substr( nPos + 1 );
		}
		else
		{
			MakeFullPath( GetDirectory( pszProjectName ).c_str(), szTemp.c_str(), szSource );
		}
		szTemp = GetDirectory( pszResultFileName ) + szTemp;
		MyCopyFile( szSource.c_str(), szTemp.c_str() );
	}
	return true;
}

void CSquadFrame::SetActiveFormation( CSquadFormationPropsItem *pFormProps )
{
	if ( pActiveFormation == pFormProps )
		return;			//x3

	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();

	pDraggingUnit = 0;
	pActiveFormation = pFormProps;

	for ( CSquadFormationPropsItem::CUnitsList::iterator it=pFormProps->units.begin(); it!=pFormProps->units.end(); ++it )
	{
		std::string szItemName = it->pMemberProps->GetItemName();
		if ( szItemName.find( "\\" ) != std::string::npos )
		{
			string szName = "units\\humans\\";
			szName += szItemName;
			szName += "\\1";
			it->pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( szName.c_str(), 0, SGVOT_SPRITE ) );
		}
		else
		{
			const IObjectsDB *pDB = GetSingleton<IObjectsDB>();
			const SGDBObjectDesc *pDesc = pDB->GetDesc( szItemName.c_str() );
			if ( pDesc )
			{
				std::string szPath = pDesc->szPath;
				szPath += "\\1";
				it->pSprite = static_cast<IObjVisObj *>( pVOB->BuildObject( szPath.c_str(), 0, SGVOT_SPRITE ) );
			}
		}

		if ( it->pSprite == 0 )
			continue;
		
		it->pSprite->SetPosition( it->vPos );
		it->pSprite->SetDirection( GetIntAngle( SumAngle(it->fDir, pActiveFormation->fFormationDir) ) );
		it->pSprite->SetAnimation( 0 );
		pSG->AddObject( it->pSprite, SGVOGT_UNIT );
		//		pSprite->SetOpacity( 140 );
	}

	UpdateFormationDirection();
	pDirectionButtonDockBar->SetAngle( pActiveFormation->fFormationDir );
	GFXDraw();
}

void CSquadFrame::DeleteUnitFromScene( CTreeItem *pUnit, CSquadFormationPropsItem *pFormation )
{
	if ( pFormation != pActiveFormation )
		return;

	if ( !pActiveFormation )
		return;
	
	for ( CSquadFormationPropsItem::CUnitsList::iterator it=pActiveFormation->units.begin(); it!=pActiveFormation->units.end(); ++it )
	{
		if ( it->pMemberProps == pUnit )
		{
			if ( pDraggingUnit && pDraggingUnit->pMemberProps == pUnit )
				pDraggingUnit = 0;

			IScene *pSG = GetSingleton<IScene>();
			NI_ASSERT( it->pSprite != 0 );
			pSG->RemoveObject( it->pSprite );
			GFXDraw();
			return;
		}
	}
	
	NI_ASSERT( 0 );			//не нашла, что за фигня?
}

void CSquadFrame::SelectActiveUnit( CTreeItem *pUnit )
{
	if ( !pActiveFormation )
		return;
	
	for ( CSquadFormationPropsItem::CUnitsList::iterator it=pActiveFormation->units.begin(); it!=pActiveFormation->units.end(); ++it )
	{
		if ( it->pMemberProps == pUnit )
		{
			pDraggingUnit = &(*it);
			it->pSprite->Select( SGVOSS_SELECTED );
			pDirectionButtonDockBar->SetIntAngle( GetIntAngle( SumAngle(pDraggingUnit->fDir, pActiveFormation->fFormationDir) ) );
		}
		else
			it->pSprite->Select( SGVOSS_UNSELECTED );
	}
	GFXDraw();
}

BOOL CSquadFrame::SpecificTranslateMessage( MSG *pMsg )
{
	switch ( pMsg->message )
	{
		case WM_ANGLE_CHANGED:
			if ( !pActiveFormation )
				return true;
			
			if ( m_mode == E_SET_ZERO )
			{
				float fPrev = pActiveFormation->fFormationDir;
				pActiveFormation->fFormationDir = pDirectionButtonDockBar->GetAngle();
				CalculateNewPositions( pActiveFormation->fFormationDir - fPrev );
				UpdateFormationDirection();
				GFXDraw();
			}
			else if ( pDraggingUnit )
			{
				pDraggingUnit->fDir = SumAngle( pDirectionButtonDockBar->GetAngle(), -pActiveFormation->fFormationDir );
				pDraggingUnit->pSprite->SetDirection( GetIntAngle( pDirectionButtonDockBar->GetAngle() ) );
				SetChangedFlag( true );
				GFXDraw();
			}
			return true;
	}
	
	return false;
}

void CSquadFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	IScene *pSG = GetSingleton<IScene>();
	CVec2 pt;
	pt.x = point.x;
	pt.y = point.y;
	
	if ( m_mode == E_FREE && pActiveFormation != 0 )
	{
		//если мышка над одним из members в сцене, то надо передвинуть его на новое место
		for ( CSquadFormationPropsItem::CUnitsList::iterator it=pActiveFormation->units.begin(); it!=pActiveFormation->units.end(); ++it )
		{
			if ( IsSpriteHit( it->pSprite, pt, &objShift ) )
			{
				m_mode = E_DRAG;
				//			pDraggingUnit = &(*it);
				//			pDraggingUnit->pSprite->Select( SGVOSS_SELECTED );
				SelectActiveUnit( it->pMemberProps );
				g_frameManager.GetGameWnd()->SetCapture();
				break;
			}
		}
	}
	
	if ( m_mode == E_SET_ZERO && pActiveFormation != 0 )
	{
		SetZeroCoordinate( point );
	}

	CGridFrame::OnLButtonDown(nFlags, point);
}

void CSquadFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if ( m_mode == E_DRAG )
	{
		m_mode = E_FREE;
		//		pDraggingUnit->pSprite->Select( SGVOSS_UNSELECTED );
		//		pDraggingUnit = 0;
		ReleaseCapture();
		GFXDraw();
	}
	
	else if ( m_mode == E_SET_ZERO )
	{
		GFXDraw();
		SetChangedFlag( true );
	}

	CGridFrame::OnLButtonUp(nFlags, point);
}

void CSquadFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	IScene *pSG = GetSingleton<IScene>();
	CVec2 pt;
	pt.x = point.x;
	pt.y = point.y;

	if ( m_mode == E_DRAG && pActiveFormation != 0 )
	{
		NI_ASSERT( pDraggingUnit != 0 );
		IScene *pSG = GetSingleton<IScene>();
		CVec2 pos2;
		CVec3 pos3;
		pos2.x = point.x + objShift.x;
		pos2.y = point.y + objShift.y;
		pSG->GetPos3( &pos3, pos2 );
		pDraggingUnit->pSprite->SetPosition( pos3 );
		pSG->GetPos3( &pDraggingUnit->vPos, pos2 );
		GFXDraw();
	}
	
	if ( m_mode == E_SET_ZERO && pActiveFormation != 0 && nFlags & MK_LBUTTON )
	{
		SetZeroCoordinate( point );
	}
	
	CGridFrame::OnMouseMove(nFlags, point);
}

void CSquadFrame::OnSetZeroButton() 
{
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	if ( m_mode == E_SET_ZERO )
	{
		m_mode = E_FREE;
		pToolBar->SetButtonStyle( 4, TBBS_CHECKBOX );
	}
	else
	{
		m_mode = E_SET_ZERO;
		if ( pActiveFormation )
			pDirectionButtonDockBar->SetAngle( pActiveFormation->fFormationDir );
		pToolBar->SetButtonStyle( 4, TBBS_CHECKBOX | TBBS_CHECKED );
	}
	
	SetFocus();
	GFXDraw();
}

void CSquadFrame::OnUpdateSetZeroButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CSquadFrame::CreateKrest()
{
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	pKrestTexture = pTM->GetTexture( "editor\\krest\\1" );
}

void CSquadFrame::SetZeroCoordinate( POINT point )
{
	NI_ASSERT( pActiveFormation != 0 );
	IScene *pSG = GetSingleton<IScene>();
	CVec2 pt;
	pt.x = point.x;
	pt.y = point.y;
	
	pt.x -= zeroShiftX;
	pt.y -= zeroShiftX;
	CVec3 pt3;
	pSG->GetPos3( &pActiveFormation->vZeroPos, pt );
	UpdateFormationDirection();
	
	GFXDraw();
}

void CSquadFrame::UpdateFormationDirection()
{
	NI_ASSERT( pActiveFormation != 0 );
	IScene *pSG = GetSingleton<IScene>();
	CVec2 vPos2;
	pSG->GetPos2( &vPos2, pActiveFormation->vZeroPos );
	vPos2.x += zeroShiftX;
	vPos2.y += zeroShiftY;

	float fAlpha = pActiveFormation->fFormationDir;
	//	fAlpha += PI / 4;
	CVec3 vEnd3;
	vEnd3.x = pActiveFormation->vZeroPos.x - LINE_LENGTH * sin( fAlpha );
	vEnd3.y = pActiveFormation->vZeroPos.y + LINE_LENGTH * cos( fAlpha );
	vEnd3.z = 0;
	CVec2 vEnd2;
	pSG->GetPos2( &vEnd2, vEnd3 );
	vEnd2.x += zeroShiftX;
	vEnd2.y += zeroShiftY;
	
	//обновим линию направления
	{
		CVerticesLock<SGFXTLVertex> vertices( pFormationDirVertices );
		vertices[0].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( vEnd2.x, vEnd2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
}

void CSquadFrame::CalculateNewPositions( float fAlpha )
{
	NI_ASSERT( pActiveFormation != 0 );
	CVec3 vC = pActiveFormation->vZeroPos;
	CVec2 vC2;
	IScene *pSG = GetSingleton<IScene>();
	pSG->GetPos2( &vC2, vC );
	vC2.x += zeroShiftX;
	vC2.y += zeroShiftY;
	pSG->GetPos3( &vC, vC2 );

	for ( CSquadFormationPropsItem::CUnitsList::iterator it=pActiveFormation->units.begin(); it!=pActiveFormation->units.end(); ++it )
	{
		CVec3 vOld = it->pSprite->GetPosition();
		it->vPos.x = vC.x + (vOld.x - vC.x) * cos(fAlpha) - (vOld.y - vC.y) * sin(fAlpha);
		it->vPos.y = vC.y + (vOld.x - vC.x) * sin(fAlpha) + (vOld.y - vC.y) * cos(fAlpha);
		it->vPos.z = 0;

		it->pSprite->SetPosition( it->vPos );
		it->pSprite->SetDirection( GetIntAngle( SumAngle(pActiveFormation->fFormationDir, it->fDir) ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSquadFrame::OnShowDirectionButton()
{
	SwitchDockerVisible( pDirectionButtonDockBar );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSquadFrame::OnUpdateShowDirectionButton(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pDirectionButtonDockBar );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
