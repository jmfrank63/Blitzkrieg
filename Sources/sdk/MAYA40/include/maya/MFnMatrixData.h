#ifndef LINUX
#pragma once
#endif
#ifndef _MFnMatrixData
#define _MFnMatrixData

#if defined __cplusplus



#include <maya/MFnData.h>



class MMatrix;
class MTransformationMatrix;



/**
  Create and manipulate MMatrix dependency node data
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnMatrixData : public MFnData 
{

	declareMFn(MFnMatrixData, MFnData);

public:
	bool					isTransformation(
										MStatus* ReturnStatus = NULL ) const;
	MTransformationMatrix	transformation(
										MStatus* ReturnStatus = NULL ) const;
	const MMatrix&			matrix( MStatus* ReturnStatus = NULL ) const;
	MStatus					set( const MTransformationMatrix& transformation );
	MStatus					set( const MMatrix& matrix );
	MObject					create( MStatus* ReturnStatus=NULL );
	MObject					create( const MMatrix&,
									MStatus* ReturnStatus=NULL );
	MObject					create( const MTransformationMatrix&,
									MStatus* ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnMatrixData */
