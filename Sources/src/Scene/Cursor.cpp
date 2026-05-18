#include "StdAfx.h"

#include "resource.h"

#include "Cursor.h"
#include "..\Common\Actions.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern HINSTANCE hDLLInstance;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCursorRegister
{
	const char *pszName;									// file name (for game cursor)
	int nMode;														// mode, this cursor used for
	WORD wResourceID;											// Windows resource ID for HW cursor
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const SCursorRegister modeTypes[] = 
{	
	{ "unknown"						, USER_ACTION_UNKNOWN											, IDC_UNKNOWN						},
	{ "move"							, USER_ACTION_MOVE												, IDC_MOVE							},
	{ "move to grid"			, USER_ACTION_MOVE_TO_GRID								, IDC_MOVE2GRID					},
	{ "attack"						, USER_ACTION_ATTACK											, IDC_ATTACK						},
	{ "swarm"							, USER_ACTION_SWARM												, IDC_SWARM							},
	{ "board"							, USER_ACTION_BOARD												, IDC_BOARD							},
	{ "leave"							, USER_ACTION_LEAVE												, IDC_LEAVE							},
	{ "rotate"						, USER_ACTION_ROTATE											, IDC_ROTATE						},
	{ "guard"							, USER_ACTION_GUARD												, IDC_SWARM							},
	{ "ranging"						, USER_ACTION_RANGING											, IDC_RANGING						},
	{ "suppress"					, USER_ACTION_SUPPRESS										, IDC_SUPPRESS					},
	{ "follow"						, USER_ACTION_FOLLOW											, IDC_FOLLOW						},
	{ "entrench self"			, USER_ACTION_ENTRENCH_SELF								, IDC_ENTRENCH_SELF			},
	{ "capture artillery"	, USER_ACTION_CAPTURE_ARTILLERY						, IDC_CAPTURE_ARTILLERY },
	{ "hook artillery"		, USER_ACTION_HOOK_ARTILLERY							, IDC_HOOK_ARTILLERY		},
	{ "deploy artillery"	, USER_ACTION_DEPLOY_ARTILLERY						, IDC_DEPLOY_ARTILLERY	},
	{ "place ap mines"		, USER_ACTION_ENGINEER_PLACE_MINE_AP			, IDC_SET_MINES					},
	{ "place at mines"		, USER_ACTION_ENGINEER_PLACE_MINE_AT			, IDC_SET_MINES					},
	{ "remove mines"			, USER_ACTION_ENGINEER_CLEAR_MINES				, IDC_CLEAR_MINES				},
	{ "build fence"				,	USER_ACTION_ENGINEER_BUILD_FENCE				, IDC_BUILD_WIRE_FENCE	},
	{ "build antitank"		, USER_ACTION_ENGINEER_BUILD_ANTITANK			, IDC_BUILD_ANTITANK		},
	{ "build entrenchment", USER_ACTION_ENGINEER_BUILD_ENTRENCHMENT ,	IDC_BUILD_ENTRENCHMENT},
	{ "build bridge"			, USER_ACTION_ENGINEER_BUILD_BRIDGE				, IDC_BUILD_BRIDGE			},
	{ "repair object"			, USER_ACTION_ENGINEER_REPAIR_BUILDING		, IDC_REPAIR						},
	{ "repair"						, USER_ACTION_ENGINEER_REPAIR							, IDC_REPAIR						},
	{ "resupply"					, USER_ACTION_SUPPORT_RESUPPLY						, IDC_RESUPPLY					},
	{ "humans resupply"		, USER_ACTION_HUMAN_RESUPPLY							, IDC_HUMAN_RESUPPLY		},
	{ "fill ru"						, USER_ACTION_FILL_RU											, IDC_FILL_RU						},
	{ "call bombers"			,	USER_ACTION_OFFICER_CALL_BOMBERS				, IDC_AVIATION					},
	{ "call fighters"			,	USER_ACTION_OFFICER_CALL_FIGHTERS				, IDC_AVIATION					},
	{ "call scout"				,	USER_ACTION_OFFICER_CALL_SPY						, IDC_AVIATION					},
	{ "call paradropers"	,	USER_ACTION_OFFICER_CALL_PARADROPERS		, IDC_AVIATION					},
	{ "call gunplane"			, USER_ACTION_OFFICER_CALL_GUNPLANES			, IDC_AVIATION					},
	{ "use spyglasses"		, USER_ACTION_OFFICER_BINOCULARS					, IDC_USE_SPYGLASSES		},
	{ "place marker"			, USER_ACTION_PLACE_MARKER								, IDC_PLACE_MARKER			},
	{ "cancel"						,	USER_ACTION_CANCEL											, IDC_CANCEL						},
	{ "select friend"			,	USER_ACTION_SELECT_FRIEND								, IDC_SELECT_FRIEND			},
	{ "select neutral"		,	USER_ACTION_SELECT_NEUTRAL							, IDC_SELECT_NEUTRAL		},
	{ "select foe"				,	USER_ACTION_SELECT_FOE									, IDC_SELECT_ENEMY			},
	{ "do selfaction"			,	USER_ACTION_DO_SELFACTION								, IDC_SELF_ACTION				},
	{ "hourglass"					, USER_ACTION_HOURGLASS										, 0											},
	{ 0										, 0																		}
};
inline const SCursorRegister* FindMode( const std::string &szMode )
{
	const SCursorRegister *pMode = modeTypes;
	while ( pMode->pszName != 0 )
	{
		if ( szMode == pMode->pszName )
			return pMode;
		++pMode;
	}
	NI_ASSERT_T( false, NStr::Format("Can't recognize action \"%s\" for cursor", szMode.c_str()) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCursorModeInfo
{
	int nMode;
	std::string szTexture;
	CVec2 vHotSpot;
	CVec2 vSize;
	int wResourceID;
	//
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Texture", &szTexture );
		saver.Add( "HotSpot", &vHotSpot );
		saver.Add( "Size", &vSize );
		if ( saver.IsReading() )
		{
			std::string szMode;
			saver.Add( "Mode", &szMode );
			NStr::ToLower( szMode );
			if ( const SCursorRegister *pMode = FindMode(szMode) )
			{
				nMode = pMode->nMode;
				wResourceID = pMode->wResourceID;
			}
			else
			{
				nMode = USER_ACTION_HOURGLASS;
				wResourceID = 0;
			}
		}
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCursor::CCursor() 
{
	eUpdateMode = ICursor::UPDATE_MODE_INPUT;
	pMode = 0;
	pModifier = 0;
	vPos.Set( 0, 0 );
	bShow = true;
	bPosLocked = false;
	fSensitivity = 1;
	vLastPos = VNULL2;
	timeLast = 0;
	nCurrModifier = -1;
	nCurrMode = -1;
	bAcquired = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Init( ISingleton *pSingleton )
{
	IInput *pInput = GetSingleton<IInput>( pSingleton );
	pScrollX = pInput->CreateSlider( "cursor_x", 1000 );
	pScrollY = pInput->CreateSlider( "cursor_y", 1000 );
	pTM = GetSingleton<ITextureManager>( pSingleton );
	//
	std::vector<SCursorModeInfo> shapes;
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "cursor\\1.xml", STREAM_ACCESS_READ );
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.Add( "CursorShapes", &shapes );
	}
	for ( std::vector<SCursorModeInfo>::const_iterator it = shapes.begin(); it != shapes.end(); ++it )
		RegisterMode( it->nMode, it->szTexture.c_str(), it->vSize.x, it->vSize.y, it->vHotSpot.x, it->vHotSpot.y, it->wResourceID );
}
void CCursor::Done()
{
	DestroyContents();
	Acquire( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Clear()
{
	for ( CCursorsModeMap::iterator it = modes.begin(); it != modes.end(); ++it )
		it->second.pTexture = 0;
	SetMode( nCurrMode );
	SetModifier( nCurrModifier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Show( bool _bShow ) 
{ 
	bShow = _bShow; 
	if ( eUpdateMode == ICursor::UPDATE_MODE_WINDOWS ) 
		OnSetCursor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::SetBounds( int x1, int y1, int x2, int y2 ) 
{ 
	rcBounds.Set( x1, y1, x2, y2 ); 
	AcquireLocal();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::AcquireLocal()
{
	if ( eUpdateMode != ICursor::UPDATE_MODE_WINDOWS ) 
	{
		::ClipCursor( 0 );
		return;
	}
	//
	if ( bAcquired ) 
	{
		if ( !rcBounds.IsEmpty() ) 
		{
			const RECT rcClip = rcBounds;
			::ClipCursor( &rcClip );
		}
	}
	else
		::ClipCursor( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Acquire( bool bAcquire )
{
	bAcquired = bAcquire;
	AcquireLocal();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::SetPos( int nX, int nY ) 
{ 
	vPos = CVec2( nX, nY ); 
	if ( eUpdateMode == ICursor::UPDATE_MODE_WINDOWS ) 
		SetCursorPos( nX, nY );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::LockPos( bool bLock ) 
{ 
	bPosLocked = bLock;
	if ( eUpdateMode == ICursor::UPDATE_MODE_WINDOWS ) 
	{
		if ( bLock ) 
		{
			if ( bAcquired ) 
			{
				const RECT rcClip = { static_cast<LONG>(vPos.x), static_cast<LONG>(vPos.y), static_cast<LONG>(vPos.x), static_cast<LONG>(vPos.y) };
				::ClipCursor( &rcClip );
			}
		}
		else
			AcquireLocal();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::SetUpdateMode( const EUpdateMode _eUpdateMode ) 
{ 
	if ( (_eUpdateMode != eUpdateMode) && (_eUpdateMode == ICursor::UPDATE_MODE_WINDOWS) ) 
		SetCursorPos( vPos.x, vPos.y );
	eUpdateMode = _eUpdateMode; 
	AcquireLocal();
	OnSetCursor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::OnSetCursor()
{
	if ( eUpdateMode == ICursor::UPDATE_MODE_WINDOWS ) 
	{
		if ( pMode && bShow ) 
		{
			if ( pMode->wResourceID ) 
			{
				HCURSOR hCursor = ::LoadCursor( hDLLInstance, MAKEINTRESOURCE(pMode->wResourceID) );
				::SetCursor( hCursor );
			}
			else
				::SetCursor( ::LoadCursor(0, IDC_WAIT) );
		}
		else
			::SetCursor( 0 );
	}
	else
		::SetCursor( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Update()
{
	if ( bPosLocked )
		ResetSliders();
	const NTimer::STime timeAbs = GetSingleton<IGameTimer>()->GetAbsTime();

	if ( eUpdateMode == ICursor::UPDATE_MODE_INPUT ) 
	{
		vPos.x = Clamp( vPos.x + fSensitivity*pScrollX->GetDelta(), rcBounds.minx, rcBounds.maxx );
		vPos.y = Clamp( vPos.y + fSensitivity*pScrollY->GetDelta(), rcBounds.miny, rcBounds.maxy );
	}
	else
	{
		if ( bPosLocked ) 
			SetCursorPos( vPos.x, vPos.y );
		else
		{
			POINT point;
			GetCursorPos( &point );
			vPos.x = Clamp( float(point.x), rcBounds.minx, rcBounds.maxx );
			vPos.y = Clamp( float(point.y), rcBounds.miny, rcBounds.maxy );
			if ( (vPos.x != point.x) || (vPos.y != point.y) ) 
				SetCursorPos( vPos.x, vPos.y );
		}
	}
	//
	if ( fabs2(vPos - vLastPos) > 1 ) 
	{
		vLastPos = vPos;
		timeLast = timeAbs;
	}
	//
	if ( pMode && pMode->pVisObj ) 
		pMode->pVisObj->Update( timeAbs );
	if ( pModifier && pModifier->pVisObj ) 
		pModifier->pVisObj->Update( timeAbs );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::RegisterMode( int nMode, const char *pszPictureName, int nSizeX, int nSizeY, int hotX, int hotY, WORD wResourceID )
{
	SCursorMode mode;
	mode.pTexture = 0;
	mode.vHotSpot.Set( hotX, hotY );
	mode.szTextureName = std::string("cursor\\") + pszPictureName;
	mode.rect.Set( 0, 0, nSizeX, nSizeY );
	mode.wResourceID = wResourceID;

	modes.insert( CCursorsModeMap::value_type(nMode, mode) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCursor::LoadCursor( int nMode )
{
	CCursorsModeMap::iterator pos = modes.find( nMode );
	if ( pos == modes.end() )
		return false;
	if ( pos->second.pTexture != 0 )
		return true;
	pos->second.pVisObj = (ISpriteVisObj*)GetSingleton<IVisObjBuilder>()->BuildObject( pos->second.szTextureName.c_str(), 0, SGVOT_SPRITE );
	pos->second.pTexture = pTM->GetTexture( pos->second.szTextureName.c_str() );
	if ( pos->second.pVisObj ) 
		pos->second.pVisObj->SetAnimation( 0 );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SCursorMode* CCursor::GetCursor( int nMode )
{
	CCursorsModeMap::iterator pos = modes.find( nMode );
	if ( pos == modes.end() )
	{
		if ( LoadCursor( nMode ) == false )
			return 0;
		pos = modes.find( nMode );
	}
	else if ( pos->second.pTexture == 0 )
	{
		if ( LoadCursor( nMode ) == false )
			return 0;
	}
	//
	return &( pos->second );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCursor::SetMode( int nMode )
{
	if ( SCursorMode *pCursor = GetCursor(nMode) )
	{
		pMode = pCursor;
		nCurrMode = nMode;
		OnSetCursor();
		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCursor::SetModifier( int nMode )
{
	if ( nMode == -1 ) 
	{
		pModifier = 0;
		nCurrModifier = -1;
		return true;
	}
	else if ( SCursorMode *pCursor = GetCursor(nMode) )
	{
		pModifier = pCursor;
		nCurrModifier = nMode;
		return true;
	}
	else
	{
		pModifier = 0;
		nCurrModifier = -1;
		return false;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DrawCursor( SCursorMode *pMode, const CVec2 &vPos, IGFX *pGFX )
{
	if ( pMode == 0 ) 
		return false;
	//
	if ( pMode->pVisObj ) 
	{
		// animated
		const SSpriteInfo *pInfo = pMode->pVisObj->GetSpriteInfo();
		SGFXRect2 rect;
		CVec2 point = vPos - pMode->vHotSpot;
		rect.rect.Set( point.x + pInfo->rect.x1, point.y + pInfo->rect.y1, point.x + pInfo->rect.x2, point.y + pInfo->rect.y2 );
		rect.maps = pInfo->maps;
		rect.fZ = 0;
		//
		pGFX->SetTexture( 0, pMode->pVisObj->GetTexture() );
		pGFX->SetShadingEffect( 3 );
		return pGFX->DrawRects( &rect, 1 );
	}
	else
	{
		// static
		SGFXRect2 rect;
		CVec2 point = vPos - pMode->vHotSpot;
		rect.rect.Set( point.x, point.y, point.x + pMode->rect.Width(), point.y + pMode->rect.Height() );
		rect.maps.Set( 0, 0, 1, 1 );
		rect.fZ = 0;
		//
		pGFX->SetTexture( 0, pMode->pTexture );
		pGFX->SetShadingEffect( 3 );
		return pGFX->DrawRects( &rect, 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCursor::Draw( interface IGFX *pGFX )
{
	if ( !bShow )
		return false;
	//
	Update();
	//
	if ( eUpdateMode == ICursor::UPDATE_MODE_INPUT ) 
	{
		const bool bRetVal = DrawCursor( pMode, vPos, pGFX );
		DrawCursor( pModifier, vPos, pGFX );
		return bRetVal;
	}
	else
		return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SCursorMode::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pTexture );
	saver.Add( 2, &rect );
	saver.Add( 3, &vHotSpot );
	saver.Add( 4, &szTextureName );
	saver.Add( 5, &wResourceID );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CCursor::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &modes );
	saver.Add( 2, &nCurrMode );
	saver.Add( 3, &vPos );
	saver.Add( 4, &bShow );
	saver.Add( 5, &bPosLocked );
	saver.Add( 6, &rcBounds );
	saver.Add( 7, &fSensitivity );
	saver.Add( 8, &vLastPos );
	saver.Add( 9, &timeLast );
	saver.Add( 10, &nCurrModifier );
	saver.Add( 11, &bAcquired );
	// read/write current cursor mode
	if ( saver.IsReading() )
	{
		SetMode( nCurrMode );
		SetModifier( nCurrModifier );
		// for HW cursor
		AcquireLocal();
		SetPos( vPos.x, vPos.y );
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
