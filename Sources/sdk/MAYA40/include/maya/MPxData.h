#ifndef LINUX
#pragma once
#endif
#ifndef _MPxData
#define _MPxData

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MString;
class MObject;
class MTypeId;
class MPlug;
class istream;
class ostream;
class MArgList;



/**

Read and write user-defined Data from both ASCII and binary formatted files.

Determine the type id and type name.

Access an instance of the creator for the user-defined type.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxData  
{
public:
	enum Type {
		kData,
		kGeometryData,
		kLast
	};

	MPxData();
	virtual ~MPxData();
	virtual MStatus			readASCII( const MArgList& argList,
									   unsigned& endOfTheLastParsedElement );
	virtual MStatus			readBinary( istream& in, unsigned length );
	virtual MStatus			writeASCII( ostream& out );
	virtual MStatus			writeBinary( ostream& out );
	virtual	void			copy( const MPxData& src) = 0;
	virtual MTypeId         typeId() const = 0;
	virtual MString         name() const = 0;

protected:



	friend class            MDataHandle;
	void*					instance;

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxData */
