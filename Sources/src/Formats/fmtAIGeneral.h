#if !defined(__FMT__AI_GENERAL__H__)
#define __FMT__AI_GENERAL__H__

#pragma ONCE

struct SAIGeneralParcelInfo 
{
	enum EPatchType
	{
		EPATCH_UNKNOWN		= 0,	// очаг обороны
		EPATCH_DEFENCE		= 1,	// очаг обороны
		EPATCH_REINFORCE	= 2,	// тут копится резерв
	};

	struct SReinforcePointInfo
	{
		CVec2 vCenter;
		WORD wDir;

		SReinforcePointInfo() : vCenter( VNULL2 ), wDir( 0 ) {}
		SReinforcePointInfo( const CVec2 &rvCenter, const WORD _wDir ) : vCenter( rvCenter ), wDir( _wDir ) {}
		SReinforcePointInfo( const SReinforcePointInfo &rReinforcePointInfo ) : vCenter( rReinforcePointInfo.vCenter ), wDir( rReinforcePointInfo.wDir ) {}
		SReinforcePointInfo& operator=( const SReinforcePointInfo &rReinforcePointInfo )
		{
			if( &rReinforcePointInfo != this )
			{
				vCenter = rReinforcePointInfo.vCenter;
				wDir = rReinforcePointInfo.wDir;
			}
			return *this;
		}
		
		virtual int operator&( IDataTree &ss );
		virtual int operator&( IStructureSaver &ss );
	};
	
	std::vector<SReinforcePointInfo> reinforcePoints;		// точки, в которые нужно поставить подкрепление
	int eType;													// тип 
	CVec2 vCenter;											// центр AI points
	float fRadius;											// размер AI points
	WORD wDefenceDirection;							// направление защиты 0 - 65535

	SAIGeneralParcelInfo() : eType( EPATCH_UNKNOWN ), vCenter( VNULL2 ), fRadius( 0 ), wDefenceDirection( 0 ) {}
	SAIGeneralParcelInfo( const SAIGeneralParcelInfo &rAIGeneralParcelInfo ) : reinforcePoints( rAIGeneralParcelInfo.reinforcePoints ), eType( rAIGeneralParcelInfo.eType ), vCenter( rAIGeneralParcelInfo.vCenter ), fRadius( rAIGeneralParcelInfo.fRadius ), wDefenceDirection( rAIGeneralParcelInfo.wDefenceDirection ) {}
	SAIGeneralParcelInfo& operator=( const SAIGeneralParcelInfo &rAIGeneralParcelInfo )
	{
		if( &rAIGeneralParcelInfo != this )
		{
			reinforcePoints = rAIGeneralParcelInfo.reinforcePoints;
			eType = rAIGeneralParcelInfo.eType;
			vCenter = rAIGeneralParcelInfo.vCenter;
			fRadius = rAIGeneralParcelInfo.fRadius;
			wDefenceDirection = rAIGeneralParcelInfo.wDefenceDirection;
		}
		return *this;
	}

	virtual int operator&( IStructureSaver &ss );
	virtual int operator&( IDataTree &ss );
};

struct SAIGeneralSideInfo
{
	std::vector<int> mobileScriptIDs;	// script IDs юнитов, которые принадлежат мобильному подкреплению
	std::vector<SAIGeneralParcelInfo> parcels;

	virtual int operator&( IDataTree &ss );
	virtual int operator&( IStructureSaver &ss );
};

struct SAIGeneralMapInfo
{
	std::vector<SAIGeneralSideInfo> sidesInfo;

	virtual int operator&( IDataTree &ss );
	virtual int operator&( IStructureSaver &ss );
};
#endif //#if !defined(__FMT__AI_GENERAL__H__)
