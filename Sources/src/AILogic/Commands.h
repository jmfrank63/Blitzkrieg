#ifndef __COMMANDS_H__
#define __COMMANDS_H__
#pragma ONCE
#include "..\Common\Actions.h"
#include "..\Misc\FreeIDs.h"
interface IStaticPathFinder;
interface IStaticPath;
struct SGroupPathInfo
{
	DECLARE_SERIALIZE;
public:
	CPtr<IStaticPath> pPath;
	CPtr<IStaticPathFinder> pPathFinder;
	int nSubGroup;
	BYTE cTileSize;
	BYTE aiClass;
};
class CAICommand : public IRefCount
{
	OBJECT_NORMAL_METHODS( CAICommand );

	DECLARE_SERIALIZE;

	static CQueuesSet<SGroupPathInfo> paths;
	static CFreeIds cmdIds;

	SAIUnitCmd unitCmd;
	int id;
	int nFlag;

	void InitCmdId();
public:
	CAICommand() : id( 0 ), nFlag( -1 ) { }
	CAICommand( const SAIUnitCmd &unitCmd );
	CAICommand( const CAICommand &cmd );
	
	static void Clear() { paths.Clear(); cmdIds.Clear(); }

	SAIUnitCmd& ToUnitCmd() { return unitCmd; }
	
	bool IsFromAI()const{return unitCmd.bFromAI;}
	void SetFromAI( const bool bFromAI ){ unitCmd.bFromAI = bFromAI; }

	~CAICommand() 
	{ 
		if ( id != 0 )
		{
			paths.DelQueue( id );
			cmdIds.AddToFreeId( id );
		}
	}

	const int GetID() const { return id; }

	interface IStaticPath* CreateStaticPath( class CCommonUnit *pUnit );

	const int GetFlag() const { return nFlag; }
	const void SetFlag( const int _nFlag ) { nFlag = _nFlag; }

	friend class CGroupLogic;
	friend class CStaticMembers;
};
#endif // __COMMANDS_H__
