#ifndef _IReaction_h_included_
#define _IReaction_h_included_

interface IMessageReactionB2 : public IRefCount
{
	virtual bool STDCALL Execute( interface IScreen *pScreen, class Script *pScript ) const = 0;
	virtual int operator&( IDataTree &ss ) = 0;
};


interface ICustomCheck : public IRefCount
{
	virtual int STDCALL Check( interface IScreen *pScreen, class Script *pScript ) const = 0;
	virtual int operator&( IDataTree &ss ) = 0;
};
#endif //_IReaction_h_included_