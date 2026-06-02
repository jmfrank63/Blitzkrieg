
#if !defined(AFX_EFFECTORMOVETO_H__61F18B52_A5E3_4375_B24C_A297C73DFF29__INCLUDED_)
#define AFX_EFFECTORMOVETO_H__61F18B52_A5E3_4375_B24C_A297C73DFF29__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IUIInternal.h"
#include "Interface.h"

class CEffectorMoveTo : public IUIEffector
{
	DECLARE_SERIALIZE
	OBJECT_COMPLETE_METHODS(CEffectorMoveTo)

	CPtr<CWindow> pElement;
	bool bFinished;
	CVec2 vMoveOffset;
	float fMoveTime;												// points per second
	CVec2 vSpeed;														// speed
	float fElapsedTime;											// time elapsed so far
	
	CVec2 vMoveFrom;
	bool bForward;

	const CVec2 GetCur() const;
public:
	virtual void STDCALL SetElement( CWindow *_pElement );
	virtual bool STDCALL IsFinished() const { return bFinished; }
	
	virtual void STDCALL Configure( const struct SUIStateCommand &cmd, interface IScreen *pScreen ) 
	{ 
		vMoveOffset = cmd.vParam;
		fMoveTime = LOWORD(cmd.dwParam1);
		bForward = true;
	}

	virtual void STDCALL Segment( const NTimer::STime timeDiff, interface IScreen *pScreen );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) { }
	virtual bool STDCALL NeedElement() const { return true; }
	virtual void STDCALL Reverse()
	{
		const CVec2 vCur( GetCur() );
		bForward = !bForward;
				
		const CVec2 vTmp( vMoveFrom );
		vMoveFrom = vMoveOffset + vMoveFrom;
		vMoveOffset = -vMoveOffset;
		
		fElapsedTime = fabs(vCur - vMoveFrom) / fabs( vSpeed );

		vSpeed.Negate();
		bFinished = false;
	}

};
class CEffectorMoveToEC : public CEffectorCommandBase<CEffectorMoveTo, EUISM_PS_MOVETO>
{
	OBJECT_COMPLETE_METHODS(CEffectorMoveToEC)
};

#endif // !defined(AFX_EFFECTORMOVETO_H__61F18B52_A5E3_4375_B24C_A297C73DFF29__INCLUDED_)
