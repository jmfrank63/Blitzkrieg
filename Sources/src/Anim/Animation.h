#ifndef __ANIMATION_H__
#define __ANIMATION_H__
#pragma ONCE
#include "../Formats/fmtSprite.h"
enum
{
	ANIM_BASE_VALUE								= 0x10020000,
	ANIM_ANIM											= ANIM_BASE_VALUE + 1,
	ANIM_ANIMATION_MANAGER				= ANIM_BASE_VALUE + 2,
	ANIM_SPRITE_ANIMATION					= ANIM_BASE_VALUE + 3,
	ANIM_SPRITE_ANIMATION_FORMAT	= ANIM_BASE_VALUE + 4,
	ANIM_MESH_ANIMATION						= ANIM_BASE_VALUE + 5,
	ANIM_MESH_SKELETON						= ANIM_BASE_VALUE + 6,
	ANIM_MESH_SKELETON_DATA				= ANIM_BASE_VALUE + 7,
	ANIM_MESH_ANIMATION_DATA			= ANIM_BASE_VALUE + 8,
	ANIM_COMPLEX_SPRITE						= ANIM_BASE_VALUE + 9,
	ANIM_COMPLEX_SPRITE_FORMAT		= ANIM_BASE_VALUE + 10,
	ANIM_EFFECTOR_RECOIL					= ANIM_BASE_VALUE + 50,
	ANIM_EFFECTOR_JOGGING					= ANIM_BASE_VALUE + 51,
	ANIM_EFFECTOR_LEVELING				= ANIM_BASE_VALUE + 52,

