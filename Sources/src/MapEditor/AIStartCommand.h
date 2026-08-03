#if !defined(__AIStartCommand__MANIPULATOR__)
#define __AIStartCommand__MANIPULATOR__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Formats\FmtMap.h"
#include "..\Misc\Manipulator.h"

class CTemplateEditorFrame;

class CMutableAIStartCommand;
typedef std::list<CMutableAIStartCommand> TMutableAIStartCommandList;

class CAISCHelper
{
	bool isInitialized;

public:
	static const int DEFAULT_ACTION_COMMAND_INDEX;

	struct SCommand
	{
		std::string szName;
		int nID;
	};
	std::vector<SCommand> commands;

	CAISCHelper() : isInitialized( false ) {}
	
	bool IsInitialized(){ return isInitialized; }
	void Initialize();
};

class CMutableAIStartCommand : public SAIStartCommand
{
public:
	std::list<SMapObject*> pMapObjects;
	bool flag;
	
	virtual IManipulator* GetManipulator();

	CMutableAIStartCommand() : flag( false ) {}
	
	CMutableAIStartCommand( const SAIStartCommand &rAIStartCommand )
		: flag( false ),
			SAIStartCommand( rAIStartCommand ) {}
	operator SAIStartCommand() const
	{
		return SAIStartCommand( cmdType, unitLinkIDs, linkID, vPos, fromExplosion, fNumber );
	}
};

class CAIStartCommandManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( SAIStartCommandManipulator );

	CMutableAIStartCommand *pMutableObject;

public:
	CAIStartCommandManipulator();
	
	void SetCmdType( const variant_t &value );		
	void GetCmdType( variant_t *pValue, int nIndex = -1 );	
	
	void SetUnitNumber( const variant_t &value );
	void GetUnitNumber( variant_t *pValue, int nIndex = -1 );

	void SetVPosX( const variant_t &value );
	void GetVPosX( variant_t *pValue, int nIndex = -1 );

	void SetVPosY( const variant_t &value );
	void GetVPosY( variant_t *pValue, int nIndex = -1 );

	void SetNumber( const variant_t &value );
	void GetNumber( variant_t *pValue, int nIndex = -1 );

	inline void SetObject( CMutableAIStartCommand *_pMutableObject ) { pMutableObject = _pMutableObject; }
};
#endif // !defined(__AIStartCommand__MANIPULATOR__)
