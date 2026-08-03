#ifndef __UNIT_STATES_H__
#define __UNIT_STATES_H__
#pragma ONCE
#include "../Common/Actions.h"
enum EUnitStateNames
{
	EUSN_ERROR											= 0,
	EUSN_REST												= 1,						// ��������
	EUSN_WAIT_FOR_PASSENGER					= 2,						// ��� �������� ���������
	EUSN_REST_ON_BOARD							= 3,						// ��������� ������ �����
	EUSN_LAND												= 4,						// ����������� �� �����
	EUSN_ENTER_TRANSPORT						= 5,						// ����������� � ���������
	EUSN_ENTER											= 6,						// ������ � ������
	EUSN_ENTER_ENTRENCHMENT					= 7,						// ������ � ����
	EUSN_REST_IN_BUILDING						= 8,						// ��������� ������ ������
	EUSN_REST_ENTRENCHMENT					= 9,						// ��������� ������ �����
	EUSN_GO_OUT											= 10,						// ������� �� ������
	EUSN_SWARM											= 11,						// swarm � �����
	EUSN_GO_OUT_ENTRENCHMENT				= 12,						// ��������� �� �����
	EUSN_ATTACK_STAT_OBJECT					= 13,						// ����� �������
	EUSN_PARADE											= 14,						// ����������� � ��������
	EUSN_AMBUSH											= 15,						// ������
	EUSN_RANGING										= 16,						// �������� �� ������������� �������
	EUSN_BUILD_FENCE								= 18,						// ������������� ������ � ���������
	EUSN_BUILD_ENTRENCHMENT					= 19,						// ������������� �����
	EUSN_ATTACK_UNIT								= 21,						// ����� �����
	EUSN_ATTACK_UNIT_IN_BUILDING		= 22,						// ����� ����� � ������
	EUSN_WAIT_TO_FORM								= 23,						// �������� ��� ����� ��������
	EUSN_BEING_TOWED								= 24,						// �����
	EUSN_GUN_CREW_STATE							= 25,						// ����������� �����
	EUSN_DIVE_BOMBING								= 26,						// ������������� � ������������
	EUSN_PARTROOP										= 27,						// ������������ � ��������
	EUSN_FLY_DEAD										= 28,						// ������� ������� �������
	EUSN_BUILD_LONGOBJECT						= 29,						// ������������� �������� �������(��������)
	EUSN_REPAIR_BRIDGE							= 30,
	EUSN_CLEAR_MINE									= 31,						// �������� ���� ����
	EUSN_MOVE												= 32,
	EUSN_USE_SPYGLASS								= 33,
	EUSN_BOMBARDMANET								= 34,						// suppressive fire
	EUSN_REPAIR_BUILDING						= 35,
	EUSN_REPAIR_UNIT								= 36,
	EUSN_RESUPPLY_UNIT							= 37,
	EUSN_HUMAN_RESUPPLY							= 38,
	EUSN_BUILD_BRIDGE								= 39,
	EUSN_PLACE_ANTITANK							= 40,
	EUSN_PLACE_MINE									= 41,
	EUSN_MOVE_TO_RESUPPLY_CELL			= 42,						// special state for general's transports
	EUSN_TURN_TO_POINT							= 43,
	EUSN_USE												= 44,
	EUSN_GUN_CAPTURE								= 45,
	EUSN_MOVE_TO_GRID								= 46,
	EUSN_HOOK_ARTILLERY							= 47,
};
inline bool IsRestState( const EUnitStateNames eStateName )
{
	return 
		eStateName == EUSN_REST || eStateName == EUSN_REST_ON_BOARD || 
		eStateName == EUSN_REST_ENTRENCHMENT || eStateName == EUSN_REST_IN_BUILDING;
}
inline bool IsRestCommand( const EActionCommand command )
{
	return 
		command == ACTION_COMMAND_IDLE_BUILDING ||
		command == ACTION_COMMAND_IDLE_TRENCH ||
		command == ACTION_COMMAND_IDLE_TRANSPORT ||
		command == ACTION_COMMAND_GUARD ;
}
enum ETryStateInterruptResult
{
	TSIR_YES_IMMIDIATELY,
	TSIR_YES_WAIT,
	TSIR_YES_MANAGED_ALREADY,
	TSIR_NO_COMMAND_INCOMPATIBLE,
};
interface IUnitState : public IRefCount
{
public:
	virtual void Segment() = 0;
	virtual EUnitStateNames GetName() { return EUSN_ERROR; }
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand ) = 0;

	virtual bool IsAttackingState() const = 0;
	virtual const CVec2 GetPurposePoint() const = 0;
};
interface IUnitAttackingState : public IUnitState
{
	virtual bool IsAttacksUnit() const = 0;
	virtual class CAIUnit* GetTargetUnit() const = 0;
};
#define CONVERT_OBJECT( CType, pObjTo, pObjFrom, msg )	\
	if ( pObjFrom == 0 ) break;														\
	CType *pObjTo = dynamic_cast<CType*>(pObjFrom);				\
	NI_ASSERT_T( pObjTo != 0, msg );											\
	if ( pObjTo == 0 ) break;

#define CONVERT_OBJECT_PTR( CType, pObjTo, pObjFrom, msg )		\
	if ( pObjFrom == 0 ) break;																	\
	CType *pObjTo = dynamic_cast_ptr<CType*>(pObjFrom);					\
	NI_ASSERT_T( pObjTo != 0, msg );														\
	if ( pObjTo == 0 ) break;
#endif // __UNIT_STATES_H__
