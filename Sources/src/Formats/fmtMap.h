#ifndef __FMTMAP_H__
#define __FMTMAP_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Common\Actions.h"
//#include "..\Image\Image.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "fmtSound.h"
#include "fmtVSO.h"
#include "fmtUnitCreation.h"
#include "fmtAIGeneral.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int nNumRoadTypes = 4;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** map object format
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//static std::unordered_map<std::string, EActionCommand> logics;
struct SMapObjectInfo
{
	// visualization information
	std::string szName;										// database nickname
	CVec3 vPos;														// position in world space
	int nDir;															// direction
	// diplomacy information
	int nPlayer;													// player belonging
	// script information
	int nScriptID;												// script accessing ID
	// RPG information
	float fHP;														// current HP percentage [0..1] (to get real HP value, one must multiply this value to the MaxHP)
	//
	int nFrameIndex;											//
	//
	struct SLinkInfo
	{
		int nLinkID;												// ID of this list
		bool bIntention;										// bIntention == false => unit is inside of the linked object
		int nLinkWith;											// ID of the link to link with

		SLinkInfo() : nLinkID( 0 ), bIntention( false ), nLinkWith( 0 ) { }
		int operator&( IDataTree &ss );
	};

	SLinkInfo link;
	//std::string szLogic;

	SMapObjectInfo();

	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEntrenchmentInfo
{
	typedef std::vector<int> TSegment;
	std::vector<TSegment> sections;
	//
	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBattlePosition
{
	int nArtilleryLinkID;
	int nTruckLinkID;
	CVec2 vPos;

	SBattlePosition()
		: nArtilleryLinkID( 0 ), nTruckLinkID( 0 ), vPos( VNULL2 ) {}
	SBattlePosition( int _nArtilleryLinkID, int _nTruckLinkID, const CVec2 &rvPos )
		: nArtilleryLinkID( _nArtilleryLinkID ), nTruckLinkID( _nTruckLinkID ), vPos( rvPos ) {}
	SBattlePosition( const SBattlePosition &rReservePosition )
		: nArtilleryLinkID( rReservePosition.nArtilleryLinkID ), nTruckLinkID( rReservePosition.nTruckLinkID ), vPos( rReservePosition.vPos ) {}

	SBattlePosition& operator=( const SBattlePosition &rReservePosition )
	{
		if ( &rReservePosition != this )
		{
			nArtilleryLinkID = rReservePosition.nArtilleryLinkID;
			nTruckLinkID = rReservePosition.nTruckLinkID;
			vPos = rReservePosition.vPos;
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Artillery", &nArtilleryLinkID );
		saver.Add( "Truck", &nTruckLinkID );
		saver.Add( "Pos", &vPos );
		
		return 0;
	}
	
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &nArtilleryLinkID );
		saver.Add( 2, &nTruckLinkID );
		saver.Add( 3, &vPos );
		
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** map sound format
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapSoundInfo
{
	std::string szName;										// �������� ����� 
	CVec3 vPos;														// ����� ��������
	NTimer::STime timeRepeat;							// ����� ����� ���������
	NTimer::STime timeRepeatRandom;
	bool bMuteDuringCombat;								// ���� ������ ���������� �� ����� ���
	int nMinRadius;												// ��������� ���������� vis tiles
	int nMaxRadius;

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Sound", &szName );
		saver.Add( "Position", &vPos );
		saver.Add( "Interval", &timeRepeat );
		saver.Add( "IntervalRandom", &timeRepeatRandom );
		saver.Add( "MinRadius", &nMinRadius );
		saver.Add( "MaxRadius", &nMaxRadius );
		saver.Add( "MuteDuringCombat", &bMuteDuringCombat );
		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &szName );
		saver.Add( 2, &vPos );
		saver.Add( 3, &timeRepeat );
		saver.Add( 4, &timeRepeatRandom );
		saver.Add( 5, &nMinRadius );
		saver.Add( 6, &nMaxRadius );
		saver.Add( 6, &bMuteDuringCombat );
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSoundInfo 
{
	std::vector < SMapSoundInfo > sounds;

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Sounds", &sounds );
		return 0;
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &sounds );
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** terrain map format
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack( 1 )
struct SMainTileInfo
{
	BYTE tile;														// tile index
	BYTE noise;														// tile noise value
};
struct SCrossTileInfo
{
	enum
	{
		CROSS = 0x01,												// cross only
		NOISE = 0x02,												// noise only
		MIXED	= CROSS | NOISE								// mixed - cross + noise
	};
	//
	BYTE x, y;														// cross tile coords (in patch coords system)
	BYTE tile;														// cross tile index
	BYTE cross;														// cross mask
	BYTE flags;														// cross only, noise only, cross + noise
	//
	int operator&( IDataTree &ss );
};
struct SRoadTileInfo
{
	BYTE x, y;														// tile coords (in patch coords system)
	BYTE tile;                            // road tile index
	//
	SRoadTileInfo() {  }
	SRoadTileInfo( int x1, int y1, int t ) : x( x1 ), y( y1 ), tile( t ) {  }
	int operator&( IDataTree &ss );
};
struct SRoadItem
{
	enum
	{
		VERTICAL = 0,
		HORIZONTAL = 1,
	};
	CTRect<int> rect;											// road segment rect
	int nType;														// road type
	int nDir;															// direction ( VERTICAL = 0, HORIZONTAL = 1 )
	//
	int operator&( IDataTree &ss );
};
inline bool operator==( const SRoadItem &s1, const SRoadItem &s2 )
{
	return ( (s1.rect.top == s2.rect.top) && (s1.rect.left == s2.rect.left) &&
		       (s1.rect.bottom == s2.rect.bottom) && (s1.rect.right == s2.rect.right)	&&
			     (s1.nType == s2.nType) && (s1.nDir == s2.nDir) );
}
#pragma pack()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainPatchInfo
{
	typedef std::vector<SCrossTileInfo> CCrossesList;
	//typedef std::vector<SRoadTileInfo> CRoadsList;
	//
	static const int nSizeX;							// all patches uniform size X (in tiles)
	static const int nSizeY;							// all patches uniform size Y (in tiles)
	//
	short nStartX, nStartY;								// patch start coords in tiles
	CCrossesList basecrosses;							// base cross tiles. each tile coords are relative to patch (X,Y)
	std::vector<CCrossesList> layercrosses;	// cross tiles by layers (each layer consist of cross and noise tiles)
	CCrossesList noisecrosses;						// noise-only cross tiles
	//CRoadsList roads[nNumRoadTypes];		// road tiles. each tile coords are relative to patch (X,Y)
	float fMinHeight;											// minimal height in this patch
	float fMaxHeight;											// maximal height in this patch
	float fSubMinHeight[4];								// minimal height for each sub-patch in this patch
	float fSubMaxHeight[4];								// maximal height for each sub-patch in this patch
	//
	STerrainPatchInfo()
		: fMinHeight( 0 ), fMaxHeight( 0 ) {  }
	//
	bool HasCrosses() const { return !basecrosses.empty() || !noisecrosses.empty() || !layercrosses.empty(); }
	//
	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
struct SVertexAltitude
{
	float fHeight;												// height of the vertex => [-inf...+inf] :)
	BYTE shade;														// shading in the terrain vertex
	//
	SVertexAltitude() : fHeight( 0 ), shade( 255 ) {  }
	//
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Height", &fHeight );
		saver.Add( "Shade", &shade );
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainInfo
{
	typedef CArray2D<SVertexAltitude> TVertexAltitudeArray2D;
	//
	std::string szTilesetDesc;						// tileset descriptor name
	std::string szCrossetDesc;						// crosset descriptor name
	//std::string szRoadsetDesc;						// roadset descriptor name
	std::string szNoise;									// noise texture
	CArray2D<STerrainPatchInfo> patches;	// all patches of this terrain
	CArray2D<SMainTileInfo> tiles;				// all tiles of this terrain. ����� �� ����������� ������ �� ������� ����, ��� �� ������� ����� �� ������ ������ ����������� � ���������
	//std::vector<SRoadItem> roads;					// vector road information
	TVSOList rivers;											// rivers information
	TVSOList roads3;											// 3D roads information
	//CArray2D<float> heights;						// ������
	TVertexAltitudeArray2D altitudes;			// ���������� ������ � ��������� (�� 1 ������ ��� ������ �� ������� ���������!)
	//
	float X2World( float x ) const { return x; }
	float Y2World( float y ) const { return tiles.GetSizeY() - y - 1; }
	float X2Terra( float x ) const { return x; }
	float Y2Terra( float y ) const { return tiles.GetSizeY() - y - 1; }
	int AIY2Terra( int y ) const { return tiles.GetSizeY()*2 - y - 1; }
	void ToWorld( float *pfWX, float *pfWY, float x, float y ) const { *pfWX = X2World( x ); *pfWY = Y2World( y ); }
	void ToTerra( float *pfWX, float *pfWY, float x, float y ) const { *pfWX = X2Terra( x ); *pfWY = Y2Terra( y ); }
	// fill min/max heights in patches
	void FillMinMaxHeights();
	//
	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SReinforcementGroupInfo
{
	struct SGroupsVector
	{
		std::vector<int> ids;
		int operator&( IDataTree &ss );
		int operator&( IStructureSaver &ss );
	};

	std::unordered_map< int, SGroupsVector > groups;
	int GetGroupById( const int scriptID ) const
	{
		for( std::unordered_map< int, SGroupsVector >::const_iterator it = groups.begin(); it != groups.end(); ++it )
		{
			int i = 0;
			while ( i < it->second.ids.size() && it->second.ids[i] != scriptID )
				++i;

			if ( i < it->second.ids.size() )
				return it->first;
		}

		return -1;
	}
	//
	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SScriptArea
{
	const static std::string names[2];

	enum EAreaTypes{ EAT_RECTANGLE = 0, EAT_CIRCLE = 1 };
	EAreaTypes eType;

	std::string szName;

	CVec2 center;						// ����� �������
	CVec2 vAABBHalfSize;    // ��� ��������������, ������� �������������� ����������� ���� ���������
	float fR;								// ��� ����������
	
	SScriptArea() : eType( EAT_CIRCLE ), center( VNULL2 ), vAABBHalfSize( VNULL2 ), fR( 0.0f ) { }
	//
	int operator&( IDataTree &ss );
	int operator&( interface IStructureSaver &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
struct SUnitsLogics
{
	std::unordered_map<std::string, EActionCommand> logics;

	SUnitsLogics()
	{
		logics["Ambush"]	= ACTION_COMMAND_AMBUSH;
	}
};
/**/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIStartCommand
{
	EActionCommand cmdType;								// command type
	std::vector<int> unitLinkIDs;					// affected units

	int linkID;														// parameter - unit
	CVec2 vPos;														// parameter - point for ground pointing commands
	bool fromExplosion;										// parameter - for death from explosion
	float fNumber;												// parameter - numer;

	SAIStartCommand()
		: cmdType( ACTION_COMMAND_STOP ), linkID( 0 ), vPos( VNULL2 ), fromExplosion( false ), fNumber( 0.0f ) {}
	SAIStartCommand( EActionCommand _cmdType, const std::vector<int> &rUnitLinkIDs, int _linkID, const CVec2 rvPos, bool _fromExplosion, float _fNumber )
		: cmdType( _cmdType ), unitLinkIDs( rUnitLinkIDs ), linkID( _linkID ), vPos( rvPos ), fromExplosion( _fromExplosion ), fNumber( _fNumber ) {}
	SAIStartCommand( const SAIStartCommand &rStartCommand )
		: cmdType( rStartCommand.cmdType ), unitLinkIDs( rStartCommand.unitLinkIDs ), linkID( rStartCommand.linkID ), vPos( rStartCommand.vPos ), fromExplosion( rStartCommand.fromExplosion ), fNumber( rStartCommand.fNumber ) {}

	SAIStartCommand& operator=( const SAIStartCommand &rStartCommand )
	{
		if ( &rStartCommand != this )
		{
			cmdType = rStartCommand.cmdType;
			unitLinkIDs = rStartCommand.unitLinkIDs;
			linkID = rStartCommand.linkID;
			vPos = rStartCommand.vPos;
			fromExplosion = rStartCommand.fromExplosion;
			fNumber = rStartCommand.fNumber;
		}
		return *this;
	}

	//serializing
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Type", &cmdType );
		saver.Add( "Units", &unitLinkIDs );
		saver.Add( "ID", &linkID );
		saver.Add( "Pos", &vPos );
		saver.Add( "Exp", &fromExplosion );
		saver.Add( "Number", &fNumber );

		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &cmdType );
		saver.Add( 2, &unitLinkIDs );
		saver.Add( 3, &linkID );
		saver.Add( 4, &vPos );
		saver.Add( 5, &fromExplosion );
		saver.Add( 6, &fNumber );

		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** structure, which incorporates all map data
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadMapInfo
{
	typedef std::list<SAIStartCommand> TStartCommandsList;
	typedef std::list<SBattlePosition> TReservePositionsList;
	//
	STerrainInfo terrain;																					// terrain information
	std::vector<SMapObjectInfo> objects;													// map objects
	std::vector<SMapObjectInfo> scenarioObjects;									// map scenarioObjects
	std::vector<SEntrenchmentInfo> entrenchments;									// entrenchemnt
	std::vector< std::vector<int> > bridges;											// �����
	SReinforcementGroupInfo reinforcements;												// reinforcements
	std::string szScriptFile;																			// file with mission's scripts
	std::vector<SScriptArea> scriptAreas;													// ���������� �������
	CVec3 vCameraAnchor;																					// camera start position
	std::vector<CVec3> playersCameraAnchors;											// ������ ��� �������
	int	nSeason;																									// ����� (����/����/������ :)
	std::string szSeasonFolder;																		// ������� �� ������������� ����������� � ������
	std::vector<BYTE> diplomacies;																// ����������, 0, 1 - ���������� �������, 2 - ��������
	SUnitCreationInfo unitCreation;																// ��� ������� ������ � ����� ���� ����� ���������� ������� � ������ ������
	TStartCommandsList startCommandsList;													// �������, ������� �������� ������ ��� ������ �����
	TReservePositionsList reservePositionsList;										// ��������� ������� ��� ����������
	TMapSoundInfoList soundsList;																	// ��� ������ ����������� � �����
	std::string szForestCircleSounds;															// ��� �������� ������ ����
	std::string szForestAmbientSounds;														// ��� �������� ������ ����
	// CRAP{ ��� �������� ����� � ��������� ������ - ������ ��� E3
	std::string szChapterName;																		// ��� �����
	int nMissionIndex;																						// ����� ������ � ���� �����
	// CRAP}
	int nType;																										//��� ����� ( ��. CMapInfo )
	int nAttackingSide;																						//��������� ������� ( ��� ����������� ( 0 - 1 ) )
	
	// CRAP{ ���� �������
	SSoundInfo sounds;																						// ��� ������ ����������� � �����
	// CRAP}
	SAIGeneralMapInfo aiGeneralMapInfo;														// ���������� ��� ��������

	//MOD support
	std::string szMODName;																				//MOD Name
	std::string szMODVersion;																			//MOD Version
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FMTMAP_H__
