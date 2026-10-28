#ifndef __CAMERA_TURN_H__
#define __CAMERA_TURN_H__
#pragma ONCE
// The Game camera's quarter turns (Remastered only,
// docs/superpowers/specs/2026-10-26-camera-turn-design.md). Step k turns the
// view about the point at the screen centre so that the world looks turned by
// k quarter turns of (x, y) -> (-y, x), the turn that takes a building's side
// s to side s + 1 (M005's `<variant>_r1.._r3`). The step is a GFX.* global
// var like the zoom steps: a session setting no save carries, 0 (the 2003
// view) unless the turn key set it, and the camera, the terrain and the
// buildings all read it here.
//
// The header reads global vars through GetGlobalVar, which each module's
// StdAfx.h brings in (StreamIO/Globals.h).
#include "Remastered.h"
namespace NCameraTurn
{
	const char * const pszStepVar = "GFX.World.CameraTurn";

	inline int Step()
	{
		return GetGlobalVar( pszStepVar, 0 ) & 3;
	}
	// Whether the player may turn the camera: the player's own Remastered option
	// (NRemastered::pszOptionVar). Unlike NRemastered::Enabled() it holds in
	// multiplayer and in a replay of any mode, because the step is local view
	// state: it never enters the game state, the commands or the network stream.
	inline bool Allowed()
	{
		return GetGlobalVar( "Options.GamePlay.Remastered", 0 ) != 0;
	}
	// The ground point under the screen's centre: where the line of sight
	// through the camera's anchor (the point at height 0 under the centre) and
	// its eye meets the ground, the first meeting seen from the eye. The camera
	// turns about it, so raised ground under the centre stays there. pTerrain
	// is the scene's ITerrain (GetHeight takes world x, y).
	template <class TTerrain>
	inline CVec3 GroundUnderCentre( const CVec3 &vAnchor, const CVec3 &vEye, TTerrain *pTerrain )
	{
		const CVec3 vBase( vAnchor.x, vAnchor.y, 0 );
		CVec3 vUp = vEye - vBase;
		if ( vUp.z <= 0 )
			return vBase;
		vUp /= vUp.z;												// the line of sight per unit of height
		const float fTop = 2048.0f;
		float fAbove = fTop;
		float fBelow = -1.0f;
		for ( float fHeight = fTop; fHeight >= 0; fHeight -= 8.0f )
		{
			const CVec3 vOn = vBase + vUp * fHeight;
			if ( pTerrain->GetHeight( CVec2( vOn.x, vOn.y ) ) >= fHeight )
			{
				fBelow = fHeight;
				break;
			}
			fAbove = fHeight;
		}
		if ( fBelow < 0 )
			return vBase;
		for ( int i = 0; i < 20; ++i )
		{
			const float fMiddle = ( fAbove + fBelow ) / 2;
			const CVec3 vOn = vBase + vUp * fMiddle;
			if ( pTerrain->GetHeight( CVec2( vOn.x, vOn.y ) ) >= fMiddle )
				fBelow = fMiddle;
			else
				fAbove = fMiddle;
		}
		return vBase + vUp * fBelow;
	}
	// The step a multiplayer game starts at: the one that puts the player's own
	// corner (its camera anchor's offset from the map's middle, world units) at
	// the bottom of the screen. Screen down is world (1, -1) at step 0, and step
	// k shows an offset where step 0 shows it turned k times by (x, y) -> (-y, x);
	// the step whose turn points the corner most nearly down wins, step 0 on a tie.
	inline int StartStep( const float fDX, const float fDY )
	{
		int nBest = 0;
		float fBest = fDX - fDY;
		float fX = fDX, fY = fDY;
		for ( int nStep = 1; nStep < 4; ++nStep )
		{
			const float fTurned = fX;
			fX = -fY;
			fY = fTurned;
			if ( fX - fY > fBest + 1.0f )
			{
				nBest = nStep;
				fBest = fX - fY;
			}
		}
		return nBest;
	}
	// What the step adds to the camera's yaw. A camera turned by -90 degrees
	// about its anchor sees the world turned by +90 degrees.
	inline float YawOffset( const int nStep )
	{
		return -float( nStep ) * FP_PI2;
	}
	// The direction (0..65535, a quarter is 16384) whose 2003 picture shows a
	// unit or effect facing nDirection at the step: the view shows the world
	// turned by step quarters, so infantry's eight facings shift by two a step.
	inline int ViewDirection( const int nDirection, const int nStep )
	{
		return ( nDirection + nStep * 16384 ) & 0xffff;
	}
	// A ground offset the 2003 code lays out along the 45 degree view's screen
	// axes (right is (1, 1), towards the viewer (1, -1); an icon row, the
	// depth a passenger's bar stands out by) turned with the camera, so it
	// keeps its place on the screen.
	inline CVec3 ViewOffset( const CVec3 &vOffset, const int nStep )
	{
		return nStep == 0 ? vOffset : CQuat( YawOffset( nStep ), V3_AXIS_Z ).Rotate( vOffset );
	}
	// A map object's own facing in quarter turns: its map Direction (0..65535,
	// the AI's: 0 faces +y, 16384 -x, as a variant's 1.xml Direction 0 and 90)
	// to the nearest quarter. The 2003 game ignored it for buildings (their AI
	// object never turns), and every building in the shipped maps has 0.
	inline int Facing( const int nDirection )
	{
		return ( ( nDirection + 0x2000 ) >> 14 ) & 3;
	}
	// The turn of its variant a building is drawn with (Game and Map Editor):
	// its own facing plus the camera step. Turn k of a variant that shows side
	// s is side (s - 1 + k) mod 4 + 1, M005's convention.
	inline int DrawnTurn( const int nFacing, const int nStep )
	{
		return ( nFacing + nStep ) & 3;
	}
	// Whether a map object's own facing counts: with Remastered on, and always
	// in the Map Editor (the "editor" var its bridge sets), where facings are
	// made. Off, every object is drawn and laid as in 2003, whatever its map
	// Direction.
	inline bool FacingCounts()
	{
		return NRemastered::Enabled() || GetGlobalVar( "editor", 0 ) != 0;
	}
	// Whether an object's folder is one side of a prop the 2003 data holds from
	// four sides: `<family>\01` to `04`, side k + 1 being side k turned by
	// (x, y) -> (-y, x) (checked on the pictures in S04: the bench's seat and the
	// columns' leg and lamp turn that way; the France fence's wall is taken to
	// follow the same naming). The box's 01 to 04 are four different things, not
	// one from four sides.
	inline bool IsFourSided( const std::string &szPath )
	{
		static const char *pszFamilies[] = { "\\bench\\0", "\\e_francefence\\0", "\\bigcolumnwithleg\\0", "\\biglightcolumn\\0",
																				 "\\littlecolumnwithleg\\0", "\\littlelightcolumn\\0" };
		const int nLength = szPath.size();
		if ( nLength < 3 || szPath[nLength - 1] < '1' || szPath[nLength - 1] > '4' )
			return false;
		std::string szFolder = szPath.substr( 0, nLength - 1 );
		std::transform( szFolder.begin(), szFolder.end(), szFolder.begin(), ::tolower );
		for ( int i = 0; i < int( sizeof( pszFamilies ) / sizeof( pszFamilies[0] ) ); ++i )
		{
			const int nFamily = strlen( pszFamilies[i] );
			if ( int( szFolder.size() ) >= nFamily && szFolder.compare( szFolder.size() - nFamily, nFamily, pszFamilies[i] ) == 0 )
				return true;
		}
		return false;
	}
	// The folder of the side a prop of four sides shows at turn k: its family's
	// variant k further on.
	inline std::string FourSidedPath( const std::string &szPath, const int nTurn )
	{
		const int nLength = szPath.size();
		return szPath.substr( 0, nLength - 1 ) + char( '1' + ( szPath[nLength - 1] - '1' + nTurn ) % 4 );
	}
	// The folder of a variant's turn k: the variant itself at 0, else M005's
	// `<variant>_r<k>` where the data holds it (bk-views side writes it, from
	// the family's 2003 variant where one of the variant's colourway shows that
	// side), else the variant itself. The caller brings IDataStorage in;
	// pfnExists, where given, answers for the storage (the Game's memo of the
	// mission, Common/TurnPreload.h).
	typedef bool (*TExists)( const std::string &szName );
	inline bool StreamExists( const std::string &szName, TExists pfnExists )
	{
		return pfnExists != 0 ? pfnExists( szName ) : GetSingleton<IDataStorage>()->IsStreamExist( szName.c_str() );
	}
	inline std::string ModelPath( const std::string &szVariant, const int nTurn, TExists pfnExists = 0 )
	{
		if ( nTurn != 0 )
		{
			const std::string szTurned = NStr::Format( "%s_r%d", szVariant.c_str(), nTurn );
			if ( StreamExists( szTurned + "\\1.san", pfnExists ) )
				return szTurned;
		}
		return szVariant;
	}
	// The folder of a variant's turn k as camera step s shows it under the sun
	// fixed in the world (M006 round 2): `<variant>_r<k>_l<s>` where the data
	// holds it (bk-views side `<k>l<s>`: the building lit as its facing k - s
	// stands in the world), else ModelPath's, lit by the 2003 light fixed to
	// the camera. At step 0 the two are one.
	inline std::string LitModelPath( const std::string &szVariant, const int nTurn, const int nStep, TExists pfnExists = 0 )
	{
		if ( nStep != 0 )
		{
			const std::string szLit = NStr::Format( "%s_r%d_l%d", szVariant.c_str(), nTurn, nStep );
			if ( StreamExists( szLit + "\\1.san", pfnExists ) )
				return szLit;
		}
		return ModelPath( szVariant, nTurn, pfnExists );
	}
	// The quarter turns a map object stands turned by, its footprint, entrances
	// and fire slots with its picture (AI and Map Editor alike): its facing
	// where facings count and its picture turns with it - a building whose
	// variant has that turn's pictures (a variant without them keeps its 2003
	// picture and footprint), a prop of four sides. 0 for everything else.
	inline int StandingTurn( const std::string &szPath, const bool bBuilding, const int nDirection )
	{
		const int nFacing = Facing( nDirection );
		if ( nFacing == 0 || !FacingCounts() )
			return 0;
		if ( bBuilding )
			return ModelPath( szPath, nFacing ) != szPath ? nFacing : 0;
		return IsFourSided( szPath ) ? nFacing : 0;
	}
};
#endif // __CAMERA_TURN_H__
