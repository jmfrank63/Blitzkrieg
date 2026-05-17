#ifndef __SPRITECOMPOSE_H__
#define __SPRITECOMPOSE_H__

#include "..\anim\animation.h"
#include "..\image\image.h"
#include "..\gfx\gfx.h"
#include "..\Formats\fmtAnimation.h"
#include "..\RandomMapGen\IB_Types.h"

//��������������� ����������
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAnimationDesc 
{
	// one direction description
	struct SDirDesc
	{
		std::vector<short> frames;					// frame indices for this direction sequence
		CVec2 ptFrameShift;									// shift for all frames, which participates in this dir sequence
		//
		int operator&( IDataTree &ss );
	};
	//
	std::string szName;										// animation name
	std::vector<SDirDesc> dirs;						// direction descritions
	std::unordered_map<int, CVec2> frames;			// each frame unique shift
	int nFrameTime;												// general one frame show time
	CVec2 ptFrameShift;										// general one frame shift
	float fSpeed;													// translation speed (for animations with movement)
	bool bCycled;													// cycled or one-shot animation
	std::vector<int> usedFrames;
	//
	SAnimationDesc() { usedFrames.reserve( 100 ); }
	//
	void AddUsedFrame( int frame )
	{
		if ( std::find(usedFrames.begin(), usedFrames.end(), frame) == usedFrames.end() )
			usedFrames.push_back( frame );
	}
	int GetUsedFrameIndex( int frame ) const
	{
		std::vector<int>::const_iterator pos = std::find( usedFrames.begin(), usedFrames.end(), frame );
		NI_ASSERT_TF( pos != usedFrames.end(), "no such frame in this animation", return -1 );
		return std::distance( usedFrames.begin(), pos );
	}
	//
	int operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* BuildAnimations( std::vector<SAnimationDesc> *pSrc, SSpriteAnimationFormat *pDst,
												 std::vector<std::string> &szFileNames,
												 bool bProcessImages = true, DWORD dwMinAlpha = 0 );

//CSpritesPackBuilder::CPackParameters parameters;
//parameters.push_back(CSpritesPackBuilder::SPackParameter() );
//parameters.back().pImage = ...;
//parameters.back().center.x = ...;
//parameters.back().center.y = ...;
bool BuildSpritesPack( const CSpritesPackBuilder::CPackParameters &rPackParameters, const std::string &rszSpritesPackFileName );
//CSpritesPackBuilder::SPackParameter parameter;
//parameter.pImage = ...;
//parameter.center.x = ...;
//parameter.center.y = ...;
bool BuildSpritesPack( const CSpritesPackBuilder::SPackParameter &rPackParameter, const std::string &rszSpritesPackFileName );

const CVec2 ComputeSpriteNewZeroPos( const interface IVisObj *pSprite, const CVec3 &vZero3, const CVec2 &vZero2Shift );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 Get3DPosition( const CVec3 &vSpritePos, const CVec2 &vPointPos, const SSpritesPack &rSpritesPack, int nSpriteIndex, const CVec2 &vCenter, IGFX *pGFX );
const CVec3 Get3DPosition( const CVec3 &vSpritePos, const CVec2 &vPointPos, const CVec2 &vCenter, const CImageAccessor &rImageAccessor, DWORD dwMinAlpha, IGFX *pGFX );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 GetOrigin2DPosition( const CVec2 &vOrigin );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetActionFromName( const std::string &szAnimName );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ComposeSingleSprite( const char *pszFileName, const char *pszResultingDir, const char *pszResName, bool bFence = false );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveCompressedTexture( IImage *pSrc, const char *pszDestFileName );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveCompressedShadow( IImage *pSrc, const char *pszDestFileName );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveTexture8888( IImage *pSrc, const char *pszDestFileName );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveTexture8888( const char *pszSrc, const char *pszDestFileName );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif		//__SPRITECOMPOSE_H__