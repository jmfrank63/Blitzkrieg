#ifndef LINUX
#pragma once
#endif
#ifndef _MFnMatrixAttribute
#define _MFnMatrixAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>




class MString;
class MMatrix;
class MFloatMatrix;


/**
  Function set for matrix attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnMatrixAttribute : public MFnAttribute 
{

	declareMFn(MFnMatrixAttribute, MFnAttribute);

public:

	enum Type {
		kFloat,
		kDouble
	};
 
	MObject     create( const MString& fullName,
					    const MString& briefName, 
						Type matrixType = kDouble,
					    MStatus* ReturnStatus = NULL ); 
	MStatus     setDefault( const MMatrix & def );
	MStatus     setDefault( const MFloatMatrix & def ); 

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnMatrixAttribute */



