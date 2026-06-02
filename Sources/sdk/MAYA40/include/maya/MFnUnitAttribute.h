#ifndef LINUX
#pragma once
#endif
#ifndef _MFnUnitAttribute
#define _MFnUnitAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>



class MString;
class MTime;
class MAngle;
class MDistance;



/**
  Function set for unit attributes of dependency nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnUnitAttribute : public MFnAttribute 
{

	declareMFn(MFnUnitAttribute, MFnAttribute);

public:
	enum Type {
		kInvalid,
		kAngle,		// defaults as kDouble
		kDistance,	// defaults as kDouble
		kTime,		// defaults as MTime
		kLast
	};
	MObject 	create( const MString& fullName,
						const MString& briefName,
						MFnUnitAttribute::Type unitType,
						double defaultValue = 0.0,
						MStatus* ReturnStatus = NULL );
	MObject 	create( const MString& fullName,
						const MString& briefName,
						const MTime& defaultValue,
						MStatus* ReturnStatus = NULL );
	MObject 	create( const MString& fullName,
						const MString& briefName,
						const MAngle& defaultValue,
						MStatus* ReturnStatus = NULL );
	MObject 	create( const MString& fullName,
						const MString& briefName,
						const MDistance& defaultValue,
						MStatus* ReturnStatus = NULL );
	MFnUnitAttribute::Type	unitType( MStatus* ReturnStatus = NULL ) const;
	bool		hasMin( MStatus* ReturnStatus = NULL) const;
	bool		hasMax( MStatus* ReturnStatus = NULL) const;
	bool		hasSoftMin( MStatus* ReturnStatus = NULL) const;
	bool		hasSoftMax( MStatus* ReturnStatus = NULL) const;
	MStatus		getMin( double& minValue ) const;
	MStatus		getMin( MTime& minValue ) const;
	MStatus		getMin( MAngle& minValue ) const;
	MStatus		getMin( MDistance& minValue ) const;
	MStatus		getMax( double& maxValue ) const;
	MStatus		getMax( MTime& maxValue ) const;
	MStatus		getMax( MAngle& maxValue ) const;
	MStatus		getMax( MDistance& maxValue ) const;
	MStatus		getSoftMin( double& minValue ) const;
	MStatus		getSoftMin( MTime& minValue ) const;
	MStatus		getSoftMin( MAngle& minValue ) const;
	MStatus		getSoftMin( MDistance& minValue ) const;
	MStatus		getSoftMax( double& maxValue ) const;
	MStatus		getSoftMax( MTime& maxValue ) const;
	MStatus		getSoftMax( MAngle& maxValue ) const;
	MStatus		getSoftMax( MDistance& maxValue ) const;
	MStatus		setMin( double minValue );
	MStatus		setMin( const MTime &minValue );
	MStatus		setMin( const MAngle &minValue );
	MStatus		setMin( const MDistance &minValue );
	MStatus		setMax( double maxValue );
	MStatus		setMax( const MTime &maxValue );
	MStatus		setMax( const MAngle &maxValue );
	MStatus		setMax( const MDistance &maxValue );
	MStatus		setSoftMin( double minValue );
	MStatus		setSoftMin( const MTime &minValue );
	MStatus		setSoftMin( const MAngle &minValue );
	MStatus		setSoftMin( const MDistance &minValue );
	MStatus		setSoftMax( double maxValue );
	MStatus		setSoftMax( const MTime &maxValue );
	MStatus		setSoftMax( const MAngle &maxValue );
	MStatus		setSoftMax( const MDistance &maxValue );
	MStatus		setDefault( double defaultValue );
	MStatus		setDefault( const MTime& defaultValue );
	MStatus     setDefault( const MAngle& defaultValue );
	MStatus     setDefault( const MDistance& defaultValue );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnUnitAttribute */
