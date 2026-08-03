#ifndef __SCENE_H__
#define __SCENE_H__
#pragma ONCE
#include "fmtSprite.h"
enum
{
	SCENE_BASE_VALUE			= 0x10060000,
	SCENE_SCENE						= SCENE_BASE_VALUE + 1,
	SCENE_VISOBJ_BUILDER	= SCENE_BASE_VALUE + 2,
	SCENE_CAMERA					= SCENE_BASE_VALUE + 3,
	SCENE_CURSOR					= SCENE_BASE_VALUE + 4,
	SCENE_VISOBJ_SPRITE		= SCENE_BASE_VALUE + 5,
	SCENE_VISOBJ_MESH			= SCENE_BASE_VALUE + 6,
	SCENE_VISOBJ_EFFECT		= SCENE_BASE_VALUE + 7,
	SCENE_ICON_BAR				= SCENE_BASE_VALUE + 8,
	SCENE_ICON_TEXT				= SCENE_BASE_VALUE + 9,
	SCENE_ICON_PIC				= SCENE_BASE_VALUE + 10,
	SCENE_BOLD_LINE				= SCENE_BASE_VALUE + 11,
	SCENE_VISOBJ_SQUAD		= SCENE_BASE_VALUE + 12,
	SCENE_ICON_HP_BAR			= SCENE_BASE_VALUE + 13,
	SCENE_VISOBJ_FLASH		= SCENE_BASE_VALUE + 14,
	SCENE_VIDEO_PLAYER		= SCENE_BASE_VALUE + 15,
	SCENE_TRANSITION			= SCENE_BASE_VALUE + 16,
	SCENE_GAMMA_EFFECT		= SCENE_BASE_VALUE + 17,
	SCENE_GAMMA_FADER			= SCENE_BASE_VALUE + 18,
	SCENE_OPEN_VIDEO_PLAYER	= SCENE_BASE_VALUE + 19,

	SCENE_EFFECTOR_RECOIL = SCENE_BASE_VALUE + 20,
	SCENE_EFFECTOR_JOGGING= SCENE_BASE_VALUE + 21,
	SCENE_SOUNDSCENE			= SCENE_BASE_VALUE + 22,
	SCENE_SOUNDSCENE_SUBSTSOUND						= SCENE_BASE_VALUE + 23,
	SCENE_SOUNDSCENE_SOUND								= SCENE_BASE_VALUE + 24,
	SCENE_SOUNDSCENE_PLAYLIST							= SCENE_BASE_VALUE + 25,
	SCENE_SOUNDSCENE_SOUNDCELL						= SCENE_BASE_VALUE + 26,
	
	PFX_MANAGER		= SCENE_BASE_VALUE + 27,
	PFX_KEYBASED	= SCENE_BASE_VALUE + 28,
	PFX_KEYDATA		= SCENE_BASE_VALUE + 29,

	SCENE_EFFECTOR_MATERIAL = SCENE_BASE_VALUE + 30,

	PFX_COMPLEX_KEYDATA = SCENE_BASE_VALUE + 31,
	PFX_COMPLEX_SOURCE	= SCENE_BASE_VALUE + 32,
	
	SCENE_FORCE_DWORD = 0x7fffffff
};
enum ESoundMixType
{
	SFX_MIX_IF_TIME_EQUALS,
	SFX_MIX_SUBSTITUTE,
	SFX_MIX_ALWAYS,
	SFX_INTERFACE,
	
	SFX_MIX_ALL = 0x7fffffff,
};
enum ESoundCombatType
{
	ESCT_GENERIC						= 0,					// �� �������� �� ����� ��� � �� �������� ������ ���
	ESCT_COMBAT							= 1,
	ESCT_MUTE_DURING_COMBAT	= 2,
	
	ESCT_ASK_RPG						= 3,					// ��� ����� ��������� � ������
	ESCT_ALL								= 0x7fffffff,
};
enum ESoundSceneMode
{
	ESSM_INTERMISSION_INTERFACE,
	ESSM_INGAME,
};
enum EVisObjSelectionState
{
	SGVOSS_UNSELECTED		= 0,
	SGVOSS_PRESELECTED	= 1,
	SGVOSS_SELECTED			= 2,