	ANIM_FORCE_DWORD = 0x7fffffff
};
enum EAnimation
{
	ANIMATION_IDLE					= 0,					// idle
	ANIMATION_IDLE_DOWN			= 1,					// idle, lying down (infantry only)
	ANIMATION_MOVE					= 2,					// move
	ANIMATION_CRAWL					= 3,					// crawl (move, lying down) (infantry only)
	ANIMATION_SHOOT					= 4,					// shoot
	ANIMATION_SHOOT_DOWN		= 5,					// shoot, lying down (infantry only)
	ANIMATION_SHOOT_TRENCH	= 6,					// shoot from trench (infantry only)
	ANIMATION_THROW					= 7,					// throw grenade (infantry only)
	ANIMATION_THROW_TRENCH	= 8,					// throw grenade from entrenchment (infantry only)
	ANIMATION_DEATH					= 9,					// dying
	ANIMATION_DEATH_DOWN		= 10,					// dying, lying down (infantry only)
	ANIMATION_USE						= 11,					// use (infantry only)
	ANIMATION_USE_DOWN			= 12,					// use, lying down (infantry only)
	ANIMATION_POINTING			= 13,					// pointing, officer (infantry only)
	ANIMATION_BINOCULARS		= 14,					// look at binoculars, officer (infantry only)
	ANIMATION_RADIO					= 15,					// talk at radio, officer (infantry only)
	ANIMATION_AIMING				= 16,					// aiming
	ANIMATION_AIMING_TRENCH	= 17,					// aiming from entrenchment (infantry only)
	ANIMATION_AIMING_DOWN		= 18,					// aiming, lying down (infantry only)
	ANIMATION_INSTALL				= 19,					// install for transporting (mech only)
	ANIMATION_UNINSTALL			= 20,					// uninstall from transporting (mech only)
	ANIMATION_INSTALL_ROT		= 21,					// install for rotation (mech only)
	ANIMATION_UNINSTALL_ROT	= 22,					// uninstall from rotation (mech only)
	ANIMATION_DEATH_FATALITY= 23,					// death with fatality - rare and nice-looking (mech only)
	ANIMATION_INSTALL_PUSH	= 24,					// install from pushing
	ANIMATION_UNINSTALL_PUSH= 25,					// uninstall for pushing
	ANIMATION_LIE						= 26,					// standing => lying cross
	ANIMATION_STAND					= 27,					// lying => standing cross
	ANIMATION_THROW_DOWN		= 28,					// throw grenade from lying
	ANIMATION_IDLE2					= 29,					// alternative idle
	ANIMATION_PRISONING			= 30,					// to get captive

	
	ANIMATION_LAST_ANIMATION,							// RR ��� ����� �������� ������ ��������. �� ��������� �������������� ���������
	ANIMATION_FORCE_DWORD = 0x7fffffff
};
#pragma pack( 2 )
struct SSpriteRect
{
	CTRect<float> maps;										// texture mapping
	CTRect<short> rect;										// shift with respect to sprite's center
	float fDepthLeft;											// left depthe
	float fDepthRight;										// right depthe
	SSpriteRect() : rect( 0, 0, 100, 100 ), maps( 0, 0, 1, 1 ), fDepthLeft( 0 ), fDepthRight( 0 ) {  }
};
#pragma pack()
interface IMatrixEffector : public IRefCount
{
	virtual bool STDCALL Update( const NTimer::STime &time ) = 0;
	virtual void STDCALL SetupTimes( const NTimer::STime &timeStart, const NTimer::STime &timeLife ) = 0;
	virtual const SHMatrix& STDCALL GetMatrix() const = 0;
};
interface IMatrixEffectorJogging : public IMatrixEffector
{
	virtual void STDCALL SetupData( float fPeriodX1, float fPeriodX2, float fAmpX1, float fAmpX2, float fPhaseX1, float fPhaseX2,
		                              float fPeriodY1, float fPeriodY2, float fAmpY1, float fAmpY2, float fPhaseY1, float fPhaseY2,
																	float fPeriodZ1, float fPeriodZ2, float fAmpZ1, float fAmpZ2, float fPhaseZ1, float fPhaseZ2 ) = 0;
};
interface IMatrixEffectorLeveling : public IMatrixEffector
{
	virtual void STDCALL SetupData( const CVec3 &vNormale, const NTimer::STime &currTime ) = 0;
	virtual const CVec3& STDCALL GetNormal() const = 0;
};
interface IAnimVisitor : public IRefCount
{
	virtual void STDCALL VisitSprite( const SSpriteRect *pSprite ) = 0;
	virtual void STDCALL VisitSprite( const SSpritesPack::SSprite *pSprite ) = 0;
	virtual void STDCALL VisitMesh( const SHMatrix *matrices, int nNumMatrices ) = 0;
};
interface IAnimation : public IRefCount
{
	virtual void STDCALL Visit( IAnimVisitor *pVisitor ) = 0;
	virtual void STDCALL SetTime( DWORD time ) = 0;
	virtual void STDCALL SetStartTime( DWORD time ) = 0;
	virtual void STDCALL SetAnimSpeedCoeff( float fCoeff ) = 0;
	virtual bool STDCALL SetAnimation( const int nAnim ) = 0;
	virtual int STDCALL GetAnimation() const = 0;
	virtual int STDCALL GetLengthOf( const int nAnim ) = 0;
};
interface ISpriteAnimation : public IAnimation
{
	virtual void STDCALL SetDirection( int nDirection ) = 0;
	virtual void STDCALL SetScale( float fScale ) = 0;
	virtual const SSpriteRect& STDCALL GetRect() = 0;
	virtual float STDCALL GetSpeed() const = 0;
	virtual void STDCALL SetFrameIndex( int nIndex ) = 0;
	virtual int STDCALL GetFrameIndex() = 0;
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CVec2 &point, CVec2 *pShift ) const = 0;
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CTRect<float> &rcRect ) const = 0;
	virtual const CScaleTimer& STDCALL GetScaleTimer() const = 0;
	virtual void STDCALL SetScaleTimer( const CScaleTimer &timer ) = 0;
};
interface IMeshAnimation : public IAnimation
{
	virtual int STDCALL GetNumNodes() const = 0;
	virtual const SHMatrix* STDCALL GetMatrices( const SHMatrix &matBase ) = 0;
	virtual void STDCALL GetBaseMatrix( const SHMatrix &matBase, SHMatrix * pResult ) = 0;
	virtual const SHMatrix* STDCALL GetCurrMatrices() const = 0;
	virtual void STDCALL AddProceduralNode( int nNodeIdx, DWORD currTime, DWORD startTime, DWORD endTime, float fValue ) = 0;
	virtual void STDCALL CutProceduralAnimation( const NTimer::STime &time, const int nModelPart = -1 ) = 0;
	virtual void STDCALL AddEffector( IMatrixEffector *pEffector, int nID, int nPart ) = 0;
	virtual void STDCALL RemoveEffector( int nID, int nPart ) = 0;
	virtual IMatrixEffector* STDCALL GetEffector( int nID, int nPart ) = 0;
};
interface IMeshAnimationEdit
{
	virtual void STDCALL GetAllNodeNames( const char **ppBuffer, int nBufferSize ) const = 0;
	virtual int STDCALL GetNumLocators() const = 0;
	virtual const int* STDCALL GetAllLocatorIndices() const = 0;
	virtual void STDCALL GetAllLocatorNames( const char **ppBuffer, int nBufferSize ) const = 0;
};
interface IAnimationManager : public ISharedManager
{
	enum { tidTypeID = ANIM_ANIMATION_MANAGER };
	virtual ISpriteAnimation* STDCALL GetSpriteAnimation( const char *pszName ) = 0;
	virtual IMeshAnimation* STDCALL GetMeshAnimation( const char *pszName ) = 0;
};
#endif // __ANIMATION_H__
