#ifndef __ACKMANAGER_H__
#define __ACKMANAGER_H__

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Common\Actions.h"
#include "..\misc\HashFuncs.h"
#include "AIHashFuncs.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckManager
{
	DECLARE_SERIALIZE;

	typedef std::pair<CPtr<CAIUnit>, bool> CUnitBoredPresence;
	typedef std::unordered_map< int/*unit unique ID */, CUnitBoredPresence> CBoredPresence;
	typedef std::unordered_map<int, CBoredPresence> CAckTypeBoredPrecence;
	CAckTypeBoredPrecence bored;

	typedef std::vector<SAIAcknowledgment> CAcknowledgments;
	CAcknowledgments acknowledgements;		// ����������� Acknolegments ������ �� AI

	void AddAcknowledgment( const SAIAcknowledgment &ack );
public:
	CAckManager();
	virtual ~CAckManager();
	//������ ������� Acknowledgements
	void UpdateAcknowledgments( SAIAcknowledgment **pAckBuffer, int *pnLen );
	//������ ������� Bored Acknowledgements
	void UpdateAcknowledgments( SAIBoredAcknowledgement **pAckBuffer, int *pnLen );

	// ��� BORED acknowledgements
	void RegisterAsBored(	EUnitAckType eAck, class CAIUnit *pObject );
	void UnRegisterAsBored(	EUnitAckType eAck, class CAIUnit *pObject );

	
	void AddAcknowledgment(	EUnitAckType eAck, struct IRefCount *pObject, const int nSet = 0 );
	void Clear();
	
	void UnitDead( class CAIUnit *pObject );
};
#endif // __ACKMANAGER_H__

