
#if !defined(AFX_CUSTOMCHECK_H__4B1CA8EE_B9DF_40E8_A49E_866FE4D81D91__INCLUDED_)
#define AFX_CUSTOMCHECK_H__4B1CA8EE_B9DF_40E8_A49E_866FE4D81D91__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IMessageReaction.h"
class CCheckRunScript : public ICustomCheck
{
	DECLARE_SERIALIZE
	OBJECT_COMPLETE_METHODS(CCheckRunScript)

	std::string szScriptFunction;
public:
	CCheckRunScript() {  }
	CCheckRunScript( int TEST );
	int operator&( IDataTree &ss );
	virtual int STDCALL Check( interface IScreen *pScreen, class Script *pScript ) const;
};

#endif // !defined(AFX_CUSTOMCHECK_H__4B1CA8EE_B9DF_40E8_A49E_866FE4D81D91__INCLUDED_)
