#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "MOObject.h"

#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../GameTT/iMission.h"
#include "../Formats/fmtTerrain.h"
#include "PlayEffect.h"
#include "../Main/ScenarioTracker.h"
#include "../UI/UI.h"
#include "../Main/CameraTurn.h"
#include "../Main/TurnTrace.h"
#include "TurnPreload.h"
#include "../AILogic/aiconsts.h"
int CMOObject::nLastMarkerID = 0;
bool CMOObject::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int nSeason, int nFrameIndex, 
											  float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	bDead = false;
	pDesc = pDescLocal;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	if ( pDesc->eGameType == SGVOGT_FLAG )
	{
		szFlagSide = pDesc->szKey.substr( 5 );
		nLastMarkerID++;
		nMarkerID = nLastMarkerID;
	}
	else
		szFlagSide = "";
	switch ( pDesc->eGameType ) 
	{
		case SGVOGT_OBJECT:
			{
				const char *pszName = nSeason == 1 ? "\\1w" : "\\1";
				pVisObj = pVOB->BuildObject( (pDesc->szPath + pszName).c_str(), 0, pDesc->eVisType );
				if ( pVisObj )									// try to create normal object (take season into account)
					pShadow = pVOB->BuildObject( (pDesc->szPath + pszName + "s").c_str(), 0, SGVOT_SPRITE );
				else														// try to create summer (default) object, if correct one failed
				{
					const char *pszName = "\\1";
					pVisObj = pVOB->BuildObject( (pDesc->szPath + pszName).c_str(), 0, pDesc->eVisType );
					if ( pVisObj )
						pShadow = pVOB->BuildObject( (pDesc->szPath + pszName + "s").c_str(), 0, SGVOT_SPRITE );
				}
			}
			break;
		case SGVOGT_EFFECT:
			pVisObj = pVOB->BuildObject( pDesc->szPath.c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_PROJECTILE:
			pVisObj = pVOB->BuildObject( pDesc->szPath.c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_ENTRENCHMENT:
			{
				const SEntrenchmentRPGStats::SSegmentRPGStats &segment = static_cast<const SEntrenchmentRPGStats*>( pRPG.GetPtr() )->GetSegmentStats( nFrameIndex );
				const char *pszName = nSeason == 1 ? "\\1w" : "\\1";
				pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel).c_str(), (pDesc->szPath + pszName).c_str(), pDesc->eVisType );
			}
			break;
		case SGVOGT_TANK_PIT:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), (pDesc->szPath + "\\1" + GetSeasonApp2(nSeason)).c_str(), pDesc->eVisType );
			break;
		case SGVOGT_MINE:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + static_cast_gdb<const SMineRPGStats*>(pRPG)->szFlagModel).c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_BRIDGE:
			{
				const SBridgeRPGStats::SSegmentRPGStats &segment = static_cast_gdb<const SBridgeRPGStats*>(pRPG)->GetSegmentStats( nFrameIndex );
				if ( segment.szModel.empty() || segment.nFrameIndex == -1 ) 
					return false;
				pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel).c_str(), 0, SGVOT_SPRITE );
				if ( pVisObj == 0 )				// bridge have no this part... skip it
					return false;
				if ( fNewHP < 0 )					// special case - "blueprint" of the bridge
					pVisObj->SetOpacity( 0x80 );
				pShadow = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel + "s").c_str(), 0, SGVOT_SPRITE );
				nFrameIndex = segment.nFrameIndex;
			}
			break;
		case SGVOGT_FENCE:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), 0, pDesc->eVisType );
			if ( pVisObj ) 
				pShadow = pVOB->BuildObject( (pDesc->szPath + "\\1s").c_str(), 0, SGVOT_SPRITE );
			break;
		default:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), 0, pDesc->eVisType );
	}
	NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create object \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	if ( pDesc->eVisType == SGVOT_SPRITE )
	{
		ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast_ptr<IObjVisObj*>(pVisObj)->GetAnimation() );
		pAnim->SetFrameIndex( nFrameIndex );
		if ( pShadow )
		{
			pAnim = static_cast<ISpriteAnimation*>( static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation() );
			pAnim->SetFrameIndex( nFrameIndex );
		}
	}	
	pAIObj = pAIObjLocal;
	UpdateModelWithHP( fNewHP / pRPG->fMaxHP, pVOB );
	fHP = fNewHP / pRPG->fMaxHP;
	FillActions();
	this->nSeason = nSeason;
	nCameraTurn = NCameraTurn::Step();
	nOwnFrame = pDesc->eGameType == SGVOGT_FENCE ? nFrameIndex : -1;
	ApplyCameraTurn();
	return true;
}
// The frame of a fence segment at camera step nStep: the segment at the same
// place among the same kind (centre, left, right or centre damaged) of the
// direction DrawnTurn( its direction, nStep ). The four directions, North-east,
// North-west, South-west and South-east, are the fence's quarter turns
// (x, y) -> (-y, x), as a building's sides are, so its direction is its own
// facing.
static int TurnedFenceFrame( const SFenceRPGStats *pStats, const int nFrame, const int nStep )
{
	const int nDirs = pStats->dirs.size();
	for ( int d = 0; d < nDirs; ++d )
	{
		const SFenceRPGStats::SDir &from = pStats->dirs[d];
		const SFenceRPGStats::SDir &to = pStats->dirs[NCameraTurn::DrawnTurn( d, nStep ) % nDirs];
		const std::vector<int> *froms[4] = { &from.centers, &from.ldamages, &from.rdamages, &from.cdamages };
		const std::vector<int> *tos[4] = { &to.centers, &to.ldamages, &to.rdamages, &to.cdamages };
		for ( int k = 0; k < 4; ++k )
		{
			for ( int j = 0; j < froms[k]->size(); ++j )
			{
				if ( pStats->stats[(*froms[k])[j]].nIndex != nFrame )
					continue;
				return tos[k]->empty() ? nFrame : pStats->stats[(*tos[k])[j % tos[k]->size()]].nIndex;
			}
		}
	}
	return nFrame;
}
// The middle of the footprint of a fence's segment (sprite index nFrame) from
// its zero cross, AI units: the footprint starts at the zero cross less its
// origin, as the AI lays it (StaticObject.cpp).
static CVec2 FenceFootprintMiddle( const SFenceRPGStats *pStats, const int nFrame )
{
	for ( int i = 0; i < pStats->stats.size(); ++i )
	{
		const SFenceRPGStats::SSegmentRPGStats &segment = pStats->stats[i];
		if ( segment.nIndex == nFrame )
			return CVec2( segment.passability.GetSizeX() * SAIConsts::TILE_SIZE / 2.0f - segment.vOrigin.x,
										segment.passability.GetSizeY() * SAIConsts::TILE_SIZE / 2.0f - segment.vOrigin.y );
	}
	return VNULL2;
}
// Whether bk-views drew the prop's turns (`<folder>_r<k>`), and whether a
// stream is in the data: asked once a mission (NTurnPreload).
static bool HasTurns( const std::string &szPath, const int nTurn )
{
	NTurnTrace::CScope scope( "probe", true );
	return NCameraTurn::ModelPath( szPath, nTurn, NTurnPreload::Exists ) != szPath;
}
static bool StreamExists( const std::string &szName )
{
	NTurnTrace::CScope scope( "probe", true );
	return NTurnPreload::Exists( szName );
}
// Moves its picture to stand vShift (world units) from its place, the shift
// before taken off; an object not in the scene yet is only placed. Its shadow
// stays at its place (ApplyCameraTurn).
void CMOObject::ShiftPicture( const CVec3 &vShift )
{
	if ( vShift == vTurnShift )
		return;
	const CVec3 vPos = pVisObj->GetPosition() - vTurnShift + vShift;
	vTurnShift = vShift;
	IScene *pScene = GetSingleton<IScene>();
	if ( !pScene->MoveObject( pVisObj, vPos ) )
		pVisObj->SetPosition( vPos );
}
// Draws it for the camera step (Remastered, Main/CameraTurn.h), each kind by
// the shortest path (docs/superpowers/specs/2026-10-26-camera-turn-design.md,
// S03), with no new pictures:
// - a terra object lies flat on the ground: its picture turns as the ground
//   does, and so does its shadow;
// - an object (flora, a prop) on a footprint a quarter turn keeps (square or
//   none) is its own picture, as M005 turns thin and small buildings; on a
//   longer rectangle its picture is mirrored at an odd step, which lays it on
//   the turned footprint (its shadow kept, as M005's mirrored sprites keep it);
// - a prop whose turns bk-views drew (`<folder>_r<k>`) shows its shape turned,
//   not mirrored (M006 round 3);
// - a fence segment shows the segment of its direction turned by the step.
// The sun stays in the world (M006 round 2): the shadow an object casts is the
// one it casts at step 0 (its own picture's, its own segment's), at its place,
// laid on the ground turned with it by the step, as a terra object is; an
// object (flora, a prop) shows its picture lit by that sun where the data
// holds one (ShowPicture).
// At step 0 each is drawn as in 2003.
void CMOObject::ApplyCameraTurn()
{
	ISpriteVisObj *pSprite = dynamic_cast<ISpriteVisObj*>( pVisObj.GetPtr() );
	if ( pSprite == 0 || pRPG == 0 )
		return;
	switch ( pDesc->eGameType )
	{
		case SGVOGT_TERRAOBJ:
			pSprite->SetScreenTurn( nCameraTurn, false );
			if ( ISpriteVisObj *pShadowSprite = dynamic_cast<ISpriteVisObj*>( pShadow.GetPtr() ) )
				pShadowSprite->SetScreenTurn( nCameraTurn, false );
			break;
		case SGVOGT_OBJECT:
			if ( ISpriteVisObj *pShadowSprite = dynamic_cast<ISpriteVisObj*>( pShadow.GetPtr() ) )
				pShadowSprite->SetScreenTurn( nCameraTurn, false );
			if ( NCameraTurn::IsFourSided( pDesc->szPath ) )
			{
				// A prop the 2003 data shows from four sides as variants `01` to
				// `04`, each the one before turned by (x, y) -> (-y, x) (bench,
				// France fence, the wooden columns), draws its variant of
				// turn DrawnTurn( its own facing, step ), as a building does.
				const int nOwnTurn = NCameraTurn::FacingCounts() ? nFacing : 0;
				const int nTurn = NCameraTurn::DrawnTurn( nOwnTurn, nCameraTurn );
				if ( nTurn == nShownTurn && nCameraTurn == nShownLight )
					break;
				nShownTurn = nTurn;
				ShowPicture( NCameraTurn::FourSidedPath( pDesc->szPath, nTurn ) );
				IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
				if ( pShadow )
				{
					const std::string szShadowSide = NCameraTurn::FourSidedPath( pDesc->szPath, nOwnTurn );
					if ( !pVOB->ChangeObject( pShadow, ( szShadowSide + ( nSeason == 1 ? "\\1ws" : "\\1s" ) ).c_str(), 0, SGVOT_SPRITE ) )
						pVOB->ChangeObject( pShadow, ( szShadowSide + "\\1s" ).c_str(), 0, SGVOT_SPRITE );
				}
				const NTimer::STime currTime = GetSingleton<IGameTimer>()->GetGameTime();
				pVisObj->Update( currTime, true );
				if ( pShadow )
					pShadow->Update( currTime, true );
			}
			else if ( nCameraTurn != 0 && HasTurns( pDesc->szPath, nCameraTurn ) )
			{
				// A prop whose turns bk-views drew (`<folder>_r<k>`, the training
				// camp's sand pile): its shape turned, never its picture mirrored
				// (M006 round 3), lit by the world's sun where `_r<k>_l<k>` is there.
				pSprite->SetScreenTurn( 0, false );
				if ( nCameraTurn == nShownTurn && nCameraTurn == nShownLight )
					break;
				nShownTurn = nCameraTurn;
				ShowPicture( NCameraTurn::LitModelPath( pDesc->szPath, nCameraTurn, nCameraTurn, NTurnPreload::Exists ) );
				pVisObj->Update( GetSingleton<IGameTimer>()->GetGameTime(), true );
			}
			else
			{
				const CArray2D<BYTE> &passability = static_cast_gdb<const SStaticObjectRPGStats*>( pRPG )->GetPassability();
				const bool bRectangle = passability.GetSizeX() != passability.GetSizeY() && passability.GetSizeX() * passability.GetSizeY() != 0;
				pSprite->SetScreenTurn( 0, bRectangle && ( nCameraTurn & 1 ) != 0 );
				if ( nCameraTurn != nShownLight )
				{
					ShowPicture( pDesc->szPath );
					pVisObj->Update( GetSingleton<IGameTimer>()->GetGameTime(), true );
				}
			}
			break;
		case SGVOGT_FENCE:
			{
				ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pSprite )->GetAnimation() );
				if ( nOwnFrame < 0 )
					nOwnFrame = pAnim->GetFrameIndex();
				const SFenceRPGStats *pStats = static_cast_gdb<const SFenceRPGStats*>( pRPG );
				const int nFrame = TurnedFenceFrame( pStats, nOwnFrame, nCameraTurn );
				// The segment drawn stands on its own footprint seen turned: a
				// 2003 segment's zero cross lies at one end of its footprint (the
				// first cell), so the picture of the turned direction is moved by
				// what puts its footprint's middle on the own one's.
				CVec3 vShift( VNULL3 );
				if ( nCameraTurn != 0 )
				{
					const CVec2 vOwn = FenceFootprintMiddle( pStats, nOwnFrame );
					CVec2 vDrawn = FenceFootprintMiddle( pStats, nFrame );
					for ( int i = 0; i < nCameraTurn; ++i )
						vDrawn = CVec2( vDrawn.y, -vDrawn.x );					// turned back, (x, y) -> (y, -x)
					AI2Vis( &vShift, vOwn.x - vDrawn.x, vOwn.y - vDrawn.y, 0 );
				}
				ShiftPicture( vShift );
				const NTimer::STime currTime = GetSingleton<IGameTimer>()->GetGameTime();
				if ( ISpriteVisObj *pShadowSprite = dynamic_cast<ISpriteVisObj*>( pShadow.GetPtr() ) )
				{
					pShadowSprite->SetScreenTurn( nCameraTurn, false );
					ISpriteAnimation *pShadowAnim = static_cast<ISpriteAnimation*>( static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation() );
					if ( pShadowAnim->GetFrameIndex() != nOwnFrame )
					{
						pShadowAnim->SetFrameIndex( nOwnFrame );
						pShadow->Update( currTime, true );
					}
				}
				if ( nFrame == pAnim->GetFrameIndex() )
					break;
				pAnim->SetFrameIndex( nFrame );
				pVisObj->Update( currTime, true );
			}
			break;
	}
}
// Shows the picture of szFolder (a prop's or its side's folder), its season's
// where the data holds it, lit by the world's sun as the camera step shows it:
// `<pack>_l<step>` (bk-views relight, M006 round 2) where the data holds it,
// else the 2003 picture.
void CMOObject::ShowPicture( const std::string &szFolder )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	const std::string szLit = NStr::Format( "_l%d", nCameraTurn );
	const std::string szPacks[2] = { szFolder + ( nSeason == 1 ? "\\1w" : "\\1" ), szFolder + "\\1" };
	for ( int i = 0; i < 2; ++i )
	{
		if ( nCameraTurn != 0 && StreamExists( szPacks[i] + szLit + ".san" ) &&
				 pVOB->ChangeObject( pVisObj, ( szPacks[i] + szLit ).c_str(), 0, SGVOT_SPRITE ) )
		{
			NTurnPreload::NoteDrawn( szPacks[i] + szLit );
			break;
		}
		if ( pVOB->ChangeObject( pVisObj, szPacks[i].c_str(), 0, SGVOT_SPRITE ) )
		{
			NTurnPreload::NoteDrawn( szPacks[i] );
			break;
		}
	}
	nShownLight = nCameraTurn;
}
// The packs ShowPicture takes for szFolder at camera step nStep.
static void PreloadPicture( const std::string &szFolder, const int nSeason, const int nStep )
{
	const std::string szPacks[2] = { szFolder + ( nSeason == 1 ? "\\1w" : "\\1" ), szFolder + "\\1" };
	for ( int i = 0; i < 2; ++i )
	{
		if ( nStep != 0 && StreamExists( szPacks[i] + NStr::Format( "_l%d", nStep ) + ".san" ) &&
				 NTurnPreload::Warm( szPacks[i] + NStr::Format( "_l%d", nStep ) ) )
			break;
		if ( NTurnPreload::Warm( szPacks[i] ) )
			break;
	}
}
// Brings the picture a flora or prop shows at camera step nStep into the
// shared caches (NTurnPreload), as ApplyCameraTurn picks it. Terra objects
// and fences show their own packs at every step, and step 0 is the picture
// it was made with.
void CMOObject::PreloadCameraTurn( const int nStep )
{
	if ( pDesc->eGameType != SGVOGT_OBJECT || pRPG == 0 || nStep == 0 )
		return;
	if ( NCameraTurn::IsFourSided( pDesc->szPath ) )
		PreloadPicture( NCameraTurn::FourSidedPath( pDesc->szPath, NCameraTurn::DrawnTurn( NCameraTurn::FacingCounts() ? nFacing : 0, nStep ) ), nSeason, nStep );
	else if ( HasTurns( pDesc->szPath, nStep ) )
		PreloadPicture( NCameraTurn::LitModelPath( pDesc->szPath, nStep, nStep, NTurnPreload::Exists ), nSeason, nStep );
	else
		PreloadPicture( pDesc->szPath, nSeason, nStep );
}
void CMOObject::SetCameraTurn( const int nTurn, IVisObjBuilder *pVOB )
{
	nCameraTurn = nTurn;
	ApplyCameraTurn();
}
void CMOObject::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitSprite( pVisObj, pDesc->eGameType, pDesc->eVisType );
	if ( pShadow ) 
		pVisitor->VisitSprite( pShadow, SGVOGT_SHADOW, SGVOT_SPRITE );
}
int CMOObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &bDead );
	saver.Add( 3, &szFlagSide );
	if ( !szFlagSide.empty() )
		saver.Add( 4, &nMarkerID );
	// A fence saved at a turned step shows a turned segment; at the 2003 view
	// nothing new is written.
	if ( saver.IsReading() || nCameraTurn != 0 || nFacing != 0 )
	{
		saver.Add( 5, &nCameraTurn );
		saver.Add( 6, &nOwnFrame );
		saver.Add( 7, &nSeason );
		saver.Add( 8, &nShownTurn );
		saver.Add( 9, &nFacing );
		saver.Add( 10, &vTurnShift );
		saver.Add( 11, &nShownLight );
	}
	if ( saver.IsReading() ) 
		FillActions();
	return 0;
}
bool CMOObject::AddAction( int nAction )
{
	if ( nAction > 63 ) 
		return false;
	const int nIndex = nAction >> 5;
	actions[nIndex] |= 1UL << ( nAction - nIndex*32 );
	return true;
}
void CMOObject::FillActions()
{
	actions[0] = actions[1] = 0;
}
// A prop's wDir is its map Direction, which the AI passes on (the 2003 AI
// passed 0): a prop of four sides draws the side of its facing by it
// (ApplyCameraTurn); every prop's sprites stay at direction 0, as 2003 had
// them.
void CMOObject::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	const WORD wSpriteDir = pDesc->eGameType == SGVOGT_OBJECT ? 0 : wDir;
	pVisObj->SetPlacement( vPos + vTurnShift, wSpriteDir );
	if ( pShadow )
		pShadow->SetPlacement( vPos, wSpriteDir );
	if ( pDesc->eGameType == SGVOGT_OBJECT && NCameraTurn::Facing( wDir ) != nFacing )
	{
		nFacing = NCameraTurn::Facing( wDir );
		ApplyCameraTurn();
	}
	if ( !szFlagSide.empty() )
	{
		IUIScreen *pUIScreen = GetSingleton<IScene>()->GetMissionScreen();
		if ( pUIScreen )
		{
			if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) ) )
			{
				pUIMiniMap->AddMarker( szFlagSide + "Flag", CVec2(vPos.x, vPos.y), true, nMarkerID, GetSingleton<IGameTimer>()->GetAbsTime(), 0, false );
			}
		}
	}
}
void CMOObject::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition() - vTurnShift;
	*pwDir = pVisObj->GetDirection();
}
void CMOObject::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pStatus->nScenarioIndex = -1;
	pStatus->dwIconsStatus = 0;
	pStatus->dwPlayer = -1;
	pStatus->params[0] = PackParams( MINT(fHP * pRPG->fMaxHP), MINT(pRPG->fMaxHP) );
	pStatus->params[1] = 0;
	pStatus->params[2] = 0;
	pStatus->params[3] = 0;
	if ( IText *pName = GetLocalName() ) 
		NPlatform::CopyWordStringToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), pName->GetString() );
	else
	{
		static std::wstring szName;
		NStr::ToUnicode( &szName, pDesc->szKey );
		NPlatform::CopyWideToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), szName.c_str() );
	}
	Zero( pStatus->weaponstats );
	Zero( pStatus->armors );
}
void CMOObject::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	if ( const CUserActions *pUserActions = pRPG->GetUserActions(eActions == IMapObj::ACTIONS_BY) ) 
		*pActions = *pUserActions;
	else if ( eActions == IMapObj::ACTIONS_WITH ) 
	{
		pActions->SetAction( USER_ACTION_ATTACK );
		pActions->SetAction( USER_ACTION_MOVE );
		pActions->SetAction( USER_ACTION_UNKNOWN );
		if ( pDesc->eGameType == SGVOGT_BRIDGE ) 
		{
			if ( fHP < 0 ) 
				pActions->SetAction( USER_ACTION_ENGINEER_BUILD_BRIDGE );
			else if ( fHP < 1 ) 
				pActions->SetAction( USER_ACTION_ENGINEER_REPAIR_BUILDING );
		}
	}
}
// A prop's placement.dir is its map Direction, as for SetPlacement: the Map
// Editor's turn of a prop of four sides redraws its side here.
void CMOObject::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	if ( pDesc->eGameType == SGVOGT_OBJECT )
	{
		pVisObj->SetDirection( 0 );
		if ( NCameraTurn::Facing( placement.dir ) != nFacing )
		{
			nFacing = NCameraTurn::Facing( placement.dir );
			ApplyCameraTurn();
		}
	}
	else
		pVisObj->SetDirection( placement.dir );
	pScene->MoveObject( pVisObj, vPos + vTurnShift );
	pVisObj->Update( currTime, true );
	if ( pShadow )
	{
		pScene->MoveObject( pShadow, vPos );
		pShadow->Update( currTime, true );
	}
}
inline int GetObjectDamageState( float fHP )
{
	if ( fHP > 0.5f )
		return 0;
	else if ( fHP > 0.0f )
		return 1;
	else
		return 2;
}
void CMOObject::UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB )
{
	if ( (fHP == -1) && (fNewHP == 1) )		// bridge segment was built
		pVisObj->SetOpacity( 0xff );
	const int nOldState = GetObjectDamageState( fHP );
	const int nNewState = GetObjectDamageState( fNewHP );
	if ( nNewState != nOldState )
	{
		if ( (pDesc->eVisType == SGVOT_SPRITE) && (pDesc->eGameType == SGVOGT_BUILDING) )
		{
			pVOB->ChangeObject( pVisObj, NStr::Format( "%s\\%d", pDesc->szPath.c_str(), nNewState + 1 ), 0, SGVOT_SPRITE );
			if ( pShadow )
				pVOB->ChangeObject( pShadow, NStr::Format( "%s\\%ds", pDesc->szPath.c_str(), nNewState + 1 ), 0, SGVOT_SPRITE );
		}
	}
}
bool CMOObject::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene )
{
	const float fNewHP = stats.fHitPoints / pRPG->fMaxHP;
	UpdateModelWithHP( fNewHP, pVOB );
	if ( fHP != fNewHP ) 
	{
		ISceneIconBar *pBar = static_cast<ISceneIconBar*>( static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon( ICON_HP_BAR ) );
		if ( pBar )
		{
			pBar->SetLength( fHP );
			if ( fHP >= 0.5f )
			{
				const DWORD r = DWORD( ( 1.0f - fHP ) * 510.0f );
				pBar->SetColor( 0xff00ff00 | (r << 16) );
			}
			else
			{
				const DWORD g = DWORD( fHP * 510.0f );
				pBar->SetColor( 0xffff0000 | (g << 8) );
			}
		}
	}
	fHP = fNewHP;
	return fNewHP > 0;
}
void CMOObject::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( hit.wShell >= hit.pWeapon->shells.size() )
		return;
	const SWeaponRPGStats::SShell &shell = hit.pWeapon->shells[hit.wShell];
	const CVec3 &vPos = pVisObj->GetPosition();
	PlayEffect( *GetHitEffect(hit, shell), vPos, currTime, false, pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
}
int CMOObject::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	switch ( action.typeID ) 
	{
		case ACTION_NOTIFY_SILENT_DEATH:
			if ( (pDesc->eGameType == SGVOGT_OBJECT) || (pDesc->eGameType == SGVOGT_FENCE) )
			{
				if ( pRPG && pVisObj ) 
				{
					const SStaticObjectRPGStats *pRPGStats = static_cast_gdb<const SStaticObjectRPGStats*>( pRPG );
					const NTimer::STime timeEffect = Min( action.time, currTime );
					const NTimer::STime timePassed = currTime - timeEffect;
					PlayEffect( pRPGStats->szEffectDeath, pVisObj->GetPosition(), timeEffect, false, pScene, pVOB, timePassed );
				}
			}
			bDead = true;
			break;
		case ACTION_NOTIFY_DIE:
			if ( !bDead && ((pDesc->eGameType == SGVOGT_OBJECT) || (pDesc->eGameType == SGVOGT_FENCE)) )
			{
				if ( pRPG && pVisObj ) 
				{
					const SStaticObjectRPGStats *pRPGStats = static_cast_gdb<const SStaticObjectRPGStats*>( pRPG );
					const NTimer::STime timeEffect = Min( action.time, currTime );
					const NTimer::STime timePassed = currTime - timeEffect;
					PlayEffect( pRPGStats->szEffectExplosion, pVisObj->GetPosition(), timeEffect, false, pScene, pVOB, timePassed );
				}
			}
			break;
		case ACTION_NOTIFY_CHANGE_FRAME_INDEX:
			{
				// A fence's new segment (it was damaged) as the camera step shows it.
				if ( pDesc->eGameType == SGVOGT_FENCE )
				{
					nOwnFrame = action.nParam;
					ApplyCameraTurn();
					break;
				}
				const int nFrame = action.nParam;
				static_cast<ISpriteAnimation*>(static_cast_ptr<IObjVisObj*>(pVisObj)->GetAnimation())->SetFrameIndex( nFrame );
				pVisObj->Update( currTime, true );
				if ( pShadow ) 
				{
					static_cast<ISpriteAnimation*>(static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation())->SetFrameIndex( nFrame );
					pShadow->Update( currTime, true );
				}
			}
			break;
		case ACTION_NOTIFY_SIDE_CHANGED:
			for ( CPtr<IPlayerScenarioInfoIterator> pIt = GetSingleton<IScenarioTracker>()->CreatePlayerScenarioInfoIterator();
				    !pIt->IsEnd(); pIt->Next() )
			{
				IPlayerScenarioInfo *pPlayer = pIt->Get();
				if ( pPlayer->GetDiplomacySide() == action.nParam ) 
				{
					IPlayerScenarioInfo *pUserPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
					std::string szFlagName = pPlayer->GetGeneralSide();
					NStr::ToLower( szFlagName );
					if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pScene->GetMissionScreen()->GetChildByID( 20000 ) ) )
					{
						WORD wCircleColor;
						std::string szSoundName;
						if ( action.nParam == pUserPlayer->GetDiplomacySide() )
						{
							wCircleColor = 0xF0F0;
							szSoundName = "Int_completed";
						}
						else if ( action.nParam == 2 )
						{
							wCircleColor = 0xF00F;
							szSoundName = IsFriend() ? "Int_failed" : "Int_information";
						}
						else
						{
							wCircleColor = 0xFF00;
							szSoundName = IsNeutral() ? "Int_failed" : "Int_flag_captured";
						}
						CVec3 vPos;
						WORD wDir;
						GetPlacement( &vPos, &wDir );
						pUIMiniMap->RemoveMarker( nMarkerID );
						pUIMiniMap->AddMarker( szFlagName + "Flag", CVec2( vPos.x, vPos.y ), true, nMarkerID, GetSingleton<IGameTimer>()->GetAbsTime(), 0, false );
						pUIMiniMap->AddCircle( CVec2( vPos.x, vPos.y ), fWorldCellSize * 16.0f, MMC_STYLE_MIXED, wCircleColor, GetSingleton<IGameTimer>()->GetAbsTime(), 5000, false, 0 );
						pUIMiniMap->AddCircle( CVec2( vPos.x, vPos.y ), fWorldCellSize * 8.0f, MMC_STYLE_MIXED, wCircleColor, GetSingleton<IGameTimer>()->GetAbsTime(), 5000, false, 0 );
						pScene->AddSound( szSoundName.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
					}
					if ( action.nParam == 2 )
						SetDiplomacy( EDI_NEUTRAL );
					else if ( action.nParam == pUserPlayer->GetDiplomacySide() )
						SetDiplomacy( EDI_FRIEND );
					else
						SetDiplomacy( EDI_ENEMY );
					szFlagName = "Flag_" + szFlagName;
					if ( const SGDBObjectDesc *pNewDesc = GetSingleton<IObjectsDB>()->GetDesc(szFlagName.c_str()) )
					{
						const std::string szFlagFileName = pNewDesc->szPath + "\\1";
						if ( pVOB->ChangeObject(pVisObj, szFlagFileName.c_str(), 0, SGVOT_SPRITE) )
						{
							pDesc = pNewDesc;
							pRPG = NGDB::GetRPGStats<SObjectBaseRPGStats>( pDesc );
							pVisObj->Update( currTime, true );
							if ( pShadow )
							{
								pVOB->ChangeObject( pShadow, (szFlagFileName + "s").c_str(), 0, SGVOT_SPRITE );
								pShadow->Update( currTime, true );
							}
						}
					}
					break;
				}
			}
			break;
	}
	if ( (pDesc->eGameType == SGVOGT_FENCE) && ((action.typeID == ACTION_NOTIFY_SILENT_DEATH) || (action.typeID == ACTION_NOTIFY_DIE)) ) 
	{
		pScene->RemoveObject( pVisObj );
		pScene->AddObject( pVisObj, SGVOGT_TERRAOBJ, pDesc );
	}

	return 0;
}
