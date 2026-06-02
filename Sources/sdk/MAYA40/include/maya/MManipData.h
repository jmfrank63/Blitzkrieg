#ifndef LINUX
#pragma once
#endif
#ifndef _MManipData
#define _MManipData
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MObject.h>





/**
This class encapulates manipulator data which is returned from the
manipulator conversion functions.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MManipData  
{
public:
	MManipData();
	~MManipData();
	MManipData(const MManipData &);
	MManipData(bool);
	MManipData(short);
	MManipData(int);
	MManipData(unsigned);
	MManipData(float);
	MManipData(double);
	MManipData(const MObject &);

	MManipData &	operator=(const MManipData &);
	MManipData &	operator=(bool);
	MManipData &	operator=(short);
	MManipData &	operator=(int);
	MManipData &	operator=(unsigned);
	MManipData &	operator=(float);
	MManipData &	operator=(double);
	MManipData &	operator=(const MObject &);

	bool			isSimple()		const;

	bool			asBool()		const;
	short			asShort()		const;
	int			asLong()		const;
	unsigned		asUnsigned()	const;
	float			asFloat()		const;
	double			asDouble()		const;
	MObject			asMObject()		const;


private:
	const char *className() const;
	bool		fIsSimple;
	double		fSimpleData;
	MObject		fComplexData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MManipData */
