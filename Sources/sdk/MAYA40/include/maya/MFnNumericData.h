#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNumericData
#define _MFnNumericData

#if defined __cplusplus



#include <maya/MFnData.h>



/**
 MFnNumericData allows the manipulation of numeric data for
 dependency node attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnNumericData : public MFnData 
{

	declareMFn(MFnNumericData, MFnData);

public:

	enum Type {
		kInvalid,
		kBoolean,
		kByte,
		kChar,
		kShort,
		k2Short,
		k3Short,
		kLong,
		kInt = kLong,
		k2Long,
		k2Int = k2Long,
		k3Long,
		k3Int = k3Long,
		kFloat,
		k2Float,
		k3Float,
		kDouble,
		k2Double,
		k3Double,
		kLast
    };

	MObject create( Type dataType, MStatus* ReturnStatus = NULL );

	Type numericType( MStatus* ReturnStatus = NULL );
	MStatus getData( short& val1, short& val2 );
	MStatus getData( int& val1, int& val2 );
	MStatus getData( float& val1, float& val2 );
	MStatus getData( double& val1, double& val2 );
	MStatus getData( short& val1, short& val2, short& val3 );
	MStatus getData( int& val1, int& val2, int& val3 );
	MStatus getData( float& val1, float& val2, float& val3 );
	MStatus getData( double& val1, double& val2, double& val3 );

	MStatus setData( short val1, short val2 );
	MStatus setData( int val1, int val2 );
	MStatus setData( float val1, float val2 );
	MStatus setData( double val1, double val2 );
	MStatus setData( short val1, short val2, short val3 );
	MStatus setData( int val1, int val2, int val3 );
	MStatus setData( float val1, float val2, float val3 );
	MStatus setData( double val1, double val2, double val3 );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNumericData */
