#ifndef LINUX
#pragma once
#endif
#ifndef _MCommandResult
#define _MCommandResult

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MObject.h>

class MIntArray;
class MDoubleArray;
class MString;
class MStringArray;
class MVector;
class MVectorArray;
class MMatrix;




/**
  An MCommandResult collects the result returned by MGlobal::executeCommand.
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MCommandResult {

public:
    enum Type {	
	  kInvalid = 0,
	  kInt,
	  kIntArray,
	  kDouble,
	  kDoubleArray,
	  kString,
	  kStringArray,
	  kVector,
      kVectorArray,
      kMatrix,
      kMatrixArray
	};

    MCommandResult(MStatus* ReturnStatus = NULL );
	virtual         ~MCommandResult();
	Type            resultType(MStatus* ReturnStatus = NULL) const;
	MStatus         getResult( int& ) const;
    MStatus         getResult( MIntArray& ) const;
	MStatus         getResult( double& ) const;
	MStatus         getResult( MDoubleArray& ) const;
	MStatus         getResult( MString& ) const;
	MStatus         getResult( MStringArray& ) const;
	MStatus         getResult( MVector& ) const;
	MStatus         getResult( MVectorArray& ) const;
	MStatus         getResult( MDoubleArray& result,
							   int &numRows, int &numColumns) const;
protected:

private:

    const char* className() const;
    void *fResult;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MCommandResult */
