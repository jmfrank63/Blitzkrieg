#ifndef __COMMON_STRUCTS_H__
#define __COMMON_STRUCTS_H__
interface ISFXVisitor
{
	virtual int STDCALL VisitSound2D( class CSound2D *pSound ) = 0;
	virtual int STDCALL VisitSound3D( class CSound3D *pSound, const CVec3 &vPos ) = 0;
};
#endif // __COMMON_STRUCTS_H__