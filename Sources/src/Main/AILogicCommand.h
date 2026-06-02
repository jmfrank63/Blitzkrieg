#ifndef __AI_LOGIC_COMMAND__
#define __AI_LOGIC_COMMAND__
#pragma ONCE
interface IAILogicCommand : public IRefCount
{
	virtual int STDCALL operator&( IDataTree &ss ) = 0;
	virtual void Execute( interface IAILogic *pAILogic ) = 0;
	virtual void Store( IDataStream *pStream ) = 0;
	virtual void Restore( IDataStream *pStream ) = 0;
	virtual bool NeedToBeStored() const = 0;
};
#endif // __AI_LOGIC_COMMAND__
