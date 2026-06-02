#ifndef __BOLDLINEVISOBJ_H__
#define __BOLDLINEVISOBJ_H__
#pragma ONCE
class CBoldLineVisObj : public IBoldLineVisObj
{
	OBJECT_NORMAL_METHODS( CBoldLineVisObj );
	DECLARE_SERIALIZE;
	CVec3 vStart;													// start of the line
	CVec3 vEnd;														// end of the line
	float fWidth;													// lines width
	DWORD color;													// color
	bool bSetuped;												// is this line completelly setuped ?
	CVec3 corners[4];
	bool IsSetuped() const { return bSetuped; }
	void SetupLocal();
public:
	CBoldLineVisObj();
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	virtual bool STDCALL Draw( interface IGFX *pGFX ) { return false; }
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL Setup( const CVec3 &vStart, const CVec3 &vEnd, float fWidth, DWORD color );
};
#endif // __BOLDLINEVISOBJ_H__