	SGVOSS_FORCE_DWORD	= 0x7fffffff
};
enum ESceneObjectType
{
	SCENE_OBJECT_TYPE_CURSOR					= 1,
	SCENE_OBJECT_TYPE_CAMERA					= 2,
	SCENE_OBJECT_TYPE_FRAME_SELECTION = 3,
	SCENE_OBJECT_TYPE_STAT_SYSTEM			= 4,
	SCENE_OBJECT_TYPE_ICON						= 5,
	SCENE_OBJECT_TYPE_SQUAD						= 6,

	SCENE_OBJECT_TYPE_FORCE_DWORD = 0x7fffffff
};
struct SMechTrace
{
	NTimer::STime birthTime;
	NTimer::STime deathTime;
	DWORD dwColor;
	CVec3 vCorners[4];
	CVec3 vPos;
	int nNumTracks;
	float alpha;
	const CVec3& GetPosition() const { return vPos; }
	bool operator==( const SMechTrace &obj ) const { return birthTime == obj.birthTime && vPos == obj.vPos; }
};
struct SGunTrace
{
	NTimer::STime birthTime;
	NTimer::STime deathTime;
	CVec3 vPoints[4];
	CVec3 vDir;
	CVec3 vStart;
	const CVec3& GetPosition() const { return vStart; }
	bool operator==( const SGunTrace &obj ) const { return birthTime == obj.birthTime && vStart == obj.vStart; }
};
struct SBasicSpriteInfo
{
	enum EType { TYPE_NORMAL_SPRITE, TYPE_COMPLEX_SPRITE };
	const EType type;
	CVec3 pos;														// position in 3D space
	DWORD color, specular;								// color(/w alpha) and specular(/w fog)
	interface IGFXTexture *pTexture;			// sprite's texture
	CVec3 relpos;													// relative screen position (one frame valid only!!!)
	DWORD dwCheckFlags;										// screen check flags (low WORD) and priority (high WORD)
	SBasicSpriteInfo( EType _type ) : type( _type ), pTexture( 0 ), dwCheckFlags( 0 ) {  }
};
struct SSpriteInfo : public SBasicSpriteInfo
{
	CTRect<float> maps;										// texture mapping coords
	CTRect<short> rect;										// rect with respect to sprite's zero point
	float fDepthLeft;											// left depth
	float fDepthRight;										// right depth
	SSpriteInfo() : SBasicSpriteInfo( TYPE_NORMAL_SPRITE ), fDepthLeft( 0 ), fDepthRight( 0 ) {  }
};
struct SComplexSpriteInfo : public SBasicSpriteInfo
{
	const SSpritesPack::SSprite *pSprite;	// complex sprite data
	SComplexSpriteInfo() : SBasicSpriteInfo( TYPE_COMPLEX_SPRITE ), pSprite( 0 ) {  }
};
interface ISceneVisitor : public IRefCount
{
	virtual void STDCALL VisitSprite( const SBasicSpriteInfo *pObj, int nType, int nPriority ) = 0;
	virtual void STDCALL VisitMeshObject( interface IMeshVisObj *pObj, int nType, int nPriority ) = 0;
	virtual void STDCALL VisitParticles( interface IParticleSource *pObj ) = 0;
	virtual void STDCALL VisitSceneObject( interface ISceneObject *pObj ) = 0;
	virtual void STDCALL VisitText( const CVec3 &vPos, const char *pszText, interface IGFXFont *pFont, DWORD color ) = 0;
	virtual void STDCALL VisitBoldLine( CVec3 *corners, float fWidth, DWORD color ) = 0;
	virtual void STDCALL VisitMechTrace( const SMechTrace &trace ) = 0;
	virtual void STDCALL VisitGunTrace( const SGunTrace &trace ) = 0;
	virtual void STDCALL VisitUIRects( interface IGFXTexture *pTexture, const int nShadingEffect, struct SGFXRect2 *rects, const int nNumRects ) = 0;
	virtual void STDCALL VisitUIText( interface IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags ) = 0;
	virtual void STDCALL VisitUICustom( interface IUIElement *pElement ) = 0;
};
interface ISceneEffector : public IRefCount
{
	virtual bool STDCALL Update( const NTimer::STime &time ) = 0;
	virtual void STDCALL SetupTimes( const NTimer::STime &timeStart, const NTimer::STime &timeLife ) = 0;
};
interface ISceneMatrixEffector : public ISceneEffector
{
	virtual const SHMatrix& STDCALL GetMatrix() const = 0;
};
interface ISceneEffectorRecoil : public ISceneMatrixEffector
{
	virtual void STDCALL SetupData( float fAngle, const CVec3 &vAxis ) = 0;
};
interface ISceneEffectorJogging : public ISceneMatrixEffector
{
	virtual void STDCALL SetupData( float fWeightCoeff ) = 0;
};
interface ISceneMaterialEffector : public ISceneEffector
{
	virtual BYTE STDCALL GetAlpha() const = 0;
	virtual DWORD STDCALL GetSpecular() const = 0;
	virtual void STDCALL SetupData( BYTE maxAlpha, DWORD maxSpecular ) = 0;
};
interface ISceneObject : public IRefCount
{
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) = 0;
	virtual bool STDCALL Draw( interface IGFX *pGFX ) = 0;
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 ) = 0;
};
static const DWORD ICON_ALIGNMENT_LEFT			= 0x00000001;
static const DWORD ICON_ALIGNMENT_HCENTER		= 0x00000002;
static const DWORD ICON_ALIGNMENT_RIGHT			= 0x00000004;
static const DWORD ICON_ALIGNMENT_TOP				= 0x00000008;
static const DWORD ICON_ALIGNMENT_VCENTER		= 0x00000010;
static const DWORD ICON_ALIGNMENT_BOTTOM		= 0x00000020;
static const DWORD ICON_PLACEMENT_VERTICAL	= 0x00000040;
static const DWORD ICON_PLACEMENT_HORIZONTAL= 0x00000080;
interface ISceneIcon : public ISceneObject
{
	virtual void STDCALL SetPosition( const CVec3 &vPos ) = 0;
	virtual void STDCALL Reposition( const CVec3 &vPos ) = 0;
	virtual const CVec2 STDCALL GetSize() = 0;
	virtual void STDCALL SetColor( DWORD color ) = 0;
	virtual void STDCALL SetAlpha( BYTE alpha ) = 0;
	virtual void STDCALL Enable( bool bEnable ) = 0;
};
interface ISceneIconBar : public ISceneIcon
{
	virtual void STDCALL LockBarColor() = 0;
	virtual void STDCALL UnlockBarColor() = 0;
	virtual void STDCALL SetBorderColor( DWORD dwColor ) = 0;
	virtual void STDCALL ForceThinIcon() = 0;
	virtual void STDCALL SetSize( const CVec2 &vSize, bool bHorizontal = true ) = 0;
	virtual void STDCALL SetLength( float fPercentage ) = 0;
};
interface ISceneIconText : public ISceneIcon
{
	virtual void STDCALL SetFont( interface IGFXFont *pFont ) = 0;
	virtual void STDCALL SetText( const char *pszText ) = 0;
};
interface ISceneIconPic : public ISceneIcon
{
	virtual void STDCALL SetTexture( interface IGFXTexture *pTexture ) = 0;
	virtual void STDCALL SetRect( const CTRect<short> &rect, const CTRect<float> &maps ) = 0;
};
interface IVisObj : public ISceneObject
{
	virtual void STDCALL SetDirection( const int nDirection ) = 0;
	virtual void STDCALL SetPosition( const CVec3 &pos ) = 0;
	virtual void STDCALL SetPlacement( const CVec3 &pos, const int nDir ) = 0;
	virtual const CVec3& STDCALL GetPosition() const = 0;
	virtual int STDCALL GetDirection() const = 0;
	virtual void STDCALL SetOpacity( BYTE opacity ) = 0;
	virtual void STDCALL SetColor( DWORD color ) = 0;
	virtual void STDCALL SetSpecular( DWORD color ) = 0;
	virtual void STDCALL Select( EVisObjSelectionState state ) = 0;
	virtual EVisObjSelectionState STDCALL GetSelectionState() const = 0;
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift ) = 0;
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const RECT &rect ) = 0;
};
interface IObjVisObj : public IVisObj
{
	virtual void STDCALL SetScale( float sx, float sy, float sz ) = 0;
	virtual void STDCALL SetGameType( const DWORD dwType ) = 0;
	virtual void STDCALL SetAnimation( const int nAnim ) = 0;
	virtual interface IAnimation* STDCALL GetAnimation() = 0;
	virtual void STDCALL AddIcon( ISceneIcon *pIcon, int nID, const CVec3 &vAddValue, const CVec3 &vAddStep, 
		                            int nPriority, DWORD placement, bool bReposition = true ) = 0;
	virtual void STDCALL RemoveIcon( int nID, bool bReposition = true ) = 0;
	virtual ISceneIcon* STDCALL GetIcon( int nID ) const = 0;
	virtual bool STDCALL IsVisible() const = 0;
	virtual void STDCALL SetVisible( const bool bVisible ) = 0;
	virtual void STDCALL SetPriority( const int nPriority ) = 0;
};
interface ISpriteVisObj : public IObjVisObj
{
	virtual const SSpriteInfo* STDCALL GetSpriteInfo() const = 0;
	virtual interface IGFXTexture* STDCALL GetTexture() const = 0;
};
interface IMeshVisObj : public IObjVisObj
{
	virtual bool STDCALL DrawBB( interface IGFX *pGFX ) = 0;
	virtual bool STDCALL DrawShadow( interface IGFX *pGFX, const SHMatrix *pMatShadow, const CVec3 &vSunDir ) = 0;
	virtual void STDCALL SetAnim( interface IAnimation *pAnim ) = 0;
	virtual interface IGFXMesh* STDCALL GetMesh() const =0;
	virtual interface IGFXTexture* STDCALL GetTexture() const = 0;
	virtual const SHMatrix& STDCALL GetPlacement() const = 0;
	virtual const SHMatrix& STDCALL GetPlacement1() const = 0;
	virtual const SHMatrix STDCALL GetBasePlacement() = 0;
	virtual const SHMatrix* STDCALL GetMatrices() = 0;
	virtual const SHMatrix* STDCALL GetExtMatrices( const SHMatrix &matExternal ) = 0;
	virtual DWORD STDCALL CheckForViewVolume( const SPlane *pViewVolumePlanes ) = 0;
	virtual void STDCALL AddEffector( int nID, ISceneMatrixEffector *pEffector, int nPart = -1 ) = 0;
	virtual void STDCALL RemoveEffector( int nID, int nPart = -1 ) = 0;
	virtual void STDCALL AddMaterialEffector( ISceneMaterialEffector *pEffector ) = 0;
	virtual void STDCALL RemoveMaterialEffector() = 0;
};
interface IEffectVisObj : public IVisObj
{
	virtual void STDCALL SetStartTime( DWORD time ) = 0;
	virtual void STDCALL SetEffectDirection( const SHMatrix &matrix ) = 0;
	virtual bool STDCALL IsFinished( const NTimer::STime &time ) = 0;
	virtual void STDCALL CalibrateDuration( const NTimer::STime &timeDuration ) = 0;
	virtual void STDCALL Stop() = 0;
	virtual void STDCALL SetSuspendedState( bool bState ) = 0;
	virtual void STDCALL SetScale( const float fScale ) = 0;
	virtual const std::string& STDCALL GetSoundEffect() const = 0;
	virtual void STDCALL GetSpriteEffects( const SSpriteInfo ***ppEffects, int *pnNumEffects, bool bAll = false ) = 0;
	virtual void STDCALL GetParticleEffects( interface IParticleSource ***ppEffects, int *pnNumEffects, bool bAll = false ) = 0;
};
interface IFlashVisObj : public IVisObj
{
	virtual void STDCALL Setup( const NTimer::STime &timeStart, const NTimer::STime &timeDuration, const int nPower, const DWORD dwColor ) = 0;
};
interface IBoldLineVisObj : public ISceneObject
{
	virtual void STDCALL Setup( const CVec3 &vStart, const CVec3 &vEnd, float fWidth, DWORD color ) = 0;
};
interface ISquadVisObj : public ISceneObject
{
	struct SData
	{
		float fHealth;
		float fAmmo;
	};
	virtual void STDCALL SetPosition( const CVec2 &vPos ) = 0;
	virtual bool STDCALL UpdateData( SData *pObjects, int nNumObjects ) = 0;
	virtual bool STDCALL ToggleSelection() = 0;
};
interface IFrameSelection : public ISceneObject
{
	virtual void STDCALL Begin( const CVec3 &point ) = 0;
	virtual void STDCALL End() = 0;
	virtual void STDCALL Update( const CVec3 &point ) = 0;
	virtual void STDCALL Reset() = 0;
	virtual CVec3 STDCALL GetBeginPoint() = 0;
	virtual CVec3 STDCALL GetEndPoint() = 0;
	virtual bool STDCALL IsActive() = 0;
};
interface IStatSystem : public ISceneObject
{
	virtual void STDCALL AddEntry( const char *pszName ) = 0;
	virtual void STDCALL RemoveEntry( const char *pszName ) = 0;
	virtual void STDCALL UpdateEntry( const char *pszName, double val ) = 0;
	virtual void STDCALL UpdateEntry( const char *pszName, const char *pszVal ) = 0;
	virtual void STDCALL ResetEntry( const char *pszName ) = 0;
	virtual void STDCALL SetPosition( int nX, int nY ) = 0;
};
interface ICamera : public IRefCount
{
	enum { tidTypeID = SCENE_CAMERA };
	virtual void STDCALL Init( ISingleton *pSingleton ) = 0;
	virtual void STDCALL SetBounds( int x1, int y1, int x2, int y2 ) = 0;
	virtual void STDCALL SetPlacement( const CVec3 &vAnchor, float fDist, float fPitch, float fYaw ) = 0;
	virtual void STDCALL SetAnchor( const CVec3 &_vAnchor ) = 0;
	virtual const SHMatrix STDCALL GetPlacement() const = 0;
	virtual const CVec3 STDCALL GetPos() const = 0;
	virtual const CVec3 STDCALL GetAnchor() = 0;
	virtual void STDCALL GetLastPos( CVec3 *pvPos, NTimer::STime *pTime ) const = 0;
	virtual void STDCALL ResetSliders() = 0;
	virtual void STDCALL SetScrollSpeedX( float fSpeed ) = 0;
	virtual void STDCALL SetScrollSpeedY( float fSpeed ) = 0;
	virtual void STDCALL AddEarthquake( const CVec3 &vPos, const float fPower ) = 0;
	virtual void STDCALL Update() = 0;
};
#ifdef ICursor
#undef ICursor
#endif
interface ISceneCursor : public ISceneObject
{
	enum { tidTypeID = SCENE_CURSOR };
	enum EUpdateMode
	{
		UPDATE_MODE_WINDOWS = 1,
		UPDATE_MODE_INPUT		= 2,
	};
	virtual void STDCALL Init( ISingleton *pSingleton ) = 0;
	virtual void STDCALL Done() = 0;
	virtual void STDCALL Clear() = 0;
	virtual void STDCALL SetUpdateMode( const EUpdateMode _eUpdateMode ) = 0;
	virtual void STDCALL OnSetCursor() = 0;
	virtual void STDCALL RegisterMode( int nMode, const char *pszPictureName, int nSizeX, int nSizeY, int hotX, int hotY, WORD wResourceID ) = 0;
	virtual bool STDCALL SetMode( int nMode ) = 0;
	virtual bool STDCALL SetModifier( int nMode ) = 0;
	virtual void STDCALL Show( bool bShow ) = 0;
	virtual bool STDCALL IsShown() const = 0;

	virtual void STDCALL SetBounds( int x1, int y1, int x2, int y2 ) = 0;
	virtual void STDCALL Acquire( bool bAcqire ) = 0;
	virtual void STDCALL LockPos( bool bLock ) = 0;
	virtual void STDCALL SetPos( int nX, int nY ) = 0;
	virtual const CVec2 STDCALL GetPos() = 0;
	virtual void STDCALL ResetSliders() = 0;
	virtual void STDCALL GetLastPos( CVec2 *pvPos, NTimer::STime *pTime ) const = 0;
	virtual void STDCALL SetSensitivity( float fSensitivity ) = 0;
};
interface IVideoPlayer : public ISceneObject
{
	enum
	{
		PLAY_FROM_MEMORY	= 0x00000001,
		PLAY_FROM_HANDLE	= 0x00000002,
		PLAY_WITH_ALPHA		= 0x00000004,
		PLAY_INFINITE			= 0x00000008,
		PLAY_LOOPED				= 0x00000010,
		COPY_ALL					= 0x00000020
	};
	virtual void STDCALL SetTarget( interface IGFXTexture *pTexture, interface IGFX *pGFX ) = 0;
	virtual void STDCALL SetDstRect( const RECT &rcDstRect, bool bMaintainAspect ) = 0;
	virtual void STDCALL SetLoopMode( bool bLooped ) = 0;
	virtual int STDCALL GetCurrentFrame() const = 0;
	virtual bool STDCALL SetCurrentFrame( const int nFrame ) = 0;
	virtual void SetShadingEffect( const int nEffect, bool bStart ) = 0;
	virtual int STDCALL Play( const char *pszFileName, DWORD dwFlags, interface IGFX *pGFX, interface ISFX *pSFX ) = 0;
	virtual bool STDCALL Stop() = 0;
	virtual bool STDCALL Pause( bool bPause ) = 0;
	virtual bool STDCALL IsPlaying() const = 0;
	virtual int STDCALL GetLength() const = 0;
	virtual int STDCALL GetNumFrames() const = 0;
	virtual bool STDCALL GetMovieSize( CVec2 *pSize ) const = 0;
};
interface ITransition : public ISceneObject
{
	virtual int STDCALL Start( const char *pszVideoName, const DWORD dwAddFlags, const NTimer::STime &currTime, const bool bFadeIn ) = 0;
};
interface IGammaEffect : public ISceneObject
{
	virtual void STDCALL Init( const float fGammaR, const float fGammaG, const float fGammaB,
														 const NTimer::STime &timeStart, const NTimer::STime &timeDuration ) = 0;
};
enum
{
	SCENE_SHOW_HAZE							= 0,
	SCENE_SHOW_UNITS						= 1,
	SCENE_SHOW_OBJECTS					= 2,
	SCENE_SHOW_BBS							= 3,
	SCENE_SHOW_SHADOWS					= 4,
	SCENE_SHOW_EFFECTS					= 5,
	SCENE_SHOW_TERRAIN					= 6,
	SCENE_SHOW_GRID							= 7,
	SCENE_SHOW_WARFOG						= 8,
	SCENE_SHOW_DEPTH_COMPLEXITY	= 9,
	SCENE_SHOW_UI								= 10,
	SCENE_SHOW_NOISE						= 11,
	SCENE_SHOW_BORDER           = 12,

	SCENE_SHOW_FORCE_DWORD = 0x7fffffff
};
enum ESoundAddMode
{
	SAM_LOOPED_NEED_ID,										// �������� ID �����, ���� ����� �����������
	SAM_NEED_ID,													// �������� ID �����
	SAM_ADD_N_FORGET,											// �������� 0, ����� ���� ������ ����.
};
interface IScene : public IRefCount
{
	enum { tidTypeID = SCENE_SCENE };
	virtual bool STDCALL Init( ISingleton *pSingleton ) = 0;
	virtual void STDCALL SetSeason( const int nSeason ) = 0;
	virtual void STDCALL InitMusic(	const std::string &szPartyName ) = 0;
	virtual void STDCALL InitMapSounds( const struct CMapSoundInfo *pSound, int nElements )=0;
	virtual void STDCALL InitTerrainSound( interface ITerrain *pTerrain ) = 0;
	virtual void STDCALL SetTerrain( interface ITerrain *pTerrain ) = 0;
	virtual interface ITerrain * STDCALL GetTerrain() = 0;
	virtual bool STDCALL AddObject( IVisObj *pObject, EObjGameType eGameType, const SGDBObjectDesc *pDesc = 0 ) = 0;
	virtual bool STDCALL AddCraterObject( IVisObj *pObject, EObjGameType eGameType ) = 0;
	virtual bool STDCALL AddOutboundObject( IVisObj *pObject, EObjGameType eGameType ) = 0;
	virtual bool STDCALL AddOutboundObject2( IVisObj *pObject, EObjGameType eGameType ) = 0;
	virtual void STDCALL AddMechTrace( const SMechTrace &trace ) = 0;
	virtual void STDCALL AddGunTrace( const SGunTrace &trace ) = 0;
	virtual bool STDCALL AddSceneObject( ISceneObject *pObject ) = 0;
	virtual bool STDCALL RemoveObject( IVisObj *pObject ) = 0;
	virtual bool STDCALL RemoveSceneObject( ISceneObject *pObject ) = 0;
	virtual bool STDCALL MoveObject( IVisObj *pObject, const CVec3 &vPos ) = 0;
	virtual bool STDCALL AddUIScreen( interface IUIScreen *pUIScreen ) = 0;
	virtual bool STDCALL RemoveUIScreen( interface IUIScreen *pUIScreen ) = 0;
	virtual interface IUIScreen* STDCALL GetUIScreen() = 0;
	virtual void STDCALL SetMissionScreen( interface IUIScreen *pMissionScreen ) = 0;
	virtual interface IUIScreen* STDCALL GetMissionScreen() = 0;
	virtual bool STDCALL AddLine( IBoldLineVisObj *pLine ) = 0;
	virtual bool STDCALL RemoveLine( IBoldLineVisObj *pLine ) = 0;
	virtual void STDCALL SetAreas( const struct SShootAreas *areas, int nNumAreas ) = 0;
	virtual void STDCALL GetAreas( struct SShootAreas **areas, int *pnNumAreas ) = 0;


	virtual void STDCALL SetSoundPos( const WORD wID, const CVec3 &vPos ) = 0;
	virtual bool STDCALL IsSoundFinished( const WORD wID ) = 0;
	virtual void STDCALL RemoveSound( const WORD wID ) = 0;
	virtual WORD STDCALL AddSound( 	const char * pszName,
																	const CVec3 &vPos,
																	const ESoundMixType eMixType,
																	const ESoundAddMode eAddMode,
																	const ESoundCombatType eCombatType = ESCT_GENERIC,
																	const int nMinRadius = 0,
																	const int nMaxRadius = 0,
																	const unsigned int nTimeAfterStart = 0 ) = 0;

	virtual WORD STDCALL AddSoundToMap( const char *pszName, const CVec3 &vPos ) = 0;
	virtual void STDCALL RemoveSoundFromMap( const WORD	wInstanceID ) = 0;
	virtual void STDCALL UpdateSound( interface ICamera *pCamera ) = 0;
	virtual void STDCALL CombatNotify()=0;
	virtual void STDCALL SetSoundSceneMode( const enum ESoundSceneMode eSoundSceneMode ) = 0;

	virtual int STDCALL AddMeshPair( interface IGFXVertices *pVertices, interface IGFXIndices *pIndices, interface IGFXTexture *pTexture, int nShadingEffect, bool bTemporary ) = 0;
	virtual int STDCALL AddMeshPair2( void *vertices, int nNumVertices, int nVertexSize, DWORD dwFormat,
		                                WORD *indices, int nNumIndices, enum EGFXPrimitiveType ePrimitiveType,
																		IGFXTexture *pTexture, int nShadingEffect, bool bTemporary ) = 0;
	virtual bool STDCALL RemoveMeshPair( int nID ) = 0;
	virtual void STDCALL AddCircle( const CVec3 &vCenter, const float fRadius, const NTimer::STime &start, const NTimer::STime &duration ) = 0;
	virtual void STDCALL SetToolTip( interface IText *pText, const CVec2 &vPos, const CTRect<float> &rcOut, const DWORD dwColor = 0 ) = 0;
	virtual bool STDCALL TransferToGraveyard( IVisObj *pObject ) = 0;
	virtual void STDCALL SetVisibleObjects( IVisObj **ppObjects, int nNumObjects ) = 0;
	virtual void STDCALL SetWarFog( struct SAIVisInfo *pObjects, int nNumObjects ) = 0;
	virtual void STDCALL Clear() = 0;
	virtual int STDCALL GetNumSceneObjects() const = 0;
	virtual int STDCALL GetAllSceneObjects( std::pair<const SGDBObjectDesc*, CVec3> *pBuffer ) const = 0;
	virtual IFrameSelection* STDCALL GetFrameSelection() = 0;
	virtual IStatSystem* STDCALL GetStatSystem() = 0;
	virtual void STDCALL Draw( interface ICamera *pCamera ) = 0;
	virtual bool STDCALL ToggleShow( int nTypeID ) = 0;
	virtual void STDCALL Pick( const CVec2 &point, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible = true ) = 0;
	virtual void STDCALL Pick( const CTRect<float> &rcRect, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible = true ) = 0;
	virtual void STDCALL GetPos3( CVec3 *pPos, const CVec2 &pos, bool bOnZero = false ) = 0;
	virtual void STDCALL GetPos2( CVec2 *pPos, const CVec3 &pos ) = 0;
	virtual void STDCALL GetScreenCoords( const CVec3 &pos, CVec3 *vScreen ) = 0;
	
	virtual void STDCALL SetDirectionalArrow( const CVec3 &vStart, const CVec3 &vEnd, bool bDraw ) = 0;
	virtual void STDCALL SetClickMarker( const CVec3 &vPos ) = 0;
	virtual void STDCALL SetPosMarker( const CVec3 &vPos ) = 0;
	virtual void STDCALL SetRotationStartAngle( float fAngle, bool bRotate = true ) = 0;
	virtual void STDCALL FlashPosMarkers() = 0;
	virtual void STDCALL ResetPosMarkers() = 0;
	
	virtual void STDCALL SwitchWeather( bool bOn ) = 0;
	
	virtual bool STDCALL IsRaining() = 0;
	virtual void STDCALL SetWeatherQuality( float fCoeff ) = 0;

	virtual void STDCALL Reposition() = 0;
};
interface IVisObjBuilder : public IRefCount
{
	enum { tidTypeID = SCENE_VISOBJ_BUILDER };
	virtual bool STDCALL Init( ISingleton *pSingleton ) = 0;
	virtual IVisObj* STDCALL BuildObject( const char *pszName, const char *pszName2, EObjVisType type ) = 0;
	virtual ISceneObject* STDCALL BuildSceneObject( const char *pszName, ESceneObjectType eType, int nSubtype = -1 ) = 0;
	virtual const char* STDCALL GetEffectSound( const std::string &szName ) = 0;
	virtual bool STDCALL ChangeObject( IVisObj *pObj, const char *pszModelName, const char *pszTextureName, EObjVisType type ) = 0;
	virtual bool STDCALL ChangeSceneObject( ISceneObject *pObj, const char *pszName, ESceneObjectType eType, int nSubtype = -1 ) = 0;
	virtual void STDCALL Clear() = 0;
};
#define ICursor ISceneCursor
namespace NScene
{
	template <class TYPE>
		inline TYPE* BuildObject( IVisObjBuilder *pVOB, const char *pszName, EObjVisType eVisType )
		{
			return static_cast<TYPE*>( pVOB->BuildObject( pszName, "", eVisType ) );
		}
};
#endif // __SCENE_H__
