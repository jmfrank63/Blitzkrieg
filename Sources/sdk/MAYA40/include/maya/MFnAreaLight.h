#ifndef LINUX
#pragma once
#endif
#ifndef _MFnAreaLight
#define _MFnAreaLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnNonExtendedLight.h>



class MFltMatrix;



/**
  Facilitate the creation and manipulation of area light nodes
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnAreaLight : public MFnNonExtendedLight 
{

	declareDagMFn(MFnAreaLight,MFnNonExtendedLight);
public:
	MObject     create( bool UIvisible = true, MStatus * ReturnStatus = NULL );
	MObject     create( const MObject& parent, bool UIvisible = true,
						MStatus * ReturnStatus = NULL );
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnAreaLight */



