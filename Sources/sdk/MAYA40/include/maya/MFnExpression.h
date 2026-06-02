#ifndef LINUX
#pragma once
#endif
#ifndef _MFnExpression
#define _MFnExpression

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MString.h>



class MDoubleArray;
class MPtrBase;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnExpression : public MFnDependencyNode
{ 
	declareMFn( MFnExpression, MFnDependencyNode ); 

public:
	
	enum UnitConversion {
		kAll,
		kNone,
		kAngularOnly
	};

	MObject  		create( const MString & expression,
							MObject & object = MObject::kNullObj,
							MStatus * ReturnStatus = NULL );

	MStatus			getExpression( MString & expression );
	MStatus			setExpression( MString & expression );

	MStatus			getDefaultObject( MObject & object );
	MStatus			setDefaultObject( MObject & object );
	bool			isAnimated( MStatus * ReturnStatus = NULL );
	MStatus			setAnimated( bool value = false );

	MStatus			evaluate( MDoubleArray & result );

	UnitConversion  unitConversion( MStatus * ReturnStatus = NULL ) const;
	MStatus         setUnitConversion( UnitConversion conversion );
							

protected:

private:

}; 

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnExpression */
