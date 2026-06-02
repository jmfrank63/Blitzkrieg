#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNonAmbientLight
#define _MFnNonAmbientLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnLight.h>





/**
  Facilitate the creation and manipulation of non-ambient light nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnNonAmbientLight : public MFnLight 
{

	declareDagMFn(MFnNonAmbientLight,MFnLight);
public:
	short		decayRate( MStatus * ReturnStatus = NULL ) const;
	MStatus		setDecayRate( const short& decay_rate );


protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNonAmbientLight */
