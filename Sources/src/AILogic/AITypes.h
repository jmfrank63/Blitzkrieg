#ifndef _AI_TYPES_H__
#define _AI_TYPES_H__
#pragma ONCE
// fixed underlying type: crosses the AILogic boundary via shared temp buffers
enum EDiplomacyInfo : unsigned int
{
	EDI_ENEMY		=	0x01,
	EDI_FRIEND	=	0x02,
	EDI_NEUTRAL = 0x04
};
// SSegment2Trench / SSoldier2Formation hold pointers and must not be packed:
// on x64 a packed alignment of 1 misaligns the 8-byte pointers (UB). Their
// x86 layout is unchanged (two 4-byte pointers pack naturally).
struct SSegment2Trench
{
	IRefCount *pSegment;									// ��������� ������� 
	IRefCount *pEntrenchment;							// ���� ����

	SSegment2Trench() : pSegment( 0 ), pEntrenchment( 0 ) { }
	SSegment2Trench( IRefCount *_pSegment, IRefCount *_pEntrenchment ) : pSegment( _pSegment ), pEntrenchment( _pEntrenchment ) { }
};
struct SSoldier2Formation
{
	IRefCount *pSoldier;
	IRefCount *pFormation;

	SSoldier2Formation() : pSoldier( 0 ), pFormation( 0 ) { }
	SSoldier2Formation( IRefCount *_pSoldier, IRefCount *_pFormation ) : pSoldier( _pSoldier ), pFormation( _pFormation ) { }
};
#pragma pack( 1 )
struct SAIVisInfo
{
	DWORD x : 14;													// x coord
	DWORD y : 14;													// y coord
	DWORD vis : 4;												// visibility: [0..5]
};
struct SAIPassabilityInfo
{
	DWORD x : 14;
	DWORD y : 14;
	DWORD pass : 4;
};
struct SMiniMapUnitInfo
{
	WORD x;
	WORD y;
	float z;
	BYTE player;

	SMiniMapUnitInfo() : x( 0 ), y( 0 ), z( 0.0f ), player( 0 ) { }
	SMiniMapUnitInfo( const WORD _x, const WORD _y, const float _z, const BYTE _player )
		: x( _x ), y( _y ), z( _z ), player( _player ) { }
};
#pragma pack()
// SShootArea's layout is identical packed or natural (4-byte members only).
// SShootAreas has a vtable pointer and a std::list — packing it is UB on x64.
struct SShootArea
{
	enum EShootAreaType
	{
		ESAT_BALLISTIC = 0,
		ESAT_AA = 1,
		ESAT_LINE = 2,
		ESAT_RANGE_AREA = 3,
	};
	EShootAreaType eType;
	
	CVec3 vCenter3D;
	float fMinR, fMaxR;

	WORD wStartAngle;
	WORD wFinishAngle;
	
	SShootArea()
		: vCenter3D( VNULL3 ), fMinR( 0.0f ), fMaxR( 0.0f ), 
			wStartAngle( 65535 ), wFinishAngle( 65535 ), eType( ESAT_LINE ) { }

	const DWORD GetColor() const
	{
		static const DWORD colors[] = { 0xff88ff88, 0xff8888ff, 0xffff8888, 0xff88ff88 };
		NI_ASSERT_T( int( eType ) < 4, NStr::Format( "Wrong type of area (%d)", (int)eType ) );
		return colors[eType];
	}

	const WORD GetMiniMapCircleColor() const
	{
		static const WORD colors[] = { 0xf0f0, 0xf00a, 0xff00, 0xf0f0 };
		NI_ASSERT_T( int( eType ) < 4, NStr::Format( "Wrong type of area (%d)", (int)eType ) );
		return colors[eType];
	}

	const WORD GetMiniMapSectorColor() const
	{
		static const WORD colors[] = { 0xf0f0, 0xf00a, 0xff00, 0xf0f0 };
		NI_ASSERT_T( int( eType ) < 4, NStr::Format( "Wrong type of area (%d)", (int)eType ) );
		return colors[eType];
	}
};

struct SShootAreas
{
	std::list<SShootArea> areas;

	virtual int STDCALL operator&( interface IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &areas );
		return 0;
	}
};
#endif // _AI_TYPES_H__
