#include "StdAfx.h"

#include "resource.h"
#include "StreamIO.h"
#include "StructureSaver.h"
#include "Cursor.h"
#include "DTHelper.h"
#include "Input.h"
#include "Actions.h"
#include "../Platform/System.h"

struct SCursorRegister
{
	const char *pszName;									// file name (for game cursor)
	int nMode;														// mode, this cursor used for
	WORD wResourceID;											// Windows resource ID for HW cursor
};
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
struct SCursorModeInfo
{
	int nMode;
	std::string szTexture;
	CVec2 vHotSpot;
	CVec2 vSize;
	int wResourceID;
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
	nSystemMode = -2;
	nSystemModifier = -2;
	nSystemScale = -1;
	bSystemCursor = false;
}
void CCursor::Init( ISingleton *pSingleton )
{
	IInput *pInput = GetSingleton<IInput>( pSingleton );
	pScrollX = pInput->CreateSlider( "cursor_x", 1000 );
	pScrollY = pInput->CreateSlider( "cursor_y", 1000 );
	pTM = GetSingleton<ITextureManager>( pSingleton );
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
	NPlatform::ClearSystemCursorImage();
	bSystemCursor = false;
}
void CCursor::Clear()
{
	for ( CCursorsModeMap::iterator it = modes.begin(); it != modes.end(); ++it )
		it->second.pTexture = 0;
	SetMode( nCurrMode );
	SetModifier( nCurrModifier );
}
void CCursor::Show( bool _bShow ) 
{ 
	bShow = _bShow; 
	ApplySystemCursor();
}
void CCursor::SetBounds( int x1, int y1, int x2, int y2 ) 
{ 
	rcBounds.Set( x1, y1, x2, y2 ); 
	AcquireLocal();
}
void CCursor::AcquireLocal()
{
	// Cursor capture is represented by bAcquired and enforced by the
	// software-coordinate clamp below. Scene does not own a native window.
}
void CCursor::Acquire( bool bAcquire )
{
	bAcquired = bAcquire;
	AcquireLocal();
}
void CCursor::SetPos( int nX, int nY ) 
{ 
	vPos = CVec2( nX, nY ); 
}
void CCursor::LockPos( bool bLock ) 
{ 
	bPosLocked = bLock;
	AcquireLocal();
}
void CCursor::SetUpdateMode( const EUpdateMode _eUpdateMode ) 
{ 
	eUpdateMode = _eUpdateMode; 
	AcquireLocal();
	if ( eUpdateMode == ICursor::UPDATE_MODE_WINDOWS )
	{
		nSystemMode = nSystemModifier = -2;		// nothing applied yet under this mode
		nSystemScale = -1;
		ApplySystemCursor();
	}
	else if ( bSystemCursor )
	{
		// Back to the cursor drawn into the frame: the window's own pointer
		// goes down, or both would be on screen at once.
		NPlatform::ClearSystemCursorImage();
		NPlatform::ShowSystemCursor( false );
		bSystemCursor = false;
	}
}
void CCursor::OnSetCursor()
{
	ApplySystemCursor();
}
// The cursor art on the CPU, for handing to the window system. The texture the
// scene draws with lives on the GPU and the file it came from is the cheaper
// source anyway: these are 32x32 images read once per shape and then cached.
bool CCursor::LoadCursorImage( SCursorMode *pCursorMode )
{
	if ( pCursorMode == 0 ) 
		return false;
	if ( pCursorMode->pCursorImage != 0 ) 
		return true;
	IImageProcessor *pIP = GetImageProcessor();
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( (pIP == 0) || (pStorage == 0) ) 
		return false;
	// The file carries a quality suffix the lookup name does not. The window
	// system composites the pointer at its own size whatever the scene's
	// texture quality is, so the best file wins; the rest of the chain is there
	// for art that ships only part of the trio (a mod's cursor folder).
	static const char *pszSuffixes[] = { "_h.dds", "_c.dds", "_l.dds", ".dds" };
	for ( int nSuffix = 0; nSuffix < int(sizeof(pszSuffixes)/sizeof(pszSuffixes[0])); ++nSuffix )
	{
		const std::string szName = pCursorMode->szTextureName + pszSuffixes[nSuffix];
		if ( !pStorage->IsStreamExist( szName.c_str() ) ) 
			continue;
		CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
		if ( pStream == 0 ) 
			continue;
		CPtr<IDDSImage> pDDSImage = pIP->LoadDDSImage( pStream );
		if ( pDDSImage == 0 ) 
			continue;
		pCursorMode->pCursorImage = pIP->Decompress( pDDSImage );
		if ( pCursorMode->pCursorImage != 0 ) 
			return true;
	}
	return false;
}
// Hand the current shape to the window system, which then draws the pointer
// itself: it follows the mouse at the device's own rate instead of moving once
// per presented frame, which is all a cursor blitted into the scene can do (and
// on a display whose refresh the frame rate does not divide evenly, that blit
// lands in uneven steps - the judder this exists to remove).
void CCursor::ApplySystemCursor()
{
	if ( eUpdateMode != ICursor::UPDATE_MODE_WINDOWS ) 
		return;
	if ( !bShow || (pMode == 0) )
	{
		NPlatform::ShowSystemCursor( false );
		nSystemMode = nSystemModifier = -2;
		bSystemCursor = false;
		return;
	}
	// How much of the art to hand over, which depends on where the pointer is
	// being used. Over the map it is a tool - it has to sit on a soldier, and
	// GFX.Cursor.Scale is the fraction that comes back the size the art was
	// drawn for, whatever the window system does to it. Over the menus it is
	// just a pointer among buttons, so it keeps the size the window system
	// gives everything else, which on a machine with a magnified system
	// pointer is the size its owner asked every pointer to be.
	const int nScalePercent = GetGlobalVar( "AreWeInMission", 0 ) != 0 ?
		Clamp( GetGlobalVar( "GFX.Cursor.Scale", 100 ), 10, 100 ) : 100;
	// SetMode runs every frame of a mission (CWorldClient::SetAutoAction), so
	// everything below is gated on the shape - or the size - actually changing.
	if ( bSystemCursor && (nSystemMode == nCurrMode) && (nSystemModifier == nCurrModifier) &&
		 (nSystemScale == nScalePercent) ) 
		return;
	nSystemMode = nCurrMode;
	nSystemModifier = nCurrModifier;
	nSystemScale = nScalePercent;
	bSystemCursor = false;
	SCursorMode *layers[2] = { pMode, pModifier };
	const int nNumLayers = pModifier != 0 ? 2 : 1;
	// Hot-spot space: every layer is placed so that its own hot spot sits at
	// the origin, which is what makes the modifier line up with the shape it
	// modifies exactly as the two blits did.
	float fMinX = 0, fMinY = 0, fMaxX = 0, fMaxY = 0;
	bool bAnyLayer = false;
	for ( int nLayer = 0; nLayer < nNumLayers; ++nLayer )
	{
		if ( !LoadCursorImage( layers[nLayer] ) ) 
			continue;
		const float fX = -layers[nLayer]->vHotSpot.x;
		const float fY = -layers[nLayer]->vHotSpot.y;
		const float fW = float( layers[nLayer]->pCursorImage->GetSizeX() );
		const float fH = float( layers[nLayer]->pCursorImage->GetSizeY() );
		if ( !bAnyLayer ) 
		{
			fMinX = fX; fMinY = fY; fMaxX = fX + fW; fMaxY = fY + fH;
			bAnyLayer = true;
		}
		else
		{
			fMinX = Min( fMinX, fX ); fMinY = Min( fMinY, fY );
			fMaxX = Max( fMaxX, fX + fW ); fMaxY = Max( fMaxY, fY + fH );
		}
	}
	if ( !bAnyLayer )
	{
		// No art to hand over (a mod without cursors, an unreadable file):
		// Draw() blits the sprite as before and the window's pointer stays down.
		NPlatform::ShowSystemCursor( false );
		return;
	}
	// One layer is the common case by far (a modifier is only set for a few
	// actions), and it needs no composite at all: its own image is the cursor.
	IImage *pSingleLayer = 0;
	int nLoadedLayers = 0;
	for ( int nLayer = 0; nLayer < nNumLayers; ++nLayer )
	{
		if ( layers[nLayer] != 0 && layers[nLayer]->pCursorImage != 0 )
		{
			pSingleLayer = layers[nLayer]->pCursorImage;
			++nLoadedLayers;
		}
	}
	CPtr<IImage> pComposite;
	if ( nLoadedLayers == 1 )
		pComposite = pSingleLayer;
	else
	{
		const int nWidth = int( fMaxX - fMinX );
		const int nHeight = int( fMaxY - fMinY );
		pComposite = GetImageProcessor()->CreateImage( nWidth, nHeight );
		if ( pComposite == 0 )
		{
			NPlatform::ShowSystemCursor( false );
			return;
		}
		pComposite->Set( SColor(0) );
		for ( int nLayer = 0; nLayer < nNumLayers; ++nLayer )
		{
			const IImage *pSrc = layers[nLayer]->pCursorImage;
			if ( pSrc == 0 ) 
				continue;
			const int nOffsetX = int( -layers[nLayer]->vHotSpot.x - fMinX );
			const int nOffsetY = int( -layers[nLayer]->vHotSpot.y - fMinY );
			for ( int nY = 0; nY < pSrc->GetSizeY(); ++nY )
			{
				const SColor *pSrcLine = pSrc->GetLine( nY );
				SColor *pDstLine = pComposite->GetLine( nY + nOffsetY ) + nOffsetX;
				for ( int nX = 0; nX < pSrc->GetSizeX(); ++nX )
				{
					const int nAlpha = pSrcLine[nX].a;
					if ( nAlpha == 0 ) 
						continue;
					if ( nAlpha == 255 ) 
					{
						pDstLine[nX] = pSrcLine[nX];
						continue;
					}
					const int nInv = 255 - nAlpha;
					pDstLine[nX] = SColor(
						BYTE( nAlpha + pDstLine[nX].a * nInv / 255 ),
						BYTE( pSrcLine[nX].r + pDstLine[nX].r * nInv / 255 ),
						BYTE( pSrcLine[nX].g + pDstLine[nX].g * nInv / 255 ),
						BYTE( pSrcLine[nX].b + pDstLine[nX].b * nInv / 255 ) );
				}
			}
		}
	}
	// The full-resolution image rides along as the high-DPI variant, so a
	// pointer handed over smaller than its art still draws from every pixel of
	// it and stays crisp however far the window system magnifies it back.
	CPtr<IImage> pScaled;
	if ( nScalePercent < 100 )
	{
		const int nScaledX = Max( 1, pComposite->GetSizeX() * nScalePercent / 100 );
		const int nScaledY = Max( 1, pComposite->GetSizeY() * nScalePercent / 100 );
		pScaled = GetImageProcessor()->CreateScaleBySize( pComposite, nScaledX, nScaledY, ISM_TRIANGLE );
	}
	const IImage *pSize = pScaled != 0 ? (const IImage*)pScaled : (const IImage*)pComposite;
	const float fSizeScale = float( pSize->GetSizeX() ) / float( pComposite->GetSizeX() );
	bSystemCursor = NPlatform::SetSystemCursorImage( pSize->GetLFB(), pSize->GetSizeX(), pSize->GetSizeY(),
		pSize->GetSizeX() * sizeof(SColor), int( -fMinX * fSizeScale ), int( -fMinY * fSizeScale ),
		pScaled != 0 ? pComposite->GetLFB() : 0, pComposite->GetSizeX(), pComposite->GetSizeY(),
		pComposite->GetSizeX() * sizeof(SColor) );
	NPlatform::ShowSystemCursor( bSystemCursor );
	// The headless evidence channel: whether the window system took the art is
	// not otherwise observable from a screenshot, because a hardware cursor is
	// exactly the thing a screenshot of the scene does not contain.
	if ( getenv( "BK_CURSOR_TRACE" ) )
		fprintf( stderr, "BK_CURSOR_TRACE: mode=%d modifier=%d art %dx%d -> %dx%d (scale %d%%) hot %d,%d -> %s\n",
			nCurrMode, nCurrModifier, pComposite->GetSizeX(), pComposite->GetSizeY(),
			pSize->GetSizeX(), pSize->GetSizeY(), nScalePercent,
			int( -fMinX * fSizeScale ), int( -fMinY * fSizeScale ),
			bSystemCursor ? "system cursor" : "refused, drawing the sprite" );
}
void CCursor::Update()
{
	if ( bPosLocked )
		ResetSliders();
	const NTimer::STime timeAbs = GetSingleton<IGameTimer>()->GetAbsTime();

	vPos.x = Clamp( vPos.x + fSensitivity*pScrollX->GetDelta(), rcBounds.minx, rcBounds.maxx );
	vPos.y = Clamp( vPos.y + fSensitivity*pScrollY->GetDelta(), rcBounds.miny, rcBounds.maxy );
	if ( fabs2(vPos - vLastPos) > 1 ) 
	{
		vLastPos = vPos;
		timeLast = timeAbs;
	}
	if ( pMode && pMode->pVisObj ) 
		pMode->pVisObj->Update( timeAbs );
	if ( pModifier && pModifier->pVisObj ) 
		pModifier->pVisObj->Update( timeAbs );
}
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
	return &( pos->second );
}
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
bool CCursor::SetModifier( int nMode )
{
	if ( nMode == -1 ) 
	{
		pModifier = 0;
		nCurrModifier = -1;
		OnSetCursor();
		return true;
	}
	else if ( SCursorMode *pCursor = GetCursor(nMode) )
	{
		pModifier = pCursor;
		nCurrModifier = nMode;
		OnSetCursor();
		return true;
	}
	else
	{
		pModifier = 0;
		nCurrModifier = -1;
		return false;
	}
}
bool DrawCursor( SCursorMode *pMode, const CVec2 &vPos, IGFX *pGFX )
{
	if ( pMode == 0 ) 
		return false;
	if ( pMode->pVisObj ) 
	{
		const SSpriteInfo *pInfo = pMode->pVisObj->GetSpriteInfo();
		SGFXRect2 rect;
		CVec2 point = vPos - pMode->vHotSpot;
		rect.rect.Set( point.x + pInfo->rect.x1, point.y + pInfo->rect.y1, point.x + pInfo->rect.x2, point.y + pInfo->rect.y2 );
		rect.maps = pInfo->maps;
		rect.fZ = 0;
		pGFX->SetTexture( 0, pMode->pVisObj->GetTexture() );
		pGFX->SetShadingEffect( 3 );
		return pGFX->DrawRects( &rect, 1 );
	}
	else
	{
		SGFXRect2 rect;
		CVec2 point = vPos - pMode->vHotSpot;
		rect.rect.Set( point.x, point.y, point.x + pMode->rect.Width(), point.y + pMode->rect.Height() );
		rect.maps.Set( 0, 0, 1, 1 );
		rect.fZ = 0;
		pGFX->SetTexture( 0, pMode->pTexture );
		pGFX->SetShadingEffect( 3 );
		return pGFX->DrawRects( &rect, 1 );
	}
}
bool CCursor::Draw( interface IGFX *pGFX )
{
	if ( !bShow )
		return false;
	Update();
	// Re-checked every frame: leaving a mission for the intermission screens
	// changes how big the pointer should be, and on that path nothing else
	// necessarily sets a cursor mode. Costs two global reads when nothing has
	// changed, which is what the gate inside it is for.
	ApplySystemCursor();
	// The window system is drawing the pointer. Update() still runs above - the
	// scene's own cursor position, the sprite animation and the dwell a tooltip
	// waits out all come from it - but blitting the art as well would put a
	// second, frame-late copy of the cursor on the screen.
	if ( bSystemCursor )
		return false;
	const bool bRetVal = DrawCursor( pMode, vPos, pGFX );
	DrawCursor( pModifier, vPos, pGFX );
	return bRetVal;
}
void CCursor::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
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
	if ( saver.IsReading() )
	{
		SetMode( nCurrMode );
		SetModifier( nCurrModifier );
		AcquireLocal();
		SetPos( vPos.x, vPos.y );
	}
	return 0;
}
