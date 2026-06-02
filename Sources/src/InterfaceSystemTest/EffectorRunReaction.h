#ifndef _EffectorRunReaction_h_Included_
#define _EffectorRunReaction_h_Included_

#include "IUIInternal.h"
#include "Interface.h"
class CEffectorRunReaction : public IUIEffector
{
	OBJECT_COMPLETE_METHODS(CEffectorRunReaction)
	SUIStateCommand cmd;
	bool bFinished;
	bool bForward;
public:
	virtual void STDCALL SetElement( CWindow *pElement ) { }
	virtual bool STDCALL IsFinished() const { return bFinished; }
	virtual void STDCALL Configure( const struct SUIStateCommand &_cmd, interface IScreen *_pScreen ) 
	{ 
		cmd = _cmd;
		bFinished = false;
		bForward = true;
	}
	virtual void STDCALL Segment( const NTimer::STime timeDiff, interface IScreen *pScreen ) 
	{ 
		if ( bForward ? !cmd.szParam1.empty() : !cmd.szParam2.empty() )
			pScreen->RunReaction( bForward ? cmd.szParam1 : cmd.szParam2 );
		bFinished = true;
	}
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) { }
	virtual bool STDCALL NeedElement() const { return false; }
	virtual void STDCALL Reverse()
	{
		bForward = !bForward;
	}
};
class CEffectorRunReactionEC : public CEffectorCommandBase<CEffectorRunReaction, EUISM_RUN_REACTION>
{
	OBJECT_COMPLETE_METHODS(CEffectorRunReactionEC)
};
#endif //_EffectorRunReaction_h_Included_